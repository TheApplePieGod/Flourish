#pragma once

#include "Flourish/Api/Buffer.h"

namespace Flourish
{
    namespace TextureUsageEnum
    {
        enum Value : u8
        {
            Readonly = 0,
            Graphics = (1 << 0),
            Compute = (1 << 1),
            Transfer = (1 << 2),
            All = Graphics | Compute | Transfer
        };
    }
    typedef TextureUsageEnum::Value TextureUsageFlags;
    typedef u8 TextureUsage;
    
    enum class ColorFormat
    {
        None = 0,

        // Uncompressed formats
        R8_UNORM, RG8_UNORM, RGBA8_UNORM, RGBA8_SRGB,
        R8_SINT, RG8_SINT, RGBA8_SINT,
        R8_UINT, RG8_UINT, RGBA8_UINT,
        BGRA8_UNORM, BGRA8_SRGB,
        R16_FLOAT, RG16_FLOAT, RGBA16_FLOAT,
        R32_FLOAT, RG32_FLOAT, RGBA32_FLOAT,
        Depth,

        // Compressed formats
        BC1,
        BC2,
        BC3,
        BC4, BC4_SIGNED,
        BC5, BC5_SIGNED,
        BC6H, BC6H_SIGNED,
        BC7,
    };
    
    enum class SamplerFilter
    {
        None = 0,
        Linear, Nearest
    };

    enum class SamplerWrapMode
    {
        None = 0,
        ClampToBorder,
        ClampToEdge,
        Repeat,
        MirroredRepeat
    };

    enum class SamplerReductionMode
    {
        None = 0,
        WeightedAverage,
        Min,
        Max
    };

    enum class TextureChannelRemap
    {
        Identity = 0,
        R, G, B, A, ZERO, ONE
    };

    struct TextureSamplerState
    {
        SamplerFilter MinFilter = SamplerFilter::Linear;
        SamplerFilter MagFilter = SamplerFilter::Linear;
        std::array<SamplerWrapMode, 3> UVWWrap = { SamplerWrapMode::Repeat, SamplerWrapMode::Repeat, SamplerWrapMode::Repeat };
        SamplerReductionMode ReductionMode = SamplerReductionMode::WeightedAverage;
        bool AnisotropyEnable = false;
        u32 MaxAnisotropy = 8;
    };
    
    struct TextureCreateInfo
    {
        u32 Width, Height;
        ColorFormat Format;
        TextureUsage Usage = TextureUsageFlags::Readonly;
        u32 ArrayCount = 1;
        u32 MipCount = 0; // Set to zero to automatically deduce mip count
        TextureSamplerState SamplerState;
        void* InitialData = nullptr;
        u32 InitialDataSize = 0;
        bool AsyncCreation = false;
        std::function<void()> CreationCallback = nullptr;
    };

    class Texture
    {
    public:
        Texture(const TextureCreateInfo& createInfo);
        virtual ~Texture() = default;
        void operator=(Texture&& other);
        
        // TS
        virtual bool IsReady() const = 0;
        #ifdef FL_USE_IMGUI
        virtual void* GetImGuiHandle(u32 layerIndex = 0, u32 mipLevel = 0) const = 0;
        #endif

        // TS  
        inline u64 GetId() const { return m_Id; }
        inline u32 GetArrayCount() const { return m_Info.ArrayCount; }
        inline u32 GetWidth() const { return m_Info.Width; }
        inline u32 GetHeight() const { return m_Info.Height; }
        inline u32 GetMipCount() const { return m_MipLevels; }
        inline u32 GetMipWidth(u32 mipLevel) const { return std::max(static_cast<u32>(m_Info.Width * pow(0.5f, mipLevel)), 0U); }
        inline u32 GetMipHeight(u32 mipLevel) const { return std::max(static_cast<u32>(m_Info.Height * pow(0.5f, mipLevel)), 0U); }
        inline u32 GetChannels() const { return m_Channels; }
        inline TextureUsage GetUsageType() const { return m_Info.Usage; }
        inline const TextureSamplerState& GetSamplerState() const { return m_Info.SamplerState; }
        inline ColorFormat GetColorFormat() const { return m_Info.Format; }

    public:
        // TS
        static std::shared_ptr<Texture> Create(const TextureCreateInfo& createInfo);
        static std::shared_ptr<Texture> CreatePlaceholder(ColorFormat format);
        static void Replace(std::shared_ptr<Texture>& ptr, const TextureCreateInfo& createInfo);
        static u32 ComputeTextureSize(ColorFormat format, u32 width, u32 height);
        static u32 ColorFormatComponentCount(ColorFormat format);
        static BufferDataType ColorFormatBufferDataType(ColorFormat format);
        static bool IsColorFormatCompressed(ColorFormat format);

    protected:
        TextureCreateInfo m_Info;
        u32 m_MipLevels;
        u32 m_Channels;
        u64 m_Id;
    };
}
