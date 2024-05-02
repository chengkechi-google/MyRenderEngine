#pragma once

#include "D3D12Headers.h"

// AMD GPU Services
namespace ags
{
    HRESULT CreateDevice(IDXGIAdapter* pAdapter, D3D_FEATURE_LEVEL minimumFeatureLevel, REFIID riid, void **ppDevice);
    void ReleaseDevice(ID3D12Device *pDevice);
    
    void BeginEvent(ID3D12GraphicsCommandList* pCommandList, const char* event);
    void EndEvent(ID3D12GraphicsCommandList* pCommandList);
}