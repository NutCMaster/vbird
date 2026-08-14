#pragma once

#include <QVulkanWindow>
#include <QVulkanWindowRenderer>

class VulkanViewport;
class QVulkanDeviceFunctions;

class SceneRenderer : public QVulkanWindowRenderer {
public:
    explicit SceneRenderer(VulkanViewport *window);

    void initResources() override;
    void initSwapChainResources() override;
    void releaseSwapChainResources() override;
    void releaseResources() override;
    void startNextFrame() override;

private:
    VkShaderModule createShaderModule(const QString &resourcePath) const;
    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;
    void createSkyboxTexture();

    VulkanViewport *m_window;
    QVulkanDeviceFunctions *m_devFuncs = nullptr;

    // One shared unit-cube mesh (24 vertices -- 4 per face, not shared across
    // faces, so each face keeps its own fixed normal). Every Part in the
    // scene reuses it, positioned/colored via a per-draw push constant.
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
    uint32_t m_indexCount = 0;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSets[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT] = {};
    VkBuffer m_uniformBuffers[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT] = {};
    VkDeviceMemory m_uniformBuffersMemory[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT] = {};
    void *m_uniformBuffersMapped[QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT] = {};

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    // Skybox: a full-screen-triangle pass drawn before any Part, depth
    // test/write both off. No vertex/index buffer -- geometry is generated
    // in skybox.vert from gl_VertexIndex.
    VkImage m_skyboxImage = VK_NULL_HANDLE;
    VkDeviceMemory m_skyboxImageMemory = VK_NULL_HANDLE;
    VkImageView m_skyboxImageView = VK_NULL_HANDLE;
    VkSampler m_skyboxSampler = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_skyboxDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_skyboxDescriptorPool = VK_NULL_HANDLE;
    // One static set, unlike the per-frame camera descriptor sets above --
    // the texture never changes after initResources(), so there's no
    // GPU-still-reading-the-previous-frame race to avoid.
    VkDescriptorSet m_skyboxDescriptorSet = VK_NULL_HANDLE;

    VkPipelineLayout m_skyboxPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_skyboxPipeline = VK_NULL_HANDLE;
};
