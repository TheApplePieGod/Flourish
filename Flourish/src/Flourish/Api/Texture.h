#pragma once

#include "Flourish/Api/Buffer.h"

namespace Flourish
{
    enum class ColorFormat
    {
        None = 0,
        RGBA8_UNORM, RGBA8_SRGB,
        BGRA8_UNORM, 
        RGB8_UNORM,
        BGR8_UNORM,
        R16_FLOAT, RGBA16_FLOAT,
        R32_FLOAT, RGBA32_FLOAT,
        Depth
    };
    
    enum class TextureUsageType
    {
        None = 0,
        Readonly,
        RenderTarget,
        ComputeTarget
    };
    
    enum class TextureWritability
    {
        None = 0,
        Once,
        PerFrame
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
        bool AnisotropyEnable = false;
        u32 MaxAnisotropy = 8;
    };
    
    struct TextureCreateInfo
    {
        u32 Width, Height;
        ColorFormat Format;
        TextureUsageType Usage = TextureUsageType::Readonly;
        TextureWritability Writability = TextureWritability::None;
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
        
        // TS
        virtual bool IsReady() const = 0;
        #ifdef FL_USE_IMGUI
        virtual void* GetImGuiHandle(u32 layerIndex = 0, u32 mipLevel = 0) const = 0;
        #endif

        // TS  
        inline u32 GetArrayCount() const { return m_Info.ArrayCount; }
        inline u32 GetWidth() const { return m_Info.Width; }
        inline u32 GetHeight() const { return m_Info.Height; }
        inline u32 GetMipCount() const { return m_MipLevels; }
        inline u32 GetMipWidth(u32 mipLevel) const { return std::max(static_cast<u32>(m_Info.Width * pow(0.5f, mipLevel)), 0U); }
        inline u32 GetMipHeight(u32 mipLevel) const { return std::max(static_cast<u32>(m_Info.Height * pow(0.5f, mipLevel)), 0U); }
        inline u32 GetChannels() const { return m_Channels; }
        inline TextureUsageType GetUsageType() const { return m_Info.Usage; }
        inline TextureWritability GetWritability() const { return m_Info.Writability; }
        inline const TextureSamplerState& GetSamplerState() const { return m_Info.SamplerState; }
        inline ColorFormat GetColorFormat() const { return m_Info.Format; }

    public:
        // TS
        static std::shared_ptr<Texture> Create(const TextureCreateInfo& createInfo);
        static u32 ColorFormatComponentCount(ColorFormat format);
        static BufferDataType ColorFormatBufferDataType(ColorFormat format);

    protected:
        TextureCreateInfo m_Info;
        u32 m_MipLevels;
        u32 m_Channels;
    };
}