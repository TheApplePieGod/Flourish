#pragma once

#include "Flourish/Api/Buffer.h"

namespace Flourish
{
    enum class ColorFormat
    {
        None = 0,
        RGBA8,
        R16F, RGBA16F,
        R32F, RGBA32F
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

    struct TextureSamplerState
    {
        SamplerFilter MinFilter = SamplerFilter::Linear;
        SamplerFilter MagFilter = SamplerFilter::Linear;
        std::array<SamplerWrapMode, 3> UVWWrap = { SamplerWrapMode::Repeat, SamplerWrapMode::Repeat, SamplerWrapMode::Repeat };
        SamplerReductionMode ReductionMode = SamplerReductionMode::WeightedAverage;
        bool AnisotropyEnable = true;
        u32 MaxAnisotropy = 8;
    };

    struct TextureCreateInfo
    {
        u32 Width, Height, Channels;
        BufferDataType DataType;
        BufferUsageType UsageType;
        u32 ArrayCount = 1;
        u32 MipCount = 0; // Set to zero to automatically deduce mip count
        TextureSamplerState SamplerState;
        void* InitialData = nullptr;
        u32 InitialDataSize = 0;
    };

    class Texture
    {
    public:
        Texture(const TextureCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~Texture() = default;

        // TS  
        inline u32 GetArrayCount() const { return m_Info.ArrayCount; }
        inline u32 GetWidth() const { return m_Info.Width; }
        inline u32 GetHeight() const { return m_Info.Height; }
        inline u32 GetMipCount() const { return m_Info.MipCount; }
        inline u32 GetMipWidth(u32 mipLevel) const { return std::max(static_cast<u32>(m_Info.Width * pow(0.5f, mipLevel)), 0U); }
        inline u32 GetMipHeight(u32 mipLevel) const { return std::max(static_cast<u32>(m_Info.Height * pow(0.5f, mipLevel)), 0U); }
        inline u32 GetChannels() const { return m_Info.Channels; }
        inline const TextureSamplerState& GetSamplerState() const { return m_Info.SamplerState; }

    public:
        // TS
        static std::shared_ptr<Texture> Create(const TextureCreateInfo& createInfo);
        static u32 ColorFormatComponentCount(ColorFormat format);
        static BufferDataType ColorFormatBufferDataType(ColorFormat format);
        static ColorFormat BufferDataTypeColorFormat(BufferDataType type, u32 channelCount);

    protected:
        TextureCreateInfo m_Info;
        u32 m_MipLevels;
    };
}