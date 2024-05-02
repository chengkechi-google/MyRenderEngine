#pragma once
#include "RHIResource.h"

class IRHIRayTracingBLAS : public IRHIResource
{
public: 
    const RHIRayTracingBLASDesc& GetDesc() const { return m_desc; }

protected:
    RHIRayTracingBLASDesc m_desc;   
};