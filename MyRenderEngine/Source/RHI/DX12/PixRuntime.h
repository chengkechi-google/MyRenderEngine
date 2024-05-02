#pragma once
#include "D3D12Headers.h"

namespace PIX
{
    void Init();
    void BeginEvent(ID3D12GraphicsCommandList* pCommandList, const char* event);
    void EndEvent(ID3D12GraphicsCommandList* pCommandList);
}