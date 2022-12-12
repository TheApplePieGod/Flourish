#pragma once

#include "Flourish/Api/Framebuffer.h"

namespace Flourish
{
    class CommandEncoder  
    {
    public:
        CommandEncoder() = default;

        virtual void EndEncoding() = 0;

        // TS
        inline bool IsEncoding() const { return m_Encoding; }
        
    protected:
        bool m_Encoding = false;
    };
}