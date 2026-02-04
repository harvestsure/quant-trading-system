#include "utils/stringsUtils.h"
#include <iostream>

// #ifdef _WIN32
// #ifndef NOMINMAX
// #define NOMINMAX
// #endif
// #ifndef WIN32_LEAN_AND_MEAN
// #define WIN32_LEAN_AND_MEAN
// #endif
// #include <windows.h>
// #endif

#include <filesystem>
#include <cstdlib>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;


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

std::string getExecutablePath()
{
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);

    std::vector<char> buf(size);
    if (_NSGetExecutablePath(buf.data(), &size) != 0) {
        throw std::runtime_error("_NSGetExecutablePath failed");
    }

    char resolved[PATH_MAX];
    if (!realpath(buf.data(), resolved)) {
        throw std::runtime_error("realpath failed");
    }
    return resolved;

#elif defined(__linux__)
    std::vector<char> buf(4096);
    ssize_t len = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (len < 0) {
        throw std::runtime_error("readlink(/proc/self/exe) failed");
    }
    buf[len] = '\0';

    char resolved[PATH_MAX];
    if (!realpath(buf.data(), resolved)) {
        throw std::runtime_error("realpath failed");
    }
    return resolved;

#elif defined(_WIN32)
    std::vector<wchar_t> wbuf(260);
    DWORD len = 0;

    while (true) {
        len = GetModuleFileNameW(nullptr, wbuf.data(),
                                 static_cast<DWORD>(wbuf.size()));
        if (len == 0) {
            throw std::runtime_error("GetModuleFileNameW failed");
        }
        if (len < wbuf.size()) {
            break; // success
        }
        wbuf.resize(wbuf.size() * 2);
    }

    // UTF-16 → UTF-8
    int utf8Len = WideCharToMultiByte(
        CP_UTF8, 0, wbuf.data(), len,
        nullptr, 0, nullptr, nullptr);

    std::string result(utf8Len, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wbuf.data(), len,
        result.data(), utf8Len, nullptr, nullptr);

    return result;

#else
    throw std::runtime_error("getExecutablePath: unsupported platform");
#endif
}