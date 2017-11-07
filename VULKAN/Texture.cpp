#include "Texture.h"
#include "Utils.h"
#include <algorithm>

void Read2DTextureData(SImageData& img, const std::string& filename, bool isSRGB)
{
    FIBITMAP* fiImage = FreeImage_Load(FreeImage_GetFIFFromFilename(filename.c_str()), filename.c_str());
    TRAP(fiImage);
    TRAP(FreeImage_GetBPP(fiImage) / 8 == 4); //images we want to be 4 channels

    img.width = FreeImage_GetWidth(fiImage);
    img.height = FreeImage_GetHeight(fiImage);
    img.depth = 1;
    img.format = (isSRGB)? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
    img.fileName = filename;

    unsigned int size = img.GetDataSize();
    img.data = new unsigned char[size];
    BYTE* imgBits = FreeImage_GetBits(fiImage);
    std::memcpy(img.data, imgBits, size);

    FreeImage_Unload(fiImage);
};

void ReadLUTTextureData(SImageData& img, const std::string& filename, bool isSRGB)
{
    FIBITMAP* fiImage = FreeImage_Load(FreeImage_GetFIFFromFilename(filename.c_str()), filename.c_str());
    TRAP(fiImage);
    TRAP(FreeImage_GetBPP(fiImage) / 8 == 4);
    
    unsigned int LUTSize = FreeImage_GetHeight(fiImage);
    unsigned int width = FreeImage_GetWidth(fiImage);

    img.height = LUTSize;
    img.width = LUTSize;
    img.depth = width / LUTSize;
    img.format = (isSRGB)? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;

    TRAP(img.width == img.depth);

    img.data = new unsigned char[img.GetDataSize()];
    BYTE* imgBits = FreeImage_GetBits(fiImage);
    memcpy(img.data, imgBits, img.GetDataSize());

    unsigned char* p = img.data;
    for(unsigned int i = 0; i < LUTSize; ++i)
        for(unsigned int j = 0; j < width; ++j)
        {
            unsigned index = (i * width + j) * 4;
            unsigned char c = p[index + 0];
            p[index + 0] = p[index + 1];
            p[index + 1] = c;
        }

    FreeImage_Unload(fiImage);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CTextureManager
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTextureManager CTextureManager::ms_instance;

CTextureManager::CTextureManager()
{
}

CTextureManager::~CTextureManager()
{
}

CTextureManager* CTextureManager::GetInstance()
{
    return &ms_instance;
}

void CTextureManager::AddTextureWrapper(CWrapper* w)
{
    auto it = std::find(m_updateWrappers.begin(), m_updateWrappers.end(), w);
    if(it != m_updateWrappers.end())
        return;

    m_updateWrappers.push_back(w);
}

void CTextureManager::Update()
{
    if(!m_freeWrappers.empty())
    {
        for(unsigned int i = 0; i < m_freeWrappers.size(); ++i)
            delete m_freeWrappers[i];

        m_freeWrappers.clear();
    }

    if (m_updateWrappers.empty())
       return;

    VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

    std::vector<VkImageMemoryBarrier> imgBarries(m_updateWrappers.size());
    std::vector<VkBufferMemoryBarrier> buffBarriers(m_updateWrappers.size());

    for(unsigned int i = 0; i < m_updateWrappers.size(); ++i)
    {
        AddBufferBarier(buffBarriers[i], m_updateWrappers[i]->GetBuffer(), VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        AddImageBarrier(imgBarries[i], m_updateWrappers[i]->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 
        (uint32_t)buffBarriers.size(), buffBarriers.data(), (uint32_t)imgBarries.size(), imgBarries.data());

    for(unsigned int i = 0; i < m_updateWrappers.size(); ++i)
    {
        m_updateWrappers[i]->AddCopyCommand();
        AddImageBarrier(imgBarries[i], m_updateWrappers[i]->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        m_freeWrappers.push_back(m_updateWrappers[i]);
    }

    vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, (uint32_t)imgBarries.size(), imgBarries.data());

    m_updateWrappers.clear();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CWrapper
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CWrapper::CWrapper(CTexture* text, const SImageData& imgData, bool ownData)
{
    m_ownData = ownData;
    m_data = imgData;
    m_textureImage = text->m_textImage;

    AllocBufferMemory(m_dataBuffer, m_dataMemory, m_data.GetDataSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    unsigned char* data = nullptr;
    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_dataMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data));

    memcpy(data, m_data.data, m_data.GetDataSize());

    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_dataMemory);
}

VkImage& CWrapper::GetImage()
{
    return m_textureImage;
}

VkBuffer& CWrapper::GetBuffer()
{
    return m_dataBuffer;
}

void CWrapper::AddCopyCommand()
{
    VkCommandBuffer cmdBuff =  vk::g_vulkanContext.m_mainCommandBuffer;

    VkImageSubresourceLayers subResourceLayers;
    cleanStructure(subResourceLayers);
    subResourceLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subResourceLayers.mipLevel = 0;
    subResourceLayers.layerCount = 1;
    subResourceLayers.baseArrayLayer = 0;

    VkOffset3D offset;
    cleanStructure(offset);

    VkExtent3D extent;
    extent.width = m_data.width;
    extent.height = m_data.height;
    extent.depth = m_data.depth;

    VkBufferImageCopy copy;
    cleanStructure(copy);
    copy.imageOffset = offset;
    copy.bufferOffset = 0;
    copy.imageExtent = extent;
    copy.imageSubresource = subResourceLayers;

    vk::CmdCopyBufferToImage(cmdBuff, m_dataBuffer, m_textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

CWrapper::~CWrapper()
{
    VkDevice dev = vk::g_vulkanContext.m_device;
    vk::FreeMemory(dev, m_dataMemory, nullptr);
    vk::DestroyBuffer(dev, m_dataBuffer, nullptr);

    if (m_ownData)
        delete [] m_data.data;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CWrapper
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


CTexture::CTexture(const SImageData& image, bool ownData)
    : m_textImage(VK_NULL_HANDLE)
    , m_textMemory(VK_NULL_HANDLE)
    , m_textImgView(VK_NULL_HANDLE)

{
    CreateTexture(image, ownData);
}

const VkDescriptorImageInfo& CTexture::GetTextureDescriptor() const  
{
    return m_textureInfo;
}

VkDescriptorImageInfo& CTexture::GetTextureDescriptor()
{
    return m_textureInfo;
}

void CTexture::FinalizeTexture()
{
   
}

CTexture::~CTexture()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::DestroySampler(dev, m_textSampler, nullptr);
    vk::DestroyImageView(dev, m_textImgView, nullptr);
    vk::DestroyImage(dev, m_textImage, nullptr);
    vk::FreeMemory(dev, m_textMemory, nullptr);

}

void CTexture::CreateTexture(const SImageData& imageData, bool ownData)
{
    TRAP(imageData.data);
    VkImageType type = (imageData.height > 1)? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D;
    type = (imageData.depth > 1)?  VK_IMAGE_TYPE_3D : type;

    VkFormat format = imageData.format;
    TRAP(format != VK_FORMAT_UNDEFINED);

    VkDevice dev = vk::g_vulkanContext.m_device;

    VkImageCreateInfo imgTextInfo;
    cleanStructure(imgTextInfo);
    imgTextInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgTextInfo.pNext = NULL;
    imgTextInfo.flags = 0;
    imgTextInfo.imageType = type;
    imgTextInfo.format = format;
    imgTextInfo.extent.width = imageData.width;
    imgTextInfo.extent.height = imageData.height;
    imgTextInfo.extent.depth = imageData.depth;
    imgTextInfo.mipLevels = 1;
    imgTextInfo.arrayLayers = 1;
    imgTextInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgTextInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgTextInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgTextInfo.usage =  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgTextInfo.queueFamilyIndexCount = 0;
    imgTextInfo.pQueueFamilyIndices = NULL;
    imgTextInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    unsigned int totalSize = imageData.GetDataSize();

    VULKAN_ASSERT(vk::CreateImage(dev, &imgTextInfo, nullptr, &m_textImage));

    AllocImageMemory(imgTextInfo, m_textImage, m_textMemory, imageData.fileName);

    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_1D;
    if (type == VK_IMAGE_TYPE_2D)
        viewType = VK_IMAGE_VIEW_TYPE_2D;
    else if(type == VK_IMAGE_TYPE_3D)
        viewType = VK_IMAGE_VIEW_TYPE_3D;

    VkImageViewCreateInfo imgViewInfo;
    cleanStructure(imgViewInfo);
    imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imgViewInfo.pNext = nullptr;
    imgViewInfo.flags = 0;
    imgViewInfo.viewType = viewType;
    imgViewInfo.image = m_textImage;
    imgViewInfo.format = format;
    imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imgViewInfo.subresourceRange.baseMipLevel = 0;
    imgViewInfo.subresourceRange.levelCount = 1;
    imgViewInfo.subresourceRange.baseArrayLayer = 0;
    imgViewInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VULKAN_ASSERT(vk::CreateImageView(dev, &imgViewInfo, nullptr, &m_textImgView));

    CreateLinearSampler(m_textSampler);

    SetObjectDebugName(m_textImgView, std::string("View_") + imageData.fileName); //this doesn't create anything ??

    m_textureInfo.sampler = m_textSampler;
    m_textureInfo.imageView = m_textImgView;
    m_textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    CTextureManager::GetInstance()->AddTextureWrapper(new CWrapper(this, imageData, ownData));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CCubeMapTexture
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CCubeMapTexture::CCubeMapTexture(std::vector<SImageData>& cubeFaces, bool ownData)
    : m_cubeMapData(cubeFaces)
    , m_ownData(ownData)
{
    CreateCubeMap();
}

CCubeMapTexture::~CCubeMapTexture()
{
}

const VkDescriptorImageInfo& CCubeMapTexture::GetCubeMapDescriptor() const
{
    return m_cubeMapInfo;
}

void CCubeMapTexture::FinalizeCubeMap()
{
    VkDevice dev = vk::g_vulkanContext.m_device;
    VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
    VkQueue queue = vk::g_vulkanContext.m_graphicQueue;

    VkFence transFence;
    VkFenceCreateInfo fenceInfo;
    cleanStructure(fenceInfo);
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = 0;

    VULKAN_ASSERT(vk::CreateFence(dev, &fenceInfo, nullptr, &transFence));
    VkCommandBufferBeginInfo bufferBeginInfo;
    cleanStructure(bufferBeginInfo);
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkImageMemoryBarrier preCopy;
    AddImageBarrier(preCopy, m_cubeMapImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT/*, 6*/);

    VULKAN_ASSERT(vk::BeginCommandBuffer(cmdBuffer, &bufferBeginInfo));

    vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &preCopy);

    VkExtent3D extent;
    extent.width = m_width;
    extent.height = m_height;
    extent.depth = 1;

    VkOffset3D offset;
    cleanStructure(offset);

    VkImageSubresourceLayers subLayers;
    cleanStructure(subLayers);
    subLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subLayers.mipLevel = 0;
    subLayers.baseArrayLayer = 0;
    subLayers.layerCount = 6;

    VkBufferImageCopy bufToImgCopy;
    cleanStructure(bufToImgCopy);
    bufToImgCopy.bufferOffset = 0;
    bufToImgCopy.bufferRowLength = 0;
    bufToImgCopy.bufferImageHeight = 0;
    bufToImgCopy.imageSubresource = subLayers;
    bufToImgCopy.imageOffset = offset;
    bufToImgCopy.imageExtent = extent;

    vk::CmdCopyBufferToImage(cmdBuffer, m_buffer, m_cubeMapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufToImgCopy);

    VkImageMemoryBarrier afterCopy;
    AddImageBarrier(afterCopy, m_cubeMapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT/*, 6*/);

    vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &afterCopy);

    vk::EndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo;
    cleanStructure(submitInfo);
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    vk::QueueSubmit(queue, 1, &submitInfo, transFence);
    VULKAN_ASSERT(vk::WaitForFences(dev, 1, &transFence, VK_TRUE, UINT64_MAX));
    vk::DestroyFence(dev, transFence, nullptr);

    vk::FreeMemory(dev, m_bufferMemory, nullptr);
    vk::DestroyBuffer(dev, m_buffer, nullptr);

    VkImageViewCreateInfo imgViewInfo;
    cleanStructure(imgViewInfo);
    imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imgViewInfo.pNext = nullptr;
    imgViewInfo.flags = 0;
    imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    imgViewInfo.image = m_cubeMapImage;
    imgViewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
    imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imgViewInfo.subresourceRange.baseMipLevel = 0;
    imgViewInfo.subresourceRange.levelCount = 1;
    imgViewInfo.subresourceRange.baseArrayLayer = 0;
    imgViewInfo.subresourceRange.layerCount = 6;

    VULKAN_ASSERT(vk::CreateImageView(dev, &imgViewInfo, nullptr, &m_cubeMapView));


    VkSamplerCreateInfo samplerCreateInfo;
    cleanStructure(samplerCreateInfo);
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
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
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    VULKAN_ASSERT(vk::CreateSampler(dev, &samplerCreateInfo, NULL, &m_cubeMapSampler));

    m_cubeMapInfo.sampler = m_cubeMapSampler;
    m_cubeMapInfo.imageView = m_cubeMapView;
    m_cubeMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

}

void CCubeMapTexture::Validate()
{
    TRAP(m_cubeMapData.size() == 6);

    m_width = m_cubeMapData[0].width;
    m_height = m_cubeMapData[0].height;

    TRAP(m_width == m_height);
    TRAP(m_cubeMapData[0].data);

    for(unsigned int i = 1; i < 6; ++i)
    {
        TRAP(m_cubeMapData[i].data);
        TRAP(m_cubeMapData[i].width == m_width);
        TRAP(m_cubeMapData[i].height == m_height);
    }

}

void CCubeMapTexture::CreateCubeMap()
{
    Validate();
    unsigned int width = m_cubeMapData[0].width;
    unsigned int height = m_cubeMapData[0].height;

    VkDevice dev = vk::g_vulkanContext.m_device;

    VkImageCreateInfo imgTextInfo;
    cleanStructure(imgTextInfo);
    imgTextInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgTextInfo.pNext = NULL;
    imgTextInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imgTextInfo.imageType = VK_IMAGE_TYPE_2D;
    imgTextInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
    imgTextInfo.extent.width = width;
    imgTextInfo.extent.height = height;
    imgTextInfo.extent.depth = 1;
    imgTextInfo.mipLevels = 1;
    imgTextInfo.arrayLayers = 6;
    imgTextInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgTextInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgTextInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgTextInfo.usage =  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgTextInfo.queueFamilyIndexCount = 0;
    imgTextInfo.pQueueFamilyIndices = NULL;
    imgTextInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    unsigned int totalSize = m_cubeMapData[0].GetDataSize() * 6;
    AllocImageMemory(imgTextInfo, m_cubeMapImage, m_cubeMapMemory);
    AllocBufferMemory(m_buffer, m_bufferMemory, totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    unsigned char* data = nullptr;
    VULKAN_ASSERT(vk::MapMemory(dev, m_bufferMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data));

    unsigned int imageSize = m_cubeMapData[0].GetDataSize();
    for(unsigned int i = 0; i < 6; ++i, data += imageSize)
        memcpy(data, m_cubeMapData[i].data, imageSize);

    vk::UnmapMemory(dev, m_bufferMemory);
    
    if (m_ownData)
    {
        for(unsigned int i = 0; i < 6; ++i)
            delete[] m_cubeMapData[i].data;
    }
}

CCubeMapTexture* CreateCubeMapTexture(std::vector<std::string>& facesFileNames)
{
    TRAP(facesFileNames.size() == 6);
    std::vector<SImageData> imgData;
    imgData.resize(6);

    for(unsigned int i = 0; i < facesFileNames.size(); ++i)
        Read2DTextureData(imgData[i], facesFileNames[i]);

    return new CCubeMapTexture(imgData, true);
}