#pragma once

#include "glm/glm.hpp"
#include "Camera.h"
#include "VulkanLoader.h"
#include "Mesh.h"
#include "Lights.h"
#include "DebugMarkers.h"

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
void AllocDescriptorSets(VkDescriptorPool descPool, const VkDescriptorSetLayout& layout, VkDescriptorSet* pSet);

VkWriteDescriptorSet InitUpdateDescriptor(VkDescriptorSet dstSet, unsigned int dstBinding, VkDescriptorType type, VkDescriptorBufferInfo* buffInfo);
VkWriteDescriptorSet InitUpdateDescriptor(VkDescriptorSet dstSet, unsigned int dstBinding, VkDescriptorType type, VkDescriptorImageInfo* imgInfo);
VkWriteDescriptorSet InitUpdateDescriptor(VkDescriptorSet dstSet, unsigned int dstBinding, VkDescriptorType type, unsigned int startArrayElem, const std::vector<VkDescriptorBufferInfo>& buffersInfo);
VkWriteDescriptorSet InitUpdateDescriptor(VkDescriptorSet dstSet, unsigned int dstBinding, VkDescriptorType type, unsigned int startArrayElem, const std::vector<VkDescriptorImageInfo>& imagesInfo);

VkDescriptorImageInfo CreateDescriptorImageInfo(VkSampler sampler, VkImageView imgView, VkImageLayout layot);
VkDescriptorBufferInfo CreateDescriptorBufferInfo(VkBuffer buff, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);

VkDescriptorSetLayoutBinding CreateDescriptorBinding(unsigned int binding, VkDescriptorType type, VkShaderStageFlags flags, unsigned int count = 1); //TODO REFACTOR
void NewDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayout* layout);//TODO REFACTOR

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
void CreateImageView(VkImageView& outImgView, const VkImage& img, const VkImageCreateInfo& crtInfo); //TODO delete this function use the new one instead
void CreateImageView(VkImageView& outImgView, const VkImage& img, VkFormat format, const VkExtent3D& extent, uint32_t arrayLayers, uint32_t baseLayer, uint32_t mipLevels, uint32_t baseMipLevel); // NEW ONE
void AllocBufferMemory(VkBuffer& buffer, VkDeviceMemory& memory, uint32_t size, VkBufferUsageFlags usage);
void AllocImageMemory(const VkImageCreateInfo& imgInfo, VkImage& outImage, VkDeviceMemory& outMemory, const std::string& debugName = std::string());

//TODO remove this function
void AddImageBarrier(VkImageMemoryBarrier& outBarrier, const VkImage& img, VkImageLayout oldLayout, VkImageLayout newLayout, 
                     VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectFlags, unsigned int layersCount = VK_REMAINING_ARRAY_LAYERS);
void AddBufferBarier(VkBufferMemoryBarrier& outBarrier, const VkBuffer& buffer, VkAccessFlags srcMask, VkAccessFlags dstMask);

VkPipelineShaderStageCreateInfo CreatePipelineStage(VkShaderModule modue, VkShaderStageFlagBits stage);

void ReadXmlFile(const std::string& xml, char** fileContent);

float GetDeltaTime();

extern CCamera ms_camera;
extern CDirectionalLight directionalLight;