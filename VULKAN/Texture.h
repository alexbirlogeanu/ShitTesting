#pragma once

#include "VulkanLoader.h"
#include "defines.h"
#include "freeimage/FreeImage.h"
#include <vector>
#include <string>
#include "Utils.h"

#define TEXTDIR "text/"

struct SImageData
{
    SImageData() 
        : width(0)
        , height(0)
        , depth(0)
        , format(VK_FORMAT_UNDEFINED)
        , data(nullptr)
    {}

    SImageData(unsigned int w, unsigned int h, unsigned int d, VkFormat f, unsigned char* dt)
        : width(w)
        , height(h)
        , depth(d)
        , format(f)
        , data(dt)
    {}

    unsigned int GetDataSize() const 
    {
        TRAP(format != VK_FORMAT_UNDEFINED)
        return width * height * depth * GetBytesFromFormat(format); 
    }

    unsigned int width;
    unsigned int height;
    unsigned int depth;
    VkFormat     format;

    unsigned char* data;
    std::string     fileName;
};

void Read2DTextureData(SImageData& img, const std::string& filename, bool isSRGB = true);
void ReadLUTTextureData(SImageData& img, const std::string& filename, bool isSRGB = true);

class CWrapper;

class CTextureManager
{
public:
    virtual ~CTextureManager();

    static CTextureManager* GetInstance();
    void AddTextureWrapper(CWrapper* text);

    void Update();
private:
    CTextureManager();

private:
    static CTextureManager ms_instance;

    std::vector<CWrapper*>  m_updateWrappers;
    std::vector<CWrapper*>  m_freeWrappers;
};
class CTexture;
class CWrapper
{
public:
    VkImage&    GetImage();
    VkBuffer&   GetBuffer() ;

    void            AddCopyCommand();
    virtual ~CWrapper();
    //void            CleanUp();
private:
    friend class CTexture;

    CWrapper(CTexture* text, const SImageData& imgData, bool ownData);
    

    VkImage                 m_textureImage;
    VkBuffer                m_dataBuffer;
    VkDeviceMemory          m_dataMemory;

    SImageData              m_data;
    bool                    m_ownData;
};

class CTexture
{
public:
    CTexture(const SImageData& image, bool ownData = false);
    ~CTexture();

    const VkDescriptorImageInfo& GetTextureDescriptor() const;
    VkDescriptorImageInfo& GetTextureDescriptor();

    void FinalizeTexture();

    VkImageView  GetImageView() const { return m_textImgView; }

protected:
    friend class CWrapper;

    //void CleanUp();
    void CreateTexture(const SImageData& imageData, bool ownData);
protected:
    VkImage                 m_textImage;
    VkImageView             m_textImgView;
    VkDeviceMemory          m_textMemory;
    VkSampler               m_textSampler;

    VkDescriptorImageInfo   m_textureInfo;
};


class CCubeMapTexture
{
public:
    CCubeMapTexture(std::vector<SImageData>& cubeFaces, bool ownData = false);
    virtual ~CCubeMapTexture();

    const VkDescriptorImageInfo& GetCubeMapDescriptor() const;
    void FinalizeCubeMap();

private:
    void Validate();

    void CreateCubeMap();
    VkImage                 m_cubeMapImage;
    VkImageView             m_cubeMapView;
    VkDeviceMemory          m_cubeMapMemory;
    VkSampler               m_cubeMapSampler;

    VkDeviceMemory          m_bufferMemory;
    VkBuffer                m_buffer;

    bool                                m_ownData;
    std::vector<SImageData>           m_cubeMapData;

    VkDescriptorImageInfo   m_cubeMapInfo;
    unsigned int            m_width, m_height;
};

CCubeMapTexture* CreateCubeMapTexture(std::vector<std::string>& facesFileNames);