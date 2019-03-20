#include "RenderTaskGraph.h"

#include "Utils.h"
#include "Framebuffer.h"
#include "GraphicEngine.h"
#include "Batch.h"

#include <unordered_map>

/////////////////////////////////////
//Render task attempt
//////////////////////////////////////

RenderTask::RenderTask(const std::vector<AttachmentInfo*>& inAttachments, 
	const std::vector<AttachmentInfo*>& outAttachments, 
	const std::function<void(void)>& execFunc,
	const std::function<void(VkRenderPass, uint32_t)>& setupFunc)
	: Task(execFunc)
	, m_inAttachments(inAttachments)
	, m_outAttachments(outAttachments)
	, m_parentGroup(nullptr)
	, m_setup(setupFunc)
{
}

//RenderTask::RenderTask(std::vector<AttachmentInfo*> inAttachments, 
//	std::vector<AttachmentInfo*> outAttachments, 
//	const std::function<void(void)>& execFunc,
//	const std::function<void(VkRenderPass, uint32_t)>& setupFunc)
//	: Task(execFunc)
//	, m_inAttachments(inAttachments)
//	, m_outAttachments(outAttachments)
//	, m_parentGroup(nullptr)
//	, m_setup(setupFunc)
//{
//}

void RenderTask::Execute()
{
	//wait for events if needed
	if (!m_externalEvents.empty())
	{
		vk::CmdWaitEvents(vk::g_vulkanContext.m_mainCommandBuffer, (uint32_t)m_externalEvents.size(), m_externalEvents.data(),
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_HOST_BIT, 
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, nullptr, 0, nullptr, 
			(uint32_t)m_externalImageBarrier.size(), m_externalImageBarrier.data()); //the srcPipelineStage will not be a constant if a compute task group exists as dependecy TODO
	}

	Task::Execute();
}

void RenderTask::SetParent(GPUTaskGroup* parentGroup)
{
	TRAP(m_parentGroup == nullptr && "Cannot change the parent if already set!!");
	m_parentGroup = parentGroup;
}

void RenderTask::Setup(VkRenderPass renderPass, uint32_t subpassId)
{
	if (!m_externalEvents.empty())
	{
		TRAP(!m_externalDependeciesMap.empty());
		for (auto& extDep : m_externalDependeciesMap)
			m_externalImageBarrier.push_back(extDep.second);
	}

	if (m_setup)
		m_setup(renderPass, subpassId);
}

void RenderTask::AddExternalEvent(VkEvent e)
{
	TRAP(e != VK_NULL_HANDLE);
	TRAP(std::find(m_externalEvents.begin(), m_externalEvents.end(), e) == m_externalEvents.end());
	m_externalEvents.push_back(e);
}

void RenderTask::AddExternalDependecies(AttachmentInfo* att, const VkImageMemoryBarrier& barrier)
{
	auto& extDep = m_externalDependeciesMap.find(att);

	if (extDep != m_externalDependeciesMap.end())
	{
		VkImageMemoryBarrier& regBarrier = extDep->second;
		TRAP(regBarrier.image == barrier.image);
		TRAP(regBarrier.oldLayout == barrier.oldLayout && "if this happens i have to rethink this functionality");
		regBarrier.dstAccessMask |= barrier.dstAccessMask;
		regBarrier.srcAccessMask |= barrier.srcAccessMask;
	}
	else
		m_externalDependeciesMap.emplace(att, barrier);
}

/////////////////////////////////////
//GPU task group
//////////////////////////////////////

GPUTaskGroup::GPUTaskGroup(const std::string& groupName)
	: TaskGroupBase<RenderTask, GPUTaskGroup>(groupName)
	, m_groupDoneEvent(VK_NULL_HANDLE)
{

}

GPUTaskGroup::~GPUTaskGroup()
{
	vk::DestroyEvent(vk::g_vulkanContext.m_device, m_groupDoneEvent, nullptr);
}

void GPUTaskGroup::DefineGroupIO()
{
	//std::unordered_set<AttachmentInfo*>	inputs;
	//std::unordered_set<AttachmentInfo*> outputs;
	for (auto task : m_tasks)
	{
		for (const auto& in : task->GetInAttachments())
			m_Inputs.insert(in);

		for (const auto& out : task->GetOutAttachments())
			m_Outputs.insert(out);

	}

	//std::copy(inputs.begin(), inputs.end(), std::back_inserter(m_Inputs));
	//std::copy(outputs.begin(), outputs.end(), std::back_inserter(m_Outputs));
}

void GPUTaskGroup::Init()
{
	DefineGroupIO();
	VkEventCreateInfo evCrtInfo;
	cleanStructure(evCrtInfo);
	evCrtInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	VULKAN_ASSERT(vk::CreateEvent(vk::g_vulkanContext.m_device, &evCrtInfo, nullptr, &m_groupDoneEvent));
}

void GPUTaskGroup::PostInit()
{

}

void GPUTaskGroup::AddTask(RenderTask* task)
{
	TaskGroupBase<RenderTask, GPUTaskGroup>::AddTask(task);
	task->SetParent(this);
}

/////////////////////////////////////
//Render task group
//////////////////////////////////////

RenderTaskGroup::RenderTaskGroup(const std::string& groupName)
	: GPUTaskGroup(groupName)
	, m_renderPass(VK_NULL_HANDLE)
	, m_framebuffer(nullptr)
{

}

RenderTaskGroup::~RenderTaskGroup()
{
	VkDevice dev = vk::g_vulkanContext.m_device;
	vk::DestroyRenderPass(dev, m_renderPass, nullptr);
	m_framebuffer->Destroy();
}


void RenderTaskGroup::Init()
{
	GPUTaskGroup::Init();
}

void RenderTaskGroup::PostInit()
{
	CreateSubpassesDescriptions();
	DetectTaskDependecies();
	
	ConstructRenderPass();

	m_framebuffer = new Framebuffer(m_renderPass, m_Outputs);

	for (unsigned int i = 0; i < m_tasks.size(); ++i)
		m_tasks[i]->Setup(m_renderPass, i);
}

void RenderTaskGroup::Execute()
{
	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
	vk::CmdResetEvent(cmdBuffer, m_groupDoneEvent, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

	StartRenderPass();

	m_tasks[0]->Execute();
	for (unsigned int i = 1; i < m_tasks.size(); ++i)
	{
		vk::CmdNextSubpass(vk::g_vulkanContext.m_mainCommandBuffer, VK_SUBPASS_CONTENTS_INLINE);
		m_tasks[i]->Execute();
	}

	EndRenderPass();

	vk::CmdSetEvent(cmdBuffer, m_groupDoneEvent, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
}

void RenderTaskGroup::DetectTaskDependecies()
{
	std::vector<VkSubpassDependency>& subpassDeps = m_renderPassContext.SubpassDependecies;
	std::cout << "=======Group: " << m_groupName << std::endl;

	for (uint32_t i = 0; i < m_tasks.size(); ++i)
	{
		std::cout << "Task " << i << ":" << std::endl;
		std::cout << "\t Inputs: { ";
		for (const auto& in : m_tasks[i]->GetInAttachments())
			std::cout << in->GetIdentifier() << ", ";
		std::cout << " } " << std::endl;

		std::cout << "\t Outputs: { ";
		for (const auto& out : m_tasks[i]->GetOutAttachments())
			std::cout << out->GetIdentifier() << ", ";
		std::cout << " } " << std::endl << std::endl;
	}

	std::unordered_map<AttachmentInfo*, std::unordered_set<RenderTask*>> outputList;
	std::unordered_map<GPUTaskGroup*, RenderTask*> externalDependeciesMap;

	std::cout << "Dependecies: " << std::endl;

	std::unordered_map<uint32_t, std::unordered_map<uint32_t, VkSubpassDependency>> dependeciesMap; //key of the parent map = srcIndex, key of the value map = dstIndex
	for (uint32_t i = 0; i < m_tasks.size(); ++i)
	{
		auto task = m_tasks[i];
		for (const auto& in : task->GetInAttachments())
		{
			auto taskDeps = outputList.find(in);
			if (taskDeps != outputList.end())
			{
				for (auto taskDep : taskDeps->second)
				{
					if (taskDep == task)
					{//dont add to dependecies the current task
						TRAP(false && "This shouldn't happen");
						continue;
					}

					auto secondTaskIt = std::find(m_tasks.begin(), m_tasks.end(), taskDep);
					TRAP(secondTaskIt != m_tasks.end());
					uint32_t srcIndex = secondTaskIt - m_tasks.begin();

					if (srcIndex >= i)
					{
						TRAP(false && "This shouldn't happen");
						continue;
					}
					VkPipelineStageFlags srcStage = (IsColorFormat(in->GetFormat())) ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
					VkAccessFlags srcAccess = (IsColorFormat(in->GetFormat())) ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

					auto taskDepMap = dependeciesMap.find(srcIndex);
					if (taskDepMap == dependeciesMap.end())
					{
						dependeciesMap[srcIndex].emplace(i, CreateSubpassDependency(srcIndex, i, srcStage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, srcAccess, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));
						std::cout << "Task " << secondTaskIt - m_tasks.begin() << " -> ";
						std::cout << "Task " << i << std::endl;
					}
					else
					{
						//
						auto& dstDepMap = taskDepMap->second; //for clarity
						auto dstIt = dstDepMap.find(i);

						if (dstIt != dstDepMap.end())
						{
							dstIt->second.srcStageMask |= srcStage;
							dstIt->second.srcAccessMask |= srcAccess;
						}
						else
						{
							dstDepMap.emplace(i, CreateSubpassDependency(srcIndex, i, srcStage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, srcAccess, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));
							std::cout << "Task " << secondTaskIt - m_tasks.begin() << " -> ";
							std::cout << "Task " << i << std::endl;
						}
					}
				}
			}

			for (const auto& dep : m_dependencies) //check external dependecies
			{
				//maybe add here the input to the external inputs list
				if (dep->m_Outputs.find(in) != dep->m_Outputs.end())
				{
					auto groupIt = externalDependeciesMap.find(dep);
					if (groupIt == externalDependeciesMap.end())
						externalDependeciesMap.emplace(dep, task);
				}
			}		
		}

		//add outputs to the output list
		for (const auto& outs : task->GetOutAttachments())
			outputList[outs].insert(task);

		task->TaskIndex = i; //debug index;
		if (task->GetInAttachments().empty())
			std::cout << "Task " << i << " no deps" << std::endl;
	}

	for (auto& srcMap : dependeciesMap)
		for (auto dep : srcMap.second)
			subpassDeps.push_back(dep.second);

	for (auto depPair : externalDependeciesMap)
	{
		GPUTaskGroup* group = depPair.first;
		RenderTask* task = depPair.second;
		group->AddExternalDependecyToTask(task);
	}
}

void RenderTaskGroup::CreateSubpassesDescriptions()
{
	std::vector<VkAttachmentDescription>& attDesc = m_renderPassContext.AttachmentDescriptions;
	std::unordered_map<AttachmentInfo*, VkAttachmentReference> attRef; //keep track of the attachmentInfo -> attachmentReference, because the attachmentInfo are kept in a set and we need to create a vector
	attDesc.reserve(m_Outputs.size());
	TRAP(attDesc.empty());

	uint32_t attIndex = 0;
	//group outputs will be the views that will be found in framebuffers, so the attachments description will be constructed from these 
	for (auto out : m_Outputs)
	{
		VkImageLayout renderPassLayout = IsColorFormat(out->GetFormat()) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attRef[out] = CreateAttachmentReference(attIndex, renderPassLayout);

		VkImageLayout finalLayout = renderPassLayout;//GetFinalLayout(out, renderPassLayout); //maybe we dont need this anymore, layout change will take place in VkCmdWaitEvents()
		VkAttachmentLoadOp loadOp = out->IsFirstOccurence(this) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		
		attDesc.push_back(AddAttachementDesc(VK_IMAGE_LAYOUT_UNDEFINED, finalLayout, out->GetFormat(), loadOp));
		++attIndex;

	}
	std::vector < SubpassDescription>& subpassDesc = m_renderPassContext.SubpassDescriptions;
	subpassDesc.resize(m_tasks.size());

	for (unsigned int i = 0; i < m_tasks.size(); ++i)
	{
		auto& task = m_tasks[i];
		for (auto outAtt : task->GetOutAttachments())
		{
			auto outAttRef = attRef.find(outAtt);
			TRAP(outAttRef != attRef.end() && "Something is wrong. The map is not completed!");
			if (IsDepthFormat(outAtt->GetFormat()))
			{
				TRAP(subpassDesc[i].DepthAttachment.layout == VK_IMAGE_LAYOUT_UNDEFINED && "You can only have one depth attachment and try to put it at the end of attchment list");
				subpassDesc[i].DepthAttachment = outAttRef->second;
			}
			else
				subpassDesc[i].ColorAttachments.push_back(outAttRef->second);
		}
	}
}

VkImageLayout RenderTaskGroup::GetFinalLayout(AttachmentInfo* att, VkImageLayout defaultLayout)
{
	//find a nicer way to achieve this
	for (auto child : m_children)
	{
		if (child->m_Inputs.find(att) != child->m_Inputs.end())
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; //for now we consider that an attachment is  used only as a read resource
	}
	return defaultLayout;
}

void RenderTaskGroup::ConstructRenderPass()
{
	//convert SubpassDescription to the vulkan counter part
	std::vector<VkSubpassDescription> subpassDescriptions;
	subpassDescriptions.reserve(m_renderPassContext.SubpassDescriptions.size());
	for (auto& sd : m_renderPassContext.SubpassDescriptions) 
	{
		VkAttachmentReference* depthAtt = (sd.DepthAttachment.layout != VK_IMAGE_LAYOUT_UNDEFINED) ? &sd.DepthAttachment : nullptr;
		subpassDescriptions.push_back(CreateSubpassDesc(sd.ColorAttachments.data(), sd.ColorAttachments.size(), depthAtt)); //this should be const.
	}

	TRAP(m_renderPassContext.AttachmentDescriptions.size() == m_Outputs.size());
	NewRenderPass(&m_renderPass, m_renderPassContext.AttachmentDescriptions, subpassDescriptions, m_renderPassContext.SubpassDependecies);
}

void RenderTaskGroup::StartRenderPass()
{
	VkRect2D renderArea = m_framebuffer->GetRenderArea();
	const std::vector<VkClearValue>& clearValues = m_framebuffer->GetClearColors();

	VkRenderPassBeginInfo renderBeginInfo;
	cleanStructure(renderBeginInfo);
	renderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderBeginInfo.pNext = nullptr;
	renderBeginInfo.renderPass = m_renderPass;
	renderBeginInfo.framebuffer = m_framebuffer->GetFramebufferHandle();
	renderBeginInfo.renderArea = renderArea;
	renderBeginInfo.clearValueCount = (uint32_t)clearValues.size();
	renderBeginInfo.pClearValues = clearValues.data();

	StartDebugMarker(m_groupName);

	vk::CmdBeginRenderPass(vk::g_vulkanContext.m_mainCommandBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderTaskGroup::EndRenderPass()
{
	vk::CmdEndRenderPass(vk::g_vulkanContext.m_mainCommandBuffer);
	EndDebugMarker(m_groupName);
}

void RenderTaskGroup::AddExternalDependecyToTask(RenderTask* task)
{
	std::cout << "External deps: " << GetName() << " -> " << task->TaskIndex << std::endl;

	task->AddExternalEvent(m_groupDoneEvent);

	for (auto& taskIn : task->GetInAttachments())
		if (m_Outputs.find(taskIn) != m_Outputs.end())
		{
			ImageHandle* inHandle = taskIn->GetHandle();
			VkAccessFlags srcAccess = (IsColorFormat(taskIn->GetFormat())) ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			VkImageAspectFlags imgAspect = 0;
			imgAspect |= (IsColorFormat(taskIn->GetFormat())) ? VK_IMAGE_ASPECT_COLOR_BIT : 0;
			imgAspect |= (IsDepthFormat(taskIn->GetFormat())) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
			imgAspect |= (IsStencilFormat(taskIn->GetFormat())) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;

			task->AddExternalDependecies(taskIn, inHandle->CreateMemoryBarrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, srcAccess, VK_ACCESS_SHADER_READ_BIT,imgAspect));
		}
}

/////////////////////////////////////
//Render gragph
//////////////////////////////////////
void  RenderGraph::Prepare()
{
	START_PROFILE_SECTION("PrepareRenderGraph");
	for (auto group : m_groups)
		group->Init();

	std::unordered_map<AttachmentInfo*, std::unordered_set<RenderTaskGroup*>> outputList;

	for (auto group : m_groups)
		for (const auto& out : group->m_Outputs)
			outputList[out].insert(group);

	for (auto group : m_groups)
	{
		for (const auto& in : group->m_Inputs)
		{
			auto it = outputList.find(in);
			if (it != outputList.end())
			{
				std::vector<GPUTaskGroup*> deps;

				std::copy(it->second.begin(), it->second.end(), std::back_inserter(deps));
				auto selfIt = std::find(deps.begin(), deps.end(), group); //remove the self group
				if (selfIt != deps.end())
					deps.erase(selfIt);

				group->AddDependencies(deps);
			}
		}
	}

	TopologicalSort();
	
	//check the order of clear operation on the attachments.
	{
		//this is a set that keeps the first occurence of a attachment in the graph
		std::unordered_set<AttachmentInfo*> attSetCache;
		std::cout << "====== Att clear operation -> during group exection" << std::endl;

		for (auto group : m_sortedTaskGroups)
		{
			for (auto& att : group->m_Outputs)
			{
				if (attSetCache.find(att) == attSetCache.end())
				{
					att->SetFirstGroup(group);
					attSetCache.insert(att);
					std::cout << "\t " << att->GetIdentifier() << " -> " << group->GetName() << std::endl;
				}
			}
		}
	}

	for (auto group : m_sortedTaskGroups)
		group->PostInit();

	END_PROFILE_SECTION("PrepareRenderGraph");
}

void PrintString(const std::string& str)
{
	std::cout << "\t" << str << std::endl;
}

void  RenderGraph::Init()
{
	Prepare();
}
