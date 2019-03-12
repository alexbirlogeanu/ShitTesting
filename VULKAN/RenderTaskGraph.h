#pragma once

#include "TaskGraph.h"
#include "VulkanLoader.h"

class GPUTaskGroup;
class AttachmentInfo;
class Framebuffer;
/////////////////////////////////////
//Render task attempt
//////////////////////////////////////
class RenderTask : public Task
{
public:
	RenderTask(std::vector<AttachmentInfo*>& inAttachments,
		std::vector<AttachmentInfo*>& outAttachments,
		const std::function<void(void)>& exec);

	RenderTask(std::vector<AttachmentInfo*> inAttachments,
		std::vector<AttachmentInfo*> outAttachments,
		const std::function<void(void)>& exec);

	const std::vector<AttachmentInfo*>& GetInAttachments() const { return m_inAttachments; }
	const std::vector<AttachmentInfo*>& GetOutAttachments() const { return m_outAttachments; }
	//TODO debug value
	int TaskIndex;

	void SetParent(GPUTaskGroup* parentGroup);
private:
	std::vector<AttachmentInfo*>		m_inAttachments; //could be empty
	std::vector<AttachmentInfo*>		m_outAttachments; //should not be empty. Every render task will write at least one framebuffer attachment

	std::function<void(int)>			m_prepareFunc;
	GPUTaskGroup*						m_parentGroup;
};

class ComputeTask : public RenderTask
{
public:
	ComputeTask(const std::function<void(void)>& exec)
		: RenderTask({}, {}, exec)
	{
	}
};



/////////////////////////////////////
//GPU task group
//////////////////////////////////////
class GPUTaskGroup : public TaskGroupBase<RenderTask, GPUTaskGroup>
{
public:

	GPUTaskGroup(const std::string& groupName);
	virtual void Init();
	virtual void PostInit();

	virtual bool PrecedeTask(RenderTask* task) = 0;
	virtual void AddExternalDependecyToTask(RenderTask* task) = 0;

	virtual void AddTask(RenderTask* task) override;
protected:
	//in this method, the inputs and outputs will be defined based on the task list. So, this method should be called only when all the tasks that is wanted to be executed by the group, will be inserted
	void DefineGroupIO();

public:
	std::unordered_set<AttachmentInfo*>			m_Inputs;
	std::unordered_set<AttachmentInfo*>			m_Outputs;

	VkEvent										m_groupDoneEvent;
};


/////////////////////////////////////
//Render task group
//////////////////////////////////////

class RenderTaskGroup : public GPUTaskGroup
{
public:
	RenderTaskGroup(const std::string& groupName);
	virtual ~RenderTaskGroup();

	void Init() override;
	void PostInit() override;

	virtual void Execute() override;

	//this method feels like hack
	bool PrecedeTask(RenderTask* task) override;
	void AddExternalDependecyToTask(RenderTask* task) override;
private:
	
	//in this method we detect the task dependecies that will be translate later in subpass dependencies for render pass creation. Task will be executed in the order in which they were added to the task group. A task it's equivalent with a subpass in a render pass
	void DetectTaskDependecies();

	
	void CreateSubpassesDescriptions();
	VkImageLayout GetFinalLayout(AttachmentInfo* att, VkImageLayout defaultLayout);
	//methods for render pass creation and manipulation
	void ConstructRenderPass();
	void StartRenderPass();
	void EndRenderPass();
private:
	struct SubpassDescription
	{
		SubpassDescription()
		{
			DepthAttachment.layout = VK_IMAGE_LAYOUT_UNDEFINED; //invalidate depth attachment
		}

		std::vector<VkAttachmentReference>	ColorAttachments;
		VkAttachmentReference				DepthAttachment;
	};

	struct RenderPassCreateContext
	{
		std::vector<VkAttachmentDescription>							AttachmentDescriptions;
		std::vector<SubpassDescription>									SubpassDescriptions;
		std::vector<VkSubpassDependency>								SubpassDependecies;
	};

public: //TODO change private
	
		
	//Add render pass that will be created in the init method
	VkRenderPass							m_renderPass;
	RenderPassCreateContext					m_renderPassContext; //this member should be cleared after the render creation job is done

	Framebuffer*							m_framebuffer;
};

/////////////////////////////////////
//Render graph attempt
//////////////////////////////////////

class RenderGraph : public TaskGraph<RenderTaskGroup>
{
public:
	void Prepare();
	void Init();

	void UnitTest();
};
