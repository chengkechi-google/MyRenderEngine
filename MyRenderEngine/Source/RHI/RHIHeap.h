#pragma once
#include "RHIResource.h"

class IRHIHeap : public IRHIResource
{
public:
    const RHIHeapDesc& GetDesc() const { return m_desc; }

protected:
    RHIHeapDesc m_desc = {};
};