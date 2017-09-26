#pragma once

#include "VulkanLoader.h"
#include "rapidxml/rapidxml.hpp"
#include "defines.h"
#include "Texture.h"
#include "glm/glm.hpp"
#include "Renderer.h"
#include "Mesh.h"

#include <unordered_map>
#include <string>
#include <array>

template<typename T>
class CDirtyValue
{
public:
    T c;
    T p;
    
    CDirtyValue() 
        : c(0)
        , p(0)
        , m_forceDirty(true)
    {
    }

    bool IsDirty() { return m_forceDirty || (c != p); }
    void Set(T value) { p = c; c = value; }
    void Reset() { p = c; m_forceDirty = false; }
    void SetDirty() { m_forceDirty = true; }
private:
    bool m_forceDirty;
};

typedef rapidxml::xml_document<char>  TXmlDocument;
typedef rapidxml::xml_node<char>      TXmlNode;
typedef rapidxml::xml_attribute<char> TXmlAttribute;

class CTexture;
struct SFontMetadata
{
    std::string     Name;
    float           TextWidth;
    float           TextHeight;
    float           GlyphSize;
    unsigned int    CharsCount;
    glm::vec2       TexelSize;
};

class CFont2;
class CFontImporter
{
public:
    CFontImporter(std::string fntXml);
    virtual ~CFontImporter();

    CFont2* GetFont();
    void BuildFont();

private:
    void GatherPointsOfInterests(TXmlDocument* doc);
    void GatherMetadata();
    void ReadXmlFile(const std::string& xml, char** fileContent);
    void LoadFontTexture();
    void GetGlyphs();
    void ParseGlyphs(TXmlNode* charNode);
private:
    struct xmlPOI
    {
        xmlPOI()
        {
            charsNode = commonNode = infoNode = pagesNode = nullptr;
        }

        TXmlNode* charsNode;
        TXmlNode* infoNode;
        TXmlNode* commonNode;
        TXmlNode* pagesNode;
    };

    xmlPOI          m_xmlPointOfInterests;

    CFont2*          m_importedFont;
    std::string     m_fntXml;
    char*           m_xmlContent;
    SFontMetadata   m_fontMetadata;
    CTexture*       m_fontTexture;

};

struct SGlyph
{
    unsigned char   Id;
    glm::vec2       Size; //in pixels
    glm::vec2       TopLeftUV;
    glm::vec2       TopRightUV;
    glm::vec2       BotRightUV;
    glm::vec2       BotLeftUV;
};

class CFont2
{
public:
    typedef std::vector<SVertex> UIString;
    virtual ~CFont2();

    float GetGlyphPixelsSize() const { return m_metaData.GlyphSize; }
    void GetUIString(const std::string& text, UIString& uiString);

    VkImageView  GetFontImageView() const { return m_fontTexture->GetImageView(); }
private:
    CFont2();
    
    void SetMetadata(const SFontMetadata& metadata) { m_metaData = metadata;};
    void SetFontTexture(CTexture* text) { TRAP(text); m_fontTexture = text;}
    void AddGlyph(unsigned char id, glm::vec2 pos, glm::vec2 size);
    SGlyph& GetGlyph(unsigned char c);

    void Validate();

    friend class CFontImporter;
private:
    SFontMetadata                               m_metaData;
    CTexture*                                   m_fontTexture;

    std::unordered_map<unsigned char, SGlyph>   m_glyphs;
};

class Mesh;

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
    virtual void Render() = 0;
    virtual void Update() = 0;
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
private:
    CUIText(CFont2* font, const std::string& text, glm::uvec2 screenPos, unsigned int maxChar);

    void Update() override;

    void CreateMesh();
    void UpdateMesh();

    void Render() override;

    friend class CUIManager;
    friend class CUIRenderer;
private:
    glm::uvec2      m_screenPosInPixels;
    std::string     m_text;

    CFont2*         m_font;
    Mesh*           m_textMesh;

    unsigned int    m_maxCharPerString;

    bool            m_isDirty;
    
};

class CUITextContainer : public CUIItem
{
public:
    CUIText* GetTextItem(unsigned int index);
    void SetTextItem(unsigned int index, const std::string& text);

    virtual ~CUITextContainer();
private:
    CUITextContainer(std::vector<CUIText*>& texts);

    void Update() override;
    void Render() override;

    friend class CUIManager;
private:
    std::vector<CUIText*>   m_textItems;
};

class CUIVector : public CUIItem
{
public:
    virtual ~CUIVector();

    void SetColor(glm::vec4 color);
    void SetPosition(glm::vec3 position);
    void SetVector(glm::vec3 dir);
private:
    CUIVector(glm::vec3 pos, glm::vec3 vector, glm::vec4 color);

    virtual void Render() override;
    virtual void Update() override;

    unsigned char GetColorComponent(float component);
    friend class CUIManager;
    friend class CUIRenderer;
private:
    glm::vec3       m_position;
    glm::vec3       m_vector;
    glm::vec4       m_color;

    Mesh*           m_mesh;

    unsigned int    m_vertexCount;

    bool            m_dirty;
};

class CUIAxisSystem : public CUIItem
{
public:
    virtual ~CUIAxisSystem();

    void SetPosition(glm::vec3 pos);
private:
    CUIAxisSystem(CUIVector* x, CUIVector* y, CUIVector* z);

    virtual void Render() override;
    virtual void Update() override;
    friend class CUIManager;
    friend class CUIRenderer;
private:
    CUIVector*      m_xAxis;
    CUIVector*      m_yAxis;
    CUIVector*      m_zAxis;
};

class CUIRenderer : public CRenderer
{
public:
    CUIRenderer(VkRenderPass renderPass);
    virtual ~CUIRenderer();

    virtual void Render() override;
    void SetFont(CFont2* font);
    
    virtual void Init() override;
private:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets);

    void AddUIText(CUIText* item);
    void RemoveUIText(CUIText* item);

    void AddUIVector(CUIVector* item);
    void RemoveUIVector(CUIVector* item);

    friend class CUIManager;
private:
    void UpdateUIItemsDescSet();
    void UpdateCommonDescSet();
    void UpdateCommonParams();

private:
    struct CommonShaderParams
    {
        glm::vec4 ScreenSize;
        glm::mat4 ProjViewMatrix;
    };

    struct UiItemsShaderParams
    {
        glm::vec4 ScreenPosition;

        bool IsDirty(glm::uvec2& other)
        {
            return ScreenPosition.x != (float)other.x || ScreenPosition.y != (float)other.y;
        }
    };

    struct UINode
    {
        CUIText*                uiItem;
        UiItemsShaderParams     uiParams;
        uint64_t                buffOffset;
        VkDescriptorSet         descSet;

        UINode() 
            : uiItem(nullptr)
        {}

        void Invalidate() { uiItem = nullptr; }
        bool IsValid() const { return uiItem != nullptr; }
    };

    VkDescriptorSetLayout           m_uiItemDescLayout;
    VkDescriptorSetLayout           m_commonDescSetLayout;
    std::vector<VkDescriptorSet>    m_uiItemDescSet;
    VkDescriptorSet                 m_commonDescSet;
    VkSampler                       m_sampler;

    VkBuffer                        m_uiItemUniformBuffer;
    VkDeviceMemory                  m_uiItemUniformMemory;

    VkBuffer                        m_commonUniformBuffer;
    VkDeviceMemory                  m_commonUniformMemory;

    std::array<UINode, MAXUIITEMS>  m_uiNodes;
    CGraphicPipeline                       m_textElemPipeline;

    std::vector<CUIVector*>         m_uiVectors;            
    CGraphicPipeline                       m_vectorElemPipeline;

    glm::mat4                       m_projMatrix;

    CFont2*                         m_usedFont;
    bool                            m_needUpdateCommon;
};

class CUIManager
{
public:
    CUIManager();
    virtual ~CUIManager();

    CUIText* CreateTextItem(const std::string& text, glm::uvec2 screenPos, unsigned int maxChars = 16);
    CUIVector* CreateVectorItem(glm::vec3 position, glm::vec3 vector, glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    CUIAxisSystem* CreateAxisSystemItem(glm::vec3 position, glm::vec3 x, glm::vec3 y, glm::vec3 z);
    CUITextContainer* CreateTextContainerItem(std::vector<std::string>& texts, glm::uvec2 screenStartPos, unsigned int yPadding, unsigned int maxChars = 16 );

    void SetupRenderer(CUIRenderer* uiRenderer);

    void Update();

    void ToggleDisplayInfo();
private:
    void ShowUIItems();
    void AddDisplayInfo();
private:
    CFont2*         m_font;
    CUIRenderer*    m_uiRenderer;

    std::vector<CUIItem*> m_uiItems;

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

    CUITextContainer*       m_displayInfo;

    CDirtyValue<bool>       m_showUi;
};