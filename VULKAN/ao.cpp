#include "ao.h"

#include "Utils.h"
#include "Mesh.h"
#include "MemoryManager.h"
#include "GraphicEngine.h"
#include "Framebuffer.h"

#include <random>

struct SSAOConstParams
{
    glm::vec4 Samples[64];
    glm::vec4 Noise[16];
};

struct SSAOVarParams
{
    glm::mat4 ProjMatrix;
    glm::mat4 ViewMatrix;
};

struct SBlurParams
{
    glm::vec4 Horizontal;
};

enum Bindings
{
    Bindings_Normals = 0,
    Bindings_Positions,
    Bindings_Depth,
    Bindings_Uniform,
    Bindings_Count
};

 ///////////////////////////////////////////////////////////////////
 //AORenderer
 ///////////////////////////////////////////////////////////////////

 AORenderer::AORenderer()
	 : Renderer()
	 , m_quad(nullptr)
	 , m_constParamsBuffer(nullptr)
	 , m_varParamsBuffer(nullptr)
	 , m_sampler(VK_NULL_HANDLE)
 {
 }

 AORenderer::~AORenderer()
 {
	 VkDevice dev = vk::g_vulkanContext.m_device;

	 MemoryManager::GetInstance()->FreeHandle(m_constParamsBuffer->GetRootParent()); //free the parrent buffer, that frees the memory for varParamBuffer too

	 vk::DestroySampler(dev, m_sampler, nullptr);
 }

 void AORenderer::InitInternal()
 {
	 //Alloc Memory
	 {
		 BufferHandle* bigBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, { sizeof(SSAOConstParams), sizeof(SSAOVarParams) }, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		 m_constParamsBuffer = bigBuffer->CreateSubbuffer(sizeof(SSAOConstParams));
		 m_varParamsBuffer = bigBuffer->CreateSubbuffer(sizeof(SSAOVarParams));
	 }

	 InitSSAOParams();

	 CreateNearestSampler(m_sampler);
	 m_quad = CreateFullscreenQuad();
 }

 void AORenderer::SetupMainPass(VkRenderPass renderPass, uint32_t subpassId)
 {
	 std::vector<VkDescriptorSetLayout> mainPassLayouts;

	 mainPassLayouts.push_back(m_constDescSetLayout.Get());
	 mainPassLayouts.push_back(m_varDescSetLayout.Get());

	 m_mainPipeline.SetVertexShaderFile("screenquad.vert");
	 m_mainPipeline.SetFragmentShaderFile("ssao.frag");
	 m_mainPipeline.SetDepthTest(false);
	 m_mainPipeline.SetDepthWrite(false);
	 m_mainPipeline.SetVertexInputState(Mesh::GetVertexDesc());
	 m_mainPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 2);
	 m_mainPipeline.CreatePipelineLayout(mainPassLayouts);
	 m_mainPipeline.Setup(renderPass, subpassId);

	 RegisterPipeline(&m_mainPipeline);
 }

 void AORenderer::RenderMain()
 {
	 VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;

	 BeginMarkerSection("ResolveAO");
	 vk::CmdBindPipeline(cmdBuff, m_mainPipeline.GetBindPoint(), m_mainPipeline.Get());
	 vk::CmdBindDescriptorSets(cmdBuff, m_mainPipeline.GetBindPoint(), m_mainPipeline.GetLayout(), 0, (uint32_t)m_mainPassSets.size(), m_mainPassSets.data(), 0, nullptr);
	 m_quad->Render();
	 EndMarkerSection();
 }

 void AORenderer::SetupVBlurPass(VkRenderPass renderPass, uint32_t subpassId)
 {
	 m_vblurPipeline.SetVertexShaderFile("screenquad.vert");
	 m_vblurPipeline.SetFragmentShaderFile("vblur.frag");
	 m_vblurPipeline.SetDepthTest(false);
	 m_vblurPipeline.SetDepthWrite(false);
	 m_vblurPipeline.SetVertexInputState(Mesh::GetVertexDesc());
	 m_vblurPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
	 m_vblurPipeline.CreatePipelineLayout(m_blurDescSetLayout.Get());
	 m_vblurPipeline.Setup(renderPass, subpassId);
 }

 void AORenderer::RenderVBlur()
 {
	 VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;

	 BeginMarkerSection("AOBlurVertical");

	 vk::CmdBindPipeline(cmdBuff, m_vblurPipeline.GetBindPoint(), m_vblurPipeline.Get());
	 vk::CmdBindDescriptorSets(cmdBuff, m_vblurPipeline.GetBindPoint(), m_vblurPipeline.GetLayout(), 0, 1, &m_blurPassSets[1], 0, nullptr);

	 m_quad->Render();
	 EndMarkerSection();
 }

 void AORenderer::SetupHBlurPass(VkRenderPass renderPass, uint32_t subpassId)
 {
	 m_hblurPipeline.SetVertexShaderFile("screenquad.vert");
	 m_hblurPipeline.SetFragmentShaderFile("hblur.frag");
	 m_hblurPipeline.SetDepthTest(false);
	 m_hblurPipeline.SetDepthWrite(false);
	 m_hblurPipeline.SetVertexInputState(Mesh::GetVertexDesc());
	 m_hblurPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
	 m_hblurPipeline.CreatePipelineLayout(m_blurDescSetLayout.Get());
	 m_hblurPipeline.Setup(renderPass, subpassId);

	 RegisterPipeline(&m_hblurPipeline);
 }

 void AORenderer::RenderHBlur()
 {
	 VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;

	 BeginMarkerSection("AOBlurHorizontal");

	 vk::CmdBindPipeline(cmdBuff, m_hblurPipeline.GetBindPoint(), m_hblurPipeline.Get());
	 vk::CmdBindDescriptorSets(cmdBuff, m_hblurPipeline.GetBindPoint(), m_hblurPipeline.GetLayout(), 0, 1, &m_blurPassSets[0], 0, nullptr);

	 m_quad->Render();
	 EndMarkerSection();
 }


 void AORenderer::PreRender()
 {
	 UpdateParams();
 }

 void AORenderer::UpdateGraphicInterface()
 {
	 ImageHandle* normalImage = GraphicEngine::GetAttachment("Normals")->GetHandle();
	 ImageHandle* positionImage = GraphicEngine::GetAttachment("Positions")->GetHandle();
	 ImageHandle* depthImage = GraphicEngine::GetAttachment("Depth")->GetHandle();
	 ImageHandle* aoFinal = GraphicEngine::GetAttachment("AOFinal")->GetHandle();
	 ImageHandle* aoBlur = GraphicEngine::GetAttachment("AOBlurAux")->GetHandle();

	 VkDescriptorImageInfo normalsImgInfo = CreateDescriptorImageInfo(m_sampler, normalImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	 VkDescriptorImageInfo positionsImgInfo = CreateDescriptorImageInfo(m_sampler, positionImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	 VkDescriptorImageInfo depthImgInfo = CreateDescriptorImageInfo(m_sampler, depthImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	 VkDescriptorImageInfo blurImgInfo = CreateDescriptorImageInfo(m_sampler, aoFinal->GetView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	 VkDescriptorImageInfo blurImgInfo2 = CreateDescriptorImageInfo(m_sampler, aoBlur->GetView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	 VkDescriptorBufferInfo constBuffInfo = m_constParamsBuffer->GetDescriptor();
	 VkDescriptorBufferInfo varBuffInfo = m_varParamsBuffer->GetDescriptor();

	 std::vector<VkWriteDescriptorSet> wDescSets;
	 wDescSets.push_back(InitUpdateDescriptor(m_mainPassSets[0], Bindings_Normals, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalsImgInfo));
	 wDescSets.push_back(InitUpdateDescriptor(m_mainPassSets[0], Bindings_Positions, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &positionsImgInfo));
	 wDescSets.push_back(InitUpdateDescriptor(m_mainPassSets[0], Bindings_Depth, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthImgInfo));
	 wDescSets.push_back(InitUpdateDescriptor(m_mainPassSets[0], Bindings_Uniform, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &constBuffInfo));
	 wDescSets.push_back(InitUpdateDescriptor(m_mainPassSets[1], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &varBuffInfo));
	 wDescSets.push_back(InitUpdateDescriptor(m_blurPassSets[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &blurImgInfo));
	 wDescSets.push_back(InitUpdateDescriptor(m_blurPassSets[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &blurImgInfo2));

	 vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDescSets.size(), wDescSets.data(), 0, nullptr);
 }

 void AORenderer::CreateDescriptorSetLayouts()
 {
	 {
		m_constDescSetLayout.AddBinding(Bindings_Normals, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_constDescSetLayout.AddBinding(Bindings_Positions, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_constDescSetLayout.AddBinding(Bindings_Depth, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_constDescSetLayout.AddBinding(Bindings_Uniform, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_constDescSetLayout.Construct();

		RegisterDescriptorSetLayout(&m_constDescSetLayout);
	 }

	 //this is reserved for varying params
	 {
		 m_varDescSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		 m_varDescSetLayout.Construct();
	
		 RegisterDescriptorSetLayout(&m_varDescSetLayout);
	 }

	 //blur subpass
	 {
		 m_blurDescSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		 m_blurDescSetLayout.Construct();

		 RegisterDescriptorSetLayout(&m_blurDescSetLayout);
	 }
 }


 void AORenderer::AllocateDescriptorSets()
 {
	 m_mainPassSets.resize(2);
	 m_mainPassSets[0] = m_descriptorPool.AllocateDescriptorSet(m_constDescSetLayout);
	 m_mainPassSets[1] = m_descriptorPool.AllocateDescriptorSet(m_varDescSetLayout);

	 m_blurPassSets.resize(2);
	 m_blurPassSets[0] = m_descriptorPool.AllocateDescriptorSet(m_blurDescSetLayout);
	 m_blurPassSets[1] = m_descriptorPool.AllocateDescriptorSet(m_blurDescSetLayout);

 }

 void AORenderer::InitSSAOParams()
 {
	 TRAP(m_constParamsBuffer);

	 m_samples.resize(64);
	 m_noise.resize(16);

	 unsigned int seed = 162039;
	 std::mt19937 generator(seed);
	 std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

	 SSAOConstParams* mem = m_constParamsBuffer->GetPtr<SSAOConstParams*>();

	 for (unsigned int i = 0; i < 64; ++i)
	 {
		 //tangent space
		 float x = distribution(generator) * 2.0f - 1.0f;
		 float y = distribution(generator) * 2.0f - 1.0f;
		 float z = distribution(generator);
		 glm::vec3 sample = glm::normalize(glm::vec3(x, y, z));
		 //see what happens if you multiply with random float
		 float scale = float(i) / 64.0f;
		 scale = glm::mix(0.1f, 1.0f, scale * scale); //nici asta nu inteleg de ce e la patrat
		 m_samples[i] = glm::vec4(sample * scale, 0.0f);
	 }

	 memcpy(mem->Samples, m_samples.data(), sizeof(mem->Samples));

	 for (unsigned int i = 0; i < 16; ++i)
	 {
		 //tangent space too
		 //this is a random rotation. we dont rotate around the z axis
		 float x = distribution(generator) * 2.0f - 1.0f;
		 float y = distribution(generator) * 2.0f - 1.0f;

		 glm::vec3 noise = glm::vec3(x, y, 0.0f);
		 m_noise[i] = glm::vec4(noise, 0.0f);
	 }

	 memcpy(mem->Noise, m_noise.data(), sizeof(mem->Noise));
 }

 void AORenderer::UpdateParams()
 {
	 const FrameConstants& fc = GraphicEngine::GetFrameConstants();

	 SSAOVarParams* params = m_varParamsBuffer->GetPtr<SSAOVarParams*>();
	 params->ProjMatrix = fc.ProjMatrix;
	 params->ViewMatrix = fc.ViewMatrix;
 }