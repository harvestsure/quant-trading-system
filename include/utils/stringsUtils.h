#pragma once
#include <string>
#include <vector>
#include <stdexcept>


std::string UTF8ToLocal(const std::string &strUTF8);

void PrintToConsole(const std::string& message);

std::string getExecutablePath();