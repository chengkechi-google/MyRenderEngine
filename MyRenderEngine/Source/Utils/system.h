#pragma once
#include "string.h"

inline void SetCurrentThreadName(const eastl::string& name)
{
    SetThreadDescription(GetCurrentThread(), string_to_wstring(name).c_str());
}