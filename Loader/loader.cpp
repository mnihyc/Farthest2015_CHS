#include "resource.h"
#include "../Static/debug.h"
#include "../Static/pe.h"
#include <windows.h>
#include <tuple>
#include <ctime>
#include <stdio.h>

// define macros
#define TEMPPATH_APPEND L"Farthest2015_CHS-" // abandoned
#define EXE_FILENAME L"Farthest2015_CHS.exe"
#define ORIG_EXEHASH 0x629dee55cadf29c0

// global debug
DEBUG dbg{ L"Loader", L"D:/Projects/Farthest2015_CHS/Release/d_loader.txt", true};

// for easier coding
using std::wstring;
using std::tuple;
using std::tie;

// main procedure
void MainProc();
// simple hash function
__int64 LBuffHash(const void *buf, int len);
// read file into buffer
DWORD LReadFileToBuf(const wstring& path, void*& buf);
// write buffer to file
bool LWriteBufToFile(const wstring& path, const void* buf, int len);
// check if file exists
bool LFileExists(const wstring& path);
/* // abandoned */
// get temporary directory
tuple<bool, wstring> LGetTempDir();
// create directory if not exists
bool LCreateDir(const wstring& dir);
// empty directory
bool LEmptyDir(const wstring& dir);
// generate random string
wstring LGenRandomString(int len=8);
// load resource into file
bool LLoadResource(UINT uid, const wstring& filename);
// inject remote dll
tuple<bool, wstring, HANDLE> LInjectDll(const wstring& dllpath, const wstring& exepath);
/* */

// entry point wWinMain
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	MainProc();
	return 0;
}

void MainProc()
{
	// remember to delete
	SetCurrentDirectory(L"D:/Projects/Farthest2015_CHS/Release/");
	
	STARTUPINFO sii = { 0 }; sii.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pii = { 0 };
	if (!CreateProcess(EXE_FILENAME, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &sii, &pii))
		dbg.FatalPopup(L"Unable to create process " + wstring(EXE_FILENAME));
	return;
	
	bool suc = false;
	LPVOID lpBuffer{}; DWORD dwLength;
	
	if (!LFileExists(EXE_FILENAME))
		suc = true;
	else
	{
		dwLength = LReadFileToBuf(EXE_FILENAME, lpBuffer);
		if (dwLength == 0)
			dbg.FatalPopup(L"Unable to read file " + wstring(EXE_FILENAME));
		if (LBuffHash(lpBuffer, dwLength) != ORIG_EXEHASH)
		{
			int ret = MessageBox(NULL, L"Original program seems to be already modified, would you like to\n" \
				L"ABORT, exiting immediately\n" \
				L"RETRY, overwriting the existing program to ensure the ability\n" \
				L"IGNORE, continuing which may cause unexpected behaviours\n" \
				, L"Warning", MB_ABORTRETRYIGNORE | MB_ICONEXCLAMATION);
			if (ret & IDABORT)
				ExitProcess(0);
			else if (ret & IDRETRY)
				suc = true;
		}
	}
	if (suc)
	{
		PLoadResource(IDR_FILE_EXE1, lpBuffer, dwLength);
		if (!LWriteBufToFile(EXE_FILENAME, lpBuffer, dwLength))
			dbg.FatalPopup(L"Unable to write file " + wstring(EXE_FILENAME));
	}
	STARTUPINFO si = { 0 }; si.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pi = { 0 };
	if (!CreateProcess(EXE_FILENAME, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
		dbg.FatalPopup(L"Unable to create process " + wstring(EXE_FILENAME));
	PLoadResource(IDR_FILE_DLL1, lpBuffer, dwLength);
	if (!PLoadDll(pi.hProcess, lpBuffer, dwLength))
		dbg.FatalPopup(L"Unable to load dll");
	ResumeThread(pi.hThread);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	
	/* // abandoned
	bool suc; wstring tempdir;
	tie(suc, tempdir) = LGetTempDir();
	if (!suc)
		dbg.FatalPopup(L"Unable to get temporary directory");
	suc = LCreateDir(tempdir);
	if (!suc)
		dbg.FatalPopup(L"Unable to create directory " + tempdir);
	LEmptyDir(tempdir);
	if (!suc)
		;
	wstring dllpath = tempdir + LGenRandomString() + L".dll";
	suc = LLoadResource(IDR_FILE_DLL1, dllpath);
	if (!suc)
		dbg.FatalPopup(L"Unable to write file to " + dllpath);
	wstring exepath = tempdir + L"Farthest2015_CHS.exe";
	suc = LLoadResource(IDR_FILE_EXE1, exepath);
	if (!suc)
		dbg.FatalPopup(L"Unable to write file to " + exepath);
	
	// inject remote dll
	dbg.Log(L"ready to inject");
	wstring err; HANDLE hdl;
	tie(suc, err, hdl) = LInjectDll(dllpath, exepath);
	if(!suc)
		dbg.FatalPopup(err);
	WaitForSingleObject(hdl, INFINITE);
	dbg.Log(L"program exited");
	LEmptyDir(tempdir); RemoveDirectory(tempdir.c_str());
	*/
}

__int64 LBuffHash(const void* buf, int len)
{
	__int64 hash = 0;
	for (int i = 0; i < len; i++)
		hash = (hash << 5) + hash + ((unsigned char*)buf)[i];
	return hash;
}

DWORD LReadFileToBuf(const wstring& path, void*& buf)
{
	HANDLE hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return 0;
	DWORD dwSize = GetFileSize(hFile, NULL);
	if (dwSize == INVALID_FILE_SIZE)
		return 0;
	buf = new char[dwSize];
	DWORD dwRead;
	if (!ReadFile(hFile, buf, dwSize, &dwRead, NULL))
		return 0;
	CloseHandle(hFile);
	return (dwSize==dwRead ? dwSize : 0);
}

bool LWriteBufToFile(const wstring& path, const void* buf, int len)
{
	HANDLE hFile = CreateFile(path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;
	DWORD dwWritten;
	if (!WriteFile(hFile, buf, len, &dwWritten, NULL))
		return false;
	CloseHandle(hFile);
	return (len == dwWritten ? true : false);
}

bool LFileExists(const wstring& path)
{
	DWORD dwAttrib = GetFileAttributes(path.c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

tuple<bool, wstring> LGetTempDir()
{
	WCHAR str[MAX_PATH + 1] = {};
	DWORD dwRet = GetTempPath(MAX_PATH, str);
	if (dwRet > MAX_PATH || dwRet == 0)
		return {false, L""};
	return {true, static_cast<wstring>(str) + TEMPPATH_APPEND + LGenRandomString() + L"\\"};
}

bool LCreateDir(const wstring& dir)
{
	if (CreateDirectory(dir.c_str(), NULL) || ERROR_ALREADY_EXISTS == GetLastError())
		return true;
	return false;
}

bool LEmptyDir(const wstring& dir)
{
	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	DWORD dwError = 0, dwError1 = 0;

	hFind = FindFirstFile((dir + L"*").c_str(), &ffd);
	if (INVALID_HANDLE_VALUE == hFind)
		return false;
	do
	{
		if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			if (!DeleteFile((dir + ffd.cFileName).c_str()))
			{
				dwError = GetLastError();
				// continue anyways
			}
	} while (FindNextFile(hFind, &ffd) != 0);

	dwError1 = GetLastError();
	FindClose(hFind);

	if (dwError != 0)
		return false;
	if (dwError1 == ERROR_NO_MORE_FILES)
		return true;
	return false;
}

wstring LGenRandomString(int len)
{
	srand(time(NULL) + rand());
	static const wchar_t chars[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	wstring str;
	for (int i = 0; i < len; i++)
		str += chars[rand() % (sizeof(chars) / sizeof(wchar_t) - 1)];
	return str;
}

bool LLoadResource(UINT uid, const wstring& filename)
{
	HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(uid), L"FILE");
	if (hRes == NULL)
		return false;
	HGLOBAL hGlob = LoadResource(NULL, hRes);
	if (hGlob == NULL)
		return false;
	DWORD dwSize = SizeofResource(NULL, hRes);
	if (dwSize == 0)
		return false;
	LPVOID pData = LockResource(hGlob);
	if (pData == NULL)
		return false;
	HANDLE hFile = CreateFile(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;
	DWORD dwWritten;
	if (!WriteFile(hFile, pData, dwSize, &dwWritten, NULL))
		return false;
	CloseHandle(hFile);
	return true;
}

tuple<bool, wstring, HANDLE> LInjectDll(const wstring& dllpath, const wstring& exepath)
{
	STARTUPINFO si = { 0 }; si.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pi = { 0 };
	if (!CreateProcess(NULL, (LPWSTR)exepath.c_str(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
		return { false, L"Unable to create game process", NULL };
	HMODULE hKernel = GetModuleHandle(L"kernel32.dll");
	if (hKernel == NULL)
		return { false, L"Unable to get handle of kernel32.dll", NULL };
	LPVOID lpFunc = GetProcAddress(hKernel, "LoadLibraryW");
	if (lpFunc == NULL)
		return { false, L"Unable to get address of LoadLibraryW", NULL };
	SIZE_T szPath = (dllpath.size() + 1) * sizeof(wchar_t);
	LPCVOID lpPath = dllpath.c_str();
	LPVOID lpAddr = VirtualAllocEx(pi.hProcess, NULL, szPath, MEM_COMMIT, PAGE_READWRITE);
	if (lpAddr == NULL)
		return { false, L"Unable to call VirtualAllocEx", NULL };
	if (!WriteProcessMemory(pi.hProcess, lpAddr, lpPath, szPath, NULL))
		return {false, L"Unable to write remote memory", NULL};
	HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)lpFunc, lpAddr, 0, NULL);
	if (!hThread)
		return { false, L"Unable to create remote DLL thread", NULL };
	CloseHandle(hThread);
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	return { true, L"", pi.hProcess};
}

