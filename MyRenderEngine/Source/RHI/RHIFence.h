#pragma once
#include "RHIResource.h"

class IRHIFence : public IRHIResource
{
public:
    virtual ~IRHIFence() {}
    
    virtual void Wait(uint64_t) = 0;
    virtual void Signal(uint64_t) = 0;
};