#pragma once

#include "glm/glm.hpp"
#include "Camera.h"
#include "VulkanLoader.h"
#include "Mesh.h"
#include "Lights.h"

#include <vector>

unsigned int GetBytesFromFormat(VkFormat format);

void PerspectiveMatrix(glm::mat4& projMat);

void ConvertToProjMatrix(glm::mat4& inOutProj);
float CreateRandFloat(float min, float max);

void AddDescriptorType(std::vector<VkDescriptorPoolSize>& pool, VkDescriptorType type, unsigned int count);
void AddAttachementDesc(VkAttachmentDescription& ad, VkImageLayout initialLayout, VkImageLayout finalLayot, VkFormat format, VkAttachmentLoadOp colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR);
VkAttachmentDescription AddAttachementDesc(VkImageLayout initialLayout, VkImageLayout finalLayot, VkFormat format, VkAttachmentLoadOp colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR);

VkAttachmentReference CreateAttachmentReference(unsigned int index, VkImageLayout layout);

void AllocDescriptorSets(VkDescriptorPool descPool, const std::vector<VkDescriptorSetLayout>& setLayouts, std::vector<VkDescriptorSet>& sets);
void AllocDescriptorSets(VkDescriptorPool descPool, VkDescriptorSetLayout& layout, VkDescriptorSet* pSet);

VkWriteDescriptorSet InitUpdateDescriptor(VkDescriptorSet dstSet, unsigned int dstBinding, VkDescriptorType type, VkDescriptorBufferInfo* buffInfo);
VkWriteDescriptorSet InitUpdateDescriptor(VkDescriptorSet dstSet, unsigned int dstBinding, VkDescriptorType type, VkDescriptorImageInfo* imgInfo);

VkDescriptorImageInfo CreateDescriptorImageInfo(VkSampler sampler, VkImageView imgView, VkImageLayout layot);
VkDescriptorBufferInfo CreateDescriptorBufferInfo(VkBuffer buff, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);

VkDescriptorSetLayoutBinding CreateDescriptorBinding(unsigned int binding, VkDescriptorType type, VkShaderStageFlags flags, unsigned int count = 1);
void NewDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayout* layout);

VkSubpassDependency CreateSubpassDependency(unsigned int src, unsigned int dst, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkDependencyFlags depFlags = 0);
VkSubpassDescription CreateSubpassDesc(VkAttachmentReference* pColorAtts, unsigned int clrCnt, VkAttachmentReference* depthAtt = nullptr);

void NewRenderPass(VkRenderPass* renderPass, const std::vector<VkAttachmentDescription>& attachments, const std::vector<VkSubpassDescription>& subpasses, const std::vector<VkSubpassDependency>& dependecies = std::vector<VkSubpassDependency>());

void CreateNearestSampler(VkSampler& sampler, bool clampToEdge = false);
void CreateLinearSampler(VkSampler& sampler, bool clampToEdge = true);

Mesh* CreateFullscreenQuad();
Mesh* CreateUnitCube();

bool IsDepthFormat(VkFormat format);
bool IsStencilFormat(VkFormat format);
bool IsColorFormat(VkFormat format);
bool CreateShaderModule(const std::string& inFileName, VkShaderModule& outModule);
void CreateImageView(VkImageView& outImgView, VkImage& img, const VkImageCreateInfo& crtInfo);
void AllocBufferMemory(VkBuffer& buffer, VkDeviceMemory& memory, uint32_t size, VkBufferUsageFlags usage);
void AllocImageMemory(const VkImageCreateInfo& imgInfo, VkImage& outImage, VkDeviceMemory& outMemory);

void AddImageBarrier(VkImageMemoryBarrier& outBarrier, VkImage& img, VkImageLayout oldLayout, VkImageLayout newLayout, 
                     VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectFlags, unsigned int layersCount = VK_REMAINING_ARRAY_LAYERS);
void AddBufferBarier(VkBufferMemoryBarrier& outBarrier, VkBuffer& buffer, VkAccessFlags srcMask, VkAccessFlags dstMask);

VkPipelineShaderStageCreateInfo CreatePipelineStage(VkShaderModule modue, VkShaderStageFlagBits stage);

void StartDebugMarker(const std::string& markerName);
void EndDebugMarker(const std::string& markerName);

extern CCamera ms_camera;
extern CDirectionalLight directionalLight;