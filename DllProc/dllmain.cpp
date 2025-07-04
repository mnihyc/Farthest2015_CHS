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
#include <fstream>
#include <iostream>
#include <numeric>

#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

// switch release mode?
#undef RELEASE

// global debug
#ifdef RELEASE
DEBUG dbg{ L"DllProc", L"", false, true };
#else
//DEBUG dbg{ L"DllProc", L"d_dllproc.txt", false, true };
DEBUG dbg{ L"DllProc", L"", false, true };
#endif

// for easier coding
using std::wstring;

// main procedure
void MainProc();
// convert multi-byte to wstring
wstring MBTWS(const char* str, int page=932);
// convert wstring to multi-byte
char* WSTMB(const wstring& str, int page=936);

// png replacement table
std::unordered_map<unsigned, std::vector<char>> png_replacement_table;

// text translation table
template<typename T>
using Map2D = std::map<std::uint16_t,
	std::map<std::uint16_t, std::vector<T>>>;

static Map2D<std::wstring> jap_map, chs_map;
static Map2D<std::tuple<int, int, int>> hyp_map;


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
	if (lpClassName == lpWindowName)
		lpWindowName = "\xd7\xee\xb9\xfb\xa4\xc6\xa4\xce\xa5\xa4\xa5\xde COMPLETE \xa1\xaa\xa1\xaa \xb2\xe2\xca\xd4\xba\xba\xbb\xaf\xb2\xb9\xb6\xa1 v0.1.1 (2025.07.03)"; // 最果てのイマ COMPLETE —— 测试汉化补丁 v0.1 (2025.07.03)
	return CWEA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}


// perform virtual file map (deprecated)
HOOKJMP hkCreateFileA;
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
	static wchar_t s[1024] = {0};
	wsprintf(s, L"CreateFileA: %s, access: 0x%x, share: 0x%x, disp: 0x%x, attr: 0x%x", MBTWS(lpFileName).c_str(), dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes);
	dbg.Log(s);
	if (PathFileExistsW(L".\\chs"))
	{
		// get basename of lpFileName
		const char* baseName = strrchr(lpFileName, '\\');
		if (baseName == nullptr)
			baseName = lpFileName; // no path, use full name
		else
			baseName++; // skip the backslash
		// check if file exists in chs folder
		for (wstring search_path : { L".\\chs\\", L".\\chs\\cd\\" })
		{
			wstring chsFilePath = search_path + MBTWS(baseName);
			if (PathFileExistsW(chsFilePath.c_str()))
			{
				// file exists in chs folder, replace it
				wsprintf(s, L"Using replaced virtual file: %s with original %s", chsFilePath.c_str(), MBTWS(lpFileName).c_str());
				dbg.Log(s);
				return CreateFileW(chsFilePath.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
			}
		}
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
		pszFaceName = "\xCB\xCE\xCC\xE5"; // GBK 宋体
	}
	if (simple_hash(pszFaceName) == 0xb7319fef)
	{
		// ＭＳ Ｐゴシック
		pszFaceName = "\xBA\xDA\xCC\xE5"; // GBK 黑体
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
	if (wch[0] == L'\u9f1d') wch = L"♪";
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
		; add esp, 0x4
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
	LogCurInst();
	char ret = orgsub_4BAA10(a1);
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
	static HINSTANCE      g_hInst = GetModuleHandleW(nullptr);
	static HWND           g_hWnd = nullptr;
	static HWND           g_hLabel = nullptr;
	static HFONT          g_hFont = nullptr;   // currently selected font
	static std::wstring   g_text;              // current UTF‑16 text
	static std::wstring   g_caption;           // current UTF‑16 caption
	static std::mutex     g_stateMutex;        // protects caption/text swaps

	HHOOK g_kbHook = nullptr;
	static void start_window(const char* caption_utf8, const char* text_utf8);

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
		static int currentPt = 0;
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
			g_hLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
				0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);
			SetWindowTextW(g_hLabel, g_text.c_str());
			AdjustFontToFit();
			return 0;
		}

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

			if (g_hist.size() && g_hWnd && g_hLabel)
			{
				g_text = g_hist[g_prevIdx];

				SetWindowTextW(g_hLabel, g_text.c_str());
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
			if (g_hFont) { DeleteObject(g_hFont); g_hFont = nullptr; }
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
		AdjustFontToFit();
		InvalidateRect(g_hWnd, nullptr, TRUE);

		// Standard message loop
		MSG msg;
		while (GetMessageW(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		g_hWnd = nullptr;
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
					start_window("Debug Window", "Wait Until Scenario ......\n");
					Sleep(100);
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
	static void start_window(const char* caption_utf8, const char* text_utf8)
	{
		std::lock_guard lg(g_stateMutex);

		g_caption = Utf8ToUtf16(caption_utf8);
		g_text = Utf8ToUtf16(text_utf8);

		if (g_hWnd) // already up — just update caption/text
		{
			SetWindowTextW(g_hWnd, g_caption.c_str());
			SetWindowTextW(g_hLabel, g_text.c_str());
			ShowWindow(g_hWnd, SW_SHOWNOACTIVATE); // ensure visible
			AdjustFontToFit();
			InvalidateRect(g_hWnd, nullptr, TRUE);
			return;
		}

		uintptr_t hThread = _beginthreadex(nullptr, 0, WindowThread, nullptr, 0, nullptr);
		if (hThread) CloseHandle((HANDLE)hThread);
	}

	// Optional helpers ----------------------------------------------------------
	static void update_caption(wstring _caption)
	{
		std::lock_guard lg(g_stateMutex);
		//g_caption = Utf8ToUtf16(caption_utf8);
		g_caption = _caption;
		if (g_hWnd) SetWindowTextW(g_hWnd, g_caption.c_str());
	}

	static void update_text(wstring _text)
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
			start_window("Debug Window", "...");
			Sleep(100);
			g_stateMutex.lock();
		}
#endif

		if (g_hWnd && g_hLabel)
		{
			SetWindowTextW(g_hLabel, g_text.c_str());
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



void MainProc()
{
	DWORD base = (DWORD) GetModuleHandleA(NULL);
	bool suc = true;
	
	// patch font validation
	const BYTE _PFont1[] = { "\xCB\xCE\xCC\xE5" }; // GBK 宋体
	suc = HOOK::patch(base + 0xFA874, (BYTE)0, 0xD) && HOOK::patch(base + 0xFA874, _PFont1, sizeof(_PFont1));
	suc &= HOOK::patch(base + 0xFA998, (BYTE)0x86, 0x1); // GB2312_CHARSET
	const BYTE _PFont2[] = { "\xBA\xDA\xCC\xE5" }; // GBK 黑体
	suc &= HOOK::patch(base + 0xFA9A4, (BYTE)0, 0xF) && HOOK::patch(base + 0xFA9A4, _PFont2, sizeof(_PFont2));
	suc &= HOOK::patch(base + 0xFAAC8, (BYTE)0x86, 0x1); // GB2312_CHARSET
	if (!suc)
		dbg.FatalPopup(L"Unable to patch unk_4FA870");

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
	suc = hkSHGetSpecialFolderPathA.hook(mySHGetSpecialFolderPathA, SHGetSpecialFolderPathA, 5);
	if (!suc)
		dbg.FatalPopup(L"Unable to hook SHGetSpecialFolderPathA");
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
		


	// loading replacement files
	if (PathFileExistsW(L".\\chs"))
	{
		// list all .png files in the chs folder
		WIN32_FIND_DATAW findData;
		HANDLE hFind = FindFirstFileW(L".\\chs\\*.png", &findData);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				wstring filename = findData.cFileName;
				if (filename.size() > 4 && filename.substr(filename.size() - 4) == L".png")
				{
					wstring fullpath = L".\\chs\\" + filename;
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
								wsprintf(s, L"Loaded PNG replacement: %s (hash: %x)", filename.c_str(), hash);
								dbg.Log(s);
							}
						}
						CloseHandle(file);
					}
				}
			} while (FindNextFileW(hFind, &findData));
			FindClose(hFind);
		}

		// load ExText.txt to construct translation tables
		std::ifstream fp(".\\chs\\ExText.txt", std::ios::binary);   // UTF-8 source
		std::string line;
		unsigned jpcount = 0, chcount = 0, mkcount = 0;
		while (std::getline(fp, line))
		{
			line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
			line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
			line.erase(std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) -> bool
				{
					return !std::isspace(ch);
				}
			).base(), line.end());

			if (! line.empty() && ! [&]() -> bool
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
		static wchar_t s[1024] = { 0 };
		wsprintf(s, L"Loaded translation texts: %d cdnums; %d japs, %d chns; %d markers", jap_map.size(), jpcount, chcount, mkcount);
		dbg.Log(s);
	}

#ifndef RELEASE
	DbgWindow::start_window("Debug Window", "Wait Until Scenario ......\n");
#endif
	DbgWindow::install_hook();
}


