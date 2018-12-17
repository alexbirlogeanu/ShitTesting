#pragma once

#include "VulkanLoader.h"
#include "rapidxml/rapidxml.hpp"
#include "defines.h"
#include "Texture.h"
#include "glm/glm.hpp"
#include "Renderer.h"
#include "Mesh.h"
#include "UiUtils.h"
#include "DescriptorsUtils.h"
#include "Singleton.h"

#include <unordered_map>
#include <string>
#include <array>

class CTexture;
class Mesh;
class CFont2;
class BufferHandle;

class CUIItem
{
public:
    CUIItem()
    : m_isVisible(true)
    {
    };
    virtual ~CUIItem(){};

    void SetVisible(bool visible) { m_isVisible = visible; }
    bool GetVisible() const { return m_isVisible; }

protected:
    virtual void Render(CGraphicPipeline* pipeline) = 0;
    virtual void PreRender() = 0;

    friend class CUIManager;
    friend class CUIRenderer;
protected:
    bool            m_isVisible;
};

class CUIText : public CUIItem
{
public:
	virtual ~CUIText();
	const std::string& GetText() const { return m_text; }
	const glm::uvec2& GetPosition() const { return m_screenPosInPixels; }

	void SetText(const std::string& text);
	void SetPosition(glm::uvec2& pos);

	bool IsDirty() const { return m_isDirty; }
private:
	CUIText(CFont2* font, const std::string& text, glm::uvec2 screenPos);

	virtual void PreRender() override;
	void Create(VkDescriptorSet descSet);

	void Render(CGraphicPipeline* pipeline) override;

	friend class CUIManager;
	friend class CUIRenderer;
private:
	struct UITextGlyph
	{
		glm::vec4	TextCoords; //uv - start uv from Font texture, st - delta uv of the glyph (width, height ) / font_texture_size.xy
		glm::vec4	ScreenPosition;  //xy - screen space start pos of the glyph in pixels, zw glyph height
	};

	glm::uvec2      m_screenPosInPixels;
	std::string     m_text;

	CFont2*         m_font;
	Mesh*           m_textMesh;
	uint32_t		m_charactersCapacity;

	VkDescriptorSet m_descriptorSet;

	BufferHandle*	m_shaderParameters;
	bool			m_isDirty;
	uint32_t		m_padding;
};


class DebugBoundingBox : public CUIItem
{
public:
	DebugBoundingBox(const BoundingBox3D& bb, const glm::vec4& color);
	virtual ~DebugBoundingBox(){}

	const BoundingBox3D&	GetBoundingBox() const { return m_boundingBox; }
	const glm::vec4&		GetColor() const { return m_color; }

protected:
	void Render(CGraphicPipeline* pipeline) override {} //not used. We render in batch this item
	void PreRender() override{};
private:
	BoundingBox3D		m_boundingBox;
	glm::vec4			m_color;
};

class CUIRenderer : public CRenderer
{
public:
    CUIRenderer(VkRenderPass renderPass);
    virtual ~CUIRenderer();
	
	virtual void Init() override;
    virtual void Render() override;
	virtual void PreRender() override;
    
	void SetFont(CFont2* font);
private:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets);

	void AddNewDescriptorPool(std::vector<std::pair<DescriptorSetLayout*, uint32_t>> layoutsNumber);

    void AddUIText(CUIText* item);
    void RemoveUIText(CUIText* item);

	void AddDebugBoundingBox(DebugBoundingBox* bb);
	void RemoveDebugBoundingBox(DebugBoundingBox* bb);

	void UpdateGraphicInterface() override;

	VkDescriptorSet AllocElementDescriptorSet(const DescriptorSetLayout& layout);
	void CreateBBMesh();

    friend class CUIManager;
private:
	struct PushConstant
	{
		glm::mat4 ProjViewMatrix;
	} m_constants;
private:
	CGraphicPipeline				m_vectorElemPipeline;
	CGraphicPipeline				m_textElemPipeline;

    glm::mat4                       m_projMatrix;

	DescriptorSetLayout				m_textElemDescriptorLayout;
	DescriptorSetLayout				m_globalsDescSetLayout;;
	VkDescriptorSet					m_globalsDescriptorSet;
	BufferHandle*					m_globalsBuffer;

	std::vector<DescriptorPool*>	m_uiElemDescriptorPool;

	VkSampler						m_sampler;
    CFont2*                         m_usedFont;

	std::vector<CUIText*>			m_uiTexts;

	//debug bounding boxes
	DescriptorSetLayout				m_bbDescriptorLayout;
	VkDescriptorSet					m_bbDescriptorSet;
	CGraphicPipeline				m_debugBBPipeline;

	BufferHandle*					m_debugBBBuffer;
	Mesh*							m_boundingBoxMesh;
	std::vector<DebugBoundingBox*>	m_debugBoundingBoxes;
	uint32_t						m_visibleBb;
};

class CUIManager : public Singleton<CUIManager>
{
	friend class Singleton<CUIManager>;
public:
    CUIText* CreateTextItem(const std::string& text, glm::uvec2 screenPos);
	void DestroyTextItem(CUIText* item);

	DebugBoundingBox* CreateDebugBoundingBox(const BoundingBox3D& bb, glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
	void DestroyDebugBoundingBox(DebugBoundingBox* bb);

    void SetupRenderer(CUIRenderer* uiRenderer);

    void Update();

    void ToggleDisplayInfo();
private:
	CUIManager();
	virtual ~CUIManager();

    void ShowUIItems();
    void AddDisplayInfo();
private:
    CFont2*         m_font;
    CUIRenderer*    m_uiRenderer;

    std::vector<CUIItem*> m_uiItems;
	std::vector<DebugBoundingBox*>		m_debugBBToAdd;
	std::vector<DebugBoundingBox*>		m_debugBBToRemove;

    enum EDisplayInfo
    {
        EWidget_Reload = 0,
        EWidget_CenterMouse,
        EWidget_PointLight,
        EWidget_Objects,
        //EWidget_ScreenShot,
        EWidget_Camera,
        EWidget_Light,
        EWidget_DisplayInfo,
        EWidget_Count,
        EWidget_Start = EWidget_Reload,
        EWidget_MaxChars = 48
    };

   // CUITextContainer*       m_displayInfo;

    CDirtyValue<bool>       m_showUi;
};