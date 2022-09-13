#include "flpch.h"
#include "Texture.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/Texture.h"

namespace Flourish
{
    std::shared_ptr<Texture> Texture::Create(const TextureCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a Texture");

        switch (Context::BackendType())
        {
            case BackendType::Vulkan: { return std::make_shared<Vulkan::Texture>(createInfo); }
        }

        FL_ASSERT(false, "Texture not supported for backend");
        return nullptr;
    }

    u32 Texture::ColorFormatComponentCount(ColorFormat format)
    {
        switch (format)
        {
            case ColorFormat::RGBA8: return 4;
            case ColorFormat::R16F: return 1;
            case ColorFormat::RGBA16F: return 4;
            case ColorFormat::R32F: return 1;
            case ColorFormat::RGBA32F: return 4;
        }

        FL_ASSERT(false, "ColorFormatComponentCount unsupported ColorFormat");
        return 0;
    }
    
    BufferDataType Texture::ColorFormatBufferDataType(ColorFormat format)
    {
        switch (format)
        {
            case ColorFormat::RGBA8: return BufferDataType::UInt8;
            case ColorFormat::R16F: return BufferDataType::HalfFloat;
            case ColorFormat::RGBA16F: return BufferDataType::HalfFloat;
            case ColorFormat::R32F: return BufferDataType::Float;
            case ColorFormat::RGBA32F: return BufferDataType::Float;
        }

        FL_ASSERT(false, "ColorFormatBufferDataType unsupported ColorFormat");
        return BufferDataType::None;
    }
    
    ColorFormat Texture::BufferDataTypeColorFormat(BufferDataType type, u32 channelCount)
    {
        FL_ASSERT(channelCount == 1 || channelCount == 4, "Channel counts of 2 or 3 are not supported");
        if (channelCount == 1)
        {
            switch (type)
            {
                // case BufferDataType::UInt8: return ColorFormat::R8; // Unsupported
                case BufferDataType::Float: return ColorFormat::R32F;
                case BufferDataType::HalfFloat: return ColorFormat::R16F;
            }
        }
        else
        {
            switch (type)
            {
                case BufferDataType::UInt8: return ColorFormat::RGBA8;
                case BufferDataType::Float: return ColorFormat::RGBA32F;
                case BufferDataType::HalfFloat: return ColorFormat::RGBA16F;
            }
        }

        FL_ASSERT(false, "BufferDataTypeColorFormat unsupported BufferDataType and ChannelCount combination");
        return ColorFormat::None;
    }
}