#pragma once

#include "Singleton.h"
#include "VulkanLoader.h"
#include "TaskGraph.h"
#include "glm/glm.hpp"
#include "defines.h"
#include "Lights.h"
#include "Camera.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <array>

//#define GetGraphicEngine() GraphicEngine::GetInstance()
class RenderGraph;
class AttachmentInfo;
class Renderer;
class CFrustum;
class KeyInput;

struct ShadowSplit
{
	glm::mat4 ProjViewMatrix;
	glm::vec4 NearFar;
};

typedef std::array<ShadowSplit, SHADOWSPLITS> SplitsArrayType;

struct FrameConstants
{
	glm::vec4			DirectionalLightDirection;
	glm::vec4			DirectionaLightIrradiance;
	glm::mat4			ViewMatrix;
	glm::mat4			ProjMatrix;
	glm::mat4			ProjViewMatrix;
	SplitsArrayType		ShadowSplits;
	uint32_t			NumberOfShadowSplits;
	float				DirectionalLightIntensity;
};

class GraphicEngine : public Singleton<GraphicEngine>
{
	friend class Singleton<GraphicEngine>;
public:
	void						Init(HWND windowHandle, HINSTANCE appInstance);
	void						PreRender();
	void						Render();
	void						Update(float dt);

	static AttachmentInfo*		GetAttachment(const std::string& name);
	static const FrameConstants&	GetFrameConstants();
	static CCamera&				GetActiveCamera();
private:
	GraphicEngine();
	virtual ~GraphicEngine();

	void						CreateFramebufferAttachments();
	VkImageCreateInfo			GetAttachmentCreateImageInfo(VkFormat format, VkExtent3D dimensions, uint32_t layers, VkImageUsageFlags additionalUsage);
	void						InitInputMap();

	//init section
	void						InitRenderGraph();
	void						InitPreRenderGraph();
	void						InitUpdateGraph();
	void						InitRenderers();
	void						CreateCommandBuffer();
	void						CreateSurface(HWND windowHandle, HINSTANCE appInstance);
	void						CreateSwapChains();
	void						CreateSynchronizationHelpers();
	void						GetQueue();
	//frame lifecycle section
	void						StartFrame();
	void						EndFrame();
	void						TransferToPresentImage();
	void						StartCommandBuffer();
	void						EndCommandBuffer();
	void						UpdateFrameConstants();

	//shadow map methods
	void ComputeCascadeSplitMatrices();
	void ComputeCascadeViewMatrix(const CFrustum& splitFrustrum, const glm::vec3& lightDir, const glm::vec3& lightUp, glm::mat4& outView);
	void ComputeCascadeProjMatrix(const CFrustum& splitFrustum, const glm::mat4& lightViewMatrix, const glm::mat4& lightProjMatrix, glm::mat4& outCroppedProjMatrix);

	bool OnKeyPressed(const KeyInput& key);
private:
	typedef TaskGroup<Task> UpdateTaskGroup;

	class UpdateTaskGraph : public TaskGraph<UpdateTaskGroup>
	{
	public:
		void Prepare();
	};

private:
	std::unordered_map<std::string, AttachmentInfo*>	m_framebufferAttachments;
	std::unordered_map<std::string, Renderer*>			m_renderers;

	RenderGraph*										m_renderGraph;
	UpdateTaskGraph*									m_preRenderTaskGraph;
	UpdateTaskGraph*									m_updateTaskGraph; //this is named poorly
	FrameConstants										m_frameConstants;
	
	//mandatory objects
	CDirectionalLight									m_directionalLight;
	CCamera												m_activeCamera;

	//shadow map utils
	float												m_splitsAlphaFactor;
	//TODO uncomment and fix it
	//bool												m_isShadowDebugMode;
	//CUIText*											m_shadowDebugText;

	//Vulkan render context
	std::vector<VkImage>								m_presentImages;

	unsigned int										m_currentBuffer;
	VkQueue												m_queue;
	VkSwapchainKHR										m_swapChain;
	VkSurfaceKHR										m_surface;

	VkCommandPool										m_commandPool;
	VkCommandBuffer										m_mainCommandBuffer;
	//synchronization objects
	VkFence												m_aquireImageFence;
	VkFence												m_renderFence;
	VkSemaphore											m_renderSemaphore;
};
