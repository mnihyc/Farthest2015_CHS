#pragma once
#include <windows.h>

namespace HOOK
{
	/*
		patch(addr, arr, size) => bool, whether succeeds
		addr (DWORD_PTR) address to patch
		arr (const BYTE*) data
		size (DWORD) bytes to patch
	*/
	bool patch(DWORD_PTR addr, const BYTE* arr, DWORD size);

	/*
		patch(addr, arr, size) => bool, whether succeeds
		addr (DWORD_PTR) address to patch
		char (const BYTE) data
		size (DWORD) bytes to patch
	*/
	bool patch(DWORD_PTR addr, const BYTE data, DWORD size);
};

#pragma pack(push, 1)
typedef struct _JMPCODE
{
	BYTE jmp = 0xE9;
	DWORD addr = 0;
}JMPCODE, * PJMPCODE;
#pragma pack(pop)

class HOOKJMP
{
// x86 only
private:
	DWORD size;
	LPVOID myF, orgF, gadF=NULL;
	bool hooked;
public:
	/*
		HOOKJMP(myF, orgF)
		myF (LPVOID) my hook function address, default NULL
		orgF (LPVOID) original function address, default NULL
		size (DWORD) size of a complete command to patch, default 0
	*/
	HOOKJMP(LPVOID myF=NULL, LPVOID orgF=NULL, DWORD size=0);
	
	/*
		hook(myF, orgF) => bool
		myF (LPVOID) my hook function address, default NULL
		orgF (LPVOID) original function address, default NULL
		size (DWORD) size of complete commands to patch, default 0
	*/
	bool hook(LPVOID myF=NULL, LPVOID orgF=NULL, DWORD size=0);
	
	/*
		unhook(orgF) => bool
		orgF (LPVOID) original function address, default NULL
	*/
	bool unhook(LPVOID orgF=NULL);
	
	/*
		get() => LPVOID, gadget function address
	*/
	LPVOID get();
};

class HOOKIAT
{
private:
	LPVOID myF, orgF;
	LPCWSTR orgName;
	bool hooked;
	
	/*
		replace(rpF) => bool, whether succeeds
		rpF (LPVOID) function address to replace to
		replace IAT table
	*/
	bool replace(LPVOID rpF);
	
	/*
		compare(ws, mb) => bool, whether equals
		ws (LPCWSTR) wstring
		mb (LPCSTR) string
	*/
	bool compare(LPCWSTR ws, LPCSTR mb);
public:
	/*
		HOOKIAT(myF, orgName, orgF)
		myF (LPVOID) my hook function address, default NULL
		orgName (LPCWSTR) original function name, default L""
		orgF (LPVOID) original function address, default NULL
	*/
	HOOKIAT(LPVOID myF=NULL, LPCWSTR orgName=L"", LPVOID orgF=NULL);

	/*
		hook(myF, orgName, orgF) => bool, whether succeeds
		myF (LPVOID) my hook function address, default NULL
		orgName (LPCWSTR) original function name, default L""
		orgF (LPVOID) original function address, default NULL
	*/
	bool hook(LPVOID myF=NULL, LPCWSTR orgName=L"", LPVOID orgF=NULL);

	/*
		unhook(orgName) => bool, whether succeeds
		orgName (LPCWSTR) original function name, default L""
	*/
	bool unhook(LPCWSTR orgName=L"");
	
	/*
		get() => LPVOID, original function address
	*/
	LPVOID get();
};