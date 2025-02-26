#pragma once

#include "Flourish/Core/Assert.h"

namespace Flourish
{
    /*! @brief The possible data types for elements within a buffer. */
    enum class BufferDataType
    {
        None = 0,
        Bool,
        UInt, UInt8,
        Double,
        Int, Int2, Int3, Int4,
        HalfFloat,
        Float, Float2, Float3, Float4,
        Mat3, Mat4  
    };

    /**
     * @brief Get the size of a buffer data type.
     *
     * @param type The buffer data type.
     * @return The size of the type in bytes.
     */
    static u32 BufferDataTypeSize(BufferDataType type)
    {
        switch (type)
        {
            default: break;
            case BufferDataType::Bool: return 4;
            case BufferDataType::UInt: return 4;
            case BufferDataType::UInt8: return 1;
            case BufferDataType::Double: return 8;
            case BufferDataType::Int: return 4;
            case BufferDataType::Int2: return 4 * 2;
            case BufferDataType::Int3: return 4 * 3;
            case BufferDataType::Int4: return 4 * 4;
            case BufferDataType::HalfFloat: return 2;
            case BufferDataType::Float: return 4;
            case BufferDataType::Float2: return 4 * 2;
            case BufferDataType::Float3: return 4 * 3;
            case BufferDataType::Float4: return 4 * 4;
            case BufferDataType::Mat3: return 4 * 3 * 3;
            case BufferDataType::Mat4: return 4 * 4 * 4;
        }

        FL_ASSERT(false, "BufferDataTypeSize unsupported BufferDataType");
        return 0;
    }

    /**
     * @brief Get the number of components in a buffer data type.
     *
     * For example, a Float2 represents two individual float components,
     * so this function would return 2.
     *
     * @param type The buffer data type.
     * @return The number of components.
     */
    static u32 BufferDataTypeComponents(BufferDataType type)
    {
        switch (type)
        {
            default: break;
            case BufferDataType::Bool: return 1;
            case BufferDataType::UInt: return 1;
            case BufferDataType::UInt8: return 1;
            case BufferDataType::Double: return 1;
            case BufferDataType::Int: return 4;
            case BufferDataType::Int2: return 2;
            case BufferDataType::Int3: return 3;
            case BufferDataType::Int4: return 4;
            case BufferDataType::HalfFloat: return 1;
            case BufferDataType::Float: return 1;
            case BufferDataType::Float2: return 2;
            case BufferDataType::Float3: return 3;
            case BufferDataType::Float4: return 4;
            case BufferDataType::Mat3: return 3;
            case BufferDataType::Mat4: return 4;
        }

        FL_ASSERT(false, "BufferDataTypeComponents unsupported BufferDataType");
        return 0;
    }

    /*! @brief The structure encompassing each element in a buffer layout. */
    struct BufferLayoutElement
    {
        BufferLayoutElement() = default;
        BufferLayoutElement(u32 size) // for padding fields
            : CalculatedSize(size)
        {}
        BufferLayoutElement(BufferDataType type)
            : DataType(type), CalculatedSize(BufferDataTypeSize(type))
        {}

        BufferDataType DataType;
        u32 CalculatedSize;
        u32 Offset;
    };

    class BufferLayout
    {
    public:
        BufferLayout() = default;
        BufferLayout(std::initializer_list<BufferLayoutElement> elements);

        inline u32 GetCalculatedStride() const { return m_CalculatedStride; }
        inline const std::vector<BufferLayoutElement>& GetElements() const { return m_Elements; }
    
    private:
        u32 CalculateStrideAndOffsets();

    private:
        u32 m_CalculatedStride = 0;
        std::vector<BufferLayoutElement> m_Elements;
    };

    namespace BufferUsageEnum
    {
        enum Value : u8
        {
            Generic = 0,
            Uniform = (1 << 0),
            Storage = (1 << 1),
            Vertex = (1 << 2),
            Index = (1 << 3),
            Indirect = (1 << 4),
            AccelerationStructureBuild = (1 << 5)
        };
    }
    typedef BufferUsageEnum::Value BufferUsageFlags;
    typedef u8 BufferUsage;

    enum class BufferMemoryType
    {
        GPUOnly = 0,
        CPURead,
        CPUWrite,
        CPUWriteFrame // Writing to the CPU in a per-frame context
    };

    class TransferCommandEncoder;
    struct BufferCreateInfo
    {
        BufferUsage Usage;
        BufferMemoryType MemoryType;
        BufferLayout Layout;
        u32 Stride = 0; // If zero, layout must be defined. Otherwise, the size specified in stride will be used.
        u32 ElementCount = 0;
        void* InitialData = nullptr;
        u32 InitialDataSize = 0; // Bytes
        bool ExposeGPUAddress = false;

        // Only used when populating initial data
        // TODO: this is more of a temporary solution, a better one would be to
        // always defer the initial data upload
        TransferCommandEncoder* UploadEncoder = nullptr;
    };

    class Buffer
    {
    public:
        Buffer(const BufferCreateInfo& createInfo);
        virtual ~Buffer() = default;

        // TS
        void SetElements(const void* data, u32 elementCount, u32 elementOffset);
        virtual void SetBytes(const void* data, u32 byteCount, u32 byteOffset) = 0;
        virtual void ReadBytes(void* outData, u32 byteCount, u32 byteOffset) const = 0;
        virtual void Flush(bool immediate = false) = 0;
        virtual void* GetBufferGPUAddress() const = 0;

        // TS
        inline u64 GetId() const { return m_Id; }
        inline BufferUsage GetUsage() const { return m_Info.Usage; }
        inline BufferMemoryType GetMemoryType() const { return m_Info.MemoryType; }
        inline const BufferLayout& GetLayout() const { return m_Info.Layout; }
        inline u32 GetStride() const { return m_Stride; }
        inline u32 GetAllocatedSize() const { return m_Info.ElementCount * GetStride(); }
        inline u32 GetAllocatedCount() const { return m_Info.ElementCount; }

    public:
        // TS
        static std::shared_ptr<Buffer> Create(const BufferCreateInfo& createInfo);

    protected:
        BufferCreateInfo m_Info;
        u64 m_Id;
        u32 m_Stride;
    };
}
