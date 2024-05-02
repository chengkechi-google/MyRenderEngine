#pragma once
#include "RHIResource.h"

class IRHIPipelineState : public IRHIResource
{
public:
    RHIPipelineType GetType() const { return m_type; }
    virtual bool Create() = 0;

protected:
    RHIPipelineType m_type;
};