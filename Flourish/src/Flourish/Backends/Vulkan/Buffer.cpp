#include "flpch.h"
#include "Buffer.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    Buffer::Buffer(const BufferCreateInfo& createInfo)
    {
        m_Info = createInfo;

        #if defined(FL_DEBUG) && defined(FL_LOGGING) 
        if (GetAllocatedSize() == 0)
        { FL_LOG_WARN("Creating a buffer with zero allocated size"); }
        if (m_Info.Usage == BufferUsageType::Static && m_Info.InitialData == nullptr)
        { FL_LOG_WARN("Creating a static buffer with no initial data"); }
        if (m_Info.Usage == BufferUsageType::Static && GetAllocatedSize() != m_Info.InitialDataSize)
        { FL_LOG_WARN("Creating a static buffer with initial data of a different size"); }
        #endif

        auto device = Context::Devices().Device();
        VkDeviceSize bufSize = static_cast<u64>(GetAllocatedSize());

        VkBufferCreateInfo bufCreateInfo{};
        bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCreateInfo.size = bufSize;
        VkBufferCreateInfo stagingBufCreateInfo{};
        stagingBufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufCreateInfo.size = bufSize;
        stagingBufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo allocCreateInfo{};
        VmaAllocationCreateInfo stagingAllocCreateInfo = {};
        stagingAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                       VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo allocInfo;
        VmaAllocationInfo stagingAllocInfo;
        switch (m_Info.Type)
        {
            default: { FL_CRASH_ASSERT(false, "Failed to create VulkanBuffer of unsupported type") } break;
            case BufferType::Uniform:
            {
                // Uniform bu
                bufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                        VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                                        VMA_ALLOCATION_CREATE_MAPPED_BIT;
                
                VkBuffer buffer;
                VmaAllocation alloc;
                FL_VK_ENSURE_RESULT(vmaCreateBuffer(
                    Context::Allocator(),
                    &bufCreateInfo,
                    &allocCreateInfo,
                    &buffer,
                    &alloc,
                    &allocInfo
                ));

                VkMemoryPropertyFlags memPropFlags;
                vmaGetAllocationMemoryProperties(Context::Allocator(), alloc, &memPropFlags);
                if (memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                    memcpy(allocInfo.pMappedData, m_Info.InitialData, m_Info.InitialDataSize);
                else
                {
                    VkBuffer stagingBuffer;
                    VmaAllocation stagingAlloc;
                    FL_VK_ENSURE_RESULT(vmaCreateBuffer(
                        Context::Allocator(),
                        &stagingBufCreateInfo,
                        &stagingAllocCreateInfo,
                        &stagingBuffer,
                        &stagingAlloc,
                        &stagingAllocInfo
                    ));

                    memcpy(stagingAllocInfo.pMappedData, m_Info.InitialData, m_Info.InitialDataSize);

                    //vkCmdPipelineBarrier()
                }
            } break;
        }
    }

    Buffer::~Buffer()
    {
        // SYNC QUEUE ACCESS
        Context::DeleteQueue().Push([=]()
        {

        });
    }

    void Buffer::Flush()
    {
        VkCommandBuffer buffer;
        Context::Commands().AllocateBuffers(GPUWorkloadType::Transfer, false, &buffer, 1);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(buffer, &beginInfo));

        VkBufferMemoryBarrier memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;

        // Ensure data has been copied from the host
        vkCmdPipelineBarrier(
            buffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            1, &memoryBarrier,
            0, nullptr
        );

        VkBufferCopy copy{};
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        copy.size = GetAllocatedSize();

        vkCmdCopyBuffer(buffer, src, dst, 1, &copy);

        auto thread = std::this_thread::get_id();
        Context::Queues().PushTransferCommand({ buffer, [buffer, thread](){
            Context::Commands().FreeBuffers(GPUWorkloadType::Transfer, &buffer, 1, thread);
        }});
    }
}