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

    void Texture::operator=(Texture&& other)
    {
        m_Info = other.m_Info;
        m_MipLevels = other.m_MipLevels;
        m_Channels = other.m_Channels;
        m_Id = other.m_Id;
    }

    std::shared_ptr<Texture> Texture::Create(const TextureCreateInfo& createInfo)
    {
        std::shared_ptr<Texture> replace;
        Replace(replace, createInfo);
        return replace;
    }

    std::shared_ptr<Texture> Texture::CreatePlaceholder(ColorFormat format)
    {
        TextureCreateInfo createInfo;
        createInfo.Width = 1;
        createInfo.Height = 1;
        createInfo.Format = ColorFormat::R32_FLOAT;
        createInfo.Usage = TextureUsageFlags::Readonly;
        createInfo.Writability = TextureWritability::None;

        return Create(createInfo);
    }

    void Texture::Replace(std::shared_ptr<Texture>& ptr, const TextureCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a Texture");

        try
        {
            switch (Context::BackendType())
            {
                default: break;
                case BackendType::Vulkan:
                {
                    if (ptr.get())
                        (Vulkan::Texture&)*ptr = std::move(Vulkan::Texture(createInfo));
                    else
                        ptr = std::make_shared<Vulkan::Texture>(createInfo);
                } return;
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create texture");
    }

    u32 Texture::ColorFormatSize(ColorFormat format)
    {
        u32 components = ColorFormatComponentCount(format);
        u32 size = BufferDataTypeSize(ColorFormatBufferDataType(format));
        return components * size;
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
