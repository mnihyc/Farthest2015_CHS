#include "hook.h"
#include "../Static/debug.h"
#include "../Static/pe.h"
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <process.h>   // _beginthreadex
#include <mutex>
#include <map>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <numeric>

#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

// switch release mode?
#define RELEASE

// global debug
#ifdef RELEASE
DEBUG dbg{ L"DllProc", L"", false, true };
#else
DEBUG dbg{ L"DllProc", L"d_dllproc.txt", false, true };
//DEBUG dbg{ L"DllProc", L"", false, true };
#endif

// for easier coding
using std::wstring;

// main procedure
void MainProc();
// convert multi-byte to wstring
wstring MBTWS(const char* str, int page=CP_ACP);
// convert wstring to multi-byte
char* WSTMB(const wstring& str, int page=CP_ACP);

// png replacement table
std::unordered_map<unsigned, std::vector<char>> png_replacement_table;

// text translation table
template<typename T>
using Map2D = std::map<std::uint16_t,
	std::map<std::uint16_t, std::vector<T>>>;

static Map2D<std::wstring> jap_map, chs_map;
static Map2D<std::tuple<int, int, int>> hyp_map;

static HWND g_game_main_hwnd = nullptr;

struct TooltipEntry
{
	std::uint16_t cdnum = 0;
	std::uint16_t text_idx = 0;
	std::uint16_t line = 0;      // 1-based line index in text block
	std::uint16_t start = 0;     // 1-based character index
	std::uint16_t len = 0;       // character length
	std::wstring tip;    // tooltip content
};
static Map2D<TooltipEntry> tip_map;
static std::unordered_set<const TooltipEntry*> g_tip_entry_ptrs;

// Tooltip interruption policy switches:
// false -> keep tooltip regions rendered but do not interrupt that mode.
// true  -> let tooltip regions behave like regular buttons for that mode.
static bool g_tooltip_interrupt_skip = false;
static bool g_tooltip_interrupt_auto = false;

static void RebuildTooltipEntryPointerIndex()
{
	g_tip_entry_ptrs.clear();
	for (const auto& cd_pair : tip_map)
	{
		for (const auto& idx_pair : cd_pair.second)
		{
			for (const auto& tip : idx_pair.second)
				g_tip_entry_ptrs.insert(&tip);
		}
	}
}

// current text identity tracked from LogCurText: hi=cdnum, lo=text_idx
static volatile LONG g_cur_text_key = -1;
static std::unordered_map<DWORD, LONG> g_tooltip_registered_key_by_field;
static volatile LONG g_last_register_tfl = 0;
static volatile LONG g_last_register_line_idx = 0;
static volatile LONG g_last_register_glyph_idx = 0;

static inline LONG PackTextKey(std::uint16_t cdnum, std::uint16_t text_idx)
{
	return (static_cast<LONG>(cdnum) << 16) | static_cast<LONG>(text_idx);
}



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

unsigned simple_hash(const char* str)
{
	unsigned hash = 5381;
	int c;
	while ((c = *str++))
	{
		hash = ((hash << 5) + hash) + c; // hash * 33 + c
	}
	return hash;
}

unsigned simple_hash(const unsigned char* bytes, unsigned len)
{
	unsigned hash = 5381;
	for (unsigned i = 0; i < len; ++i)
	{
		hash = ((hash << 5) + hash) + bytes[i]; // hash * 33 + c
	}
	return hash;
}

// Given Enigma may mess up Import Table; use traditional HOOK for WinAPIs instead.

// dynamic window title name
HOOKJMP hkCreateWindowExA;
typedef HWND(WINAPI* tpCreateWindowExA)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
HWND WINAPI myCreateWindowExA(
	DWORD dwExStyle,
	LPCSTR lpClassName,
	LPCSTR lpWindowName,
	DWORD dwStyle,
	int X,
	int Y,
	int nWidth,
	int nHeight,
	HWND hWndParent,
	HMENU hMenu,
	HINSTANCE hInstance,
	LPVOID lpParam)
{
	tpCreateWindowExA CWEA = static_cast<tpCreateWindowExA>(hkCreateWindowExA.get());
	const bool probable_main_window = (lpClassName == lpWindowName);
	if (probable_main_window)
		lpWindowName = "\xd7\xee\xb9\xfb\xa4\xc6\xa4\xce\xa5\xa4\xa5\xde COMPLETE \xa1\xaa\xa1\xaa \xb2\xe2\xca\xd4\xba\xba\xbb\xaf\xb2\xb9\xb6\xa1 v0.3.1 PRE-RELEASE (2026.3.7)"; // 最果てのイマ COMPLETE —— 测试汉化补丁 v0.1 (2025.07.03)
	HWND ret = CWEA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	if (ret && probable_main_window)
		g_game_main_hwnd = ret;
	return ret;
}


// direct virtual file map
HOOKJMP hkCreateFileW;
typedef HANDLE(WINAPI* tpCreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
HANDLE WINAPI myCreateFileW(
	LPCWSTR               lpFileName,
	DWORD                 dwDesiredAccess,
	DWORD                 dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD                 dwCreationDisposition,
	DWORD                 dwFlagsAndAttributes,
	HANDLE                hTemplateFile
)
{
	if (wcslen(lpFileName) == 0)
		return INVALID_HANDLE_VALUE;
	static wchar_t s[1024] = { 0 };
	wsprintf(s, L"CreateFileW: %.600s, access: 0x%x, share: 0x%x, disp: 0x%x, attr: 0x%x", lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes);
	dbg.Log(s);
	if (PathFileExistsW(L".\\chs"))
	{
		// get basename of lpFileName
		const wchar_t* baseName = wcsrchr(lpFileName, L'\\');
		if (baseName == nullptr)
			baseName = lpFileName; // no path, use full name
		else
			baseName++; // skip the backslash
		// check if file exists in chs folder
		for (wstring search_path : { L".\\chs\\", L".\\chs\\cd\\" })
		{
			wstring chsFilePath = search_path + baseName;
			if (PathFileExistsW(chsFilePath.c_str()))
			{
				// file exists in chs folder, replace it
				wsprintf(s, L"Using replaced virtual file: %.400s with original %.400s", chsFilePath.c_str(), lpFileName);
				dbg.Log(s);
				lpFileName = chsFilePath.c_str();
				break;
			}
		}
	}
	tpCreateFileW CFW = static_cast<tpCreateFileW>(hkCreateFileW.get());
	HANDLE ret = CFW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	return ret;
}

// perform virtual file map (deprecated)
HOOKJMP hkCreateFileA;
typedef HANDLE(WINAPI* tpCreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
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
	static wchar_t s[1024] = { 0 };
	wsprintf(s, L"CreateFileA: %.600s, access: 0x%x, share: 0x%x, disp: 0x%x, attr: 0x%x", MBTWS(lpFileName).c_str(), dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes);
	dbg.Log(s);
	if (PathFileExistsW(L".\\chs"))
	{
		// load possible replacement
		return myCreateFileW(
			MBTWS(lpFileName).c_str(),
			dwDesiredAccess,
			dwShareMode,
			lpSecurityAttributes,
			dwCreationDisposition,
			dwFlagsAndAttributes,
			hTemplateFile
		);
	}
	tpCreateFileA CFA = static_cast<tpCreateFileA>(hkCreateFileA.get());
	HANDLE ret = CFA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	return ret;
}

// hook output debug message
HOOKJMP hkOutputDebugStringA;
typedef void (WINAPI* tpOutputDebugStringA)(LPCSTR);
void WINAPI myOutputDebugStringA(LPCSTR lpOutputString)
{
	tpOutputDebugStringA ODA = static_cast<tpOutputDebugStringA>(hkOutputDebugStringA.get());
	//ODA(lpOutputString);
	wstring ws = MBTWS(lpOutputString, 936);
	dbg.Log(ws);
}

// hook my document path, forcing SaveData to current path
HOOKJMP hkSHGetSpecialFolderPathA;
typedef BOOL (WINAPI* tpSHGetSpecialFolderPathA)(HWND, LPSTR, int, BOOL);
BOOL WINAPI mySHGetSpecialFolderPathA(HWND hwnd, LPSTR pszPath, int csidl, BOOL fCreate)
{
	tpSHGetSpecialFolderPathA SHGSPA = static_cast<tpSHGetSpecialFolderPathA>(hkSHGetSpecialFolderPathA.get());
	if (csidl == CSIDL_MYDOCUMENTS)
		return pszPath[0]=0;
	return SHGSPA(hwnd, pszPath, csidl, fCreate);
}

// hook registry, bypass installation check (No-CD crack)
HOOKJMP hkRegOpenKeyExA;
typedef LONG(WINAPI* tpRegOpenKeyExA)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
LONG WINAPI myRegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	tpRegOpenKeyExA ROKEA = static_cast<tpRegOpenKeyExA>(hkRegOpenKeyExA.get());
	if (strcmp(lpSubKey, "Software\\XUSE_CORP\\Farthest2015") == 0)
	{
		return ERROR_FILE_NOT_FOUND;
	}
	return ROKEA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

// recognize chinese
HOOKJMP hkCreateFontA;
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
	iCharSet = 0x86; // GB2312_CHARSET
	if (simple_hash(pszFaceName) == 0x8eabf25c)
	{
		// ＭＳ ゴシック
		pszFaceName = "SimHei";
	}
	if (simple_hash(pszFaceName) == 0xb7319fef)
	{
		// ＭＳ Ｐゴシック
		pszFaceName = "SimHei";
	}
	HFONT ret = CFA(cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic, bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision, iQuality, iPitchAndFamily, pszFaceName);
	return ret;
}

// recognize chinese, or boundary check
HOOKJMP hkIsDBCSLeadByte;
typedef BOOL (WINAPI* tpIsDBCSLeadByte)(BYTE);
BOOL WINAPI myIsDBCSLeadByte(BYTE TestChar)
{
	tpIsDBCSLeadByte IDB = static_cast<tpIsDBCSLeadByte>(hkIsDBCSLeadByte.get());
	return IsDBCSLeadByteEx(936, TestChar);
}

// recognize chinese, or bounary check
HOOKJMP hkCharNextA;
typedef LPSTR(WINAPI* tpCharNextA)(LPCSTR);
LPSTR WINAPI myCharNextA(LPCSTR lpsz)
{
	tpCharNextA CNA = static_cast<tpCharNextA>(hkCharNextA.get());
	return CharNextExA(936, lpsz, 0);
}

// render special characters absent in gbk
HOOKJMP hkGetGlyphOutlineA;
typedef DWORD(WINAPI* tpGetGlyphOutlineA)(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, const MAT2*);
DWORD WINAPI myGetGlyphOutlineA(HDC hdc, UINT uChar, UINT fuFormat, LPGLYPHMETRICS lpgm, DWORD cjBuffer, LPVOID pvBuffer, const MAT2* lpmat2)
{
	tpGetGlyphOutlineA GGOA = static_cast<tpGetGlyphOutlineA>(hkGetGlyphOutlineA.get());
	/*wchar_t s[100] = {0};
	wsprintf(s, L"GetGlyphOutlineA: 0x%x", uChar);
	dbg.Log(s);*/
	if (uChar <= 0xFF)
	{
		// we don't bother with single byte characters
		return GGOA(hdc, uChar, fuFormat, lpgm, cjBuffer, pvBuffer, lpmat2);
	}
	char uchar[] = { (uChar >> 8) & 0xFF, uChar & 0xFF }; // why is this reversed? anyway, it is working like this.
	wstring wch = MBTWS(uchar, 936);
	if (wch[0] == L'\u9f1d')
	{
		wch = L"♪";
		HFONT hOrig = static_cast<HFONT>(GetCurrentObject(hdc, OBJ_FONT));
		LOGFONTW lf{};
		if (!GetObjectW(hOrig, sizeof(lf), &lf))
			dbg.FatalPopup(L"GetObjectW failed in GetGlyphOutlineA");
		wcsncpy_s(lf.lfFaceName, L"MS Gothic", _TRUNCATE);
		HFONT hNewFont = CreateFontIndirectW(&lf);
		if (hNewFont == NULL)
		{
			dbg.FatalPopup(L"CreateFontIndirectW failed in GetGlyphOutlineA");
			return GGOA(hdc, uChar, fuFormat, lpgm, cjBuffer, pvBuffer, lpmat2);
		}
		HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hNewFont));
		DWORD ret = GetGlyphOutlineW(hdc, wch[0], fuFormat, lpgm, cjBuffer, pvBuffer, lpmat2);
		SelectObject(hdc, hOldFont);
		DeleteObject(hNewFont); // delete the new font
		return ret;
	}
	return GetGlyphOutlineW(hdc, wch[0], fuFormat, lpgm, cjBuffer, pvBuffer, lpmat2);
}


// translate dynamic sjis texts in win components
HOOKJMP hkSetDlgItemText;
typedef BOOL(WINAPI* tpSetDlgItemTextA)(HWND, int, LPCSTR);
BOOL WINAPI mySetDlgItemTextA(HWND hDlg, int nIDDlgItem, LPCSTR lpString)
{
	tpSetDlgItemTextA SDITA = static_cast<tpSetDlgItemTextA>(hkSetDlgItemText.get());
	if (lpString == NULL || strlen(lpString) == 0)
		return SDITA(hDlg, nIDDlgItem, lpString);
	if (strlen(lpString) % 2 == 0)
	{
		for (unsigned i = 0; i < strlen(lpString); i += 2)
		{
			if (!(lpString[i] == '\x81' && lpString[i + 1] == '\xA1'))
			{
				goto end;
			}
		}
		// all chars are '■'  replace to gbk
		static char newstr[128] = { 0 };
		for (unsigned i = 0; i < strlen(lpString) && i < 100; i += 2)
		{
			newstr[i] = '\xA1';
			newstr[i + 1] = '\xF6';
			newstr[i + 2] = '\x00';
		}
		lpString = newstr;
	}
end:
	return SDITA(hDlg, nIDDlgItem, lpString);
}

HOOKJMP hkSetWindowText;
typedef BOOL(WINAPI* tpSetWindowTextA)(HWND, LPCSTR);
BOOL WINAPI mySetWindowTextA(HWND hWnd, LPCSTR lpString)
{
	tpSetWindowTextA SWTA = static_cast<tpSetWindowTextA>(hkSetWindowText.get());
	if (lpString == NULL || strlen(lpString) == 0)
		return SWTA(hWnd, lpString);
	return SWTA(hWnd, lpString);
}


void LogCurText(DWORD* buf);

// read script text function
HOOKJMP hksub_475E90;
__declspec(naked) char __cdecl orgsub_475E90(DWORD* a1, DWORD* a2, int a3)
{
	__asm
	{
		push ebp
		mov ebp, esp
		lea ecx, hksub_475E90
		call HOOKJMP::get
		mov ecx, eax
		push a3
		mov edi, a2
		mov eax, a1
		call ecx
		; //add esp, 0x4
		leave
		ret
	}
}
char __stdcall mysub_475E90(DWORD* a1, DWORD* a2, int a3)
{
	LogCurText(a2);
	char c = orgsub_475E90(a1, a2, a3);
	return c;
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


void LogCurInst();

// ReadFuncFromGCScenario
HOOKJMP hksub_472AB0;
__declspec(naked) DWORD* __cdecl orgsub_472AB0(DWORD* a1)
{
	__asm
	{
		push ebp
		mov ebp, esp
		lea ecx, hksub_472AB0
		call HOOKJMP::get
		mov ecx, eax
		mov eax, a1
		call ecx
		leave
		ret
	}
}
DWORD* __stdcall mysub_472AB0(DWORD* a1)
{
	LogCurInst();
	DWORD* ret = orgsub_472AB0(a1);
	return ret;
}
__declspec(naked) DWORD* __cdecl sub_472AB0()
{
	__asm
	{
		push ebp
		mov ebp, esp
		push eax
		call mysub_472AB0
		leave
		ret
	}
}

// MoveNextScenarioInstruction
HOOKJMP hksub_4BAA10;
__declspec(naked) char __cdecl orgsub_4BAA10(DWORD* a1)
{
	__asm
	{
		push ebp
		mov ebp, esp
		lea ecx, hksub_4BAA10
		call HOOKJMP::get
		mov ecx, eax
		mov eax, a1
		call ecx
		leave
		ret
	}
}
char __stdcall mysub_4BAA10(DWORD* a1)
{
	char ret = orgsub_4BAA10(a1);
	LogCurInst();
	return ret;
}
__declspec(naked) char __cdecl sub_4BAA10()
{
	__asm
	{
		push ebp
		mov ebp, esp
		push eax
		call mysub_4BAA10
		leave
		ret
	}
}

// backlog text stripping (fix original bug) in sub_420F10
HOOKJMP hkinst_421009;
int __stdcall myinst_421009(unsigned char* buf, unsigned line)
{
	unsigned pos = 0;
	while (pos < line)
		pos += 1 + static_cast<unsigned>(IsDBCSLeadByte(buf[pos]));
	if (pos > line)
		return 1;
	if (pos == line)
		return 0;
	dbg.FatalPopup(L"myinst_421009: line number mismatch");
}
__declspec(naked) char __cdecl inst_421009()
{
	__asm
	{
		pop ecx; //strip arguments

		push ebp
		mov ebp, esp
		push ebx
		push edx
		push edi
		push esi; //save registers

		sub eax, edi
		add eax, ebx
		add eax, 0x4; //start offset of buffer

		push edi; //length
		push eax; //buffer
		call myinst_421009
		push eax; //eax = return value

		lea ecx, hkinst_421009
		call HOOKJMP::get
		mov ecx, eax
		add ecx, 0x6; //skip original code
		push ecx; //ecx = continue execution address

		pop ecx
		pop eax

		pop esi
		pop edi
		pop edx
		pop ebx; //restore registers
		leave
		jmp ecx; //continue
	}
}


// global variable to indicate first clear
static int g_first_clear = -1;
// hook ScriptInstruction_0x99_SetPGlobByte to know unlock progress
HOOKJMP hkinst_478B00;
void __stdcall myinst_478B00()
{
	DWORD base = (DWORD)GetModuleHandleA(NULL);
	DWORD GCScenario = base + 0x1194C0;
	DWORD pos = *(DWORD*)(GCScenario + 0x28);
	BYTE param = *(BYTE*)(pos + 0x2);

	DWORD GlobVar_51EB3C = *(DWORD*)(base + 0x11EB3C);

	static wchar_t s[200] = { 0 };
	wsprintf(s, L"0x99_SetPGlobByte called with param: 0x%x; prev value=0x%x", param, GlobVar_51EB3C);
	dbg.Log(s);

	if (param == 0x3)
	{
		// only called in 0003.cd
		g_first_clear = !(GlobVar_51EB3C & (1 << 0x3));

		if (g_first_clear)
		{
			// first clear, create indicator file
			HANDLE hFile = CreateFileW(L".\\隐藏章节提示（无需可删除）.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				//// if file is already, skip. Use Ex version to bypass hook.
				//SetFilePointerEx(hFile, { 0 }, NULL, FILE_END);
				//LARGE_INTEGER size;
				//GetFileSizeEx(hFile, &size);
				//if (size.QuadPart > 0)
				//{
				//	CloseHandle(hFile);
				//	return;
				//}

				DWORD written = 0;
				const wchar_t* msg = L"恭喜通关！\r\n\r\n隐藏章节提示：从游戏开头开始，不选任何“链接”循环三次即可解锁隐藏章节。\r\n\r\n感谢您游玩本汉化补丁！";
				// convert to utf-8
				const char* utf8msg = WSTMB(msg, CP_UTF8);
				WriteFile(hFile, utf8msg, (DWORD)strlen(utf8msg), &written, NULL);
				delete[] utf8msg;
				CloseHandle(hFile);
				dbg.Log(L"Game cleared indicator file created.");
			}
		}
	}
}

__declspec(naked) void __cdecl inst_478B00()
{
	__asm
	{
		call myinst_478B00

		lea ecx, hkinst_478B00
		call HOOKJMP::get
		mov ecx, eax
		jmp ecx; // jump back to continue
	}
}


// hook ScriptInstruction_0x96_Unknown_Menu (only called in 0003.cd) to show hint afer game clear
HOOKJMP hkinst_478330;
void __stdcall myinst_478330()
{
	dbg.Log(L"Game cleared! (in 0x96_U_Menu)");

	if (g_first_clear != 1)
	{
		return; // not first clear, do nothing
	}

	// relocated to previous hook
}

__declspec(naked) void __cdecl inst_478330()
{
	__asm
	{
		call myinst_478330

		lea ecx, hkinst_478330
		call HOOKJMP::get
		mov ecx, eax
		jmp ecx; // jump back to continue
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

// if we use HOOK JMP, we need to manually call original function
typedef DWORD(WINAPI* tpSetFilePointer)(HANDLE, LONG, PLONG, DWORD);
static tpSetFilePointer SFP = nullptr;

namespace VirtualFS
{
	constexpr size_t kPageSize = 4 * 1024 * 1024;   // 4 MiB per region

	struct Region {
		HANDLE           org;     // Real WinAPI handle backing the region
		size_t           off;     // Physical offset where region starts
		size_t           orgSize; // Original full size of the file (for rollback)
		size_t           size;    // Current virtual size (grows with Append)
		size_t           cursor;  // Current position relative to off
		std::vector<char> data;   // In‑memory page (<= kPageSize)
	};

	inline std::mutex g_mtx;
	inline std::map<std::pair<HANDLE, size_t>, Region> g_regions;

	// Helper: locate by key
	inline auto find_region(HANDLE org, size_t off)
	{
		return g_regions.find({ org, off });
	}

	// Helper: locate by physical offset inside a file
	inline Region* resolve_region(HANDLE org, size_t physOffset)
	{
		for (auto& [key, reg] : g_regions)
			if (reg.org == org &&
				physOffset >= reg.off &&
				physOffset < reg.off + reg.size)
				return &reg;
		return nullptr;
	}

	// 1. Create an empty virtual region at (org, off)
	inline bool OpenRegion(HANDLE org, size_t off)
	{
		std::lock_guard lg{ g_mtx };
		if (find_region(org, off) != g_regions.end())
			return false;         // already exists
		if (resolve_region(org, off) != nullptr)
			dbg.FatalPopup(L"VirtualFS::OpenRegion: region already exists");

		Region r{};
		r.org = org;
		r.off = off;
		r.size = r.orgSize = 0;
		r.cursor = 0;
		r.data.reserve(kPageSize);
		g_regions.emplace(std::make_pair(org, off), std::move(r));
		return true;
	}

	// 2. Append bytes to a region – fails if page would overflow.
	inline bool Append(HANDLE org, size_t off, const void* src, size_t len)
	{
		std::lock_guard lg{ g_mtx };
		auto it = find_region(org, off);
		if (it == g_regions.end()) return false;

		Region& reg = it->second;
		if (reg.size + len > kPageSize) return false;  // page full

		const char* p = static_cast<const char*>(src);
		reg.data.insert(reg.data.end(), p, p + len);
		reg.size += len;
		return true;
	}

	// 3. Wrapper for SetFilePointer – returns virtual offset when inside a region.
	inline DWORD WINAPI SetFilePointer(HANDLE hFile,
		LONG  lDistanceToMove,
		PLONG lpDistanceToMoveHigh,
		DWORD dwMoveMethod)
	{
		DWORD phys = ::SFP(hFile, lDistanceToMove,
			lpDistanceToMoveHigh, dwMoveMethod);

		if (phys == INVALID_SET_FILE_POINTER && ::GetLastError() != NO_ERROR)
			return phys; // propagate error

		std::lock_guard lg{ g_mtx };
		if (Region* reg = resolve_region(hFile, phys)) {
			dbg.Log(L"VirtualFS::SetFilePointer: region found, syncing virtual offset.");
			reg->cursor = phys - reg->off;
		}
		return phys; // passthrough
	}

	// 4. Wrapper for ReadFile – sources bytes from the region when applicable.
	inline BOOL WINAPI ReadFile(HANDLE       hFile,
		LPVOID       lpBuffer,
		DWORD        nBytesToRead,
		LPDWORD      lpBytesRead,
		LPOVERLAPPED lpOverlapped)
	{
		DWORD phys = ::SFP(hFile, 0, nullptr, FILE_CURRENT);
		if (phys == INVALID_SET_FILE_POINTER && ::GetLastError() != NO_ERROR)
		{
			// Maybe not a file? Fallback to kernel.
			return ::ReadFile(hFile, lpBuffer, nBytesToRead, lpBytesRead, lpOverlapped);
		}

		std::lock_guard lg{ g_mtx };
		if (Region* reg = resolve_region(hFile, phys)) {
			size_t remain = reg->size - reg->cursor;
			DWORD  want = std::min<DWORD>(nBytesToRead,
				static_cast<DWORD>(remain));

			if (nBytesToRead > remain)
			{
				// overflow the VirtualFS region; passthrough
				return ::ReadFile(hFile, lpBuffer, nBytesToRead, lpBytesRead, lpOverlapped);
			}

			std::memcpy(lpBuffer, reg->data.data() + reg->cursor, want);
			reg->cursor += want;
			if (lpBytesRead) *lpBytesRead = want;

			// 4a. Update physical file pointer to reflect the read.
			if (lpOverlapped)
			{
				lpOverlapped->Offset = reg->off + reg->cursor;
				lpOverlapped->OffsetHigh = 0;
			}
			else
			{
				::SFP(hFile, reg->off + reg->cursor, nullptr, FILE_BEGIN);
			}

			// 4b. Auto‑close region at virtual EOF if desired.
			if (reg->cursor == reg->size)
			{
				dbg.Log(L"VirtualFS::ReadFile: region reached EOF, closing it.");
				// We believe this is the end of PNG file.
				g_regions.erase({ reg->org, reg->off });
				// Rollback to original size to keep the file intact.
				if (reg->orgSize > 0)
				{
					::SFP(reg->org, reg->off + reg->orgSize, nullptr, FILE_BEGIN);
				}
			}

			return TRUE;
		}

		// Fallback to kernel for all other cases.
		return ::ReadFile(hFile, lpBuffer, nBytesToRead, lpBytesRead, lpOverlapped);
	}

	// 5. Wrapper for CloseHandle – disposes every region bound to org.
	inline BOOL WINAPI CloseHandle(HANDLE hFile)
	{
		{
			std::lock_guard lg{ g_mtx };
			for (auto it = g_regions.begin(); it != g_regions.end(); )
				if (it->second.org == hFile)
					it = g_regions.erase(it);
				else
					++it;
		}
		return ::CloseHandle(hFile);
	}

	// 6. Manually remove a region when it reaches EOF.
	inline void CloseRegion(HANDLE org, size_t physOffset)
	{
		std::lock_guard lg{ g_mtx };
		auto it = std::find_if(g_regions.begin(), g_regions.end(),
			[org, physOffset](const auto& pair) {
				return pair.second.org == org && pair.second.off + pair.second.size == physOffset;
			});
		if (it == g_regions.end())
			dbg.FatalPopup(L"VirtualFS::CloseRegion: region not found");
		else
			g_regions.erase(it);
	}

}


// virtual file mapping (hook)
HOOKJMP hkSetFilePointer;
DWORD WINAPI mySetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	SFP = static_cast<tpSetFilePointer>(hkSetFilePointer.get());
	return VirtualFS::SetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}

// encryption handled below
/*HOOKIAT hkReadFile;
typedef BOOL(WINAPI* tpReadFile)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
BOOL WINAPI myReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToRead, LPDWORD lpBytesRead, LPOVERLAPPED lpOverlapped)
{
	tpReadFile RF = static_cast<tpReadFile>(hkReadFile.get());
	return VirtualFS::ReadFile(hFile, lpBuffer, nBytesToRead, lpBytesRead, lpOverlapped);
}

HOOKIAT hkCloseHandle;
typedef BOOL(WINAPI* tpCloseHandle)(HANDLE);
BOOL WINAPI myCloseHandle(HANDLE hFile)
{
	tpCloseHandle CH = static_cast<tpCloseHandle>(hkCloseHandle.get());
	return VirtualFS::CloseHandle(hFile);
}*/


// hook Gaf004Loader::png_Read for resource replacement
HOOKJMP hksub_4AFAC0;
__declspec(naked) int __cdecl orgsub_4AFAC0(LPVOID buf, DWORD num, int u2, DWORD* a4)
{
	__asm
	{
		push ebp
		mov ebp, esp
		lea ecx, hksub_4AFAC0
		call HOOKJMP::get
		mov ecx, eax
		push a4
		push u2
		push num
		push buf
		call ecx
		; add esp, 0x10
		leave
		ret
	}
}
int __stdcall mysub_4AFAC0(LPVOID buf, DWORD num, int u2, DWORD* a4)
{
	HANDLE handle = (HANDLE) *(a4 + 1);
	DWORD pos = SetFilePointer(handle, 0, NULL, FILE_CURRENT);
	if (pos == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
	{
		dbg.FatalPopup(L"SetFilePointer failed in png_Read");
		return -1;
	}
	if (VirtualFS::resolve_region(handle, pos) != nullptr)
	{
		static wchar_t s1[1024] = {0};
		wsprintf(s1, L"png_Read: virtual mapping detected, handle: %x, pos: %x, num: %x", (DWORD)handle, pos, num);
		dbg.Log(s1);
		// hook this
		return VirtualFS::ReadFile(handle, buf, num, nullptr, nullptr) ? 0 : -1;
	}
	orgsub_4AFAC0(buf, num, u2, a4);
	if (num == 8 && memcmp(buf, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0) // PNG signature
	{
		// perform virtual mapping; read all content first
		HANDLE org = handle; DWORD off = pos;
		VirtualFS::OpenRegion(org, off);
		VirtualFS::Append(org, off, buf, num);
		// read PNG structure
		while (true)
		{
			static char tmp[4 * 1024 * 1024] = { 0 }; // 4 MiB buffer
			orgsub_4AFAC0(tmp, 8, u2, a4);
			VirtualFS::Append(org, off, tmp, 8);
			UINT32 chunkSize;
			memcpy(&chunkSize, tmp, 4); // read chunk size
			chunkSize = _byteswap_ulong(chunkSize); // convert to little-endian
			if (chunkSize > 0)
			{
				orgsub_4AFAC0(tmp, chunkSize, u2, a4);
				VirtualFS::Append(org, off, tmp, chunkSize);
			}
			orgsub_4AFAC0(tmp, 4, u2, a4);
			VirtualFS::Append(org, off, tmp, 4);
			if (memcmp(tmp, "\xAE\x42\x60\x82", 4) == 0) // IEND chunk CRC
				break; // end of PNG file
		}
		SetFilePointer(handle, pos + 8, NULL, FILE_BEGIN); // reset file pointer
		VirtualFS::SetFilePointer(handle, pos + 8, NULL, FILE_BEGIN); // reset file pointer
		
		// check hash for replacement
		VirtualFS::Region* region = VirtualFS::resolve_region(handle, pos);
		unsigned hash = simple_hash((const unsigned char*)region->data.data(), region->size);
		if (png_replacement_table.find(hash) != png_replacement_table.end())
		{
			// found replacement
			const std::vector<char>& replacement = png_replacement_table[hash];
			region->orgSize = region->size; // save original size for rollback
			region->size = replacement.size(); // update size
			region->data = replacement; // replace data
			static wchar_t s[1024] = { 0 };
			wsprintf(s, L"Replaced PNG resource (hash: %x) at 0x%x with custom data, size: %d bytes", hash, pos, (int)replacement.size());
			dbg.Log(s);
		}
	}
	return 0;
}
__declspec(naked) int __cdecl sub_4AFAC0(LPVOID buf, DWORD num, int u2, DWORD* a4)
{
	__asm
	{
		push ebp
		mov ebp, esp
		push a4
		push u2
		push num
		push buf
		call mysub_4AFAC0
		leave
		ret
	}
}


// ---------------------------------------------------------------------------
// Tooltip keywords: register custom button regions per text index and handle
// hover/click timeout behavior without entering scenario jump flow.
// ---------------------------------------------------------------------------
namespace TooltipPopup
{
	static HWND g_hwnd = nullptr;
	static HFONT g_font = nullptr;
	static std::wstring g_text;
	static const TooltipEntry* g_src = nullptr;
	static RECT g_last_host_bounds{};
	static bool g_have_last_host_bounds = false;

	static void Hide()
	{
		if (!g_hwnd) return;
		ShowWindow(g_hwnd, SW_HIDE);
		g_src = nullptr;
	}

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		(void)wParam;
		(void)lParam;
		switch (msg)
		{
		case WM_MOUSEACTIVATE:
			return MA_NOACTIVATE;
		case WM_NCHITTEST:
			// Pass all mouse hit-tests through so tooltip never captures clicks/focus.
			return HTTRANSPARENT;
		case WM_PAINT:
		{
			PAINTSTRUCT ps{};
			HDC hdc = BeginPaint(hWnd, &ps);
			RECT rc{};
			GetClientRect(hWnd, &rc);
			HBRUSH bg = CreateSolidBrush(RGB(255, 255, 225));
			FillRect(hdc, &rc, bg);
			DeleteObject(bg);
			FrameRect(hdc, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
			if (g_font)
				SelectObject(hdc, g_font);
			SetBkMode(hdc, TRANSPARENT);
			rc.left += 8; rc.top += 6; rc.right -= 8; rc.bottom -= 6;
			DrawTextW(hdc, g_text.c_str(), static_cast<int>(g_text.size()), &rc,
				DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
			EndPaint(hWnd, &ps);
			return 0;
		}
		}
		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	static bool EnsureWindow()
	{
		if (g_hwnd) return true;
		static constexpr LPCWSTR kClass = L"FarthestTooltipPopupCls";
		WNDCLASSW wc{};
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.lpfnWndProc = WndProc;
		wc.lpszClassName = kClass;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
		RegisterClassW(&wc);
		g_hwnd = CreateWindowExW(
			WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
			kClass,
			L"",
			WS_POPUP,
			0, 0, 10, 10,
			nullptr, nullptr, GetModuleHandleW(nullptr), nullptr
		);
		if (!g_hwnd)
			return false;
		if (!g_font)
		{
			g_font = CreateFontW(
				-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
				CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_DONTCARE, L"Segoe UI");
		}
		return true;
	}

	static int ClampInt(int v, int lo, int hi)
	{
		return (v < lo) ? lo : ((v > hi) ? hi : v);
	}

	static void Show(const TooltipEntry* src, const std::wstring& text)
	{
		if (!src || text.empty())
			return;
		if (!EnsureWindow())
			return;

		const bool was_visible = (IsWindowVisible(g_hwnd) != FALSE);
		const bool content_changed = (g_src != src) || (g_text != text);

		g_src = src;
		if (content_changed)
			g_text = text;

		POINT pt{};
		GetCursorPos(&pt);
		MONITORINFO mi{};
		mi.cbSize = sizeof(mi);
		HMONITOR hm = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
		GetMonitorInfoW(hm, &mi);
		RECT bounds = mi.rcWork;
		bool have_bounds = false;
		auto TryAcceptBounds = [&](HWND hwnd) -> bool
			{
				if (!hwnd || !IsWindow(hwnd))
					return false;
				if (hwnd == g_hwnd)
					return false;
				RECT rw{};
				if (!GetWindowRect(hwnd, &rw))
					return false;
				if (rw.right <= rw.left || rw.bottom <= rw.top)
					return false;
				bounds = rw;
				have_bounds = true;
				return true;
			};

		// 1) Prefer known game main window (stable across mouse-over tooltip window).
		if (!TryAcceptBounds(g_game_main_hwnd))
		{
			// 2) Fallback: resolve from cursor root window.
			HWND hwnd_under_cursor = WindowFromPoint(pt);
			if (hwnd_under_cursor)
			{
				HWND hwnd_root = GetAncestor(hwnd_under_cursor, GA_ROOT);
				TryAcceptBounds(hwnd_root);
			}
		}

		// 3) Use last valid host bounds if current lookup failed.
		if (!have_bounds && g_have_last_host_bounds)
		{
			bounds = g_last_host_bounds;
			have_bounds = true;
		}
		if (have_bounds)
		{
			g_last_host_bounds = bounds;
			g_have_last_host_bounds = true;
		}
		const int work_w = (bounds.right > bounds.left) ? (bounds.right - bounds.left) : 1280;
		const int work_h = (bounds.bottom > bounds.top) ? (bounds.bottom - bounds.top) : 720;
		const int max_w_by_screen = ClampInt(work_w / 2, 200, 1200);
		const int max_h_by_screen = ClampInt(work_h / 2, 120, 800);

		int w = 0, h = 0;
		const bool position_only = (!content_changed && was_visible);
		if (position_only)
		{
			RECT rw{};
			if (GetWindowRect(g_hwnd, &rw))
			{
				w = rw.right - rw.left;
				h = rw.bottom - rw.top;
			}
		}
		if (w <= 0 || h <= 0)
		{
			HDC hdc = GetDC(g_hwnd);
			if (g_font)
				SelectObject(hdc, g_font);
			TEXTMETRICW tm{};
			GetTextMetricsW(hdc, &tm);
			const int avg_char_w = (tm.tmAveCharWidth > 0) ? tm.tmAveCharWidth : 8;
			const int keyword_w = 16 + static_cast<int>(src->len) * avg_char_w;
			const int min_w = ClampInt(keyword_w, 80, max_w_by_screen);
			const int max_w = max_w_by_screen;

			RECT one_line_rc{ 0, 0, 0, 0 };
			DrawTextW(hdc, g_text.c_str(), static_cast<int>(g_text.size()), &one_line_rc,
				DT_LEFT | DT_TOP | DT_NOPREFIX | DT_SINGLELINE | DT_CALCRECT);
			int desired_w = (one_line_rc.right - one_line_rc.left) + 16;
			const int tip_chars = static_cast<int>(g_text.size());
			int long_pref_w = 0;
			if (tip_chars >= 96)
				long_pref_w = (work_w * 50) / 100;
			else if (tip_chars >= 48)
				long_pref_w = (work_w * 40) / 100;
			else if (tip_chars >= 24)
				long_pref_w = (work_w * 30) / 100;
			if (long_pref_w > 0)
				desired_w = max(desired_w, long_pref_w);
			desired_w = ClampInt(desired_w, min_w, max_w);

			RECT rc{ 0, 0, desired_w - 16, 0 };
			DrawTextW(hdc, g_text.c_str(), static_cast<int>(g_text.size()), &rc,
				DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT);

			w = (rc.right - rc.left) + 16;
			h = (rc.bottom - rc.top) + 12;
			if (w < 60) w = 60;
			if (h < 28) h = 28;

			// If height is too tall, try max width to improve readability first.
			if (h > max_h_by_screen && desired_w < max_w)
			{
				RECT rc_wide{ 0, 0, max_w - 16, 0 };
				DrawTextW(hdc, g_text.c_str(), static_cast<int>(g_text.size()), &rc_wide,
					DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT);
				const int w_wide = (rc_wide.right - rc_wide.left) + 16;
				const int h_wide = (rc_wide.bottom - rc_wide.top) + 12;
				if (h_wide < h)
				{
					w = w_wide;
					h = h_wide;
				}
			}
			if (h > max_h_by_screen) h = max_h_by_screen;
			ReleaseDC(g_hwnd, hdc);
		}

		const int space_left = pt.x - bounds.left;
		const int space_right = bounds.right - pt.x;
		const int space_above = pt.y - bounds.top;
		const int space_below = bounds.bottom - pt.y;
		const bool fit_left = space_left >= w;
		const bool fit_right = space_right >= w;
		const bool fit_above = space_above >= h;
		const bool fit_below = space_below >= h;

		const int MARGIN = 10; // minimum margin from cursor to tooltip edge

		int x = pt.x;
		if (fit_right && !fit_left)
			x = pt.x + MARGIN;
		else if (fit_left && !fit_right)
			x = pt.x - w - MARGIN;
		else if (fit_left && fit_right)
			x = (space_right >= space_left) ? pt.x + MARGIN : (pt.x - w - MARGIN);
		else
			x = (space_right >= space_left) ? pt.x + MARGIN : (pt.x - w - MARGIN);

		int y = pt.y - h;
		if (fit_above && !fit_below)
			y = pt.y - h - MARGIN;
		else if (fit_below && !fit_above)
			y = pt.y + MARGIN;
		else if (fit_above && fit_below)
			y = (space_above >= space_below) ? (pt.y - h - MARGIN) : (pt.y + MARGIN);
		else
			y = (space_above >= space_below) ? (pt.y - h - MARGIN) : (pt.y + MARGIN);

		if (x + w > bounds.right) x = bounds.right - w;
		if (y + h > bounds.bottom) y = bounds.bottom - h;
		if (x < bounds.left) x = bounds.left;
		if (y < bounds.top) y = bounds.top;

		if (position_only)
		{
			// Move only: do not "show" repeatedly or touch z-order to avoid flicker.
			SetWindowPos(g_hwnd, nullptr, x, y, 0, 0,
				SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
		}
		else
		{
			SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
			InvalidateRect(g_hwnd, nullptr, TRUE);
		}
	}

	static void OnHover(const TooltipEntry* src)
	{
		if (!src) return;
		Show(src, src->tip);
	}

	static void OnLeave(const TooltipEntry* src)
	{
		if (!g_src)
			return;
		if (src && g_src != src)
			return;
		Hide();
	}
}


static TooltipEntry* ResolveTooltipEntryFromCallbackCtx(int* callback_ctx)
{
	__try
	{
		if (!callback_ctx || !callback_ctx[0])
			return nullptr;
		DWORD p_ctx0 = static_cast<DWORD>(callback_ctx[0]);
		DWORD p_node_desc = *reinterpret_cast<DWORD*>(p_ctx0);
		if (!p_node_desc)
			return nullptr;
		return reinterpret_cast<TooltipEntry*>(*reinterpret_cast<DWORD*>(p_node_desc + 0x10));
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return nullptr;
	}
}


static bool IsKnownTooltipEntryPtr(const TooltipEntry* tip)
{
	return tip && g_tip_entry_ptrs.find(tip) != g_tip_entry_ptrs.end();
}

char __cdecl TooltipKeyword_Callback(int mode, int button_node, int* callback_ctx);

static bool IsTooltipButtonNode(const DWORD* node)
{
	if (!node)
		return false;
	if (node[5] == reinterpret_cast<DWORD>(&TooltipKeyword_Callback))
		return true;
	const TooltipEntry* tip = reinterpret_cast<const TooltipEntry*>(node[6]);
	return IsKnownTooltipEntryPtr(tip);
}

static COLORREF TooltipKeywordColorForMode(int mode)
{
	// Cool Slate (neutral, modern); a little bit grey
	switch (mode)
	{
	case 0:  return RGB(0xC9, 0xD1, 0xDB); // hover
	case 1:  return RGB(0xB6, 0xBE, 0xC8); // press-in
	default: return RGB(0xDE, 0xE6, 0xF0); // normal/reset
	}
}

static void ApplyTooltipKeywordColor(int* callback_ctx, int color)
{
	if (!callback_ctx)
		return;

	DWORD fn = reinterpret_cast<DWORD>(GetModuleHandleA(NULL)) + 0x8A1A0;
	__asm
	{
		push color
		mov eax, callback_ctx
		mov ecx, fn
		call ecx
		add esp, 4
	}
}

char __cdecl TooltipKeyword_Callback(int mode, int button_node, int* callback_ctx)
{
	TooltipEntry* tip = ResolveTooltipEntryFromCallbackCtx(callback_ctx);
	if (!IsKnownTooltipEntryPtr(tip))
		tip = nullptr;
	(void)button_node;
	static const TooltipEntry* s_last_hover_tip = nullptr;

	if (mode == 11)
	{
		if (callback_ctx)
			callback_ctx[8] = 1; // always register/render tooltip keyword region
		return 1;
	}

	// Keep jump sentinel untouched for tooltip-only entries: must remain -1.
	if (callback_ctx)
		callback_ctx[8] = -1;

	auto ShowTooltipForMode = [&](int color_mode)
		{
			if (!tip)
				return;
			ApplyTooltipKeywordColor(callback_ctx, TooltipKeywordColorForMode(color_mode));
			TooltipPopup::OnHover(tip);
			s_last_hover_tip = tip;
		};

	auto LeaveTooltip = [&](int color_mode)
		{
			const TooltipEntry* leave_tip = tip ? tip : s_last_hover_tip;
			ApplyTooltipKeywordColor(callback_ctx, TooltipKeywordColorForMode(color_mode));
			TooltipPopup::OnLeave(leave_tip);
			s_last_hover_tip = nullptr;
		};

	switch (mode)
	{
	case 0: // hover
		ShowTooltipForMode(mode);
		break;
	case 1: // press-in
		ShowTooltipForMode(mode);
		break;
	case 8: // pre-hover gate path in some button-framework flows
		ShowTooltipForMode(0);
		break;
	case 3: // release-inside
	case 4: // confirm
		ShowTooltipForMode(mode);
		break;
	case 2: // drag-out
	case 5: // release-outside
		LeaveTooltip(mode);
		break;
	case 6: // reset
		if (tip)
		{
			ApplyTooltipKeywordColor(callback_ctx, TooltipKeywordColorForMode(mode));
		}
		// Do not drive tooltip hide here: mode 6 can fire repeatedly in idle/reset paths.
		break;
	default:
		break;
	}
	return 1;
}


static bool HasNonTooltipButtonForCurrentText(DWORD* text_field)
{
	if (!text_field)
		return false;
	const DWORD key = text_field[0xB4 / 4];
	if (key == 0xFFFFFFFF)
		return false;
	DWORD* const list_head = reinterpret_cast<DWORD*>(text_field[0x54 / 4]);
	if (!list_head)
		return false;

	const DWORD sentinel = reinterpret_cast<DWORD>(list_head);
	DWORD node_ptr = list_head[0];
	for (unsigned guard = 0; guard < 0x4000 && node_ptr && node_ptr != sentinel; ++guard)
	{
		const DWORD* const node = reinterpret_cast<const DWORD*>(node_ptr);
		if (node[7] == key && !IsTooltipButtonNode(node))
			return true;
		node_ptr = node[0];
	}
	return false;
}

static bool IsSkipModeActive()
{
	const DWORD base = reinterpret_cast<DWORD>(GetModuleHandleA(nullptr));
	__try
	{
		// Verified from WindowIconTray toggle paths:
		// byte_51F82C is the skip mode flag.
		const BYTE* const skip_flag = reinterpret_cast<const BYTE*>(base + 0x11F82C);
		return (*skip_flag != 0);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

static bool IsAutoModeActive()
{
	const DWORD base = reinterpret_cast<DWORD>(GetModuleHandleA(nullptr));
	__try
	{
		// Verified from WindowIconTray toggle paths:
		// byte_51F82D is the auto mode flag.
		const BYTE* const auto_flag = reinterpret_cast<const BYTE*>(base + 0x11F82D);
		return (*auto_flag != 0);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

// has active button key?
HOOKJMP hksub_48F7E0;
__declspec(naked) char __cdecl orgsub_48F7E0(DWORD* text_field)
{
	__asm
	{
		push ebp
		mov ebp, esp
		push ebx
		lea ecx, hksub_48F7E0
		call HOOKJMP::get
		mov ecx, eax
		mov ebx, text_field
		call ecx
		pop ebx
		leave
		ret
	}
}

char __stdcall mysub_48F7E0(DWORD* text_field)
{
	const bool skip_active = IsSkipModeActive();
	const bool auto_active = IsAutoModeActive();
	const bool suppress_on_skip = skip_active && !g_tooltip_interrupt_skip;
	const bool suppress_on_auto = auto_active && !g_tooltip_interrupt_auto;
	if (suppress_on_skip || suppress_on_auto)
	{
		__try
		{
			return HasNonTooltipButtonForCurrentText(text_field) ? 1 : 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return orgsub_48F7E0(text_field);
		}
	}
	return orgsub_48F7E0(text_field);
}

__declspec(naked) char __cdecl sub_48F7E0()
{
	__asm
	{
		push ebp
		mov ebp, esp
		push ebx
		call mysub_48F7E0
		leave
		ret
	}
}


static bool RegisterTooltipButtonRegion(
	DWORD* text_field,
	unsigned short line0,
	unsigned short start0,
	unsigned short len,
	const TooltipEntry* tip)
{
	if (!text_field || !tip || len == 0)
		return false;

	DWORD req[5]{};
	req[0] = static_cast<DWORD>(line0) | (static_cast<DWORD>(start0) << 16);
	req[1] = static_cast<DWORD>(len);
	req[2] = 0;
	req[3] = reinterpret_cast<DWORD>(&TooltipKeyword_Callback);
	req[4] = reinterpret_cast<DWORD>(tip);

	DWORD fn = reinterpret_cast<DWORD>(GetModuleHandleA(NULL)) + 0x8E500;
	BYTE ok = 0;
	__asm
	{
		lea eax, req
		mov edi, text_field
		mov ecx, fn
		call ecx
		mov ok, al
	}
	return ok != 0;
}

static void RegisterTooltipsForCurrentText(DWORD* text_field, unsigned line_idx, unsigned glyph_idx)
{
	if (!text_field)
		return;
	LONG key = InterlockedCompareExchange(&g_cur_text_key, 0, 0);
	if (key < 0)
		return;
	std::uint16_t cdnum = static_cast<std::uint16_t>((key >> 16) & 0xFFFF);
	std::uint16_t text_idx = static_cast<std::uint16_t>(key & 0xFFFF);

	auto cd_it = tip_map.find(cdnum);
	if (cd_it == tip_map.end())
		return;
	auto idx_it = cd_it->second.find(text_idx);
	if (idx_it == cd_it->second.end())
		return;

	for (const auto& tip : idx_it->second)
	{
		if (tip.line == 0 || tip.start == 0 || tip.len == 0)
			continue;
		unsigned short line0 = static_cast<unsigned short>(tip.line - 1);
		unsigned short start0 = static_cast<unsigned short>(tip.start - 1);
		unsigned short len = static_cast<unsigned short>(tip.len);
		if (line0 < static_cast<unsigned short>(line_idx))
			continue;
		if (line0 == static_cast<unsigned short>(line_idx) &&
			start0 < static_cast<unsigned short>(glyph_idx))
			continue;

		RegisterTooltipButtonRegion(text_field, line0, start0, len, &tip);
	}
}


static DWORD* GetActiveScenarioTextField()
{
	// Verified from IDA:
	// - GCTaskInfo global: imagebase + 0x11E670 (abs 0x51E670 in original image)
	// - CTaskInfo::Task at +0x48
	// - scenario text field used by hyperlink flow at Task + 0x70
	const DWORD base = reinterpret_cast<DWORD>(GetModuleHandleA(nullptr));
	__try
	{
		DWORD* const task_info = *reinterpret_cast<DWORD**>(base + 0x11E670);
		if (!task_info)
			return nullptr;
		if ((task_info[0x44 / 4] & 1u) == 0)
			return nullptr;
		const DWORD task = task_info[0x48 / 4];
		if (!task)
			return nullptr;
		return reinterpret_cast<DWORD*>(task + 0x70);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return nullptr;
	}
}

static DWORD g_render_text_on_attribs_trampoline = 0;

// RenderTextOnAttribs, get line and glyph
HOOKJMP hksub_4868D0;
__declspec(naked) char __cdecl sub_4868D0()
{
	__asm
	{
		// Save argument registers.
		push eax
		push edx
		push ecx
		push edi

		// After 4 pushes:
		// [esp+00] = saved edi  (TextField*)
		// [esp+04] = saved ecx  (glyph_idx)
		// [esp+28] = original [esp+18] (line_idx stack arg)
		mov eax, dword ptr[esp + 28h]
		mov dword ptr[g_last_register_line_idx], eax
		mov eax, dword ptr[esp + 4]
		mov dword ptr[g_last_register_glyph_idx], eax
		mov eax, dword ptr[esp + 0]
		mov dword ptr[g_last_register_tfl], eax

		lea ecx, hksub_4868D0
		call HOOKJMP::get
		mov dword ptr[g_render_text_on_attribs_trampoline], eax

		// Restore original argument registers and continue original execution.
		pop edi
		pop ecx
		pop edx
		pop eax
		jmp dword ptr[g_render_text_on_attribs_trampoline]
	}
}


// register callback to buttons
HOOKJMP hksub_48E840;
__declspec(naked) char __cdecl orgsub_48E840(DWORD* text_field)
{
	__asm
	{
		push ebp
		mov ebp, esp
		lea ecx, hksub_48E840
		call HOOKJMP::get
		mov ecx, eax
		mov eax, text_field
		call ecx
		leave
		ret
	}
}

char __stdcall mysub_48E840(DWORD* text_field)
{
	// Register before TextField_RebuildButtonRegionsAndDispatch() so current pass
	// consumes the new regions and we do not induce extra dirty-bit rebuild cycles.
	if (text_field)
	{
		const DWORD field = reinterpret_cast<DWORD>(text_field);
		DWORD* const scenario_tfl = GetActiveScenarioTextField();
		if (scenario_tfl != text_field)
		{
			// This rebuild path is generic across many UI text fields (save/chapter/menu).
			// Never register dialogue tooltip regions into non-scenario text fields.
			g_tooltip_registered_key_by_field.erase(field);
			return orgsub_48E840(text_field);
		}
		if (InterlockedCompareExchange(&g_last_register_tfl, 0, 0) != static_cast<LONG>(field))
		{
			// Register only when render capture belongs to this same field.
			// Otherwise line/glyph lower-bound context is undefined for this callback pass.
			return orgsub_48E840(text_field);
		}
		LONG key = InterlockedCompareExchange(&g_cur_text_key, 0, 0);
		if (key >= 0)
		{
			auto it = g_tooltip_registered_key_by_field.find(field);
			if (it == g_tooltip_registered_key_by_field.end() || it->second != key)
			{
				const unsigned line_idx = static_cast<unsigned>(InterlockedCompareExchange(&g_last_register_line_idx, 0, 0));
				const unsigned glyph_idx = static_cast<unsigned>(InterlockedCompareExchange(&g_last_register_glyph_idx, 0, 0));
				RegisterTooltipsForCurrentText(text_field, line_idx, glyph_idx);
				g_tooltip_registered_key_by_field[field] = key;
			}
		}
	}
	return orgsub_48E840(text_field);
}

__declspec(naked) char __cdecl sub_48E840()
{
	__asm
	{
		push ebp
		mov ebp, esp
		push eax
		call mysub_48E840
		leave
		ret
	}
}

// ----------------------------------------------------------
// Tooltip runtime reset tied to hyperlink lifecycle:
// when the engine clears hyperlink entries, clear tooltip popup/registration state.
// ----------------------------------------------------------
static void ResetTooltipRuntimeState(const wchar_t* reason)
{
	TooltipPopup::Hide();
	InterlockedExchange(&g_cur_text_key, -1);
	InterlockedExchange(&g_last_register_tfl, 0);
	InterlockedExchange(&g_last_register_line_idx, 0);
	InterlockedExchange(&g_last_register_glyph_idx, 0);
	g_tooltip_registered_key_by_field.clear();
	if (reason)
	{
		std::wstring msg = L"Tooltip runtime reset: ";
		msg += reason;
		dbg.Log(msg);
	}
}


// clear hyper links
HOOKJMP hksub_493090;
__declspec(naked) DWORD* __cdecl orgsub_493090(DWORD* hyperlink_info_buf)
{
	__asm
	{
		push ebp
		mov ebp, esp
		lea ecx, hksub_493090
		call HOOKJMP::get
		mov ecx, eax
		mov edi, hyperlink_info_buf
		call ecx
		leave
		ret
	}
}

DWORD* __stdcall mysub_493090(DWORD* hyperlink_info_buf)
{
	DWORD* ret = orgsub_493090(hyperlink_info_buf);
	ResetTooltipRuntimeState(L"TextHyperlink_ClearEntries (0x493090)");
	return ret;
}

__declspec(naked) DWORD* __cdecl sub_493090()
{
	__asm
	{
		push ebp
		mov ebp, esp
		push edi
		call mysub_493090
		leave
		ret
	}
}


// ---------------------------------------------------------------------
// Tooltip save/load hook stubs.
// Current patch has no tooltip viewed-state persistence behavior.
// ---------------------------------------------------------------------
static bool SaveTooltipStateChunk(HANDLE hFile)
{
	(void)hFile;
	return true;
}

// save hyper links
HOOKJMP hksub_462550;
using tpSub_462550 = char(__stdcall*)(int, HANDLE);
char __stdcall orgsub_462550(int local_scene_pack, HANDLE hFile)
{
	tpSub_462550 fn = reinterpret_cast<tpSub_462550>(hksub_462550.get());
	return fn(local_scene_pack, hFile);
}
char __stdcall mysub_462550(int local_scene_pack, HANDLE hFile)
{
	dbg.Log(L"Saving local scene pack...");
	const char ok = orgsub_462550(local_scene_pack, hFile);
	if (!ok)
		return 0;
	if (!SaveTooltipStateChunk(hFile))
	{
		dbg.WarnPopup(L"Tooltip chunk save failed. Corrupt savedata.");
		return 0;
	}
	return 1;
}
__declspec(naked) char __cdecl sub_462550()
{
	__asm
	{
		push ebp
		mov ebp, esp
		push dword ptr[ebp + 0xC]  // hFile
		push dword ptr[ebp + 0x8]  // local_scene_pack
		call mysub_462550
		leave
		ret 0x8
	}
}

// load hyper links
HOOKJMP hksub_4621D0;
using tpSub_4621D0 = char(__stdcall*)(DWORD*, HANDLE);
char __stdcall orgsub_4621D0(DWORD* local_scene_pack, HANDLE hFile)
{
	tpSub_4621D0 fn = reinterpret_cast<tpSub_4621D0>(hksub_4621D0.get());
	return fn(local_scene_pack, hFile);
}
char __stdcall mysub_4621D0(DWORD* local_scene_pack, HANDLE hFile)
{
	dbg.Log(L"Loading local scene pack...");
	const char ok = orgsub_4621D0(local_scene_pack, hFile);
	if (!ok)
		return 0;

	TooltipPopup::Hide();
	(void)hFile;
	return 1;
}
__declspec(naked) char __cdecl sub_4621D0()
{
	__asm
	{
		push ebp
		mov ebp, esp
		push dword ptr[ebp + 0xC]  // hFile
		push dword ptr[ebp + 0x8]  // local_scene_pack
		call mysub_4621D0
		leave
		ret 0x8
	}
}


wstring MBTWS(const char* str, int page)
{
	int len = MultiByteToWideChar(page, 0, str, strlen(str) + 1, NULL, 0);
	wchar_t* wstr = new wchar_t[len];
	if (MultiByteToWideChar(page, 0, str, strlen(str) + 1, wstr, len) == 0)
		dbg.FatalPopup(L"MBTWS() failed");
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
	char* mstr = new char[len];
	if (WideCharToMultiByte(page, 0, str.data(), str.size() + 1, mstr, len, NULL, NULL) == 0)
		dbg.FatalPopup(L"WSTMB() failed");
	return mstr;
}


namespace DbgWindow
{
	// ----------------------------------------------------------
	// Globals (kept minimal & threadsafe where it matters)
	// ----------------------------------------------------------
	static constexpr int     kMargin = 8;        // px padding all sides
	static constexpr LPCWSTR kFontFace = L"Segoe UI";
	static constexpr int     kPtMin = 6;
	static constexpr int     kPtMax = 120;      // allow very large for 4K scaling
	static int currentPt = 0;  // store current font size in points
	static HINSTANCE      g_hInst = GetModuleHandleW(nullptr);
	static HWND           g_hWnd = nullptr;
	//static HWND           g_hLabel = nullptr;
	static HFONT          g_hFont = nullptr;   // currently selected font
	static std::wstring   g_text = L"Wait Until Scenario ......\n";
	static std::wstring   g_caption = L"Debug Window";
	static std::mutex     g_stateMutex;        // protects caption/text swaps

	HHOOK g_kbHook = nullptr;
	static void start_window(const wstring& caption, const wstring& text);

	static std::deque<std::wstring> g_hist;   // previously rendered texts
	static int g_prevIdx = 0;

	// Helper: Convert UTF‑8 → UTF‑16
	static std::wstring Utf8ToUtf16(const char* utf8)
	{
		return ::MBTWS(utf8, CP_UTF8);
	}

	static std::vector<std::wstring> SplitLines(const std::wstring& text)
	{
		std::vector<std::wstring> out;
		size_t start = 0;
		for (size_t i = 0; i < text.size(); ++i)
		{
			if (text[i] == L'\n')
			{
				size_t len = (i > start && text[i - 1] == L'\r') ? (i - start - 1) : (i - start);
				out.emplace_back(text.substr(start, len));
				start = i + 1;
			}
		}
		out.emplace_back(text.substr(start));
		return out;
	}

	// -----------------------------------------------------------------------------
	// Font sizing: use DrawTextW with DT_CALCRECT (no word‑wrap, no prefix).
	// Accurate because same path as WM_PAINT.
	// -----------------------------------------------------------------------------
	static void AdjustFontToFit()
	{
		if (!g_hWnd) return;

		RECT rcClient{}; GetClientRect(g_hWnd, &rcClient);
		int availW = rcClient.right - rcClient.left - 2 * kMargin;
		int availH = rcClient.bottom - rcClient.top - 2 * kMargin;
		if (availW <= 0 || availH <= 0) return;

		HDC hdc = GetDC(g_hWnd);

		int low = kPtMin, high = kPtMax, best = kPtMin;
		const DWORD flags = DT_LEFT | DT_TOP | DT_NOPREFIX;

		while (low <= high)
		{
			int midPt = (low + high) / 2;
			int lfHeight = -MulDiv(midPt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
			HFONT hTest = CreateFontW(lfHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
				CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_DONTCARE, kFontFace);
			HFONT hOld = (HFONT)SelectObject(hdc, hTest);

			RECT rcCalc = { 0,0,availW,availH };
			DrawTextW(hdc, g_text.c_str(), static_cast<int>(g_text.size()), &rcCalc,
				flags | DT_CALCRECT);
			int neededW = rcCalc.right - rcCalc.left;
			int neededH = rcCalc.bottom - rcCalc.top;

			bool fits = (neededW <= availW) && (neededH <= availH);

			SelectObject(hdc, hOld);
			DeleteObject(hTest);

			if (fits) { best = midPt; low = midPt + 1; }
			else { high = midPt - 1; }
		}

		// (Re)build font if size changed ----------------------------------------
		if (best != currentPt)
		{
			currentPt = best;
			if (g_hFont) DeleteObject(g_hFont);
			int lfHeight = -MulDiv(best, GetDeviceCaps(hdc, LOGPIXELSY), 72);
			g_hFont = CreateFontW(lfHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
				CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_DONTCARE, kFontFace);
			InvalidateRect(g_hWnd, nullptr, TRUE);
		}

		ReleaseDC(g_hWnd, hdc);
	}

	// -----------------------------------------------------------------------------
	// PAINT — draw the text ourselves (same flags as measuring)
	// -----------------------------------------------------------------------------
	static void PaintContent(HWND hWnd)
	{
		PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
		HGDIOBJ hOld = nullptr;
		if (g_hFont) hOld = SelectObject(hdc, g_hFont);
		SetBkMode(hdc, TRANSPARENT);

		RECT rc{}; GetClientRect(hWnd, &rc);
		rc.left += kMargin;
		rc.top += kMargin;
		rc.right -= kMargin;
		rc.bottom -= kMargin;

		DrawTextW(hdc, g_text.c_str(), static_cast<int>(g_text.size()), &rc,
			DT_LEFT | DT_TOP | DT_NOPREFIX);

		if (hOld) SelectObject(hdc, hOld);
		EndPaint(hWnd, &ps);
	}


	// ----------------------------------------------------------
	// Window procedure
	// ----------------------------------------------------------
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_CREATE:
		{
			//g_hLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
			//	0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			//SetWindowTextW(g_hLabel, g_text.c_str());
			AdjustFontToFit();
			return 0;
		}

		case WM_SHOWWINDOW:
			if (wParam && g_hWnd)
			{
				AdjustFontToFit();
				InvalidateRect(g_hWnd, nullptr, TRUE);
			}
			return 0;

		case WM_SIZE:
			AdjustFontToFit();
			return 0;

		case WM_PAINT:
			PaintContent(hWnd);
			return 0;

		case WM_MOUSEACTIVATE:
			return MA_NOACTIVATE; // stay unfocused even when clicked/dragged

		case WM_MOUSEWHEEL:
		{
			int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			if (delta > 0 && g_prevIdx < g_hist.size() - 1) // wheel-UP → previous
				++g_prevIdx;
			else if (delta < 0 && g_prevIdx > 0)			// wheel-DOWN → next
				--g_prevIdx;
			else
				break;

			if (g_hist.size() && g_hWnd)
			{
				g_text = g_hist[g_prevIdx];

				//SetWindowTextW(g_hLabel, g_text.c_str());
				AdjustFontToFit();

				SetWindowTextW(g_hWnd, (g_caption + L" / prev+" + std::to_wstring(g_prevIdx)).c_str());

				// always repaint
				InvalidateRect(g_hWnd, nullptr, TRUE);
			}
			return 0;
		}

		case WM_CLOSE:
			DestroyWindow(hWnd);
			return 0;

		/*case WM_SYSCOMMAND:
			if ((wParam & 0xFFF0) == SC_CLOSE)
				return 0;  // Ignore close
			break;*/

		case WM_DESTROY:
			if (g_hFont) { DeleteObject(g_hFont); g_hFont = nullptr; currentPt = 0; }
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	// ----------------------------------------------------------
	// Thread entry: own message loop so main program keeps running
	// ----------------------------------------------------------
	static unsigned __stdcall WindowThread(void*)
	{
		std::lock_guard lg(g_stateMutex);

		const wchar_t* kClass = L"DebugGameWindowClass";
		WNDCLASSEXW wc{ sizeof(wc) };
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc;
		wc.hInstance = g_hInst;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wc.lpszClassName = kClass;
		RegisterClassExW(&wc);

		// Create window (default size — user can resize later)
		DWORD ex = WS_EX_TOPMOST | WS_EX_NOACTIVATE;
		g_hWnd = CreateWindowExW(ex, kClass, g_caption.c_str(),
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 700, 200,
			nullptr, nullptr, g_hInst, nullptr);
		if (!g_hWnd) return 0;

		lg.~lock_guard();

		/*HMENU hMenu = GetSystemMenu(g_hWnd, FALSE);
		if (hMenu != nullptr) {
			DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
		}*/

		ShowWindow(g_hWnd, SW_SHOWNOACTIVATE);
		UpdateWindow(g_hWnd);

		// Standard message loop
		MSG msg;
		while (GetMessageW(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		g_hWnd = nullptr;
		UnregisterClassW(kClass, g_hInst); // cleanup class registration
		_endthreadex(0);
		return 0;
	}


	// ----------------------------------------------------------
	// Low-level keyboard hook: listen for Ctrl+N or Alt+N to reopen the debug window
	// ----------------------------------------------------------
	static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
		{
			KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
			bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
			bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

			if (p->vkCode == 'N' && (ctrl || alt))
			{
				// Reopen or restart debug window
				if (!g_hWnd)
				{
					start_window(g_caption, g_text);
				}
				else
				{
					ShowWindow(g_hWnd, SW_SHOWNOACTIVATE); // ensure visible
					AdjustFontToFit();
					InvalidateRect(g_hWnd, nullptr, TRUE);
				}
			}
		}
		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}


	// ----------------------------------------------------------
	// Public API — call from outside code.
	// Creates/updates the window on demand.
	// ----------------------------------------------------------
	static void start_window(const wstring& caption, const wstring& text)
	{
		std::lock_guard lg(g_stateMutex);

		//g_caption = Utf8ToUtf16(caption_utf8);
		//g_text = Utf8ToUtf16(text_utf8);
		g_caption = caption;
		g_text = text;

		if (g_hWnd) // already up — just update caption/text
		{
			SetWindowTextW(g_hWnd, g_caption.c_str());
			//SetWindowTextW(g_hLabel, g_text.c_str());
			ShowWindow(g_hWnd, SW_SHOWNOACTIVATE); // ensure visible
			AdjustFontToFit();
			InvalidateRect(g_hWnd, nullptr, TRUE);
			return;
		}

		uintptr_t hThread = _beginthreadex(nullptr, 0, WindowThread, nullptr, 0, nullptr);
		if (hThread) CloseHandle((HANDLE)hThread);
	}

	// Optional helpers ----------------------------------------------------------
	static void update_caption(const wstring& _caption)
	{
		std::lock_guard lg(g_stateMutex);
		//g_caption = Utf8ToUtf16(caption_utf8);
		g_caption = _caption;
		if (g_hWnd) SetWindowTextW(g_hWnd, g_caption.c_str());
	}

	static void update_text(const wstring& _text)
	{
		std::lock_guard lg(g_stateMutex);
		//g_text = Utf8ToUtf16(text_utf8);
		if (g_text == _text) return;
		g_text = _text;

		g_prevIdx = 0;
		g_hist.push_front(g_text); // save previous text
		if (g_hist.size() > 100) g_hist.pop_back();

#ifndef RELEASE
		if (!g_hWnd)
		{
			g_stateMutex.unlock();
			// window closed? reopen
			start_window(g_caption, g_text);
			g_stateMutex.lock();
		}
#endif

		if (g_hWnd)
		{
			//SetWindowTextW(g_hLabel, g_text.c_str());
			AdjustFontToFit();
			// always repaint
			InvalidateRect(g_hWnd, nullptr, TRUE);
		}
	}

	static void install_hook()
	{
		if (!g_kbHook)
			g_kbHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
	}

}

void LogCurText(DWORD* buf)
{
	DWORD base = (DWORD)GetModuleHandleA(NULL);
	DWORD GCScenario = base + 0x1194C0;
	DWORD data = *(DWORD*)(GCScenario + 0x24);
	DWORD pos = *(DWORD*)(GCScenario + 0x28);
	int cdnum = *(int*)(data + 0x4);
	DWORD secondBlock = *(DWORD*)(data + 0x28);
	DWORD offset = (pos - secondBlock) / 12;
	int idx = buf[2];

	static LONG prev_key = -1;
	LONG cur_key = PackTextKey(static_cast<std::uint16_t>(cdnum), static_cast<std::uint16_t>(idx));
	InterlockedExchange(&g_cur_text_key, cur_key);
	if (prev_key != cur_key)
	{
		TooltipPopup::Hide(); // next text always clears any active tooltip
		g_tooltip_registered_key_by_field.clear();
		InterlockedExchange(&g_last_register_tfl, 0);
		InterlockedExchange(&g_last_register_line_idx, 0);
		InterlockedExchange(&g_last_register_glyph_idx, 0);
		prev_key = cur_key;
	}

	static wchar_t tmp[1024];
	wsprintf(tmp, L"cdnum: %04d, text index: 0x%x", cdnum, idx);
	dbg.Log(tmp);
	wsprintf(tmp, L"Debug Window  (cd: %04d | off: 0x%x | text: 0x%x)", cdnum, offset, idx);
	DbgWindow::update_caption(tmp);
	if (!jap_map[cdnum].contains(idx) || jap_map[cdnum][idx].empty())
	{
		DbgWindow::update_text(L"");
		return;
	}

	std::vector<std::wstring> texts = jap_map[cdnum][idx];
	int prev_line = 0;
	if (hyp_map[cdnum].contains(idx) && hyp_map[cdnum][idx].size() > 0)
		for (const auto& hyp : hyp_map[cdnum][idx])
		{
			auto [line, start, len] = hyp;
			if (! (line - 1 < texts.size() && start - 1 + len < texts[line - 1].size()))
				line++; // maybe character name
			if (line != prev_line && line - 1 < texts.size() && start - 1 + len < texts[line - 1].size())
			{
				prev_line = line;
				// use quotes to represent hyper text
				auto& text = texts[line - 1];
				text.insert(text.begin() + start - 1, L'"');
				text.insert(text.begin() + start + len, L'"');
			}
		}
	auto join = [](auto vec, const auto& delim) { return std::accumulate(std::next(vec.begin()), vec.end(), vec[0],
		[&](const auto& a, const auto& b) { return a + delim + b; }); };
	DbgWindow::update_text(join(texts, L"\n"));
}


void LogCurInst()
{
	DWORD base = (DWORD)GetModuleHandleA(NULL);
	DWORD GCScenario = base + 0x1194C0;
	DWORD data = *(DWORD*)(GCScenario + 0x24);
	DWORD pos = *(DWORD*)(GCScenario + 0x28);
	int cdnum = *(int*)(data + 0x4);
	DWORD secondBlock = *(DWORD*)(data + 0x28);
	DWORD offset = (pos - secondBlock) / 12;
	static wchar_t tmp[1024];
	wsprintf(tmp, L"cdnum: %04d, inst offset: 0x%x", cdnum, offset);
	dbg.Log(tmp);
	wsprintf(tmp, L"Debug Window  (cd: %04d | off: 0x%x)", cdnum, offset);
	DbgWindow::update_caption(tmp);
}

bool CheckLoader()
{
	WCHAR exePath[MAX_PATH];
	if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0)
	{
		dbg.FatalPopup(L"GetModuleFileNameW() failed");
		return false;
	}
	WCHAR* lastBackslash = wcsrchr(exePath, L'\\');
	if (lastBackslash)
		*lastBackslash = L'\0';
	if (!SetCurrentDirectoryW(exePath))
	{
		dbg.FatalPopup(L"SetCurrentDirectoryW() failed");
		return false;
	}

	auto LReadFileToBuf = [](const wstring& path, void*& buf) -> DWORD
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
		return (dwSize == dwRead ? dwSize : 0);
	};

	auto LBuffHash = [](const void* buf, int len) -> __int64
	{
		__int64 hash = 0;
		for (int i = 0; i < len; i++)
			hash = (hash << 5) + hash + ((unsigned char*)buf)[i];
		return hash;
	};

	LPVOID lpBuffer{}; DWORD dwLength;
	if ((dwLength = LReadFileToBuf(L"HD\\gd.arc", lpBuffer)) == 0)
	{
		dbg.FatalPopup(L"Put this file into the Farthest2015 installation path!");
		return false;
	}
	if (LBuffHash(lpBuffer, dwLength) != 0xc655c64a2457fefa)
	{
		dbg.FatalPopup(L"Only Farthest2015 COMPLETE version is supported!");
		return false;
	}
	delete[] lpBuffer; lpBuffer = nullptr; dwLength = 0;

	return true;
}

void MainProc()
{
	if (!CheckLoader())
		ExitProcess(0);

	DWORD base = (DWORD) GetModuleHandleA(NULL);
	bool suc = true;
	
	// patch font validation (deprecated)
	/*const BYTE _PFont1[] = {"\xCB\xCE\xCC\xE5"}; // GBK 宋体
	suc = HOOK::patch(base + 0xFA874, (BYTE)0, 0xD) && HOOK::patch(base + 0xFA874, _PFont1, sizeof(_PFont1));
	suc &= HOOK::patch(base + 0xFA998, (BYTE)0x86, 0x1); // GB2312_CHARSET
	const BYTE _PFont2[] = { "\xBA\xDA\xCC\xE5" }; // GBK 黑体
	suc &= HOOK::patch(base + 0xFA9A4, (BYTE)0, 0xF) && HOOK::patch(base + 0xFA9A4, _PFont2, sizeof(_PFont2));
	suc &= HOOK::patch(base + 0xFAAC8, (BYTE)0x86, 0x1); // GB2312_CHARSET
	if (!suc)
		dbg.FatalPopup(L"Unable to patch unk_4FA870");*/

	// load custom font (deprecated)
	/*suc = AddFontResourceExW(L".\\LXGWWenKai-Regular.ttf", FR_PRIVATE, NULL);
	if (!suc)
		dbg.FatalPopup(L"Unable to load custom font LXGWWenKai-Regular.ttf");*/

	// patch font validation mbscmp()
	suc = HOOK::patch(base + 0x87E52, (BYTE)0xEB, 0x1);
	if (!suc)
		dbg.FatalPopup(L"Unable to patch hex:487E52 JMP");
	// patch 0xxx.cd signature validation
	const BYTE NOPs[] = { "\x90\x90" };
	suc = HOOK::patch(base + 0xBA268, NOPs, 0x2);
	if (!suc)
		dbg.FatalPopup(L"Unable to patch hex:4BA268 NOP");

	// patch Gaf004Loader::ReadITEM overflow validation
	suc = HOOK::patch(base + 0xB188D, (BYTE)0xEB, 0x1); // JMP
	if (!suc)
		dbg.FatalPopup(L"Unable to patch hex:4B188D JMP");
	
	// patch WINAPI
	suc = hkCreateWindowExA.hook(myCreateWindowExA, CreateWindowExA, 5);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook CreateWindowExA");
	suc = hkCreateFileA.hook(myCreateFileA, CreateFileA, 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook CreateFileA");
	suc = hkCreateFileW.hook(myCreateFileW, CreateFileW, 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook CreateFileW");
	suc = hkSHGetSpecialFolderPathA.hook(mySHGetSpecialFolderPathA, SHGetSpecialFolderPathA, 5);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook SHGetSpecialFolderPathA");
	suc = hkRegOpenKeyExA.hook(myRegOpenKeyExA, RegOpenKeyExA, 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook RegOpenKeyExA");
	suc = hkOutputDebugStringA.hook(myOutputDebugStringA, OutputDebugStringA, 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook OutputDebugStringA");
	suc = hkCreateFontA.hook(myCreateFontA, CreateFontA, 5);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook CreateFontA");
	suc = hkIsDBCSLeadByte.hook(myIsDBCSLeadByte, IsDBCSLeadByte, 5);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook IsDBCSLeadByte");
	suc = hkCharNextA.hook(myCharNextA, CharNextA, 5);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook CharNextA");
	suc = hkGetGlyphOutlineA.hook(myGetGlyphOutlineA, GetGlyphOutlineA, 5);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook GetGlyphOutlineA");
	suc = hkSetDlgItemText.hook(mySetDlgItemTextA, SetDlgItemTextA, 5);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook SetDlgItemTextA");
	suc = hkSetWindowText.hook(mySetWindowTextA, SetWindowTextA, 5);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook SetWindowTextA");
	

	suc = hksub_475E90.hook(sub_475E90, (LPVOID)(base + 0x75E90), 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_475E90 ReadScriptText");
	
	suc = hksub_472AB0.hook(sub_472AB0, (LPVOID)(base + 0x72AB0), 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_472AB0 ReadFuncFromGCScenario");
	suc = hksub_4BAA10.hook(sub_4BAA10, (LPVOID)(base + 0xBAA10), 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_4BAA10 MoveNextScenarioInstruction");
	suc = hkinst_421009.hook(inst_421009, (LPVOID)(base + 0x21009), 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook inst_421009 (in sub_420F10) BacklogTextStrip");
	suc = hkinst_478B00.hook(inst_478B00, (LPVOID)(base + 0x78B00), 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook inst_478B00 (in sub_478B00) ScriptInstruction_0x99_SetPGlobByte");
	suc = hkinst_478330.hook(inst_478330, (LPVOID)(base + 0x78330), 7);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook inst_478330 (in sub_478330) ScriptInstruction_0x96_Unknown_Menu");
	

	suc = hkRoundKey.hook(gdRoundKey, (LPVOID)(base + 0xA7BE0), 8);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook hex:4A7BE0 RoundKey");

	suc = hkSetFilePointer.hook(mySetFilePointer, SetFilePointer, 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook SetFilePointer");
	/*suc = hkReadFile.hook(myReadFile, L"ReadFile");
	if (!suc)
		dbg.FatalPopup(L"Unable to hook ReadFile");
	suc = hkCloseHandle.hook(myCloseHandle, L"CloseHandle");
	if (!suc)
		dbg.FatalPopup(L"Unable to hook CloseHandle");*/
	suc = hksub_4AFAC0.hook(sub_4AFAC0, (LPVOID)(base + 0xAFAC0), 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_4AFAC0 Gaf004Loader::png_Read");

	// patch BgmMode button song_20 image size (not irreversible)
	suc = HOOK::patch(base + 0x1004E0, (BYTE)101, 0x1); // from 86 to 101
	if (!suc)
		dbg.FatalPopup(L"Unable to patch hex:5004E3 BgmMode button song_20 image size");

	// patch tooltips, buttons, texts and savedata
	suc = hksub_48F7E0.hook(sub_48F7E0, (LPVOID)(base + 0x8F7E0), 10);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_48F7E0 TextField_HasActiveButtonKey");
	suc = hksub_4868D0.hook(sub_4868D0, (LPVOID)(base + 0x868D0), 9);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook RenderTextOnAttribs (0x4868D0)");
	suc = hksub_48E840.hook(sub_48E840, (LPVOID)(base + 0x8E840), 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_48E840 TextField_RebuildButtonRegionsAndDispatch");
	suc = hksub_462550.hook(sub_462550, (LPVOID)(base + 0x62550), 9);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_462550 LocalScenePack_SaveToFile");
	suc = hksub_4621D0.hook(sub_4621D0, (LPVOID)(base + 0x621D0), 9);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_4621D0 LocalScenePack_LoadFromFile");
	suc = hksub_493090.hook(sub_493090, (LPVOID)(base + 0x93090), 6);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook sub_493090 TextHyperlink_ClearEntries");


	// loading replacement files
	if (PathFileExistsW(L".\\chs"))
	{
		// list all .png files in the chs folder
		WIN32_FIND_DATAW findData;
		HANDLE hFind = FindFirstFileW(L".\\chs\\pics\\*.png", &findData);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				wstring filename = findData.cFileName;
				if (filename.size() > 4 && filename.substr(filename.size() - 4) == L".png")
				{
					wstring fullpath = L".\\chs\\pics\\" + filename;
					HANDLE file = CreateFileW(fullpath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if (file != INVALID_HANDLE_VALUE)
					{
						DWORD size = GetFileSize(file, NULL);
						if (size != INVALID_FILE_SIZE && size > 0)
						{
							std::vector<char> data(size);
							DWORD bytesRead;
							if (ReadFile(file, data.data(), size, &bytesRead, NULL) && bytesRead == size)
							{
								// hash value is extracted from filename_d55e81e2.png
								unsigned hash = 0;
								size_t pos = filename.find_last_of(L'_');
								if (pos != wstring::npos && pos + 1 < filename.size() - 4)
								{
									wstring hashStr = filename.substr(pos + 1, filename.size() - pos - 5);
									try
									{
										hash = std::stoul(hashStr, nullptr, 16);
									}
									catch (const std::exception&)
									{
										goto invalid;
									}
								}
								else
								{
invalid:
									dbg.Log(L"Invalid PNG filename format: " + filename);
									CloseHandle(file);
									continue;
								}
								png_replacement_table[hash] = std::move(data);
								static wchar_t s[1024];
								wsprintf(s, L"Loaded PNG replacement: %.600s (hash: %x)", filename.c_str(), hash);
								dbg.Log(s);
							}
						}
						CloseHandle(file);
					}
				}
			} while (FindNextFileW(hFind, &findData));
			FindClose(hFind);
		}

		static const std::string JAP_MARK = "\xe2\x96\xa1\xe2\x96\xa1\xe2\x96\xa1\xe2\x96\xa1"; // □□□□
		static const std::string CHS_MARK = "\xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa0"; // ■■■■
		static const std::string TRI_MARK = "\xe2\x96\xb3\xe2\x96\xb3\xe2\x96\xb3\xe2\x96\xb3"; // △△△△
		static const std::string TRF_MARK = "\xe2\x96\xb2\xe2\x96\xb2\xe2\x96\xb2\xe2\x96\xb2"; // ▲▲▲▲
		constexpr std::size_t MARK_LEN = 12;
		constexpr std::size_t ID_LEN = 8;

		auto RTrimInPlace = [](std::string& t)
			{
				t.erase(std::remove(t.begin(), t.end(), '\r'), t.end());
				t.erase(std::remove(t.begin(), t.end(), '\n'), t.end());
				t.erase(std::find_if(t.rbegin(), t.rend(), [](unsigned char ch) -> bool
					{
						return !std::isspace(ch);
					}
				).base(), t.end());
			};

		auto ParseMarkedLine = [&](const std::string& src, const std::string& mark,
			std::uint16_t& out_cd, std::uint16_t& out_idx, std::string& out_payload) -> bool
			{
				if (src.size() < MARK_LEN * 2 + ID_LEN)
					return false;
				const std::string marker1 = src.substr(0, MARK_LEN);
				const std::string idHex = src.substr(MARK_LEN, ID_LEN);
				const std::string marker2 = src.substr(MARK_LEN + ID_LEN, MARK_LEN);
				if (marker1 != mark || marker2 != mark)
					return false;
				for (char c : idHex)
					if (!std::isxdigit(static_cast<unsigned char>(c)))
						return false;
				out_cd = static_cast<std::uint16_t>(std::strtoul(idHex.substr(0, 4).c_str(), nullptr, 10));
				out_idx = static_cast<std::uint16_t>(std::strtoul(idHex.substr(4, 4).c_str(), nullptr, 16));
				out_payload = src.substr(MARK_LEN * 2 + ID_LEN);
				return true;
			};

		// load ExText.txt to construct translation tables
		std::ifstream fp(".\\chs\\ExText.txt", std::ios::binary);   // UTF-8 source
		std::string line;
		unsigned jpcount = 0, chcount = 0, mkcount = 0;
		while (std::getline(fp, line))
		{
			RTrimInPlace(line);

			if (!line.empty() && ![&]() -> bool
				{
					/*  Four squares/blocks in UTF-8 are 4 × 3 bytes = 12 bytes              */
					static const std::string JAP_MARK = "\xe2\x96\xa1\xe2\x96\xa1\xe2\x96\xa1\xe2\x96\xa1"; // □□□□
					static const std::string CHS_MARK = "\xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa0"; // ■■■■
					static const std::string TRI_MARK = "\xe2\x96\xb3\xe2\x96\xb3\xe2\x96\xb3\xe2\x96\xb3"; // △△△△
					static const std::string TRF_MARK = "\xe2\x96\xb2\xe2\x96\xb2\xe2\x96\xb2\xe2\x96\xb2"; // ▲▲▲▲
					constexpr std::size_t MARK_LEN = 12;                               // bytes
					constexpr std::size_t ID_LEN = 8;                                // ASCII

					/*  Minimal length:  marker + id + marker = 12 + 8 + 12 = 32 bytes      */
						if (line.size() < MARK_LEN * 2 + ID_LEN) return false;

					const std::string marker1 = line.substr(0, MARK_LEN);
					const std::string idHex = line.substr(MARK_LEN, ID_LEN);
					const std::string marker2 = line.substr(MARK_LEN + ID_LEN, MARK_LEN);
					const std::string payload = line.substr(MARK_LEN * 2 + ID_LEN); // may be ""

					/*  Basic sanity checks  */
					if (marker1 != marker2)                       return false;
					if (marker1 == TRF_MARK)					return true; // not used
					if (marker1 != JAP_MARK && marker1 != CHS_MARK &&
						marker1 != TRI_MARK && marker1 != TRF_MARK) return false;
					for (char c : idHex)                          // hex digits only?
						if (!std::isxdigit(static_cast<unsigned char>(c))) return false;

					const std::uint16_t hi =
						static_cast<std::uint16_t>(std::strtoul(idHex.substr(0, 4).c_str(), nullptr, 10));
					const std::uint16_t lo =
						static_cast<std::uint16_t>(std::strtoul(idHex.substr(4, 4).c_str(), nullptr, 16));

					if (marker1 == TRI_MARK)
					{
						mkcount++;
						int line, start, len;
						sscanf_s(payload.c_str(), "Line=%d Char=%d Len=%d", &line, &start, &len);
						if (line <= 0 || start <= 0 || len <= 0)
							return false;
						hyp_map[hi][lo].emplace_back(std::make_tuple(line, start, len));
						return true;
					}

					auto& target = (marker1 == JAP_MARK) ? jap_map : chs_map;
					(marker1 == JAP_MARK ? jpcount++ : chcount++);
					target[hi][lo].emplace_back(MBTWS(payload.c_str(), CP_UTF8));
					return true;
				}())
			{
				dbg.Log(L"Unexpected translation text: " + MBTWS(line.c_str(), CP_UTF8));
			}
		}

		// load newtips.txt to construct tooltip table (two-line format)
		std::ifstream fp_tip(".\\chs\\newtips.txt", std::ios::binary);
		std::string tip_line_raw;
		unsigned tipcount = 0;
		struct PendingTipEntry
		{
			bool active = false;
			std::uint16_t cdnum = 0;
			std::uint16_t text_idx = 0;
			std::uint16_t line = 0;
			std::uint16_t start = 0;
			std::uint16_t len = 0;
		} pending_tip;

		while (std::getline(fp_tip, tip_line_raw))
		{
			RTrimInPlace(tip_line_raw);
			if (tip_line_raw.size() >= 3 &&
				static_cast<unsigned char>(tip_line_raw[0]) == 0xEF &&
				static_cast<unsigned char>(tip_line_raw[1]) == 0xBB &&
				static_cast<unsigned char>(tip_line_raw[2]) == 0xBF)
			{
				tip_line_raw.erase(0, 3);
			}
			if (tip_line_raw.empty())
				continue;

			std::uint16_t cdnum = 0, text_idx = 0;
			std::string payload;
			if (!ParseMarkedLine(tip_line_raw, TRF_MARK, cdnum, text_idx, payload))
			{
				dbg.Log(L"Unexpected tooltip text line: " + MBTWS(tip_line_raw.c_str(), CP_UTF8));
				pending_tip.active = false;
				continue;
			}

			if (payload.rfind("Line=", 0) == 0)
			{
				int mark_line = 0, mark_start = 0, mark_len = 0;
				if (sscanf_s(payload.c_str(), "Line=%d Char=%d Len=%d", &mark_line, &mark_start, &mark_len) != 3 ||
					mark_line <= 0 || mark_start <= 0 || mark_len <= 0 ||
					mark_line > 65535 || mark_start > 65535 || mark_len > 65535)
				{
					dbg.Log(L"Invalid tooltip range line: " + MBTWS(tip_line_raw.c_str(), CP_UTF8));
					pending_tip.active = false;
					continue;
				}
				pending_tip.active = true;
				pending_tip.cdnum = cdnum;
				pending_tip.text_idx = text_idx;
				pending_tip.line = static_cast<std::uint16_t>(mark_line);
				pending_tip.start = static_cast<std::uint16_t>(mark_start);
				pending_tip.len = static_cast<std::uint16_t>(mark_len);
				continue;
			}

			if (!pending_tip.active || pending_tip.cdnum != cdnum || pending_tip.text_idx != text_idx)
			{
				dbg.Log(L"Tooltip text line without matching Line/Char/Len line: " + MBTWS(tip_line_raw.c_str(), CP_UTF8));
				continue;
			}

			std::wstring tip_text = payload.empty() ? L"(empty tooltip)" : MBTWS(payload.c_str(), CP_UTF8);
			TooltipEntry tip_entry{};
			tip_entry.cdnum = pending_tip.cdnum;
			tip_entry.text_idx = pending_tip.text_idx;
			tip_entry.line = pending_tip.line;
			tip_entry.start = pending_tip.start;
			tip_entry.len = pending_tip.len;
			tip_entry.tip = std::move(tip_text);
			tip_map[pending_tip.cdnum][pending_tip.text_idx].push_back(std::move(tip_entry));
			pending_tip.active = false;
			tipcount++;
		}
		RebuildTooltipEntryPointerIndex();

		static wchar_t s[1024] = { 0 };
		wsprintf(s, L"Loaded translation texts: %d cdnums; %d japs, %d chns; %d hyperlink markers; %d tooltip entries",
			jap_map.size(), jpcount, chcount, mkcount, tipcount);
		dbg.Log(s);
	}

#ifndef RELEASE
	DbgWindow::start_window(L"Debug Window", L"Wait Until Scenario ......\n");
#endif
	DbgWindow::install_hook();
}


