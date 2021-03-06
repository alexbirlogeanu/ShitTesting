#include "UI.h"

#include "glm/gtc/constants.hpp"

#include <iostream>
#include <cstdlib>
#include <utility>
#include <vector>
#include "Mesh.h"
#include "Utils.h"
#include "glm/gtc/matrix_transform.hpp"
#include "Font.h"
#include "MemoryManager.h"

//CCamera ms_camera;

struct UIGlobals
{
	glm::vec4 ScreenSize;
};

//////////////////////////////////////////////////////////////////////////
//CUIText
//////////////////////////////////////////////////////////////////////////
CUIText::CUIText(CFont2* font, const std::string& text, glm::uvec2 screenPos)
    : m_font(font)
    , m_text(text)
    , m_screenPosInPixels(screenPos)
	, m_shaderParameters(nullptr)
    , m_textMesh(nullptr)
    , m_isDirty(true)
	, m_padding(0)
{
	TRAP(!m_text.empty());
	m_charactersCapacity = (uint32_t)m_text.size() * 2; //allocate enough memory for changing texts. For now I will not implement a realloc mechanism if the new text will be bigger than the capacity, but in the future i will
}

CUIText::~CUIText()
{
    //delete m_textMesh; //dont delete m_textMesh - its a static mesh that is used by  a lot of graphic elemetns
	MemoryManager::GetInstance()->FreeHandle(m_shaderParameters);
}


void CUIText::SetText(const std::string& text)
{
    if (m_text != text && text.size() < m_charactersCapacity)
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


void CUIText::PreRender()
{
	TRAP(m_isDirty);
	//if (m_isDirty)
	{
		UITextGlyph* memPtr = m_shaderParameters->GetPtr<UITextGlyph*>();
		CFont2::UIString uiString;
		
		glm::uvec2 screenPos = m_screenPosInPixels;

		m_font->GetUIString(m_text, uiString);

		for (unsigned int i = 0; i < uiString.size(); ++i)
		{
			UITextGlyph& uiGlyph = memPtr[i];
			SGlyph glyph = uiString[i];
			uiGlyph.ScreenPosition = glm::vec4(screenPos, glyph.Size);
			uiGlyph.TextCoords = glm::vec4(glyph.TopLeftUV, glyph.SizeUV);

			screenPos.x += (uint32_t)glyph.Size.x + m_padding;
		}

		m_isDirty = false;
	}
}

void CUIText::Create(VkDescriptorSet set)
{
	m_descriptorSet = set;
	m_shaderParameters = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UI, sizeof(UITextGlyph) * m_charactersCapacity, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	m_textMesh = CreateFullscreenQuad(); //its not a fullscreen quad, its just a quad

	{
		VkDescriptorBufferInfo buffInfo = m_shaderParameters->GetDescriptor();
		VkWriteDescriptorSet wdesc = InitUpdateDescriptor(m_descriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &buffInfo);

		vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, 1, &wdesc, 0, nullptr);
	}

	m_isDirty = true;
}

void CUIText::Render(CGraphicPipeline* pipeline)
{
    if(m_text.empty())
        return;

    TRAP(m_textMesh);
	vk::CmdBindDescriptorSets(vk::g_vulkanContext.m_mainCommandBuffer, pipeline->GetBindPoint(), pipeline->GetLayout(), 1, 1, &m_descriptorSet, 0, nullptr);
    m_textMesh->Render(-1, (unsigned int)m_text.size());
}

//////////////////////////////////////////////////////////////////////////
//DebugBoundingBox
//////////////////////////////////////////////////////////////////////////
DebugBoundingBox::DebugBoundingBox(const BoundingBox3D& bb, const glm::vec4& color)
	: m_boundingBox(bb)
	, m_color(color)
{
}


struct DebugBBParams
{
	glm::vec4 Min;
	glm::vec4 Max;
	glm::vec4 Color;
};

//////////////////////////////////////////////////////////////////////////
//CUIRenderer
//////////////////////////////////////////////////////////////////////////

CUIRenderer::CUIRenderer(VkRenderPass renderPass)
    : CRenderer(renderPass, "UIRenderPass")
    , m_sampler(VK_NULL_HANDLE)
    , m_usedFont(nullptr)
	, m_globalsDescriptorSet(VK_NULL_HANDLE)
	, m_globalsBuffer(nullptr)
	, m_debugBBBuffer(nullptr)
	, m_bbDescriptorSet(VK_NULL_HANDLE)
	, m_visibleBb(0)
	, m_boundingBoxMesh(nullptr)
{
}

CUIRenderer::~CUIRenderer()
{
	MemoryManager::GetInstance()->FreeHandle(m_globalsBuffer);
	MemoryManager::GetInstance()->FreeHandle(m_debugBBBuffer);

	VkDevice dev = vk::g_vulkanContext.m_device;

	vk::DestroySampler(dev, m_sampler, nullptr);

	for (unsigned int i = 0; i < m_uiElemDescriptorPool.size(); ++i)
		delete m_uiElemDescriptorPool[i];
}

void CUIRenderer::PreRender()
{
	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::UI);

	UIGlobals* globals = m_globalsBuffer->GetPtr<UIGlobals*>();
	globals->ScreenSize = glm::vec4(WIDTH, HEIGHT, 0.0f, 0.0f);

	m_constants.ProjViewMatrix = m_projMatrix * ms_camera.GetViewMatrix();

	for (auto uiText : m_uiTexts)
	{
		if (uiText->IsDirty())
			uiText->PreRender();
	}


	DebugBBParams* params = m_debugBBBuffer->GetPtr<DebugBBParams*>();
	
	for (auto bb : m_debugBoundingBoxes)
	{
		if (bb->GetVisible())
		{
			params->Color = bb->GetColor();
			params->Min = glm::vec4(bb->GetBoundingBox().Min, 1.0f);
			params->Max = glm::vec4(bb->GetBoundingBox().Max, 1.0f);
			++m_visibleBb;
			++params;
		}

	}

	MemoryManager::GetInstance()->UnmapMemoryContext(EMemoryContextType::UI);
}

void CUIRenderer::Render()
{
    StartRenderPass();

	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

	if (!m_debugBoundingBoxes.empty())
	{
		vk::CmdBindPipeline(cmdBuffer, m_debugBBPipeline.GetBindPoint(), m_debugBBPipeline.Get());
		vk::CmdBindDescriptorSets(cmdBuffer, m_debugBBPipeline.GetBindPoint(), m_debugBBPipeline.GetLayout(), 0, 1, &m_bbDescriptorSet, 0, nullptr);
		vk::CmdPushConstants(cmdBuffer, m_debugBBPipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &m_constants);
		m_boundingBoxMesh->Render(-1, m_visibleBb);
	}

	if (!m_uiTexts.empty())
	{
		vk::CmdBindPipeline(cmdBuffer, m_textElemPipeline.GetBindPoint(), m_textElemPipeline.Get());
		vk::CmdBindDescriptorSets(cmdBuffer, m_textElemPipeline.GetBindPoint(), m_textElemPipeline.GetLayout(), 0, 1, &m_globalsDescriptorSet, 0, nullptr);
		for (auto& uiText : m_uiTexts)
			if (uiText->GetVisible())
				uiText->Render(&m_textElemPipeline);
	}

    EndRenderPass();
}

void CUIRenderer::SetFont(CFont2* font)
{
    if(font == m_usedFont)
        return;

    TRAP(font);
    m_usedFont = font;

	VkDescriptorImageInfo fontInfo = CreateDescriptorImageInfo(m_sampler, m_usedFont->GetFontImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkWriteDescriptorSet wDesc = InitUpdateDescriptor(m_globalsDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &fontInfo);

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, 1, &wDesc, 0, nullptr);
}

void CUIRenderer::AddUIText(CUIText* item)
{
	VkDescriptorSet set = AllocElementDescriptorSet(m_textElemDescriptorLayout);
	item->Create(set);

	m_uiTexts.push_back(item);
}

void CUIRenderer::RemoveUIText(CUIText* item)
{
	//WARNING this function is a time bomb. probably if i will implement double buffering this function will blow the driver up.
	// you need to delay the distruction of the descriptor set and do it in a safe place
	auto del = std::find(m_uiTexts.begin(), m_uiTexts.end(), item);
	if (del != m_uiTexts.end())
		m_uiTexts.erase(del);

	for (auto pool : m_uiElemDescriptorPool)
	{
		if (pool->FreeDescriptorSet(item->m_descriptorSet))
			break;
	}
}

void CUIRenderer::AddDebugBoundingBox(DebugBoundingBox* bb)
{
	uint32_t neededMemory = (uint32_t(m_debugBoundingBoxes.size()) + 1) * sizeof(DebugBBParams);

	if (neededMemory > m_debugBBBuffer->GetSize())
	{
		MemoryManager::GetInstance()->FreeHandle(m_debugBBBuffer);
		m_debugBBBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UI, m_debugBoundingBoxes.size() * 2 * sizeof(DebugBBParams), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

		VkDescriptorBufferInfo bbBufferInfo = m_debugBBBuffer->GetDescriptor();
		std::vector<VkWriteDescriptorSet> wDesc;
		wDesc.push_back(InitUpdateDescriptor(m_bbDescriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bbBufferInfo));

		vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
	}

	m_debugBoundingBoxes.push_back(bb);
}

void CUIRenderer::RemoveDebugBoundingBox(DebugBoundingBox* bb)
{
	auto it = std::find(m_debugBoundingBoxes.begin(), m_debugBoundingBoxes.end(), bb);
	if (it != m_debugBoundingBoxes.end())
		m_debugBoundingBoxes.erase(it);
}

void CUIRenderer::UpdateGraphicInterface()
{
	//VkDescriptorImageInfo fontInfo = CreateDescriptorImageInfo(m_sampler, m_usedFont->GetFontImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorBufferInfo globalsInfo = m_globalsBuffer->GetDescriptor();
	VkDescriptorBufferInfo bbBufferInfo = m_debugBBBuffer->GetDescriptor();
	std::vector<VkWriteDescriptorSet> wDesc;
	wDesc.push_back(InitUpdateDescriptor(m_globalsDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &globalsInfo));
	wDesc.push_back(InitUpdateDescriptor(m_bbDescriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bbBufferInfo));
	//wDesc.push_back(InitUpdateDescriptor(m_globalsDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &fontInfo));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

VkDescriptorSet CUIRenderer::AllocElementDescriptorSet(const DescriptorSetLayout& layout)
{
	for (auto pool : m_uiElemDescriptorPool)
	{
		if (pool->CanAllocate(layout))
			return pool->AllocateDescriptorSet(layout);
	}

	DescriptorPool* pool = new DescriptorPool();
	pool->Construct(layout, 20); //TODO this number should be tweaked

	m_uiElemDescriptorPool.push_back(pool);
	return pool->AllocateDescriptorSet(layout);
}

void CUIRenderer::CreateDescriptorSetLayout()
{
	m_textElemDescriptorLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);

	m_globalsDescSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
	m_globalsDescSetLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	m_globalsDescSetLayout.Construct();
	m_textElemDescriptorLayout.Construct();

	m_bbDescriptorLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
	m_bbDescriptorLayout.Construct();
}

void CUIRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
	maxSets = 0;
}

void CUIRenderer::AddNewDescriptorPool(std::vector<std::pair<DescriptorSetLayout*, uint32_t>> layoutsNumber)
{
	std::vector<VkDescriptorPoolSize> poolSize;
	uint32_t maxSets = 0;
	for (uint32_t i = 0; i < layoutsNumber.size(); ++i)
	{
		const std::vector<VkDescriptorSetLayoutBinding>& bindings = layoutsNumber[i].first->GetBindings();

		for (auto& binding : bindings)
		{
			AddDescriptorType(poolSize, binding.descriptorType, layoutsNumber[i].second * binding.descriptorCount);
		}
		maxSets += layoutsNumber[i].second;
	}

	DescriptorPool* pool = new DescriptorPool();
	pool->Construct(poolSize, maxSets);

	m_uiElemDescriptorPool.push_back(pool);
}

void CUIRenderer::Init()
{
    CRenderer::Init();

    CreateNearestSampler(m_sampler);
	std::vector<std::pair<DescriptorSetLayout*, uint32_t>> layoutsNumbers;
	layoutsNumbers.emplace_back(&m_textElemDescriptorLayout, 20);
	layoutsNumbers.emplace_back(&m_globalsDescSetLayout, 1);
	AddNewDescriptorPool(layoutsNumbers); //intialize first descriptor pool with the font layout. We only need one

	m_globalsDescriptorSet = m_uiElemDescriptorPool[0]->AllocateDescriptorSet(m_globalsDescSetLayout); //this should not fail

    m_textElemPipeline.SetVertexShaderFile("ui2d.vert");
    m_textElemPipeline.SetFragmentShaderFile("ui2d.frag");
    m_textElemPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_textElemPipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_textElemPipeline.SetDepthTest(false);
    m_textElemPipeline.SetDepthWrite(false);

    VkPipelineColorBlendAttachmentState blendState;
    blendState.blendEnable = VK_TRUE;
    blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendState.colorBlendOp =  VK_BLEND_OP_ADD;
    blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendState.alphaBlendOp = VK_BLEND_OP_ADD;
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;

    m_textElemPipeline.AddBlendState(blendState);

	std::vector<VkDescriptorSetLayout> layouts = { m_globalsDescSetLayout.Get(), m_textElemDescriptorLayout.Get()};
	m_textElemPipeline.CreatePipelineLayout(layouts);
    m_textElemPipeline.Init(this, m_renderPass, 0);

    PerspectiveMatrix(m_projMatrix);
    ConvertToProjMatrix(m_projMatrix);

	m_globalsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UI, sizeof(UIGlobals), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	m_debugBBBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UI, 10 * sizeof(DebugBBParams), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); //for start alloc only for 10 bb to test the Add/Remove BB interaction

	m_bbDescriptorSet = AllocElementDescriptorSet(m_bbDescriptorLayout);

	m_debugBBPipeline.SetVertexShaderFile("debugbb.vert");
	m_debugBBPipeline.SetFragmentShaderFile("debugbb.frag");
	m_debugBBPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
	m_debugBBPipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_debugBBPipeline.SetLineWidth(2.0f);
	m_debugBBPipeline.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant) });
	m_debugBBPipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	m_debugBBPipeline.CreatePipelineLayout(m_bbDescriptorLayout.Get());
	m_debugBBPipeline.Init(this, m_renderPass, 0);

	CreateBBMesh();

    //m_vectorElemPipeline.SetVertexShaderFile("uivector3d.vert");
    //m_vectorElemPipeline.SetFragmentShaderFile("uivector3d.frag");
    //m_vectorElemPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    //m_vectorElemPipeline.SetDepthTest(true);
    //m_vectorElemPipeline.SetDepthWrite(false);
    //m_vectorElemPipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
    //m_vectorElemPipeline.SetLineWidth(1.5f);
    //m_vectorElemPipeline.AddBlendState(blendState);
    //m_vectorElemPipeline.CreatePipelineLayout(m_commonDescSetLayout);
    //m_vectorElemPipeline.Init(this, m_renderPass, 0);
}

void CUIRenderer::CreateBBMesh()
{
	SVertex vertices[] = {
		SVertex(glm::vec3(-1, 1, 1)),
		SVertex(glm::vec3(1, 1, 1)),
		SVertex(glm::vec3(1, -1, 1)),
		SVertex(glm::vec3(-1, -1, 1)),
		SVertex(glm::vec3(-1, 1, -1)),
		SVertex(glm::vec3(1, 1, -1)),
		SVertex(glm::vec3(1, -1, -1)),
		SVertex(glm::vec3(-1, -1, -1))
	};

	//lines
	unsigned int indices[] = {
		0, 1, 1, 2, 2, 3, 3, 0,
		0, 3, 3, 7, 7, 4, 4, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		1, 5, 5, 6, 6, 2, 2, 1,
		2, 3, 3, 7, 7, 6, 6, 2,
		0, 1, 1, 5, 5, 4, 4, 0
	};

	m_boundingBoxMesh = new Mesh(std::vector<SVertex>(vertices, vertices + 8), std::vector<unsigned int>(indices, indices + sizeof(indices) / sizeof(unsigned int)));

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

CUIText* CUIManager::CreateTextItem(const std::string& text, glm::uvec2 screenPos)
{
    TRAP(m_uiRenderer);
    CUIText* item = new CUIText(m_font, text, screenPos);
    m_uiRenderer->AddUIText(item);
    m_uiItems.push_back(item);

    return item;
}

void CUIManager::DestroyTextItem(CUIText* item)
{
	TRAP(m_uiRenderer);
	auto itItem = std::find(m_uiItems.begin(), m_uiItems.end(), item);
	if (itItem != m_uiItems.end())
	{
		m_uiItems.erase(itItem);

		m_uiRenderer->RemoveUIText(item);
		delete item;
	}
}

DebugBoundingBox* CUIManager::CreateDebugBoundingBox(const BoundingBox3D& bb, glm::vec4 color)
{
	DebugBoundingBox* dBB = new DebugBoundingBox(bb, color);
	m_debugBBToAdd.push_back(dBB);
	return dBB;
}

void CUIManager::DestroyDebugBoundingBox(DebugBoundingBox* bb)
{
	m_debugBBToRemove.push_back(bb);
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

	if (!m_debugBBToAdd.empty())
	{
		for (auto bb : m_debugBBToAdd)
			m_uiRenderer->AddDebugBoundingBox(bb);

		m_debugBBToAdd.clear();
	}

	if (!m_debugBBToRemove.empty())
	{
		for (auto bb : m_debugBBToRemove)
			m_uiRenderer->RemoveDebugBoundingBox(bb);

		m_debugBBToRemove.clear();
	}
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

   //m_displayInfo->SetVisible(m_showUi.c);

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

    //m_displayInfo = CreateTextContainerItem(widgetStrings, startPos, yPadding, EWidget_MaxChars);
}
