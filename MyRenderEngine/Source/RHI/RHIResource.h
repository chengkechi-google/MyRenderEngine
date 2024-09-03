#pragma once
#include "RHIDefines.h"

class IRHIDevice;

class IRHIResource
{
public:
    virtual ~IRHIResource(){};
    
    virtual void* GetHandle() const = 0;
    virtual bool IsTexture() const { return false; }
    virtual bool IsBuffer() { return false; }

    IRHIDevice* GetDevice() { return m_pDevice; }
    const eastl::string& GetName() { return m_name; }
    
protected:
    IRHIDevice* m_pDevice;
    eastl::string m_name;   
};