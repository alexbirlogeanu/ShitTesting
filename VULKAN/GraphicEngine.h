#pragma once

#include "Singleton.h"
#include "VulkanLoader.h"

#include <string>
#include <unordered_map>
#include <utility>

//#define GetGraphicEngine() GraphicEngine::GetInstance()
class AttachmentInfo;
class RenderGraph;
class GraphicEngine : public Singleton<GraphicEngine>
{
	friend class Singleton<GraphicEngine>;
public:
	void					Init(HWND windowHandle, HINSTANCE appInstance);
	void					Render();

	static AttachmentInfo*	GetAttachment(const std::string& name);
private:
	GraphicEngine();
	virtual ~GraphicEngine();

	void						CreateFramebufferAttachments();
	VkImageCreateInfo			GetAttachmentCreateImageInfo(VkFormat format, VkExtent3D dimensions, uint32_t layers, VkImageUsageFlags additionalUsage = 0);

	//init section
	void						InitRenderGraph();
	void						CreateCommandBuffer();
	void						CreateSurface(HWND windowHandle, HINSTANCE appInstance);
	void						CreateSwapChains();
	void						CreateSynchronizationHelpers();
	void						GetQueue();
	//frame lifecycle section
	void						TransferToPresentImage();
	void						StartCommandBuffer();
	void						EndCommandBuffer();
private:
	std::unordered_map<std::string, AttachmentInfo*>	m_framebufferAttachments;
	RenderGraph*										m_renderGraph;

	//
	std::vector<VkImage>        m_presentImages;

	unsigned int                m_currentBuffer;
	VkQueue						m_queue;
	VkSwapchainKHR              m_swapChain;
	VkSurfaceKHR                m_surface;

	VkCommandPool               m_commandPool;
	VkCommandBuffer             m_mainCommandBuffer;
	//synchronization objects
	VkFence                     m_aquireImageFence;
	VkFence                     m_renderFence;
	VkSemaphore                 m_renderSemaphore;
};
