#include "scene_renderer.h"

#include <QFile>
#include <QImage>
#include <QMatrix4x4>
#include <QVector4D>
#include <QVulkanFunctions>
#include <cstring>

#include "scene.h"
#include "vulkan_viewport.h"

namespace {
constexpr int kFrames = QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT;
constexpr VkDeviceSize kUboSize = sizeof(float) * 16; // one mat4
constexpr VkDeviceSize kMeshPushConstantSize = sizeof(float) * 16 + sizeof(float) * 4; // mat4 + vec4
constexpr VkDeviceSize kSkyboxPushConstantSize = sizeof(float) * 16; // one mat4
}

SceneRenderer::SceneRenderer(VulkanViewport *window)
    : m_window(window) {
}

VkShaderModule SceneRenderer::createShaderModule(const QString &resourcePath) const {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("Failed to open shader resource: %s", qPrintable(resourcePath));
    }
    const QByteArray blob = file.readAll();

    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = static_cast<size_t>(blob.size());
    shaderInfo.pCode = reinterpret_cast<const uint32_t *>(blob.constData());

    VkShaderModule module = VK_NULL_HANDLE;
    if (m_devFuncs->vkCreateShaderModule(m_window->device(), &shaderInfo, nullptr, &module) != VK_SUCCESS) {
        qFatal("Failed to create shader module from: %s", qPrintable(resourcePath));
    }
    return module;
}

uint32_t SceneRenderer::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const {
    // Physical-device-level query -- goes through the instance's function
    // table (vkGetPhysicalDeviceMemoryProperties takes a VkPhysicalDevice,
    // not a VkDevice), unlike every other Vulkan call in this file, which
    // goes through m_devFuncs.
    VkPhysicalDeviceMemoryProperties memProps;
    m_window->vulkanInstance()->functions()->vkGetPhysicalDeviceMemoryProperties(
        m_window->physicalDevice(), &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    qFatal("Failed to find a suitable memory type");
    return 0;
}

void SceneRenderer::createSkyboxTexture() {
    const VkDevice dev = m_window->device();

    QImage image(QStringLiteral(":/skybox/skybox-day.png"));
    if (image.isNull()) {
        qFatal("Failed to load skybox resource");
    }
    image = image.convertToFormat(QImage::Format_RGBA8888);
    const uint32_t width = static_cast<uint32_t>(image.width());
    const uint32_t height = static_cast<uint32_t>(image.height());
    // Format_RGBA8888 is 4 bytes/pixel, so QImage's 4-byte scanline alignment
    // never introduces row padding here -- a single flat memcpy is safe.
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = imageSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (m_devFuncs->vkCreateBuffer(dev, &stagingInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        qFatal("Failed to create the skybox staging buffer");
    }

    VkMemoryRequirements stagingMemReq;
    m_devFuncs->vkGetBufferMemoryRequirements(dev, stagingBuffer, &stagingMemReq);

    VkMemoryAllocateInfo stagingAllocInfo{};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemReq.size;
    stagingAllocInfo.memoryTypeIndex = m_window->hostVisibleMemoryIndex();
    if (m_devFuncs->vkAllocateMemory(dev, &stagingAllocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        qFatal("Failed to allocate the skybox staging buffer memory");
    }
    m_devFuncs->vkBindBufferMemory(dev, stagingBuffer, stagingMemory, 0);

    void *mapped = nullptr;
    m_devFuncs->vkMapMemory(dev, stagingMemory, 0, imageSize, 0, &mapped);
    std::memcpy(mapped, image.constBits(), static_cast<size_t>(imageSize));
    m_devFuncs->vkUnmapMemory(dev, stagingMemory);

    // Device-local, optimal tiling -- not VK_IMAGE_TILING_LINEAR, which would
    // skip the staging buffer entirely but isn't guaranteed supported for a
    // sampled image at this resolution on every GPU/driver. This project's
    // whole premise is running on arbitrary Windows machines, so the robust
    // path is used even though it's more code.
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    if (m_devFuncs->vkCreateImage(dev, &imageInfo, nullptr, &m_skyboxImage) != VK_SUCCESS) {
        qFatal("Failed to create the skybox image");
    }

    VkMemoryRequirements imageMemReq;
    m_devFuncs->vkGetImageMemoryRequirements(dev, m_skyboxImage, &imageMemReq);

    VkMemoryAllocateInfo imageAllocInfo{};
    imageAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageAllocInfo.allocationSize = imageMemReq.size;
    imageAllocInfo.memoryTypeIndex = findMemoryType(imageMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (m_devFuncs->vkAllocateMemory(dev, &imageAllocInfo, nullptr, &m_skyboxImageMemory) != VK_SUCCESS) {
        qFatal("Failed to allocate the skybox image memory");
    }
    m_devFuncs->vkBindImageMemory(dev, m_skyboxImage, m_skyboxImageMemory, 0);

    // One-time command buffer for the layout transitions + buffer->image
    // copy. graphicsCommandPool()/graphicsQueue() are QVulkanWindow's own
    // API surface for exactly this kind of user-driven transfer work.
    VkCommandBufferAllocateInfo cbAllocInfo{};
    cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAllocInfo.commandPool = m_window->graphicsCommandPool();
    cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAllocInfo.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    m_devFuncs->vkAllocateCommandBuffers(dev, &cbAllocInfo, &cb);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    m_devFuncs->vkBeginCommandBuffer(cb, &beginInfo);

    VkImageMemoryBarrier toTransferDst{};
    toTransferDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.image = m_skyboxImage;
    toTransferDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    toTransferDst.srcAccessMask = 0;
    toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    m_devFuncs->vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      0, 0, nullptr, 0, nullptr, 1, &toTransferDst);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };
    m_devFuncs->vkCmdCopyBufferToImage(cb, stagingBuffer, m_skyboxImage,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier toShaderRead{};
    toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.image = m_skyboxImage;
    toShaderRead.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    m_devFuncs->vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                      0, 0, nullptr, 0, nullptr, 1, &toShaderRead);

    m_devFuncs->vkEndCommandBuffer(cb);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;
    m_devFuncs->vkQueueSubmit(m_window->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    // One-time setup cost, not a per-frame operation -- blocking here is fine.
    m_devFuncs->vkQueueWaitIdle(m_window->graphicsQueue());

    m_devFuncs->vkFreeCommandBuffers(dev, m_window->graphicsCommandPool(), 1, &cb);
    m_devFuncs->vkDestroyBuffer(dev, stagingBuffer, nullptr);
    m_devFuncs->vkFreeMemory(dev, stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_skyboxImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (m_devFuncs->vkCreateImageView(dev, &viewInfo, nullptr, &m_skyboxImageView) != VK_SUCCESS) {
        qFatal("Failed to create the skybox image view");
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    // U (longitude) wraps around the horizon; V (latitude) must not wrap or
    // the poles would show a mirrored seam.
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    if (m_devFuncs->vkCreateSampler(dev, &samplerInfo, nullptr, &m_skyboxSampler) != VK_SUCCESS) {
        qFatal("Failed to create the skybox sampler");
    }
}

void SceneRenderer::initResources() {
    // Qt suppresses the raw Vulkan function prototypes (VK_NO_PROTOTYPES), so
    // every device-level call has to go through this dispatch table, resolved
    // dynamically via vkGetDeviceProcAddr -- not a style choice.
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    const VkDevice dev = m_window->device();

    // One unit cube (-0.5..0.5), 4 vertices per face (not shared across
    // faces) so each face keeps its own fixed normal, feeding the flat
    // per-face shading computed in mesh.vert. Winding is CCW as seen from
    // outside each face, verified against the right-hand rule.
    static const float vertexData[] = {
        // Front (+Z)
        -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
        // Back (-Z)
         0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
        // Right (+X)
         0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
        // Left (-X)
        -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
        // Top (+Y)
        -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
        // Bottom (-Y)
        -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
    };
    static const uint16_t indexData[] = {
         0,  1,  2,   2,  3,  0, // front
         4,  5,  6,   6,  7,  4, // back
         8,  9, 10,  10, 11,  8, // right
        12, 13, 14,  14, 15, 12, // left
        16, 17, 18,  18, 19, 16, // top
        20, 21, 22,  22, 23, 20, // bottom
    };
    m_indexCount = static_cast<uint32_t>(std::size(indexData));

    const VkDeviceSize vertexBufferSize = sizeof(vertexData);
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = vertexBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (m_devFuncs->vkCreateBuffer(dev, &bufferInfo, nullptr, &m_vertexBuffer) != VK_SUCCESS) {
        qFatal("Failed to create the vertex buffer");
    }

    VkMemoryRequirements memReq;
    m_devFuncs->vkGetBufferMemoryRequirements(dev, m_vertexBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    // Host-visible (and, per Qt's implementation, host-coherent) memory --
    // fine for a one-time upload of a small mesh; no flush call needed and
    // no staging buffer/transfer queue required for something this small.
    allocInfo.memoryTypeIndex = m_window->hostVisibleMemoryIndex();
    if (m_devFuncs->vkAllocateMemory(dev, &allocInfo, nullptr, &m_vertexBufferMemory) != VK_SUCCESS) {
        qFatal("Failed to allocate vertex buffer memory");
    }
    m_devFuncs->vkBindBufferMemory(dev, m_vertexBuffer, m_vertexBufferMemory, 0);

    void *mapped = nullptr;
    m_devFuncs->vkMapMemory(dev, m_vertexBufferMemory, 0, vertexBufferSize, 0, &mapped);
    std::memcpy(mapped, vertexData, vertexBufferSize);
    m_devFuncs->vkUnmapMemory(dev, m_vertexBufferMemory);

    const VkDeviceSize indexBufferSize = sizeof(indexData);
    VkBufferCreateInfo indexBufferInfo{};
    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferInfo.size = indexBufferSize;
    indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (m_devFuncs->vkCreateBuffer(dev, &indexBufferInfo, nullptr, &m_indexBuffer) != VK_SUCCESS) {
        qFatal("Failed to create the index buffer");
    }

    VkMemoryRequirements indexMemReq;
    m_devFuncs->vkGetBufferMemoryRequirements(dev, m_indexBuffer, &indexMemReq);

    VkMemoryAllocateInfo indexAllocInfo{};
    indexAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    indexAllocInfo.allocationSize = indexMemReq.size;
    indexAllocInfo.memoryTypeIndex = m_window->hostVisibleMemoryIndex();
    if (m_devFuncs->vkAllocateMemory(dev, &indexAllocInfo, nullptr, &m_indexBufferMemory) != VK_SUCCESS) {
        qFatal("Failed to allocate index buffer memory");
    }
    m_devFuncs->vkBindBufferMemory(dev, m_indexBuffer, m_indexBufferMemory, 0);

    void *indexMapped = nullptr;
    m_devFuncs->vkMapMemory(dev, m_indexBufferMemory, 0, indexBufferSize, 0, &indexMapped);
    std::memcpy(indexMapped, indexData, indexBufferSize);
    m_devFuncs->vkUnmapMemory(dev, m_indexBufferMemory);

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = 6 * sizeof(float);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDescs[2]{};
    attrDescs[0].location = 0;
    attrDescs[0].binding = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset = 0;
    attrDescs[1].location = 1;
    attrDescs[1].binding = 0;
    attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[1].offset = 3 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attrDescs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Viewport/scissor are dynamic (set every frame in startNextFrame from the
    // current swapchain size), so the pipeline never needs rebuilding on
    // resize -- the actual values here are ignored.
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    // Real closed solid now (M1/M2 explicitly deferred this) -- cull back
    // faces. The cube's vertices are authored CCW-outward in right-handed
    // object space; despite Vulkan's clip-space Y-flip (via
    // clipCorrectionMatrix in startNextFrame) potentially reversing apparent
    // winding, COUNTER_CLOCKWISE is what actually ends up meaning "front" on
    // screen here -- CLOCKWISE was tried first and produced inside-out-looking
    // boxes (only the far/interior faces visible), caught visually rather
    // than by reasoning through the flip correctly up front.
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Real depth testing now that there's a closed solid whose faces need to
    // occlude each other correctly (M1/M2 left this disabled -- a single
    // triangle never needed it).
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                         | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    const VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynStates;

    // One binding: a per-frame view/projection matrix, fed by the viewport's
    // orbit Camera (unchanged from M2).
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo dsLayoutInfo{};
    dsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsLayoutInfo.bindingCount = 1;
    dsLayoutInfo.pBindings = &uboBinding;
    if (m_devFuncs->vkCreateDescriptorSetLayout(dev, &dsLayoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        qFatal("Failed to create the descriptor set layout");
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = kFrames;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = kFrames;
    if (m_devFuncs->vkCreateDescriptorPool(dev, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        qFatal("Failed to create the descriptor pool");
    }

    VkDescriptorSetLayout setLayouts[kFrames];
    for (int i = 0; i < kFrames; ++i) {
        setLayouts[i] = m_descriptorSetLayout;
    }

    VkDescriptorSetAllocateInfo dsAllocInfo{};
    dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAllocInfo.descriptorPool = m_descriptorPool;
    dsAllocInfo.descriptorSetCount = kFrames;
    dsAllocInfo.pSetLayouts = setLayouts;
    if (m_devFuncs->vkAllocateDescriptorSets(dev, &dsAllocInfo, m_descriptorSets) != VK_SUCCESS) {
        qFatal("Failed to allocate descriptor sets");
    }

    // One uniform buffer per concurrent frame, persistently mapped: written
    // fresh every startNextFrame() and never unmapped until releaseResources()
    // frees the memory outright. A single shared buffer would race the GPU
    // still reading a previous frame's matrix while the CPU overwrites it.
    for (int i = 0; i < kFrames; ++i) {
        VkBufferCreateInfo uboInfo{};
        uboInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        uboInfo.size = kUboSize;
        uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (m_devFuncs->vkCreateBuffer(dev, &uboInfo, nullptr, &m_uniformBuffers[i]) != VK_SUCCESS) {
            qFatal("Failed to create a uniform buffer");
        }

        VkMemoryRequirements uboMemReq;
        m_devFuncs->vkGetBufferMemoryRequirements(dev, m_uniformBuffers[i], &uboMemReq);

        VkMemoryAllocateInfo uboAllocInfo{};
        uboAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        uboAllocInfo.allocationSize = uboMemReq.size;
        uboAllocInfo.memoryTypeIndex = m_window->hostVisibleMemoryIndex();
        if (m_devFuncs->vkAllocateMemory(dev, &uboAllocInfo, nullptr, &m_uniformBuffersMemory[i]) != VK_SUCCESS) {
            qFatal("Failed to allocate uniform buffer memory");
        }
        m_devFuncs->vkBindBufferMemory(dev, m_uniformBuffers[i], m_uniformBuffersMemory[i], 0);
        m_devFuncs->vkMapMemory(dev, m_uniformBuffersMemory[i], 0, kUboSize, 0, &m_uniformBuffersMapped[i]);

        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_uniformBuffers[i];
        bufInfo.offset = 0;
        bufInfo.range = kUboSize;

        VkWriteDescriptorSet descWrite{};
        descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrite.dstSet = m_descriptorSets[i];
        descWrite.dstBinding = 0;
        descWrite.descriptorCount = 1;
        descWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descWrite.pBufferInfo = &bufInfo;
        m_devFuncs->vkUpdateDescriptorSets(dev, 1, &descWrite, 0, nullptr);
    }

    // Per-object model matrix + color travel as a push constant rather than a
    // second descriptor set -- simpler than allocating/binding a descriptor
    // set per Part for a handful of simple objects, and 80 bytes comfortably
    // fits Vulkan's guaranteed minimum 128-byte push-constant budget.
    VkPushConstantRange meshPushConstantRange{};
    meshPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    meshPushConstantRange.offset = 0;
    meshPushConstantRange.size = static_cast<uint32_t>(kMeshPushConstantSize);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &meshPushConstantRange;
    if (m_devFuncs->vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        qFatal("Failed to create the pipeline layout");
    }

    const VkShaderModule vertShader = createShaderModule(QStringLiteral(":/shaders/mesh.vert.spv"));
    const VkShaderModule fragShader = createShaderModule(QStringLiteral(":/shaders/mesh.frag.spv"));

    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShader;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShader;
    fragStageInfo.pName = "main";

    const VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_window->defaultRenderPass();

    if (m_devFuncs->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        qFatal("Failed to create the graphics pipeline");
    }

    // Baked into the pipeline now; the modules themselves aren't needed after.
    m_devFuncs->vkDestroyShaderModule(dev, vertShader, nullptr);
    m_devFuncs->vkDestroyShaderModule(dev, fragShader, nullptr);

    // --- Skybox: texture, descriptor set, pipeline ---

    createSkyboxTexture();

    VkDescriptorSetLayoutBinding skyboxBinding{};
    skyboxBinding.binding = 0;
    skyboxBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    skyboxBinding.descriptorCount = 1;
    skyboxBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo skyboxDsLayoutInfo{};
    skyboxDsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    skyboxDsLayoutInfo.bindingCount = 1;
    skyboxDsLayoutInfo.pBindings = &skyboxBinding;
    if (m_devFuncs->vkCreateDescriptorSetLayout(dev, &skyboxDsLayoutInfo, nullptr, &m_skyboxDescriptorSetLayout) != VK_SUCCESS) {
        qFatal("Failed to create the skybox descriptor set layout");
    }

    VkDescriptorPoolSize skyboxPoolSize{};
    skyboxPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    skyboxPoolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo skyboxPoolInfo{};
    skyboxPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    skyboxPoolInfo.poolSizeCount = 1;
    skyboxPoolInfo.pPoolSizes = &skyboxPoolSize;
    skyboxPoolInfo.maxSets = 1;
    if (m_devFuncs->vkCreateDescriptorPool(dev, &skyboxPoolInfo, nullptr, &m_skyboxDescriptorPool) != VK_SUCCESS) {
        qFatal("Failed to create the skybox descriptor pool");
    }

    VkDescriptorSetAllocateInfo skyboxDsAllocInfo{};
    skyboxDsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    skyboxDsAllocInfo.descriptorPool = m_skyboxDescriptorPool;
    skyboxDsAllocInfo.descriptorSetCount = 1;
    skyboxDsAllocInfo.pSetLayouts = &m_skyboxDescriptorSetLayout;
    if (m_devFuncs->vkAllocateDescriptorSets(dev, &skyboxDsAllocInfo, &m_skyboxDescriptorSet) != VK_SUCCESS) {
        qFatal("Failed to allocate the skybox descriptor set");
    }

    VkDescriptorImageInfo skyboxImageInfo{};
    skyboxImageInfo.sampler = m_skyboxSampler;
    skyboxImageInfo.imageView = m_skyboxImageView;
    skyboxImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet skyboxDescWrite{};
    skyboxDescWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    skyboxDescWrite.dstSet = m_skyboxDescriptorSet;
    skyboxDescWrite.dstBinding = 0;
    skyboxDescWrite.descriptorCount = 1;
    skyboxDescWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    skyboxDescWrite.pImageInfo = &skyboxImageInfo;
    m_devFuncs->vkUpdateDescriptorSets(dev, 1, &skyboxDescWrite, 0, nullptr);

    VkPushConstantRange skyboxPushConstantRange{};
    skyboxPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    skyboxPushConstantRange.offset = 0;
    skyboxPushConstantRange.size = static_cast<uint32_t>(kSkyboxPushConstantSize);

    VkPipelineLayoutCreateInfo skyboxLayoutInfo{};
    skyboxLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    skyboxLayoutInfo.setLayoutCount = 1;
    skyboxLayoutInfo.pSetLayouts = &m_skyboxDescriptorSetLayout;
    skyboxLayoutInfo.pushConstantRangeCount = 1;
    skyboxLayoutInfo.pPushConstantRanges = &skyboxPushConstantRange;
    if (m_devFuncs->vkCreatePipelineLayout(dev, &skyboxLayoutInfo, nullptr, &m_skyboxPipelineLayout) != VK_SUCCESS) {
        qFatal("Failed to create the skybox pipeline layout");
    }

    // No vertex/index buffer -- skybox.vert generates the full-screen
    // triangle from gl_VertexIndex, so vertex input state is empty.
    VkPipelineVertexInputStateCreateInfo skyboxVertexInput{};
    skyboxVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineRasterizationStateCreateInfo skyboxRasterizer{};
    skyboxRasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    skyboxRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    skyboxRasterizer.cullMode = VK_CULL_MODE_NONE;
    skyboxRasterizer.lineWidth = 1.0f;

    // Depth test/write both off -- the skybox is drawn first, every frame,
    // and must never occlude or be occluded based on depth; it's simply
    // whatever's behind everything else.
    VkPipelineDepthStencilStateCreateInfo skyboxDepthStencil{};
    skyboxDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    skyboxDepthStencil.depthTestEnable = VK_FALSE;
    skyboxDepthStencil.depthWriteEnable = VK_FALSE;
    skyboxDepthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    const VkShaderModule skyboxVertShader = createShaderModule(QStringLiteral(":/shaders/skybox.vert.spv"));
    const VkShaderModule skyboxFragShader = createShaderModule(QStringLiteral(":/shaders/skybox.frag.spv"));

    VkPipelineShaderStageCreateInfo skyboxVertStageInfo{};
    skyboxVertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    skyboxVertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    skyboxVertStageInfo.module = skyboxVertShader;
    skyboxVertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo skyboxFragStageInfo{};
    skyboxFragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    skyboxFragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    skyboxFragStageInfo.module = skyboxFragShader;
    skyboxFragStageInfo.pName = "main";

    const VkPipelineShaderStageCreateInfo skyboxShaderStages[] = { skyboxVertStageInfo, skyboxFragStageInfo };

    VkGraphicsPipelineCreateInfo skyboxPipelineInfo{};
    skyboxPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    skyboxPipelineInfo.stageCount = 2;
    skyboxPipelineInfo.pStages = skyboxShaderStages;
    skyboxPipelineInfo.pVertexInputState = &skyboxVertexInput;
    skyboxPipelineInfo.pInputAssemblyState = &inputAssembly;
    skyboxPipelineInfo.pViewportState = &viewportState;
    skyboxPipelineInfo.pRasterizationState = &skyboxRasterizer;
    skyboxPipelineInfo.pMultisampleState = &multisampling;
    skyboxPipelineInfo.pDepthStencilState = &skyboxDepthStencil;
    skyboxPipelineInfo.pColorBlendState = &colorBlending;
    skyboxPipelineInfo.pDynamicState = &dynamicState;
    skyboxPipelineInfo.layout = m_skyboxPipelineLayout;
    skyboxPipelineInfo.renderPass = m_window->defaultRenderPass();

    if (m_devFuncs->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &skyboxPipelineInfo, nullptr, &m_skyboxPipeline) != VK_SUCCESS) {
        qFatal("Failed to create the skybox pipeline");
    }

    m_devFuncs->vkDestroyShaderModule(dev, skyboxVertShader, nullptr);
    m_devFuncs->vkDestroyShaderModule(dev, skyboxFragShader, nullptr);
}

void SceneRenderer::initSwapChainResources() {
    // Nothing size-dependent to (re)build: both pipelines use dynamic
    // viewport/scissor state, and the projection matrix is recomputed from
    // the current swapchain size every frame in startNextFrame() instead.
}

void SceneRenderer::releaseSwapChainResources() {
}

void SceneRenderer::releaseResources() {
    const VkDevice dev = m_window->device();

    if (m_skyboxPipeline != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyPipeline(dev, m_skyboxPipeline, nullptr);
        m_skyboxPipeline = VK_NULL_HANDLE;
    }
    if (m_skyboxPipelineLayout != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyPipelineLayout(dev, m_skyboxPipelineLayout, nullptr);
        m_skyboxPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_skyboxDescriptorPool != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyDescriptorPool(dev, m_skyboxDescriptorPool, nullptr);
        m_skyboxDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_skyboxDescriptorSetLayout != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyDescriptorSetLayout(dev, m_skyboxDescriptorSetLayout, nullptr);
        m_skyboxDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_skyboxSampler != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroySampler(dev, m_skyboxSampler, nullptr);
        m_skyboxSampler = VK_NULL_HANDLE;
    }
    if (m_skyboxImageView != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyImageView(dev, m_skyboxImageView, nullptr);
        m_skyboxImageView = VK_NULL_HANDLE;
    }
    if (m_skyboxImage != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyImage(dev, m_skyboxImage, nullptr);
        m_skyboxImage = VK_NULL_HANDLE;
    }
    if (m_skyboxImageMemory != VK_NULL_HANDLE) {
        m_devFuncs->vkFreeMemory(dev, m_skyboxImageMemory, nullptr);
        m_skyboxImageMemory = VK_NULL_HANDLE;
    }

    if (m_pipeline != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyPipeline(dev, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyPipelineLayout(dev, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    // Descriptor sets are freed implicitly by destroying the pool.
    if (m_descriptorPool != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyDescriptorPool(dev, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyDescriptorSetLayout(dev, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    for (int i = 0; i < kFrames; ++i) {
        if (m_uniformBuffers[i] != VK_NULL_HANDLE) {
            m_devFuncs->vkDestroyBuffer(dev, m_uniformBuffers[i], nullptr);
            m_uniformBuffers[i] = VK_NULL_HANDLE;
        }
        if (m_uniformBuffersMemory[i] != VK_NULL_HANDLE) {
            // Still mapped -- freeing mapped memory implicitly unmaps it, no
            // explicit vkUnmapMemory required.
            m_devFuncs->vkFreeMemory(dev, m_uniformBuffersMemory[i], nullptr);
            m_uniformBuffersMemory[i] = VK_NULL_HANDLE;
        }
    }
    if (m_indexBuffer != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyBuffer(dev, m_indexBuffer, nullptr);
        m_indexBuffer = VK_NULL_HANDLE;
    }
    if (m_indexBufferMemory != VK_NULL_HANDLE) {
        m_devFuncs->vkFreeMemory(dev, m_indexBufferMemory, nullptr);
        m_indexBufferMemory = VK_NULL_HANDLE;
    }
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        m_devFuncs->vkDestroyBuffer(dev, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_vertexBufferMemory != VK_NULL_HANDLE) {
        m_devFuncs->vkFreeMemory(dev, m_vertexBufferMemory, nullptr);
        m_vertexBufferMemory = VK_NULL_HANDLE;
    }
}

void SceneRenderer::startNextFrame() {
    // Applies any held WASD/arrow-key movement, scaled by real elapsed time,
    // before the camera is read below.
    m_window->updateMovement();

    const QSize sz = m_window->swapChainImageSize();
    const int frame = m_window->currentFrame();

    // Shared with the click-to-select ray-picker in VulkanViewport, so the
    // two can never disagree about what's actually on screen.
    const QMatrix4x4 proj = m_window->projectionMatrix();

    const QMatrix4x4 view = m_window->camera().viewMatrix();
    const QMatrix4x4 viewProj = proj * view;
    std::memcpy(m_uniformBuffersMapped[frame], viewProj.constData(), kUboSize);

    // Skybox: same projection, but with the view matrix's translation zeroed
    // out first -- the sky rotates with the camera but never translates with
    // it, then this whole thing gets inverted so the shader can reconstruct
    // a world-space ray per screen corner.
    QMatrix4x4 rotationOnlyView = view;
    rotationOnlyView.setColumn(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
    const QMatrix4x4 invSkyboxViewProj = (proj * rotationOnlyView).inverted();

    VkClearValue clearValues[2]{};
    clearValues[0].color = { { 0.08f, 0.08f, 0.1f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo rpBeginInfo{};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = m_window->defaultRenderPass();
    rpBeginInfo.framebuffer = m_window->currentFramebuffer();
    rpBeginInfo.renderArea.extent.width = sz.width();
    rpBeginInfo.renderArea.extent.height = sz.height();
    rpBeginInfo.clearValueCount = 2;
    rpBeginInfo.pClearValues = clearValues;

    const VkCommandBuffer cb = m_window->currentCommandBuffer();
    m_devFuncs->vkCmdBeginRenderPass(cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(sz.width());
    viewport.height = static_cast<float>(sz.height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    m_devFuncs->vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent.width = sz.width();
    scissor.extent.height = sz.height();
    m_devFuncs->vkCmdSetScissor(cb, 0, 1, &scissor);

    // Skybox first, depth test/write off, no vertex/index buffer -- covers
    // every pixel so the clear color above is never actually visible.
    m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);
    m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout,
                                         0, 1, &m_skyboxDescriptorSet, 0, nullptr);
    m_devFuncs->vkCmdPushConstants(cb, m_skyboxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                                    0, static_cast<uint32_t>(kSkyboxPushConstantSize), invSkyboxViewProj.constData());
    m_devFuncs->vkCmdDraw(cb, 3, 1, 0, 0);

    m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                                         0, 1, &m_descriptorSets[frame], 0, nullptr);

    const VkDeviceSize offset = 0;
    m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_vertexBuffer, &offset);
    m_devFuncs->vkCmdBindIndexBuffer(cb, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    const Scene &scene = m_window->scene();
    for (int i = 0; i < scene.count(); ++i) {
        const Part &part = scene.part(i);

        struct {
            float model[16];
            float color[4];
        } pushData;

        const QMatrix4x4 model = part.modelMatrix();
        std::memcpy(pushData.model, model.constData(), sizeof(pushData.model));
        pushData.color[0] = static_cast<float>(part.color.redF());
        pushData.color[1] = static_cast<float>(part.color.greenF());
        pushData.color[2] = static_cast<float>(part.color.blueF());
        pushData.color[3] = 1.0f;

        m_devFuncs->vkCmdPushConstants(cb, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                                        0, static_cast<uint32_t>(kMeshPushConstantSize), &pushData);
        m_devFuncs->vkCmdDrawIndexed(cb, m_indexCount, 1, 0, 0, 0);
    }

    m_devFuncs->vkCmdEndRenderPass(cb);

    m_window->frameReady();
    // QVulkanWindow only renders on expose/resize by default -- this is what
    // turns "renders once" into a continuous present loop, proving frames
    // genuinely keep being submitted rather than showing one static image,
    // and is also what makes mouse-driven camera updates and scene edits show
    // up without any extra invalidation logic.
    m_window->requestUpdate();
}
