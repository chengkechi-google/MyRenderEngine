#pragma once

#include <stdint.h>

struct RGHandle
{
    uint16_t m_index = uint16_t(-1);
    uint16_t m_node = uint16_t(-1);

    bool IsValid() const
    {
        return m_index != uint16_t(-1) && m_node != uint16_t(-1);
    }
};