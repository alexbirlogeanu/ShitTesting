#include "UI.h"

#include "glm/gtc/constants.hpp"

#include <iostream>
#include <cstdlib>
#include <utility>
#include <vector>
#include "Mesh.h"
#include "Utils.h"
#include "glm/gtc/matrix_transform.hpp"

#define FONT_DIR "fonts\\"

//CCamera ms_camera;

//////////////////////////////////////////////////////////////////////////
//CFontImporter
//////////////////////////////////////////////////////////////////////////

CFontImporter::CFontImporter(std::string fntXml)
    : m_fntXml(fntXml)
    , m_importedFont(nullptr)
    , m_xmlContent(nullptr)
    , m_fontTexture(nullptr)
{
}

CFontImporter::~CFontImporter()
{
    delete [] m_xmlContent;
}

CFont2* CFontImporter::GetFont()
{
    TRAP(m_importedFont);
    return m_importedFont;
}

void CFontImporter::BuildFont()
{
    m_importedFont = new CFont2();

    std::string finalFntXml = FONT_DIR + m_fntXml;
    ReadXmlFile(finalFntXml, &m_xmlContent);
    TXmlDocument docoment;
    docoment.parse<rapidxml::parse_default>(m_xmlContent);
    GatherPointsOfInterests(&docoment);
    GatherMetadata();
    LoadFontTexture();
    GetGlyphs(); //font should have the metadata set
}


void CFontImporter::GatherMetadata()
{
    TXmlNode* infoNode = m_xmlPointOfInterests.infoNode;
    TXmlNode* commonNode = m_xmlPointOfInterests.commonNode;
    TXmlNode* charsNode = m_xmlPointOfInterests.charsNode;
    TRAP(infoNode != nullptr && commonNode != nullptr && charsNode != nullptr);

    TXmlAttribute* fontName = infoNode->first_attribute("face", 0, false);
    TRAP(fontName);
    m_fontMetadata.Name = fontName->value();

    TXmlAttribute* textWidth = commonNode->first_attribute("scaleW", 0, false);
    TXmlAttribute* textHeight = commonNode->first_attribute("scaleH", 0, false);
    TRAP(textHeight && textWidth);

    m_fontMetadata.TextWidth = (float)std::atoi(textWidth->value());
    m_fontMetadata.TextHeight = (float)std::atoi(textHeight->value());
    m_fontMetadata.TexelSize = glm::vec2(1.0f, 1.0f) / glm::vec2(m_fontMetadata.TextWidth, m_fontMetadata.TextHeight);

    TXmlAttribute* glyphWH = commonNode->first_attribute("lineHeight", 0, false); //WH = widthHeight
    TRAP(glyphWH);
    m_fontMetadata.GlyphSize = (float)std::atoi(glyphWH->value());

    TXmlAttribute* charCount = charsNode->first_attribute("count", 0, false);
    TRAP(charCount);
    m_fontMetadata.CharsCount = std::atoi(charCount->value());
    
    //check metadata integrity
    TRAP(!m_fontMetadata.Name.empty());
    TRAP(m_fontMetadata.TextHeight > 0 && m_fontMetadata.TextWidth > 0);
    TRAP(m_fontMetadata.CharsCount > 0);

    m_importedFont->SetMetadata(m_fontMetadata);
}

void CFontImporter::GatherPointsOfInterests(TXmlDocument* doc)
{
    TXmlNode* fontNode = doc->first_node("font", 0, false);
    TRAP(fontNode);

    m_xmlPointOfInterests.infoNode = fontNode->first_node("info", 0, false);
    m_xmlPointOfInterests.commonNode = fontNode->first_node("common", 0, false);
    m_xmlPointOfInterests.pagesNode = fontNode->first_node("pages", 0, false);
    m_xmlPointOfInterests.charsNode = fontNode->first_node("chars", 0, false);
}

void CFontImporter::LoadFontTexture()
{
    TXmlNode* pagesNode = m_xmlPointOfInterests.pagesNode;
    TRAP(pagesNode);
    //get just first page. export font only in one texture
    TXmlNode* pageNode = pagesNode->first_node();
    TRAP(pageNode);
    TXmlAttribute* file = pageNode->first_attribute("file", 0, false);
    TRAP(file);
    std::string fileName = file->value();
    fileName = FONT_DIR + fileName;
    TRAP(!fileName.empty());
    SImageData imgData;
    Read2DTextureData(imgData, fileName, false);
    m_fontTexture = new CTexture(imgData, true);
    m_importedFont->SetFontTexture(m_fontTexture);
}

 void CFontImporter::GetGlyphs()
 {
    TXmlNode* charsNode = m_xmlPointOfInterests.charsNode;
    TRAP(charsNode); 
    TRAP(m_importedFont);
    for(TXmlNode* curNode = charsNode->first_node(); curNode; curNode = curNode->next_sibling())
        ParseGlyphs(curNode);

    m_importedFont->Validate();
 }

void CFontImporter::ParseGlyphs(TXmlNode* charNode)
{
    unsigned char uID;
    unsigned int uX, uY;
    unsigned int uWidth, uHeight;

    TXmlAttribute* id = charNode->first_attribute("id", 0, false);
    TRAP(id);
    uID = std::atoi(id->value());

    TXmlAttribute* x = charNode->first_attribute("x");
    TRAP(x);
    uX = std::atoi(x->value());

    TXmlAttribute* y = charNode->first_attribute("y");
    TRAP(y);
    uY = std::atoi(y->value());

    TXmlAttribute* width = charNode->first_attribute("width");
    TRAP(width);
    uWidth = std::atoi(width->value());

    TXmlAttribute* height = charNode->first_attribute("height");
    TRAP(height);
    uHeight = std::atoi(height->value());

    m_importedFont->AddGlyph( uID, glm::vec2(uX, uY), glm::vec2(uWidth, uHeight));
}

//////////////////////////////////////////////////////////////////////////
//CFont2
//////////////////////////////////////////////////////////////////////////
CFont2::CFont2()
    : m_fontTexture(nullptr)
{
}

CFont2::~CFont2()
{
    delete m_fontTexture;
}

void CFont2::GetUIString(const std::string& text, UIString& uiString)
{
    uiString.reserve(text.size() * 4);
    float xOffset = 0;

    for(unsigned int i = 0; i < text.size(); ++i)
    {
        SGlyph glyph = GetGlyph(text[i]);
        SVertex topLeft (glm::vec3(xOffset, 0.0, 0.0f), glyph.TopLeftUV);
        SVertex topRight (glm::vec3(xOffset + glyph.Size.x, 0.0f, 0.0f ), glyph.TopRightUV);
        SVertex botRight (glm::vec3(xOffset + glyph.Size.x, glyph.Size.y, 0.0f), glyph.BotRightUV);
        SVertex botLeft (glm::vec3(xOffset, glyph.Size.y, 0.0f), glyph.BotLeftUV);

        uiString.push_back(topLeft);
        uiString.push_back(topRight);
        uiString.push_back(botRight);
        uiString.push_back(botLeft);

        xOffset += glyph.Size.x;
    }
}

void CFont2::AddGlyph(unsigned char id, glm::vec2 pos, glm::vec2 size)
{
    TRAP(m_metaData.TexelSize.x > glm::epsilon<float>() && m_metaData.TexelSize.y > glm::epsilon<float>())
    auto it = m_glyphs.find(id);
    if (it != m_glyphs.end())
        return;

    glm::vec2 texelSize = m_metaData.TexelSize;
    //pos = glm::vec2(pos.x, 1.f - pos.y);

    SGlyph glyph;
    glyph.Id = id;
    glyph.Size = size;
    glyph.TopLeftUV = pos * glm::vec2(texelSize.x, -texelSize.y) + glm::vec2(0.f, 1.f);
    glyph.TopRightUV = (pos + glm::vec2(size.x, 0.f)) * glm::vec2(texelSize.x, -texelSize.y) + glm::vec2(0.f, 1.f);
    glyph.BotRightUV = (pos + size) * glm::vec2(texelSize.x, -texelSize.y) + glm::vec2(0.f, 1.f);
    glyph.BotLeftUV = (pos + glm::vec2(0.f, size.y)) * glm::vec2(texelSize.x, -texelSize.y) + glm::vec2(0.f, 1.f);

    m_glyphs.insert(std::pair<unsigned char, SGlyph>(id, glyph));
}

 SGlyph& CFont2::GetGlyph(unsigned char c)
 {
    auto it = m_glyphs.find(c);
    TRAP(it != m_glyphs.end());
    return it->second;
 }

void CFont2::Validate()
{
    TRAP(m_glyphs.size() == m_metaData.CharsCount);
}

//////////////////////////////////////////////////////////////////////////
//CUIText
//////////////////////////////////////////////////////////////////////////
CUIText::CUIText(CFont2* font, const std::string& text, glm::uvec2 screenPos, unsigned int maxChars)
    : m_font(font)
    , m_text(text)
    , m_screenPosInPixels(screenPos)
    , m_maxCharPerString(maxChars)
    , m_textMesh(nullptr)
    , m_isDirty(true)
{
}

CUIText::~CUIText()
{
    delete m_textMesh;
}

void CUIText::Update()
{
    if(m_isDirty)
    {
        if (!m_textMesh)
            //CreateMesh();

        UpdateMesh();
        m_isDirty = false;
    }
}

void CUIText::SetText(const std::string& text)
{
    TRAP(text.size() < m_maxCharPerString);
    if (m_text != text)
    {
        m_isDirty = true;
        m_text = text;
    }
}

void CUIText::SetPosition(glm::uvec2& pos)
{
    m_screenPosInPixels = pos;
    m_isDirty = true;
}

void CUIText::CreateMesh()
{
    TRAP(m_maxCharPerString);
    float glyphSize = m_font->GetGlyphPixelsSize();
    
    std::vector<SVertex> vertices;
    std::vector<unsigned int> indices;
    vertices.resize(m_maxCharPerString * 4); //4 vertexes/letter
    indices.reserve(m_maxCharPerString * 6); //6 indices/ letter

    for(unsigned int i = 0; i < m_maxCharPerString; ++i)
    {
        unsigned int offset = i * 4;

        //SVertex topLeft ;//(glm::vec3(i * glyphSize, 0.0, 0.0f));
        //SVertex topRight ;//(glm::vec3((i + 1) * glyphSize, 0.0f, 0.0f ));
        //SVertex botRight; //(glm::vec3((i + 1) * glyphSize, glyphSize, 0.0f));
        //SVertex botLeft;// (glm::vec3(i * glyphSize, glyphSize, 0.0f));

        //vertices.push_back(topLeft);
        //vertices.push_back(topRight);
        //vertices.push_back(botRight);
        //vertices.push_back(botLeft);

        //Face 1 : TL - BL - BR; Face2: BR - TR - TL CCW
        indices.push_back(offset);
        indices.push_back(offset + 3);
        indices.push_back(offset + 2);
        indices.push_back(offset + 2);
        indices.push_back(offset + 1);
        indices.push_back(offset);
    }

    m_textMesh = new Mesh(vertices, indices);

}

void CUIText::UpdateMesh()
{
    //TRAP(m_textMesh);
    //unsigned int textSize = (unsigned int)m_text.size();
    //if(textSize == 0)
    //    return;
    //VkDeviceMemory vertexMemory = m_textMesh->GetVertexMemory();
    //void* bufMemory;
    //vk::MapMemory(vk::g_vulkanContext.m_device, vertexMemory, 0, VK_WHOLE_SIZE, 0, &bufMemory);

    //CFont2::UIString uiString;
    //m_font->GetUIString(m_text, uiString);
    //memcpy(bufMemory, &uiString[0], uiString.size() * sizeof(SVertex));

    //vk::UnmapMemory(vk::g_vulkanContext.m_device, vertexMemory);
}

void CUIText::Render()
{
    if(m_text.empty())
        return;

    TRAP(m_textMesh);
    m_textMesh->Render((unsigned int)m_text.size() * 6);
}
//////////////////////////////////////////////////////////////////////////
//CUITextContainer
//////////////////////////////////////////////////////////////////////////
CUITextContainer::CUITextContainer(std::vector<CUIText*>& texts)
    : CUIItem()
    , m_textItems(texts)
{
}

CUITextContainer::~CUITextContainer()
{
}

CUIText* CUITextContainer::GetTextItem(unsigned int index)
{
    TRAP(index < m_textItems.size());
    return m_textItems[index];
}

void CUITextContainer::SetTextItem(unsigned int index, const std::string& text)
{
    TRAP(index < m_textItems.size());
    m_textItems[index]->SetText(text);
}


void CUITextContainer::Update()
{
    for(unsigned int i = 0; i < m_textItems.size(); ++i)
        m_textItems[i]->SetVisible(GetVisible());
}

void CUITextContainer::Render()
{
}


//////////////////////////////////////////////////////////////////////////
//CUIVector
//////////////////////////////////////////////////////////////////////////
CUIVector::CUIVector(glm::vec3 pos, glm::vec3 vector, glm::vec4 color)
    : m_position(pos)
    , m_vector(vector)
    , m_color(color)
    , m_dirty(true)
    , m_mesh(nullptr)
    , m_vertexCount(2)
{
    std::vector<SVertex> vertices;
    vertices.resize(m_vertexCount);
    std::vector<unsigned int> indexes;
    indexes.reserve(m_vertexCount);
    for(unsigned int i =0; i < m_vertexCount; ++i)
        indexes.push_back(i);

    //m_mesh = new Mesh(vertices, indexes);
}

CUIVector::~CUIVector()
{
    delete m_mesh;
}

void CUIVector::Render()
{
	if (m_mesh)
		m_mesh->Render();
}

void CUIVector::Update()
{
    //if(m_dirty)
    //{
    //    VkDeviceMemory vertexMemory = m_mesh->GetVertexMemory();
    //    void* bufMemory;
    //    vk::MapMemory(vk::g_vulkanContext.m_device, vertexMemory, 0, VK_WHOLE_SIZE, 0, &bufMemory);
    //    SVertex* p = (SVertex*)bufMemory;
    //    p[0].pos = m_position;
    //    p[1].pos = m_position + m_vector;

    //    for(unsigned int i = 0; i < m_vertexCount; ++i, ++p)
    //        p->SetColor(GetColorComponent(m_color.x), GetColorComponent(m_color.y), GetColorComponent(m_color.z), GetColorComponent(m_color.w));

    //    vk::UnmapMemory(vk::g_vulkanContext.m_device, vertexMemory);

    //    m_dirty = false;
    //}
}

void CUIVector::SetColor(glm::vec4 color)
{
    m_color = color;
    m_dirty = true;
}

void CUIVector::SetPosition(glm::vec3 position)
{
    m_position = position;
    m_dirty = true;
}

void CUIVector::SetVector(glm::vec3 dir)
{
    m_vector = dir;
    m_dirty = true;
}
    
unsigned char CUIVector::GetColorComponent(float component)
{
    return (unsigned char)(glm::clamp(component, 0.0f, 1.0f) * 255.0f);
}
//////////////////////////////////////////////////////////////////////////
//CUIAxisSystem
//////////////////////////////////////////////////////////////////////////
CUIAxisSystem::CUIAxisSystem(CUIVector* x, CUIVector* y, CUIVector* z)
    : m_xAxis(x)
    , m_yAxis(y)
    , m_zAxis(z)
{

}

CUIAxisSystem::~CUIAxisSystem()
{
}

void CUIAxisSystem::SetPosition(glm::vec3 pos)
{
    m_xAxis->SetPosition(pos);
    m_yAxis->SetPosition(pos);
    m_zAxis->SetPosition(pos);
}

void CUIAxisSystem::Render()
{
}

void CUIAxisSystem::Update()
{
    m_xAxis->SetVisible(GetVisible());
    m_yAxis->SetVisible(GetVisible());
    m_zAxis->SetVisible(GetVisible());
}
//////////////////////////////////////////////////////////////////////////
//CUIRenderer
//////////////////////////////////////////////////////////////////////////

CUIRenderer::CUIRenderer(VkRenderPass renderPass)
    : CRenderer(renderPass, "UIRenderPass")
    , m_sampler(VK_NULL_HANDLE)
    , m_uiItemDescLayout(VK_NULL_HANDLE)
    , m_uiItemUniformBuffer(nullptr)
    , m_commonUniformBuffer(nullptr)
    , m_commonDescSetLayout(VK_NULL_HANDLE)
    , m_commonDescSet(VK_NULL_HANDLE)
    , m_usedFont(nullptr)
    , m_needUpdateCommon(true)
{
}

CUIRenderer::~CUIRenderer()
{
	MemoryManager::GetInstance()->FreeHandle(m_commonUniformBuffer->GetRootParent());
}

void CUIRenderer::PreRender()
{
	if (m_needUpdateCommon)
		UpdateCommonDescSet();

	UpdateCommonParams();
	{
		for (unsigned int i = 0; i < m_uiNodes.size(); ++i)
		{
			if (m_uiNodes[i].IsValid())
			{
				glm::uvec2 pos = m_uiNodes[i].uiItem->GetPosition();
				UiItemsShaderParams& params = m_uiNodes[i].uiParams;
				if (params.IsDirty(pos))
				{
					params.ScreenPosition.x = (float)pos.x;
					params.ScreenPosition.y = (float)pos.y;

					memcpy(m_uiNodes[i].buffer->GetPtr<UiItemsShaderParams*>(), &params, sizeof(UiItemsShaderParams));
				}
			}
		}
	}
}

void CUIRenderer::Render()
{
    StartRenderPass();
    VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
    vk::CmdBindPipeline(cmdBuffer, m_textElemPipeline.GetBindPoint(), m_textElemPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuffer, m_textElemPipeline.GetBindPoint(), m_textElemPipeline.GetLayout(), 0, 1, &m_commonDescSet, 0, nullptr);

     for(unsigned int i = 0; i < m_uiNodes.size(); ++i)
     {
         if(m_uiNodes[i].IsValid() && m_uiNodes[i].uiItem->GetVisible())
         {
            vk::CmdBindDescriptorSets(cmdBuffer, m_textElemPipeline.GetBindPoint(), m_textElemPipeline.GetLayout(), 1, 1, &m_uiNodes[i].descSet, 0, nullptr);
            m_uiNodes[i].uiItem->Render();
         }
     }

     vk::CmdBindPipeline(cmdBuffer, m_vectorElemPipeline.GetBindPoint(), m_vectorElemPipeline.Get());
     vk::CmdBindDescriptorSets(cmdBuffer, m_vectorElemPipeline.GetBindPoint(), m_vectorElemPipeline.GetLayout(), 0, 1, &m_commonDescSet, 0, nullptr);

     for(unsigned int i = 0; i < m_uiVectors.size(); ++i)
         if(m_uiVectors[i]->GetVisible())
            m_uiVectors[i]->Render();

    EndRenderPass();
}

void CUIRenderer::SetFont(CFont2* font)
{
    if(font == m_usedFont)
        return;

    TRAP(font);
    m_usedFont = font;
    m_needUpdateCommon = true;
}

void CUIRenderer::AddUIText(CUIText* item)
{
    unsigned int validPos = -1;

    for(unsigned i = 0; i < m_uiNodes.size(); ++i)
    {
        if(!m_uiNodes[i].IsValid())
            validPos = i;
        else
        {
            if(m_uiNodes[i].uiItem == item) //the item is already added
                return;
        }
    }

    TRAP(validPos != -1);
    if(validPos == -1)
        return;

    m_uiNodes[validPos].uiItem = item;
}

void CUIRenderer::RemoveUIText(CUIText* item)
{
    auto it = std::find_if(m_uiNodes.begin(), m_uiNodes.end(), [item](const UINode& node) { return node.uiItem == item; });
    if (it != m_uiNodes.end())
    {
        it->Invalidate();
    }
}

void CUIRenderer::AddUIVector(CUIVector* item)
{
    auto it = std::find(m_uiVectors.begin(), m_uiVectors.end(), item);
    if(it == m_uiVectors.end())
        m_uiVectors.push_back(item);
}

void CUIRenderer::RemoveUIVector(CUIVector* item)
{
    auto it = std::find(m_uiVectors.begin(), m_uiVectors.end(), item);
    if(it != m_uiVectors.end())
        m_uiVectors.erase(it);
}

void CUIRenderer::CreateDescriptorSetLayout()
{
    {
        std::vector<VkDescriptorSetLayoutBinding> commonDescBindings;//for set 0
        commonDescBindings.resize(2);
        commonDescBindings[0] = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        commonDescBindings[1] = CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

        VkDescriptorSetLayoutCreateInfo commonDescLayout;
        cleanStructure(commonDescLayout);
        commonDescLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        commonDescLayout.pNext = nullptr;
        //flags
        commonDescLayout.bindingCount = (uint32_t)commonDescBindings.size();
        commonDescLayout.pBindings = commonDescBindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &commonDescLayout, nullptr, &m_commonDescSetLayout));
    }
    {
        //for set 1
        std::vector<VkDescriptorSetLayoutBinding> uiItemBindings;
        uiItemBindings.resize(1);
        uiItemBindings[0] = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

        VkDescriptorSetLayoutCreateInfo uiItemDescLayout;
        cleanStructure(uiItemDescLayout);
        uiItemDescLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        uiItemDescLayout.pNext = nullptr;
        //flags
        uiItemDescLayout.bindingCount = (uint32_t)uiItemBindings.size();
        uiItemDescLayout.pBindings = uiItemBindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &uiItemDescLayout, nullptr, &m_uiItemDescLayout));
    }
}

void CUIRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    maxSets = MAXUIITEMS + 1;
    
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAXUIITEMS);
}

void CUIRenderer::Init()
{
    CRenderer::Init();
    AllocDescriptorSets(m_descriptorPool, m_commonDescSetLayout, &m_commonDescSet);

    {
        std::vector<VkDescriptorSetLayout> layouts (MAXUIITEMS, m_uiItemDescLayout);
        m_uiItemDescSet.resize(MAXUIITEMS);
        AllocDescriptorSets(m_descriptorPool, layouts, m_uiItemDescSet);

    }
	uint64_t memAllign = vk::g_vulkanContext.m_limits.minUniformBufferOffsetAlignment;
	std::vector<VkDeviceSize> sizes(MAXUIITEMS, memAllign);
	sizes.push_back(sizeof(CommonShaderParams));
	BufferHandle* memoryBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	m_commonUniformBuffer = memoryBuffer->CreateSubbuffer(sizeof(CommonShaderParams));
	m_uiItemUniformBuffer = memoryBuffer->CreateSubbuffer(uint32_t(MAXUIITEMS * memAllign));

    CreateNearestSampler(m_sampler);
    UpdateUIItemsDescSet();

    m_textElemPipeline.SetVertexShaderFile("ui2d.vert");
    m_textElemPipeline.SetFragmentShaderFile("ui2d.frag");
    m_textElemPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_textElemPipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_textElemPipeline.SetDepthTest(false);
    m_textElemPipeline.SetDepthWrite(false);

    VkPipelineColorBlendAttachmentState blendState;
    blendState.blendEnable = VK_FALSE;
    blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendState.colorBlendOp =  VK_BLEND_OP_ADD;
    blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendState.alphaBlendOp = VK_BLEND_OP_ADD;
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;

    m_textElemPipeline.AddBlendState(blendState);
    std::vector<VkDescriptorSetLayout> layouts;
    layouts.push_back(m_commonDescSetLayout);
    layouts.push_back(m_uiItemDescLayout);

    m_textElemPipeline.CreatePipelineLayout(layouts);
    m_textElemPipeline.Init(this, m_renderPass, 0);

    PerspectiveMatrix(m_projMatrix);
    ConvertToProjMatrix(m_projMatrix);

    m_vectorElemPipeline.SetVertexShaderFile("uivector3d.vert");
    m_vectorElemPipeline.SetFragmentShaderFile("uivector3d.frag");
    m_vectorElemPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_vectorElemPipeline.SetDepthTest(true);
    m_vectorElemPipeline.SetDepthWrite(false);
    m_vectorElemPipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
    m_vectorElemPipeline.SetLineWidth(1.5f);
    m_vectorElemPipeline.AddBlendState(blendState);
    m_vectorElemPipeline.CreatePipelineLayout(m_commonDescSetLayout);
    m_vectorElemPipeline.Init(this, m_renderPass, 0);
}

void CUIRenderer::UpdateUIItemsDescSet()
{
    uint64_t memAllign = vk::g_vulkanContext.m_limits.minUniformBufferOffsetAlignment;
    std::array<VkDescriptorBufferInfo, MAXUIITEMS> wBufferInfo;
    //VkWriteDescriptorSet
    std::array<VkWriteDescriptorSet, MAXUIITEMS> wDescSets;

    for(unsigned int i = 0; i < m_uiNodes.size(); ++i)
    {
        m_uiNodes[i].descSet = m_uiItemDescSet[i];
		m_uiNodes[i].buffer = m_uiItemUniformBuffer->CreateSubbuffer(sizeof(UiItemsShaderParams));
		
		wBufferInfo[i] = m_uiNodes[i].buffer->GetDescriptor();
        wDescSets[i] = InitUpdateDescriptor( m_uiItemDescSet[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &wBufferInfo[i]);
    }

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDescSets.size(), wDescSets.data(), 0, nullptr);
}

 void CUIRenderer::UpdateCommonDescSet()
 {
     TRAP(m_needUpdateCommon);
     TRAP(m_usedFont);

	 VkDescriptorBufferInfo wBufferInfo = m_commonUniformBuffer->GetDescriptor();

     VkDescriptorImageInfo wImageInfo;
     cleanStructure(wImageInfo);
     wImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
     wImageInfo.imageView = m_usedFont->GetFontImageView();
     wImageInfo.sampler = m_sampler;

     VkWriteDescriptorSet wDescSet[2];
     wDescSet[0] = InitUpdateDescriptor(m_commonDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &wBufferInfo);
     wDescSet[1] = InitUpdateDescriptor(m_commonDescSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wImageInfo);

     vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, 2, &wDescSet[0], 0, nullptr);

    
     m_needUpdateCommon = false;
 }

void CUIRenderer::UpdateCommonParams()
{
    CommonShaderParams* params = m_commonUniformBuffer->GetPtr<CommonShaderParams*>();
    params->ScreenSize.x = (float)WIDTH;
    params->ScreenSize.y = (float)HEIGHT;
    params->ProjViewMatrix = m_projMatrix * ms_camera.GetViewMatrix();
}
//////////////////////////////////////////////////////////////////////////
//CUIManager
//////////////////////////////////////////////////////////////////////////
CUIManager::CUIManager()
    : m_font(nullptr)
    , m_uiRenderer(nullptr)
{
    CFontImporter fontImporter("kalinga12.fnt");
    fontImporter.BuildFont();

    m_showUi.Set(false);
    m_font = fontImporter.GetFont();
}

CUIManager::~CUIManager()
{
    for(unsigned int i = 0; i < m_uiItems.size(); ++i)
        delete m_uiItems[i];

    delete m_font;
}

CUIText* CUIManager::CreateTextItem(const std::string& text, glm::uvec2 screenPos, unsigned int maxChars )
{
    TRAP(m_uiRenderer);
    CUIText* item = new CUIText(m_font, text, screenPos, maxChars);
    m_uiRenderer->AddUIText(item);
    m_uiItems.push_back(item);

    return item;
}

CUIVector* CUIManager::CreateVectorItem(glm::vec3 position, glm::vec3 vector, glm::vec4 color )
{
    TRAP(m_uiRenderer);
    CUIVector* item = new CUIVector(position, vector, color);
    m_uiRenderer->AddUIVector(item);
    m_uiItems.push_back(item);

    return item;
}

CUIAxisSystem* CUIManager::CreateAxisSystemItem(glm::vec3 position, glm::vec3 x, glm::vec3 y, glm::vec3 z)
{
    CUIVector* xVector = CreateVectorItem(position, x, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    CUIVector* yVector = CreateVectorItem(position, y, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    CUIVector* zVector = CreateVectorItem(position, z, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    CUIAxisSystem* item = new CUIAxisSystem(xVector, yVector, zVector);
    m_uiItems.push_back(item);
    return item;
}

 CUITextContainer* CUIManager::CreateTextContainerItem(std::vector<std::string>& texts, glm::uvec2 screenStartPos, unsigned int yPadding, unsigned int maxChars)
 {
     unsigned int glyphSize =  (unsigned int)m_font->GetGlyphPixelsSize();
     glm::uvec2 position;
     std::vector<CUIText*> textsItems;
     textsItems.resize(texts.size());

     for(unsigned int i = 0; i < texts.size(); ++i)
     {
         position = screenStartPos + glm::uvec2(0, i * (glyphSize + yPadding));
         textsItems[i] = CreateTextItem(texts[i], position, maxChars);
     }

     CUITextContainer* container = new CUITextContainer(textsItems);
     m_uiItems.push_back(container);
     return container;
 }

void CUIManager::SetupRenderer(CUIRenderer* uiRenderer)
{
    TRAP(uiRenderer);
    TRAP(m_font);
    m_uiRenderer = uiRenderer;
    m_uiRenderer->SetFont(m_font);

    AddDisplayInfo();
}

void CUIManager::Update()
{
    ShowUIItems();
    for(unsigned int i = 0; i < m_uiItems.size(); ++i)
        m_uiItems[i]->Update();
}

void CUIManager::ToggleDisplayInfo()
{
    bool currValue = m_showUi.c;
    m_showUi.Set(!currValue);
}

void CUIManager::ShowUIItems()
{
    if(!m_showUi.IsDirty())
        return;

   m_displayInfo->SetVisible(m_showUi.c);

    /*for(unsigned int i = 0; i < m_uiItems.size(); ++i)
    m_uiItems[i]->SetVisible(m_showUi.c);*/

    m_showUi.Reset();
}

void CUIManager::AddDisplayInfo()
{
    glm::uvec2 startPos (0, 50);
    unsigned int yPadding = 5;
    std::vector<std::string> widgetStrings;
    widgetStrings.resize(EWidget_Count);

    widgetStrings[EWidget_Reload] = std::string("F1 - Reload shaders");
    widgetStrings[EWidget_CenterMouse] = std::string("F2 - Center cursor");
    widgetStrings[EWidget_PointLight] = std::string("F3 - Toggle Point light edit mode");
    widgetStrings[EWidget_Objects] = std::string("F4 - Toggle Objects edit mode");
    //widgetStrings[EWidget_ScreenShot] = std::string("F5 - Take screenshot");
    widgetStrings[EWidget_Camera] = std::string("Space - Reset camera");
    widgetStrings[EWidget_Light] = std::string("Mouse wheel - Rotate light");
    widgetStrings[EWidget_DisplayInfo] = std::string("TAB - Hide/show UI info");

    m_displayInfo = CreateTextContainerItem(widgetStrings, startPos, yPadding, EWidget_MaxChars);
}
