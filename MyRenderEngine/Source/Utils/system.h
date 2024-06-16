#pragma once
#include "string.h"
#include <Windows.h>

inline int ExecuteCommand(const char* cmd)
{
    // Only work for windows
    return system(cmd);
}
inline void SetCurrentThreadName(const eastl::string& name)
{
    SetThreadDescription(GetCurrentThread(), string_to_wstring(name).c_str());
}