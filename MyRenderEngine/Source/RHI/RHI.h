#pragma once
#include "RHIDefines.h"
#include "RHIDevice.h"
#include "RHITexture.h"
#include "RHIBuffer.h"
#include "RHICommandList.h"
#include "RHIFence.h"
#include "RHISwapChain.h"
#include "RHIDescriptor.h"
#include "RHIHeap.h"

IRHIDevice* CreateRHIDevice(const RHIDeviceDesc& desc);
bool IsDepthFormat(RHIFormat format);
bool IsStencilFormat(RHIFormat format);


