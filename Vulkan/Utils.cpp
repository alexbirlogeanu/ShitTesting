#include "Utils.h"

#include "Camera.h"
#include "defines.h"
#include <algorithm>
#include <fstream>
#include "Mesh.h"

#define SHADERDIR "shaders/bin/"

const glm::mat4 g_clipMatrix   (1.0f,  0.0f, 0.0f, 0.0f,
                                0.0f, -1.0f, 0.0f, 0.0f,
                                0.0f,  0.0f, 0.5f, 0.0f,
                                0.0f,  0.0f, 0.5f, 1.0f);

void ConvertToProjMatrix(glm::mat4& inOutProj)
{
    inOutProj = g_clipMatrix * inOutProj;
};

unsigned int GetBytesFromFormat(VkFormat format)
{
    switch(format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R32_SFLOAT:
        return 4;
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return 8;
    default:
        TRAP(false && "Add the desired format");
        return 0;
    };
}

void PerspectiveMatrix(glm::mat4& projMat)
{
    projMat = glm::perspective(ms_camera.GetFOV(),
        static_cast<float>(WIDTH) /
        static_cast<float>(HEIGHT), ms_camera.GetNear(), ms_camera.GetFar());
}

float CreateRandFloat(float min, float max)
{
    float val = (float)rand() / (float)RAND_MAX; //[0, 1]
    val = min + val * (max - min);
    return val;
}

void AddDescriptorType(std::vector<VkDescriptorPoolSize>& pool, VkDescriptorType type, unsigned int count)
{
    TRAP(count > 0);
    auto it = std::find_if(pool.begin(), pool.end(), [type](const VkDescriptorPoolSize& other){
        return other.type == type;
    });

    if(it == pool.end())
    {
        VkDescriptorPoolSize newType;
        newType.descriptorCount = count;
        newType.type = type;
        pool.push_back(newType);
    }
    else
    {
        it->descriptorCount += count;
    }
}

void AddAttachementDesc(VkAttachmentDescription& ad, VkImageLayout initialLayout, VkImageLayout finalLayot, VkFormat format, VkAttachmentLoadOp colorLoadOp)
{
    cleanStructure(ad);
    ad.flags = 0;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = colorLoadOp;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.stencilLoadOp = colorLoadOp;
    ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.initialLayout  = initialLayout;
    ad.finalLayout = finalLayot;
    ad.format = format;
}

VkAttachmentDescription AddAttachementDesc(VkImageLayout initialLayout, VkImageLayout finalLayot, VkFormat format, VkAttachmentLoadOp colorLoadOp )
{
    VkAttachmentDescription ad;
    AddAttachementDesc(ad, initialLayout, finalLayot, format, colorLoadOp);
    return ad;
}

VkAttachmentReference CreateAttachmentReference(unsigned int index, VkImageLayout layout)
{
    VkAttachmentReference attRef;
    attRef.attachment = index;
    attRef.layout = layout;
    return attRef;
}

void AllocDescriptorSetsInternal(VkDescriptorPool descPool, const VkDescriptorSetLayout* layouts, uint32_t nLayouts, VkDescriptorSet* pSets)
{
    VkDescriptorSetAllocateInfo descAllocInfo;
    cleanStructure(descAllocInfo);
    descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descAllocInfo.pNext = nullptr;
    descAllocInfo.descriptorPool = descPool;
    descAllocInfo.descriptorSetCount = nLayouts;
    descAllocInfo.pSetLayouts = layouts;

    VULKAN_ASSERT(vk::AllocateDescriptorSets(vk::g_vulkanContext.m_device, &descAllocInfo, pSets));
}

void AllocDescriptorSets(VkDescriptorPool descPool, const std::vector<VkDescriptorSetLayout>& setLayouts, std::vector<VkDescriptorSet>& sets)
{
    AllocDescriptorSetsInternal(descPool, setLayouts.data(), (uint32_t)setLayouts.size(), sets.data());
}

void AllocDescriptorSets(VkDescriptorPool descPool, VkDescriptorSetLayout& layout, VkDescriptorSet* pSet)
{
    AllocDescriptorSetsInternal(descPool, &layout, 1, pSet);
}

void CreateSamplerInternal(VkSampler& sampler, VkFilter filer)
{
    VkSamplerCreateInfo samplerCreateInfo;
    cleanStructure(samplerCreateInfo);
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = filer;
    samplerCreateInfo.minFilter = filer;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.mipLodBias = 0.0;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 0;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0;
    samplerCreateInfo.maxLod = 0.0;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    VULKAN_ASSERT(vk::CreateSampler(vk::g_vulkanContext.m_device, &samplerCreateInfo, nullptr, &sampler));
}

void CreateNearestSampler(VkSampler& sampler)
{
    CreateSamplerInternal(sampler, VK_FILTER_NEAREST);
}

void CreateLinearSampler(VkSampler& sampler)
{
    CreateSamplerInternal(sampler, VK_FILTER_LINEAR);
}

template<typename TYPE>
void SetDescValue(VkWriteDescriptorSet& set, TYPE* value)
{
    TRAP(false);
}

template<>
void SetDescValue(VkWriteDescriptorSet& set, VkDescriptorBufferInfo* info)
{
    set.pBufferInfo = info;
}

template<>
void SetDescValue(VkWriteDescriptorSet& set, VkDescriptorImageInfo* info)
{
    set.pImageInfo = info;
}


template <typename TYPE>
VkWriteDescriptorSet InitUpdateDescInternal(VkDescriptorSet dstSet, unsigned int dstBinding, VkDescriptorType type, TYPE* info)
{
    VkWriteDescriptorSet  wDescSet;
    cleanStructure(wDescSet);
    wDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wDescSet.dstSet = dstSet;
    wDescSet.dstBinding = dstBinding;
    wDescSet.dstArrayElement = 0;
    wDescSet.descriptorType = type;
    wDescSet.descriptorCount = 1;

    SetDescValue(wDescSet, info);
    return wDescSet;
}



VkWriteDescriptorSet InitUpdateDescriptor(VkDescriptorSet dstSet, unsigned int dstBinding, VkDescriptorType type, VkDescriptorBufferInfo* buffInfo)
{
    VkWriteDescriptorSet wDescSet;
    
    switch(type)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        wDescSet = InitUpdateDescInternal<VkDescriptorBufferInfo>(dstSet, dstBinding, type, buffInfo);
        break;
    default:
        TRAP(false && "Descriptor type invalid for buffers");
    }
    TRAP(wDescSet.pBufferInfo);
    return wDescSet;
}

VkWriteDescriptorSet InitUpdateDescriptor(VkDescriptorSet dstSet, unsigned int dstBinding, VkDescriptorType type, VkDescriptorImageInfo* imgInfo)
{
    VkWriteDescriptorSet wDescSet;// = InitUpdateDescInternal<VkDescriptorImageInfo>(dstSet, dstBinding, type, imgInfo);
    switch(type)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        wDescSet = InitUpdateDescInternal<VkDescriptorImageInfo>(dstSet, dstBinding, type, imgInfo);
        break;
    default:
        TRAP(false && "Descriptor type invalid for images");
    };

    TRAP(wDescSet.pImageInfo);
    return wDescSet;
}

VkDescriptorImageInfo CreateDescriptorImageInfo(VkSampler sampler, VkImageView imgView, VkImageLayout layot)
{
    VkDescriptorImageInfo imgInfo;
    imgInfo.sampler = sampler;
    imgInfo.imageView = imgView;
    imgInfo.imageLayout = layot;

    return imgInfo;
}

VkDescriptorBufferInfo CreateDescriptorBufferInfo(VkBuffer buff, VkDeviceSize offset, VkDeviceSize range)
{
    VkDescriptorBufferInfo info;
    info.buffer = buff;
    info.offset = offset;
    info.range = range;
    return info;
}

VkDescriptorSetLayoutBinding CreateDescriptorBinding(unsigned int binding, VkDescriptorType type, VkShaderStageFlags flags, unsigned int count)
{
    VkDescriptorSetLayoutBinding descBind;
    cleanStructure(descBind);
    descBind.binding = binding;
    descBind.descriptorType = type;
    descBind.stageFlags = flags;
    descBind.descriptorCount = count;
    return descBind;
}

void NewDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayout* layout)
{
    VkDescriptorSetLayoutCreateInfo crtInfo;
    cleanStructure(crtInfo);
    crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    crtInfo.bindingCount = (uint32_t)bindings.size();
    crtInfo.pBindings = bindings.data();

    VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, layout));
}

VkSubpassDependency CreateSubpassDependency(unsigned int src, unsigned int dst, 
                                           VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, 
                                           VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkDependencyFlags depFlags)
{
    VkSubpassDependency subDep;
    cleanStructure(subDep);
    subDep.srcSubpass = src;
    subDep.dstSubpass = dst;
    subDep.srcStageMask = srcStageMask;
    subDep.dstStageMask = dstStageMask;
    subDep.srcAccessMask = srcAccessMask;
    subDep.dstAccessMask = dstAccessMask;
    subDep.dependencyFlags = depFlags;

    return subDep;
}

VkSubpassDescription CreateSubpassDesc(VkAttachmentReference* pColorAtts, unsigned int clrCnt, VkAttachmentReference* depthAtt, VkPipelineBindPoint bindPoint)
{
    VkSubpassDescription subDesc;
    cleanStructure(subDesc);
    subDesc.flags = 0;
    subDesc.pipelineBindPoint = bindPoint;
    subDesc.pColorAttachments = pColorAtts;
    subDesc.colorAttachmentCount = clrCnt;
    subDesc.pDepthStencilAttachment = depthAtt;

    return subDesc;
}

void NewRenderPass(VkRenderPass* renderPass, const std::vector<VkAttachmentDescription>& attachments, const std::vector<VkSubpassDescription>& subpasses, const std::vector<VkSubpassDependency>& dependecies)
{
    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = (uint32_t)attachments.size();
    rpci.pAttachments = attachments.data();
    rpci.dependencyCount = (uint32_t)dependecies.size();
    rpci.pDependencies = (!dependecies.empty()) ? dependecies.data() : nullptr;
    rpci.subpassCount = (uint32_t)subpasses.size();
    rpci.pSubpasses = subpasses.data();

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, renderPass));
}

Mesh* CreateFullscreenQuad()
{
    static Mesh* quad = nullptr;
    if(!quad)
    {
        SVertex vertices[] = {
            SVertex(glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)),
            SVertex(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)),
            SVertex(glm::vec3(1.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f)),
            SVertex(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f))
        };

        unsigned int indexes[] = {2, 1, 0, 0, 3, 2};

    //return new Mesh(std::vector<SVertex> (&vertices[0], &vertices[0] + 4), std::vector<unsigned int>(&indexes[0], &indexes[0] + 6));
        quad = new Mesh(std::vector<SVertex> (&vertices[0], &vertices[0] + 4), std::vector<unsigned int>(&indexes[0], &indexes[0] + 6));
    }

    return quad;
}

Mesh* CreateUnitCube()
{
    static Mesh* cube = nullptr;
    if (!cube)
        cube = new Mesh("obj\\cube.obj");

    return cube;
}

bool IsDepthFormat(VkFormat format)
{
    return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT ;
}

bool IsStencilFormat(VkFormat format)
{
    return format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_S8_UINT;
}

bool IsColorFormat(VkFormat format)
{
    return !IsDepthFormat(format) && !IsStencilFormat(format);
}


bool CreateShaderModule(const std::string& inFileName, VkShaderModule& outModule)
{
    std::fstream shaderFile (SHADERDIR + inFileName, std::ios_base::binary | std::ios_base::in);
    if(!shaderFile.is_open())
    {
        TRAP(false);
        return false;
    }

    shaderFile.seekg(0, std::ios_base::end);
    std::streampos size = shaderFile.tellg();
    shaderFile.seekg(0, std::ios_base::beg);

    std::string shaderCode;
    shaderCode.resize((unsigned int) size);
    shaderFile.read(&shaderCode[0], size);

    std::streamsize bytesRead = shaderFile.gcount();
    TRAP(bytesRead == size);
    TRAP(shaderCode.size() % 4 == 0);

    VkShaderModuleCreateInfo smci;
    cleanStructure(smci);
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.pNext = nullptr;
    smci.flags = 0;
    smci.codeSize = shaderCode.size();
    smci.pCode = (uint32_t*)shaderCode.data();

    outModule = VK_NULL_HANDLE;
    VULKAN_ASSERT(vk::CreateShaderModule(vk::g_vulkanContext.m_device, &smci, nullptr, &outModule));

    shaderFile.close();

    return (outModule != VK_NULL_HANDLE);
}

void CreateImageView(VkImageView& outImgView, VkImage& img, const VkImageCreateInfo& crtInfo)
{
    VkFormat format = crtInfo.format;
    VkImageAspectFlags aspectFlags = 0;
    if(IsColorFormat(format))
    {
        aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    else
    {
        aspectFlags = (IsDepthFormat(format))? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
        //aspectFlags |= (IsStencilFormat(format))? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
    }

    TRAP(crtInfo.extent.depth == 1);
    TRAP(aspectFlags);
    VkImageViewType viewType = (crtInfo.arrayLayers > 1)? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;

    VkImageViewCreateInfo ImgView;
    cleanStructure(ImgView);
    ImgView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ImgView.pNext = nullptr;
    ImgView.flags = 0;
    ImgView.image = img;
    ImgView.viewType = viewType;
    ImgView.format = format;
    ImgView.subresourceRange.aspectMask = aspectFlags;
    ImgView.subresourceRange.layerCount = crtInfo.arrayLayers;
    ImgView.subresourceRange.levelCount = 1;
    ImgView.subresourceRange.baseArrayLayer = 0;
    ImgView.subresourceRange.baseMipLevel = 0;

    VULKAN_ASSERT(vk::CreateImageView(vk::g_vulkanContext.m_device, &ImgView, nullptr, &outImgView));
}

void AllocBufferMemory(VkBuffer& buffer, VkDeviceMemory& memory, uint32_t size, VkBufferUsageFlags usage)
{
    VkBufferCreateInfo bufferInfo;
    cleanStructure(bufferInfo);
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    //flags
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VULKAN_ASSERT(vk::CreateBuffer(vk::g_vulkanContext.m_device, &bufferInfo, nullptr, &buffer));

    VkMemoryRequirements memReq;
    vk::GetBufferMemoryRequirements(vk::g_vulkanContext.m_device,buffer, &memReq);

    VkMemoryAllocateInfo memAllocInfo;
    cleanStructure(memAllocInfo);
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext = nullptr;
    memAllocInfo.memoryTypeIndex = vk::SVUlkanContext::GetMemTypeIndex(memReq.memoryTypeBits);
    memAllocInfo.allocationSize = memReq.size;

    VULKAN_ASSERT(vk::AllocateMemory(vk::g_vulkanContext.m_device, &memAllocInfo, nullptr, &memory));

    VULKAN_ASSERT(vk::BindBufferMemory(vk::g_vulkanContext.m_device, buffer, memory, 0));
}

void AllocImageMemory(const VkImageCreateInfo& imgInfo, VkImage& outImage, VkDeviceMemory& outMemory)
{
    VkDevice& dev = vk::g_vulkanContext.m_device;
    VULKAN_ASSERT(vk::CreateImage(dev, &imgInfo, nullptr, &outImage));

    VkMemoryRequirements memReq;
    vk::GetImageMemoryRequirements(dev, outImage, &memReq);

    VkMemoryAllocateInfo memAllocInfo;
    cleanStructure(memAllocInfo);
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext = nullptr;
    memAllocInfo.memoryTypeIndex = vk::SVUlkanContext::GetMemTypeIndex(memReq.memoryTypeBits, 0);
    memAllocInfo.allocationSize = memReq.size;

    VULKAN_ASSERT(vk::AllocateMemory(dev, &memAllocInfo, nullptr, &outMemory));

    VULKAN_ASSERT(vk::BindImageMemory(dev, outImage, outMemory, 0));
}


void AddImageBarrier(VkImageMemoryBarrier& outBarrier, VkImage& img, VkImageLayout oldLayout, VkImageLayout newLayout, 
                     VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectFlags, unsigned int layersCount)
{
    cleanStructure(outBarrier);
    outBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    outBarrier.pNext = NULL;
    outBarrier.srcAccessMask = srcMask;
    outBarrier.dstAccessMask = dstMask ;
    outBarrier.oldLayout = oldLayout;
    outBarrier.newLayout = newLayout;
    outBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outBarrier.image = img;
    outBarrier.subresourceRange.aspectMask = aspectFlags;
    outBarrier.subresourceRange.baseMipLevel = 0;
    outBarrier.subresourceRange.levelCount = 1;
    outBarrier.subresourceRange.baseArrayLayer = 0;
    outBarrier.subresourceRange.layerCount = layersCount;
}

void AddBufferBarier(VkBufferMemoryBarrier& outBarrier, VkBuffer& buffer, VkAccessFlags srcMask, VkAccessFlags dstMask)
{
    cleanStructure(outBarrier);
    outBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    outBarrier.srcAccessMask = srcMask;
    outBarrier.dstAccessMask = dstMask;
    outBarrier.buffer = buffer;
    outBarrier.offset = 0;
    outBarrier.size = VK_WHOLE_SIZE;
    outBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
}

VkPipelineShaderStageCreateInfo CreatePipelineStage(VkShaderModule modue, VkShaderStageFlagBits stage)
{
    VkPipelineShaderStageCreateInfo pipelineStage;
    cleanStructure(pipelineStage);
    pipelineStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineStage.pNext = nullptr;
    pipelineStage.flags = 0;
    pipelineStage.stage = stage;
    pipelineStage.module = modue;
    pipelineStage.pName = "main";
    pipelineStage.pSpecializationInfo = nullptr;

    return pipelineStage;
}