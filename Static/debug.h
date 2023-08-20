#pragma once
#include <windows.h>
#include <sstream>
#include <fstream>

class DEBUG
{
private:
	std::wstring title, filename;
	bool append, nopopup;
public:
	/*
		DEBUG(title, filename, append)
		title (wstring) title when showing debug messages, default ""
		filename (wstring) filename to write debug messages, default ""
		append (bool) whether not to clear the file, default false
		nopopup (bool) whether not to pop up, default false
	*/
	DEBUG(std::wstring title=L"", std::wstring filename=L"",
		bool append=false, bool nopopup=false);
	
	/*
		SetTitle(title) => void
		title (wstring) title when showing debug messages
	*/
	void SetTitle(std::wstring title);
	
	/*
		SetFilename(filename) => void
		filename (wstring) filename to write debug messages
	*/
	void SetFilename(std::wstring filename);
	
	/*
		ClearFile() => bool, whether completed successfully
		clear the log file
	*/
	bool ClearFile();
	
	/*
		SetAppend(append) => void
		append (bool) whether not to clear the file
	*/
	void SetAppend(bool append);
	
	/*
		SetNoPopup(nopopup) => void
		nopopup (bool) whether not to pop up
	*/
	void SetNoPopup(bool nopopup);
	
	/*
		Log(msg) => void
		msg (wstring) message to log
	*/
	void Log(std::wstring msg);
	
	/*
		LogToFile(msg) => void
		msg (wstring) message to write to file
	*/
	void LogToFile(std::wstring msg);
	
	/*
		LogPopup(msg) => void
		msg (wstring) message to pop up
	*/
	void LogPopup(std::wstring msg);
	
	/*
		WarnPopup(msg) => void
		msg (wstring) message to pop up
	*/
	void WarnPopup(std::wstring msg);
	
	/*
		FatalPopup(msg) => void
		msg (wstring) message to pop up
		exit (bool) whether exit immediately, default true
	*/
	void FatalPopup(std::wstring msg, bool exit=true);
	
};