#pragma once

#include "RHIResource.h"

class IRHIRayTracingTLAS : public IRHIResource
{
public:
    const RHIRayTracingTLASDesc& GetDesc() const { return m_desc; }

protected:
    RHIRayTracingTLASDesc m_desc;
};