#include "flpch.h"
#include "Texture.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/Texture.h"

namespace Flourish
{
    Texture::Texture(const TextureCreateInfo& createInfo)
        : m_Info(createInfo)
    {
        m_Channels = ColorFormatComponentCount(createInfo.Format);
        m_Id = Context::GetNextId();
    }

    std::shared_ptr<Texture> Texture::Create(const TextureCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a Texture");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::Texture>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create texture");
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
            case ColorFormat::Depth: return 4;
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
            case ColorFormat::Depth: return BufferDataType::Float;
        }

        FL_ASSERT(false, "ColorFormatBufferDataType unsupported ColorFormat");
        return BufferDataType::None;
    }
}
