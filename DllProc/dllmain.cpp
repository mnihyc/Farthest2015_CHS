﻿#include "hook.h"
#include "../Static/debug.h"
#include "../Static/pe.h"
#include <windows.h>
#include <shlobj.h>
#include <string>

// global debug
DEBUG dbg{ L"DllProc", L"D:/Projects/Farthest2015_CHS/Release/d_dllproc.txt", false, true };

// for easier coding
using std::wstring;

// main procedure
void MainProc();
// convert multi-byte to wstring
wstring MBTWS(const char* str, int page=932);
// convert wstring to multi-byte
char* WSTMB(const wstring& str, int page=936);

extern "C" int __declspec(dllexport) a1()
{
	return 1;
}

// entry point DllMain
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		MainProc();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

// perform virtual file map
HOOKIAT hkCreateFileA;
typedef HANDLE (WINAPI *tpCreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
HANDLE WINAPI myCreateFileA(
	LPCSTR                lpFileName,
	DWORD                 dwDesiredAccess,
	DWORD                 dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD                 dwCreationDisposition,
	DWORD                 dwFlagsAndAttributes,
	HANDLE                hTemplateFile
)
{
	if (strlen(lpFileName) == 0)
		return INVALID_HANDLE_VALUE;
	char s[100]={0};
	sprintf_s(s,"open %s",lpFileName);
	//MessageBoxA(NULL, s,s,0);
	tpCreateFileA CFA = static_cast<tpCreateFileA>(hkCreateFileA.get());
	HANDLE ret = CFA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	return ret;
}

// hook output debug message
HOOKIAT hkOutputDebugStringA;
typedef void (WINAPI* tpOutputDebugStringA)(LPCSTR);
void WINAPI myOutputDebugStringA(LPCSTR lpOutputString)
{
	tpOutputDebugStringA ODA = static_cast<tpOutputDebugStringA>(hkOutputDebugStringA.get());
	//ODA(lpOutputString);
	wstring ws = MBTWS(lpOutputString);
	dbg.Log(ws);
}

// hook my document path, forcing SaveData to current path
HOOKIAT hkSHGetSpecialFolderPathA;
typedef BOOL (WINAPI* tpSHGetSpecialFolderPathA)(HWND, LPSTR, int, BOOL);
BOOL WINAPI mySHGetSpecialFolderPathA(HWND hwnd, LPSTR pszPath, int csidl, BOOL fCreate)
{
	tpSHGetSpecialFolderPathA SHGSPA = static_cast<tpSHGetSpecialFolderPathA>(hkSHGetSpecialFolderPathA.get());
	if (csidl == CSIDL_MYDOCUMENTS)
		return pszPath[0]=0;
	return SHGSPA(hwnd, pszPath, csidl, fCreate);
}

// recognize unicode
HOOKIAT hkCreateFontA;
typedef HFONT (WINAPI* tpCreateFontA)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR);
HFONT WINAPI myCreateFontA(
	int    cHeight,
	int    cWidth,
	int    cEscapement,
	int    cOrientation,
	int    cWeight,
	DWORD  bItalic,
	DWORD  bUnderline,
	DWORD  bStrikeOut,
	DWORD  iCharSet,
	DWORD  iOutPrecision,
	DWORD  iClipPrecision,
	DWORD  iQuality,
	DWORD  iPitchAndFamily,
	LPCSTR pszFaceName
)
{
	tpCreateFontA CFA = static_cast<tpCreateFontA>(hkCreateFontA.get());
	iCharSet = 0x86;
	HFONT ret = CFA(cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic, bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision, iQuality, iPitchAndFamily, pszFaceName);
	return ret;
}

// recognize unicode charset
HOOKIAT hkIsDBCSLeadByte;
typedef BOOL (WINAPI* tpIsDBCSLeadByte)(BYTE);
BOOL WINAPI myIsDBCSLeadByte(BYTE TestChar)
{
	tpIsDBCSLeadByte IDB = static_cast<tpIsDBCSLeadByte>(hkIsDBCSLeadByte.get());
	return IsDBCSLeadByteEx(936, TestChar);
}

// recognize unicode charset
HOOKIAT hkCharNextA;
typedef LPSTR(WINAPI* tpCharNextA)(LPCSTR);
LPSTR WINAPI myCharNextA(LPCSTR lpsz)
{
	tpCharNextA CNA = static_cast<tpCharNextA>(hkCharNextA.get());
	return CharNextExA(936, lpsz, 0);
}

// debug output function
HOOKJMP hksub_4AA300;
__declspec(naked) void __cdecl orgsub_4AA300(int* a1, int a2, const char* a3, va_list vl)
{
	__asm
	{
		push vl
		push a3
		push a2
		mov esi, a1
		lea ecx, hksub_4AA300
		call HOOKJMP::get
		call eax
		add esp, 0xC
		ret
	}
}
void __stdcall mysub_4AA300(int *a1, int a2, const char* a3, va_list vl)
{
	return orgsub_4AA300(a1, a2, a3, vl);
}
__declspec(naked) void __cdecl sub_4AA300(int a2, const char* a3, va_list vl)
{
	__asm
	{
		push ebp
		mov ebp, esp
		push vl
		push a3
		push a2
		push esi
		call mysub_4AA300
		leave
		ret
	}
}

// read text function function
HOOKJMP hksub_475E90;
__declspec(naked) char __cdecl orgsub_475E90(DWORD* a1, DWORD* a2, int a3)
{
	__asm
	{
		lea ecx, hksub_475E90
		call HOOKJMP::get
		mov ecx, eax
		push a3
		mov edi, a2
		mov eax, a1
		call ecx
		add esp, 0x4
		ret
	}
}
char __stdcall mysub_475E90(DWORD* a1, DWORD* a2, int a3)
{
	char c = orgsub_475E90(a1, a2, a3);
	return 0;
}
__declspec(naked) char __cdecl sub_475E90(int a3)
{
	__asm
	{
		push ebp
		mov ebp, esp
		push a3
		push edi
		push eax
		call mysub_475E90
		leave
		ret
	}
}

// patch file validation function
HOOKJMP hkRoundKey;
WORD __stdcall myRoundKey(int size, BYTE *b, WORD *key)
{
	if (key)
		*key = 0;
	return 0;
}
__declspec(naked) WORD __cdecl gdRoundKey()
{
	__asm
	{
		push ebp
		mov ebp, esp
		push esi
		push ecx
		push eax
		call myRoundKey
		leave
		ret
	}
}

wstring MBTWS(const char* str, int page)
{
	int len = MultiByteToWideChar(page, 0, str, strlen(str) + 1, NULL, 0);
	wchar_t* wstr = new wchar_t[len + 1];
	MultiByteToWideChar(page, 0, str, strlen(str) + 1, wstr, len);
#pragma warning(push)
#pragma warning(disable: 6001)
	wstring s{wstr};
#pragma warning(pop)
	delete[]wstr;
	return s;
}

char* WSTMB(const wstring& str, int page)
{
	int len = WideCharToMultiByte(page, 0, str.data(), str.size() + 1, NULL, 0, NULL, NULL);
	char* mstr = new char[len + 1];
	WideCharToMultiByte(page, 0, str.data(), str.size() + 1, mstr, len, NULL, NULL);
	return mstr;
}

void MainProc()
{
	DWORD base = (DWORD)GetModuleHandleA(NULL);
	bool suc = true;
	
	// patch font validation
	const BYTE _PFont1[] = { "\xCB\xCE\xCC\xE5" }; // GBK 宋体
	suc = HOOK::patch(base + 0xFA874, (BYTE)0, 0xD) && HOOK::patch(base + 0xFA874, _PFont1, sizeof(_PFont1));
	suc &= HOOK::patch(base + 0xFA998, (BYTE)0x86, 0x1); // GB2312_CHARSET
	const BYTE _PFont2[] = { "\xCB\xCE\xCC\xE5" }; // GBK 宋体
	suc &= HOOK::patch(base + 0xFA9A4, (BYTE)0, 0xF) && HOOK::patch(base + 0xFA9A4, _PFont2, sizeof(_PFont2));
	suc &= HOOK::patch(base + 0xFAAC8, (BYTE)0x86, 0x1); // GB2312_CHARSET
	if (!suc)
		dbg.FatalPopup(L"Unable to patch unk_4FA870");

	// patch font validation mbscmp()
	suc = HOOK::patch(base + 0x87E52, (BYTE)0xEB, 0x1);
	if (!suc)
		dbg.FatalPopup(L"Unable to patch hex:487E52 JMP");
	
	// patch WINAPI
	suc = hkCreateFileA.hook(myCreateFileA, L"CreateFileA");
	if (!suc)
		dbg.FatalPopup(L"Unable to hook CreateFileA");
	suc = hkSHGetSpecialFolderPathA.hook(mySHGetSpecialFolderPathA, L"SHGetSpecialFolderPathA");
	if (!suc)
		dbg.FatalPopup(L"Unable to hook SHGetSpecialFolderPathA");
	suc = hkOutputDebugStringA.hook(myOutputDebugStringA, L"OutputDebugStringA");
	if (!suc)
		dbg.FatalPopup(L"Unable to hook OutputDebugStringA");
	suc = hkCreateFontA.hook(myCreateFontA, L"CreateFontA");
	if (!suc)
		dbg.FatalPopup(L"Unable to hook CreateFontA");
	suc = hkIsDBCSLeadByte.hook(myIsDBCSLeadByte, L"IsDBCSLeadByte");
	if (!suc)
		dbg.FatalPopup(L"Unable to hook IsDBCSLeadByte");
	suc = hkCharNextA.hook(myCharNextA, L"CharNextA");
	if (!suc)
		dbg.FatalPopup(L"Unable to hook CharNextA");
	
	suc = hksub_4AA300.hook(sub_4AA300, (LPVOID)(base + 0xAA300), 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_4AA300");
	
	suc = hksub_475E90.hook(sub_475E90, (LPVOID)(base + 0x75E90), 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_475E90");
	
	suc = hkRoundKey.hook(gdRoundKey, (LPVOID)(base + 0xA7BE0), 8);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook hex:4A7BE0 gdRoundKey");

	
	
}

