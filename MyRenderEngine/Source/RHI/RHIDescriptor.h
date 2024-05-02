#pragma once

#include "RHIResource.h"

class IRHIDescriptor : public IRHIResource
{
public:
    virtual uint32_t GetHeapIndex() const = 0;
};