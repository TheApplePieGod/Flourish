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
            case ColorFormat::RGBA8_UNORM: return 4;
            case ColorFormat::RGBA8_SRGB: return 4;
            case ColorFormat::BGRA8_UNORM: return 4;
            case ColorFormat::RGB8_UNORM: return 3;
            case ColorFormat::BGR8_UNORM: return 3;
            case ColorFormat::R16_FLOAT: return 1;
            case ColorFormat::RGBA16_FLOAT: return 4;
            case ColorFormat::R32_FLOAT: return 1;
            case ColorFormat::RGBA32_FLOAT: return 4;
        }

        FL_ASSERT(false, "ColorFormatComponentCount unsupported ColorFormat");
        return 0;
    }
    
    BufferDataType Texture::ColorFormatBufferDataType(ColorFormat format)
    {
        switch (format)
        {
            case ColorFormat::RGBA8_UNORM: return BufferDataType::UInt8;
            case ColorFormat::RGBA8_SRGB: return BufferDataType::UInt8;
            case ColorFormat::BGRA8_UNORM: return BufferDataType::UInt8;
            case ColorFormat::RGB8_UNORM: return BufferDataType::UInt8;
            case ColorFormat::BGR8_UNORM: return BufferDataType::UInt8;
            case ColorFormat::R16_FLOAT: return BufferDataType::HalfFloat;
            case ColorFormat::RGBA16_FLOAT: return BufferDataType::HalfFloat;
            case ColorFormat::R32_FLOAT: return BufferDataType::Float;
            case ColorFormat::RGBA32_FLOAT: return BufferDataType::Float;
        }

        FL_ASSERT(false, "ColorFormatBufferDataType unsupported ColorFormat");
        return BufferDataType::None;
    }
    
    // TODO: move into texture scope?
    ColorFormat Texture::BufferDataTypeColorFormat(BufferDataType type, u32 channelCount)
    {
        // FL_ASSERT(channelCount == 1 || channelCount == 4, "Channel counts of 2 or 3 are not supported");
        if (channelCount == 1)
        {
            switch (type)
            {
                // case BufferDataType::UInt8: return ColorFormat::R8; // Unsupported
                case BufferDataType::Float: return ColorFormat::R32_FLOAT;  
                case BufferDataType::HalfFloat: return ColorFormat::R16_FLOAT;  
            }
        }
        else
        {
            switch (type)
            {
                case BufferDataType::UInt8: return ColorFormat::RGBA8_SRGB;
                case BufferDataType::Float: return ColorFormat::RGBA32_FLOAT;  
                case BufferDataType::HalfFloat: return ColorFormat::RGBA16_FLOAT;  
            }
        }

        FL_ASSERT(false, "BufferDataTypeColorFormat unsupported BufferDataType and ChannelCount combination");
        return ColorFormat::None;
    }
}