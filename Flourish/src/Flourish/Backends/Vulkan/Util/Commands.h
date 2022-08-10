#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class Commands
    {
    public:
        void Initialize();
        void Shutdown();

        // TS
        inline VkCommandPool GraphicsPool() const { return m_GraphicsPool; }
        inline VkCommandPool ComputePool() const { return m_ComputePool; }
        inline VkCommandPool TransferPool() const { return m_TransferPool; }

    private:
        VkCommandPool m_GraphicsPool, m_ComputePool, m_TransferPool;
    };
}