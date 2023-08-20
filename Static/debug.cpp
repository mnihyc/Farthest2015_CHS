#include "debug.h"

DEBUG::DEBUG(std::wstring title, std::wstring filename, bool append, bool nopopup)
{
	this->SetTitle(title);
	this->SetFilename(filename);
	this->SetAppend(append);
	this->SetNoPopup(nopopup);
}

void DEBUG::SetTitle(std::wstring title)
{
	this->title = title;
}

void DEBUG::SetFilename(std::wstring filename)
{
	this->filename = filename;
}

bool DEBUG::ClearFile()
{
	if (this->filename.empty())
		return false;
	bool suc = true;
	std::wofstream file;
	file.open(this->filename, std::ios::out | std::ios::trunc);
	if (file.fail())
		suc = false;
	file.close();
	return suc;
}

void DEBUG::SetAppend(bool append)
{
	this->append = append;
	if(!append)
		this->ClearFile();
}

void DEBUG::SetNoPopup(bool nopopup)
{
	this->nopopup = nopopup;
}

void DEBUG::Log(std::wstring msg)
{
	this->LogToFile(msg);
	if(!this->nopopup)
		this->LogPopup(msg);
}

void DEBUG::LogToFile(std::wstring msg)
{
	if (this->filename.empty())
		return;
	std::wstringstream ss;
	ss << L"[" << this->title << L"] " << msg << std::endl;
	std::wofstream file(this->filename, std::ios::app);
	if (file.fail())
		return;
	file << ss.str();
	file.close();
}

void DEBUG::LogPopup(std::wstring msg)
{
	std::wstringstream ss;
	ss << L"[" << this->title << L"] " << msg << std::endl;
	MessageBox(NULL, ss.str().c_str() ,L"DEBUG", MB_OK);
}

void DEBUG::WarnPopup(std::wstring msg)
{
	std::wstringstream ss;
	ss << L"[" << this->title << L"] " << msg << std::endl;
	MessageBox(NULL, ss.str().c_str(), L"WARNING", MB_OK | MB_ICONWARNING);
}

void DEBUG::FatalPopup(std::wstring msg, bool exit)
{
	std::wstringstream ss;
	ss << L"[" << this->title << L"] " << msg << std::endl;
	MessageBox(NULL, ss.str().c_str(), L"FATAL", MB_OK | MB_ICONERROR);
	if (exit)
		ExitProcess(1);
}
