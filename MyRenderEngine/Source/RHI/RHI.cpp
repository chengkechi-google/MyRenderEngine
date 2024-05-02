#include "RHI.h"
#include "DX12/D3D12Device.h"

IRHIDevice* CreateRHIDevice(const RHIDeviceDesc& desc)
{
    IRHIDevice *pDevice = nullptr;
    switch (desc.m_backEnd)
    {
        case RHIRenderBackEnd::D3D12:
        {
            pDevice = new D3D12Device(desc);
            if (!((D3D12Device*)pDevice)->Init())
            {
                delete pDevice;
                pDevice = nullptr;
            }
            break;
        }

        default:
            break;
    }

    return pDevice;
}

bool IsDepthFormat(RHIFormat format)
{
    return format == RHIFormat::D32FS8 || format == RHIFormat::D32F || format == RHIFormat::D16;
}

bool IsStencilFormat(RHIFormat format)
{
    return format == RHIFormat::D32FS8;
}