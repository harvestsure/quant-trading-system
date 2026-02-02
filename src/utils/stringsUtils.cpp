#include "utils/stringsUtils.h"
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(_WIN32)
std::string UTF8ToGBK(const std::string &strUTF8)
{
	int nLen = MultiByteToWideChar(CP_UTF8, 0, strUTF8.c_str(), -1, NULL, 0);
	wchar_t* wszGBK = new wchar_t[nLen + 1];
	memset(wszGBK, 0, nLen * 2 + 2);
	MultiByteToWideChar(CP_UTF8, 0, strUTF8.c_str(), -1, wszGBK, nLen);
	nLen = WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, NULL, 0, NULL, NULL);
	char* szGBK = new char[nLen + 1];
	memset(szGBK, 0, nLen + 1);
	WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, szGBK, nLen, NULL, NULL);
	std::string strGBK(szGBK);
	if (wszGBK) delete[] wszGBK;
	if (szGBK) delete[] szGBK;
	return strGBK;
}
#endif

std::string UTF8ToLocal(const std::string &strUTF8)
{
#if defined(_WIN32)
	int nCodePage = GetConsoleOutputCP();
	if (nCodePage == 936)
	{
		return UTF8ToGBK(strUTF8);
	}
	else
	{
		return strUTF8;
	}
#else
	return strUTF8;
#endif
}

void PrintToConsole(const std::string& message)
{
	std::string log_entry = UTF8ToLocal(message);
	std::cout << log_entry << std::endl;

#if defined(_WIN32)
	log_entry += "\n";
	OutputDebugStringA(log_entry.c_str());
#endif
}
