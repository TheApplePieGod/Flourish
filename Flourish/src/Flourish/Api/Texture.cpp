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

    u32 Texture::ComputeTextureSize(ColorFormat format, u32 width, u32 height)
    {
        if (!IsColorFormatCompressed(format))
        {
            u32 components = ColorFormatComponentCount(format);
            u32 size = BufferDataTypeSize(ColorFormatBufferDataType(format));
            return components * size * width * height;
        }

        // Compressed formats
        // https://github.com/GPUOpen-Tools/compressonator/blob/master/applications/_plugins/common/codec_common.cpp

        // Align width and height 
        width = ((width + 3) / 4) * 4;
        height = ((height + 3) / 4) * 4;

        switch (format)
        {
            default: break;
            case ColorFormat::BC1:
            case ColorFormat::BC4:
            case ColorFormat::BC4_SIGNED:
            {
                u32 channels = 1;
                u32 bitsPerChannel = 4;
                return (width * height * channels * bitsPerChannel) / 8;
            }
            case ColorFormat::BC2:
            case ColorFormat::BC3:
            case ColorFormat::BC5:
            case ColorFormat::BC5_SIGNED:
            {
                u32 channels = 2;
                u32 bitsPerChannel = 4;
                return (width * height * channels * bitsPerChannel) / 8;
            }
            case ColorFormat::BC6H:
            case ColorFormat::BC6H_SIGNED:
            case ColorFormat::BC7:
            {
                // 16 bytes min size
                return std::max(width * height, 16U);
            }
        }

        FL_ASSERT(false, "ComputeTextureSize unsupported ColorFormat");
        return 0;
    }

    u32 Texture::ColorFormatComponentCount(ColorFormat format)
    {
        switch (format)
        {
            case ColorFormat::None: return 0;

            case ColorFormat::R8_UNORM: return 1;
            case ColorFormat::RG8_UNORM: return 2;
            case ColorFormat::RGBA8_UNORM: return 4;
            case ColorFormat::RGBA8_SRGB: return 4;
            case ColorFormat::BGRA8_UNORM: return 4;
            case ColorFormat::BGRA8_SRGB: return 4;
            case ColorFormat::R16_FLOAT: return 1;
            case ColorFormat::RG16_FLOAT: return 2;
            case ColorFormat::RGBA16_FLOAT: return 4;
            case ColorFormat::R32_FLOAT: return 1;
            case ColorFormat::RG32_FLOAT: return 2;
            case ColorFormat::RGBA32_FLOAT: return 4;
            case ColorFormat::Depth: return 4;

            case ColorFormat::BC1: return 4;
            case ColorFormat::BC2: return 4;
            case ColorFormat::BC3: return 4;
            case ColorFormat::BC4: return 1;
            case ColorFormat::BC4_SIGNED: return 1;
            case ColorFormat::BC5: return 2;
            case ColorFormat::BC5_SIGNED: return 2;
            case ColorFormat::BC6H: return 3;
            case ColorFormat::BC6H_SIGNED: return 3;
            case ColorFormat::BC7: return 4;
        }
    }
    
    BufferDataType Texture::ColorFormatBufferDataType(ColorFormat format)
    {
        switch (format)
        {
            case ColorFormat::None: return BufferDataType::None;

            case ColorFormat::R8_UNORM: return BufferDataType::UInt8;
            case ColorFormat::RG8_UNORM: return BufferDataType::UInt8;
            case ColorFormat::RGBA8_UNORM: return BufferDataType::UInt8;
            case ColorFormat::RGBA8_SRGB: return BufferDataType::UInt8;
            case ColorFormat::BGRA8_UNORM: return BufferDataType::UInt8;
            case ColorFormat::BGRA8_SRGB: return BufferDataType::UInt8;
            case ColorFormat::R16_FLOAT: return BufferDataType::HalfFloat;
            case ColorFormat::RG16_FLOAT: return BufferDataType::HalfFloat;
            case ColorFormat::RGBA16_FLOAT: return BufferDataType::HalfFloat;
            case ColorFormat::R32_FLOAT: return BufferDataType::Float;
            case ColorFormat::RG32_FLOAT: return BufferDataType::Float;
            case ColorFormat::RGBA32_FLOAT: return BufferDataType::Float;
            case ColorFormat::Depth: return BufferDataType::Float;

            case ColorFormat::BC1: return BufferDataType::UInt8;
            case ColorFormat::BC2: return BufferDataType::UInt8;
            case ColorFormat::BC3: return BufferDataType::UInt8;
            case ColorFormat::BC4: return BufferDataType::UInt8;
            case ColorFormat::BC4_SIGNED: return BufferDataType::UInt8;
            case ColorFormat::BC5: return BufferDataType::UInt8;
            case ColorFormat::BC5_SIGNED: return BufferDataType::UInt8;
            case ColorFormat::BC6H: return BufferDataType::HalfFloat;
            case ColorFormat::BC6H_SIGNED: return BufferDataType::HalfFloat;
            case ColorFormat::BC7: return BufferDataType::UInt8;
        }
    }

    bool Texture::IsColorFormatCompressed(ColorFormat format)
    {
        switch (format)
        {
            default: return false;
            case ColorFormat::BC1:
            case ColorFormat::BC2:
            case ColorFormat::BC3:
            case ColorFormat::BC4:
            case ColorFormat::BC4_SIGNED:
            case ColorFormat::BC5:
            case ColorFormat::BC5_SIGNED:
            case ColorFormat::BC6H:
            case ColorFormat::BC6H_SIGNED:
            case ColorFormat::BC7:
                return true;
        }
    }
}
