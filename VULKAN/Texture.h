#pragma once

#include "VulkanLoader.h"
#include "defines.h"
#include "freeimage/FreeImage.h"
#include <vector>
#include <string>
#include "Utils.h"
#include "Singleton.h"
#include "Serializer.h"
#include "ResourceLoader.h"

#define TEXTDIR "text/"

class ImageHandle;
class CTexture;
class BufferHandle;

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

class TextureCreator;

class CTextureManager : public Singleton<CTextureManager>
{
	friend class Singleton<CTextureManager>;
public:
	void RegisterTextureForCreation(TextureCreator* text);

    void Update();
private:
    CTextureManager();
	virtual ~CTextureManager();

	VkDeviceSize EstimateMemory();
private:
	std::vector<TextureCreator*>  m_updateTextureCreators;
	std::vector<TextureCreator*>  m_freeTextureCreators;
};

class TextureCreator
{
	friend class CTexture;
public:
	virtual ~TextureCreator();

    ImageHandle*		GetImage();
	BufferHandle*		GetBuffer();
	VkDeviceSize		GetDataSize();

	void				Prepare(); //kinda shitty
    void				AddCopyCommand();
	void				CopyLocalData();
private:
	TextureCreator(CTexture* text, const SImageData& imgData, bool ownData);
private:
	CTexture*				m_texture;
	BufferHandle*			m_staggingBuffer;

    SImageData              m_data;
    bool                    m_ownData;
};

class CTexture : public SeriableImpl<CTexture>
{
	friend class TextureCreator;
public:
    CTexture(const SImageData& image, bool ownData = false); //for compatibility TODO delete
	CTexture(const std::string& filename); //for compatibility TODO delete
	CTexture();
    ~CTexture();

    const VkDescriptorImageInfo& GetTextureDescriptor() const;
    VkDescriptorImageInfo& GetTextureDescriptor();
	VkImageView  GetImageView() const;
	void CreateTexture(const SImageData& imageData, bool ownData);

protected:
    //void CleanUp();
protected:
	ImageHandle*			m_image;
    VkSampler               m_textSampler;
    VkDescriptorImageInfo   m_textureInfo;

	DECLARE_PROPERTY(std::string, Filename, CTexture);
	DECLARE_PROPERTY(bool, IsSRGB, CTexture);
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


template<typename BASE>
class Property<CTexture*, BASE> : public PropertyGeneric
{
public:
	typedef CTexture* BASE::* PtmType;
	Property()
	{}
	Property(PtmType offset, const std::string& label)
		: m_ptm(offset)
		, m_label(label)
	{}

	virtual void Save(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);

		(cobj->*m_ptm)->SetName(m_label);
		(cobj->*m_ptm)->Serialize(serializer);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);

		cobj->*m_ptm = new CTexture();

		(cobj->*m_ptm)->SetName(m_label);
		(cobj->*m_ptm)->Serialize(serializer);

		ResourceLoader::GetInstance()->LoadTexture(&(cobj->*m_ptm));
	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};
