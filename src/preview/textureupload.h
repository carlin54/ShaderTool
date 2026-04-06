#ifndef TEXTUREUPLOAD_H
#define TEXTUREUPLOAD_H

#include <QImage>
#include <QVulkanWindow>
#include <QVulkanDeviceFunctions>

#include <vulkan/vulkan.h>

namespace TextureUpload {

struct GpuTexture2D {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

void destroy(QVulkanDeviceFunctions *df, VkDevice dev, GpuTexture2D &t);

// Upload RGBA8 image (converted internally). Records one-time command buffer on graphics queue.
VkResult createFromImage(QVulkanWindow *window, QVulkanDeviceFunctions *df, const QImage &img,
                         GpuTexture2D *out);

} // namespace TextureUpload

#endif
