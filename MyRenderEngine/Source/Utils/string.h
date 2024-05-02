#pragma once
#include <windows.h>
#include "EASTL/string.h"

inline eastl::wstring string_to_wstring(const eastl::string& s)
{
    DWORD size = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);
    eastl::wstring result;
    result.resize(size);
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, (LPWSTR)result.c_str(), size);
    return result;
}

inline eastl::string wstring_to_string(const eastl::wstring& s)
{
    DWORD size = WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, NULL, 0, NULL, FALSE);
    eastl::string result;
    result.resize(size);
    WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, (LPSTR)result.c_str(), size, NULL, FALSE);
    return result;
}