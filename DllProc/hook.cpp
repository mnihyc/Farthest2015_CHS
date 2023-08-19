#include "hook.h"

HOOKJMP::HOOKJMP(LPVOID myF, LPVOID orgF, DWORD size)
{
	this->myF = myF;
	this->orgF = orgF;
	this->size = size;
	this->hooked = false;
	this->hook(myF, orgF);
}

bool HOOKJMP::hook(LPVOID myF, LPVOID orgF, DWORD size)
{
	if (myF != NULL)
		this->myF = myF;
	if (orgF != NULL)
		this->orgF = orgF;
	if (size != 0)
		this->size = size;
	if (this->hooked || this->myF == NULL || this->orgF == NULL || this->size == 0)
		return false;
	// copy this->size bytes for original command, extra 5 for JMPCODE
	BYTE* arr = (BYTE*)VirtualAlloc(NULL, this->size + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!arr)
		return false;
	this->gadF = arr;
	if (!ReadProcessMemory(GetCurrentProcess(), this->orgF, arr, this->size, NULL))
		return false;
	// overwrite gadget address
	JMPCODE code;
	code.addr = (DWORD)this->orgF - (DWORD)arr - 5;
	if (!WriteProcessMemory(GetCurrentProcess(), &arr[this->size], &code, 5, NULL))
		return false;
	// overwrite original function
	code.addr = (DWORD)this->myF - (DWORD)this->orgF - 5;
	if (!HOOK::patch((DWORD)this->orgF, (BYTE*)&code, 5))
		return false;
	BYTE NOPs[1024] = { 0x90 };
	if (!HOOK::patch((DWORD)this->orgF + 5, NOPs, this->size - 5))
		return false;
	// finished
	this->hooked = true;
	return true;
}

bool HOOKJMP::unhook(LPVOID orgF)
{
	if (orgF != NULL)
		this->orgF = orgF;
	if (!this->hooked || this->orgF == NULL)
		return false;
	WriteProcessMemory(GetCurrentProcess(), this->orgF, this->gadF, this->size, NULL);
	VirtualFree(this->gadF, 0, MEM_RELEASE); this->gadF = NULL;
	this->hooked = false;
	return true;
}

LPVOID HOOKJMP::get()
{
	return this->gadF;
}

HOOKIAT::HOOKIAT(LPVOID myF, LPCWSTR orgName, LPVOID orgF)
{
	this->myF = myF;
	this->orgName = orgName;
	this->orgF = orgF;
	this->hooked = false;
	this->hook(myF, orgName, orgF);
}

bool HOOKIAT::hook(LPVOID myF, LPCWSTR orgName, LPVOID orgF)
{
	if (myF != NULL)
		this->myF = myF;
	if (orgName != NULL)
		this->orgName = orgName;
	if (orgF != NULL)
		this->orgF = orgF;
	if (this->hooked || this->myF == NULL || this->orgName == NULL)
		return false;
	this->hooked = true;
	return this->replace(this->myF);
}

bool HOOKIAT::unhook(LPCWSTR orgName)
{
	if (orgName != NULL)
		this->orgName = orgName;
	if (!this->hooked || this->orgName == NULL)
		return false;
	this->hooked = false;
	return this->replace(this->orgF);
}

LPVOID HOOKIAT::get()
{
	return this->orgF;
}

bool HOOKIAT::compare(LPCWSTR ws, LPCSTR mb)
{
	if (ws == NULL || mb == NULL)
		return false;
	int len = wcslen(ws);
	if (len != strlen(mb))
		return false;
	for (int i = 0; i < len; i++)
	{
		if (ws[i] != mb[i])
			return false;
	}
	return true;
}

bool HOOKIAT::replace(LPVOID rpF)
{
	bool suc = false;
	LPVOID imageBase = GetModuleHandleA(NULL);
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
		if (_stricmp(libraryName, "DllProc.dll")!=0 && (library = LoadLibraryA(libraryName)))
		{
			PIMAGE_THUNK_DATA originalFirstThunk = NULL, firstThunk = NULL;
			originalFirstThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)imageBase + importDescriptor->OriginalFirstThunk);
			firstThunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)imageBase + importDescriptor->FirstThunk);
			
			while (originalFirstThunk->u1.AddressOfData != NULL && !IMAGE_SNAP_BY_ORDINAL(originalFirstThunk->u1.Ordinal))
			{
				functionName = (PIMAGE_IMPORT_BY_NAME)((DWORD_PTR)imageBase + originalFirstThunk->u1.AddressOfData);
				
				// find function by name
				if (this->compare(this->orgName, functionName->Name))
				{
					if (!this->orgF)
						this->orgF = (LPVOID)(firstThunk->u1.Function);
					if (!HOOK::patch((DWORD_PTR)&firstThunk->u1.Function, (BYTE*)&rpF, sizeof(DWORD_PTR)))
						return false;
					suc = true;
				}
				
				++originalFirstThunk;
				++firstThunk;
			}
		}
		++importDescriptor;
	}
	
	return suc;
}

bool HOOK::patch(DWORD_PTR addr, const BYTE* arr, DWORD size)
{
	DWORD oldProtect = 0;
	if (!VirtualProtect((LPVOID)addr, size, PAGE_READWRITE, &oldProtect))
		return false;
	BYTE* pad = (BYTE*)addr;
	for (unsigned i = 0; i < size; i++)
		pad[i] = arr[i];
	if (!VirtualProtect((LPVOID)addr, size, oldProtect, &oldProtect))
		return false;
	return true;
}

bool HOOK::patch(DWORD_PTR addr, const BYTE data, DWORD size)
{
	DWORD oldProtect = 0;
	if (!VirtualProtect((LPVOID)addr, size, PAGE_READWRITE, &oldProtect))
		return false;
	BYTE* pad = (BYTE*)addr;
	for (unsigned i = 0; i < size; i++)
		pad[i] = data;
	if (!VirtualProtect((LPVOID)addr, size, oldProtect, &oldProtect))
		return false;
	return true;
}
