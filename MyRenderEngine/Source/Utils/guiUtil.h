#pragma once

#include "Core/Engine.h"
#include "imgui/imgui.h"

inline void GUI(const eastl::string& window, const eastl::string& section, const eastl::function<void()>& command)
{
    Editor* pEditor = Engine::GetInstance()->GetEditor();
    pEditor->AddGUICommand(window, section, command);
}