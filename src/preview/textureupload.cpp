#include "textureupload.h"

#include <QDebug>

#include <cstring>

void TextureUpload::destroy(QVulkanDeviceFunctions *df, VkDevice dev, GpuTexture2D &t)
{
    if (!df || dev == VK_NULL_HANDLE)
        return;
    if (t.view) {
        df->vkDestroyImageView(dev, t.view, nullptr);
        t.view = VK_NULL_HANDLE;
    }
    if (t.image) {
        df->vkDestroyImage(dev, t.image, nullptr);
        t.image = VK_NULL_HANDLE;
    }
    if (t.memory) {
        df->vkFreeMemory(dev, t.memory, nullptr);
        t.memory = VK_NULL_HANDLE;
    }
}

VkResult TextureUpload::createFromImage(QVulkanWindow *window, QVulkanDeviceFunctions *df,
                                      const QImage &img, GpuTexture2D *out)
{
    if (!window || !df || !out || img.isNull())
        return VK_ERROR_INITIALIZATION_FAILED;

    const QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
    const uint32_t w = uint32_t(rgba.width());
    const uint32_t h = uint32_t(rgba.height());
    if (w == 0 || h == 0)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkDevice dev = window->device();
    const VkDeviceSize stagingSize = VkDeviceSize(rgba.sizeInBytes());

    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    VkBufferCreateInfo sb{};
    sb.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    sb.size = stagingSize;
    sb.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkResult err = df->vkCreateBuffer(dev, &sb, nullptr, &stagingBuf);
    if (err != VK_SUCCESS)
        return err;

    VkMemoryRequirements memReq{};
    df->vkGetBufferMemoryRequirements(dev, stagingBuf, &memReq);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = window->hostVisibleMemoryIndex();
    err = df->vkAllocateMemory(dev, &alloc, nullptr, &stagingMem);
    if (err != VK_SUCCESS) {
        df->vkDestroyBuffer(dev, stagingBuf, nullptr);
        return err;
    }
    err = df->vkBindBufferMemory(dev, stagingBuf, stagingMem, 0);
    if (err != VK_SUCCESS) {
        df->vkFreeMemory(dev, stagingMem, nullptr);
        df->vkDestroyBuffer(dev, stagingBuf, nullptr);
        return err;
    }

    void *mapped = nullptr;
    err = df->vkMapMemory(dev, stagingMem, 0, memReq.size, 0, &mapped);
    if (err != VK_SUCCESS) {
        df->vkFreeMemory(dev, stagingMem, nullptr);
        df->vkDestroyBuffer(dev, stagingBuf, nullptr);
        return err;
    }
    std::memcpy(mapped, rgba.constBits(), size_t(stagingSize));
    df->vkUnmapMemory(dev, stagingMem);

    VkImageCreateInfo im{};
    im.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    im.imageType = VK_IMAGE_TYPE_2D;
    im.format = VK_FORMAT_R8G8B8A8_UNORM;
    im.extent.width = w;
    im.extent.height = h;
    im.extent.depth = 1;
    im.mipLevels = 1;
    im.arrayLayers = 1;
    im.samples = VK_SAMPLE_COUNT_1_BIT;
    im.tiling = VK_IMAGE_TILING_OPTIMAL;
    im.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    im.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    err = df->vkCreateImage(dev, &im, nullptr, &out->image);
    if (err != VK_SUCCESS) {
        df->vkFreeMemory(dev, stagingMem, nullptr);
        df->vkDestroyBuffer(dev, stagingBuf, nullptr);
        return err;
    }

    VkMemoryRequirements imReq{};
    df->vkGetImageMemoryRequirements(dev, out->image, &imReq);

    VkMemoryAllocateInfo imAlloc{};
    imAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imAlloc.allocationSize = imReq.size;
    imAlloc.memoryTypeIndex = window->deviceLocalMemoryIndex();
    err = df->vkAllocateMemory(dev, &imAlloc, nullptr, &out->memory);
    if (err != VK_SUCCESS) {
        df->vkDestroyImage(dev, out->image, nullptr);
        out->image = VK_NULL_HANDLE;
        df->vkFreeMemory(dev, stagingMem, nullptr);
        df->vkDestroyBuffer(dev, stagingBuf, nullptr);
        return err;
    }
    err = df->vkBindImageMemory(dev, out->image, out->memory, 0);
    if (err != VK_SUCCESS) {
        df->vkFreeMemory(dev, out->memory, nullptr);
        out->memory = VK_NULL_HANDLE;
        df->vkDestroyImage(dev, out->image, nullptr);
        out->image = VK_NULL_HANDLE;
        df->vkFreeMemory(dev, stagingMem, nullptr);
        df->vkDestroyBuffer(dev, stagingBuf, nullptr);
        return err;
    }

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = window->graphicsCommandPool();
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    err = df->vkAllocateCommandBuffers(dev, &cmdAlloc, &cb);
    if (err != VK_SUCCESS) {
        destroy(df, dev, *out);
        df->vkFreeMemory(dev, stagingMem, nullptr);
        df->vkDestroyBuffer(dev, stagingBuf, nullptr);
        return err;
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    df->vkBeginCommandBuffer(cb, &begin);

    VkImageMemoryBarrier undefToDst{};
    undefToDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    undefToDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    undefToDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    undefToDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    undefToDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    undefToDst.image = out->image;
    undefToDst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    undefToDst.subresourceRange.levelCount = 1;
    undefToDst.subresourceRange.layerCount = 1;
    undefToDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    df->vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &undefToDst);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = w;
    region.imageExtent.height = h;
    region.imageExtent.depth = 1;
    df->vkCmdCopyBufferToImage(cb, stagingBuf, out->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);

    VkImageMemoryBarrier dstToSample{};
    dstToSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    dstToSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstToSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dstToSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dstToSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dstToSample.image = out->image;
    dstToSample.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    dstToSample.subresourceRange.levelCount = 1;
    dstToSample.subresourceRange.layerCount = 1;
    dstToSample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dstToSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    df->vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &dstToSample);

    df->vkEndCommandBuffer(cb);

    VkSubmitInfo sub{};
    sub.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sub.commandBufferCount = 1;
    sub.pCommandBuffers = &cb;
    err = df->vkQueueSubmit(window->graphicsQueue(), 1, &sub, VK_NULL_HANDLE);
    if (err != VK_SUCCESS) {
        df->vkFreeCommandBuffers(dev, window->graphicsCommandPool(), 1, &cb);
        destroy(df, dev, *out);
        df->vkFreeMemory(dev, stagingMem, nullptr);
        df->vkDestroyBuffer(dev, stagingBuf, nullptr);
        return err;
    }
    df->vkQueueWaitIdle(window->graphicsQueue());
    df->vkFreeCommandBuffers(dev, window->graphicsCommandPool(), 1, &cb);

    df->vkFreeMemory(dev, stagingMem, nullptr);
    df->vkDestroyBuffer(dev, stagingBuf, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = out->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    err = df->vkCreateImageView(dev, &viewInfo, nullptr, &out->view);
    if (err != VK_SUCCESS) {
        destroy(df, dev, *out);
        return err;
    }

    return VK_SUCCESS;
}
