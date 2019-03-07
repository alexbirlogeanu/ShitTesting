#pragma once


#include "defines.h"

#include <functional>
#include <vector>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <iostream>

#define HEAP_ONLY(CLASSNAME)	public: \
								void Destroy() { delete this; } \
								protected: \
								virtual ~##CLASSNAME();
 
class Task
{
	HEAP_ONLY(Task);
public:
	Task(const std::function<void(void)>& func);
	virtual void Execute();
	
protected:
	std::function<void(void)> m_task;
};


////////////////////////////////////////////////////////////////////
//TaskGroup<T>
////////////////////////////////////////////////////////////////////

template<class TaskType>
class TaskGroup
{
	HEAP_ONLY(TaskGroup);
public:
	TaskGroup(const std::string& groupName);
	
	void AddDependencies(const std::vector<TaskGroup<TaskType>*> deps);
	virtual void AddTask(TaskType* task);

	const std::unordered_set<TaskGroup<TaskType>*>& GetDependecies() const { return m_dependencies; }
	const std::string& GetName() const { return m_groupName; }
	virtual void Execute();

	virtual bool PrecedeTask(TaskType* task) { TRAP(false && "This method should be implemented"); return false; };
	virtual void AddExternalDependecyToTask(TaskType* task) { TRAP(false && "This method should be implemented"); };
protected:
	std::unordered_set<TaskGroup<TaskType>*>		m_dependencies;
	std::vector<TaskType*>							m_tasks;

	std::string					m_groupName;
};

////////////////////////////////////////////////////////////////////
//TaskGroup template definition
////////////////////////////////////////////////////////////////////

template<class TaskType>
TaskGroup<TaskType>::TaskGroup(const std::string& groupName)
	: m_groupName(groupName)
{

}

template<class TaskType>
TaskGroup<TaskType>::~TaskGroup()
{
	for (auto task : m_tasks)
		task->Destroy();
}

template<class TaskType>
void TaskGroup<TaskType>::AddDependencies(const std::vector<TaskGroup*> deps)
{
	//TODO check if some deps are already in the vector / set
	std::copy(deps.begin(), deps.end(), std::inserter(m_dependencies, m_dependencies.begin()));
}

template<class TaskType>
void TaskGroup<TaskType>::AddTask(TaskType* task)
{
	//TODO check if some deps are already in the vector / set
	m_tasks.push_back(task);
}

template<class TaskType>
void TaskGroup<TaskType>::Execute()
{
	std::cout << "Executing " << m_groupName << " ..." << std::endl;
	for (auto& task : m_tasks)
		task->Execute();
}

////////////////////////////////////////////////////////////////////
//TaskGraph<T>
////////////////////////////////////////////////////////////////////

template<class T>
class TaskGraph
{
public:
	TaskGraph();
	virtual ~TaskGraph();

	void AddTaskGroup(T* group);
	void Execute();

	//void UnitTest();
protected:
	virtual void ConstructGraph();
	void TopologicalSort();
protected:
	struct Node
	{
		enum class Status
		{
			Unvisited,
			Processing,
			Visited
		};

		Node(T* tg)
			: Group(tg)
			, Status(Status::Unvisited)
		{}

		T*								Group;
		std::unordered_set<Node*>		Children;
		enum class Status				Status;
	};
protected:
	std::vector<T*>				m_groups; //TODO maybe remove
	std::unordered_set<Node*>	m_nodes;

	std::vector<T*>				m_sortedTaskGroups;
};


////////////////////////////////////////////////////////////////////
//TaskGroup implementation
////////////////////////////////////////////////////////////////////
template<class T>
TaskGraph<T>::TaskGraph()
{

}

template<class T>
TaskGraph<T>::~TaskGraph()
{
	for (auto group : m_groups)
		group->Destroy();
}

template<class T>
void TaskGraph<T>::AddTaskGroup(T* group)
{
	//TODO check if some deps are already in the vector / set
	m_groups.push_back(group);

	TRAP(std::find_if(m_nodes.begin(), m_nodes.end(), [group](const Node* node)
	{
		return node->Group == group;
	}) == m_nodes.end());

	m_nodes.insert(new Node(group));
}

template<class T>
void TaskGraph<T>::ConstructGraph()
{
	for (auto currNode : m_nodes)
	{
		for (auto& dep : currNode->Group->GetDependecies())
		{

			auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&dep](const Node* node)
			{
				return node->Group == dep;
			});
			TRAP(it != m_nodes.end());
			(*it)->Children.insert(currNode);
		}
		currNode->Status = Node::Status::Unvisited;
	}
}

template<class T>
void TaskGraph<T>::Execute()
{
	for (auto& group : m_sortedTaskGroups)
	{
		group->Execute();
	}
}

template<class T>
void TaskGraph<T>::TopologicalSort()
{
	ConstructGraph();
	m_sortedTaskGroups.clear();
	std::vector<Node*> traversalStack;

	for (auto node : m_nodes)
	{
		if (node->Group->GetDependecies().empty())
			traversalStack.push_back(node);
	}

	TRAP(!traversalStack.empty()  && "the task graph is not correct. At least one group should have no dependecies");

	while (!traversalStack.empty())
	{
		Node* currNode = traversalStack.back();

		if (currNode->Status == Node::Status::Processing || currNode->Children.empty())
		{
			currNode->Status = Node::Status::Visited;
			traversalStack.pop_back();
			m_sortedTaskGroups.push_back(currNode->Group);
			continue;
		}

		currNode->Status = Node::Status::Processing;

		for (auto& child : currNode->Children)
		{
			TRAP(child->Status != Node::Status::Processing && "The graph has a cycle and cant be sorted based on dependencies");
			if (child->Status == Node::Status::Unvisited)
				traversalStack.push_back(child);
		}
	}

	std::reverse(m_sortedTaskGroups.begin(), m_sortedTaskGroups.end());
}

/////////////////////////////////////
//Render task attempt
//////////////////////////////////////

class RenderTask : public Task
{
public:
	RenderTask(std::vector<std::string>& inAttachments,
		std::vector<std::string>& outAttachments,
		const std::function<void(void)>& exec);

	RenderTask(std::vector<std::string> inAttachments,
		std::vector<std::string> outAttachments,
		const std::function<void(void)>& exec);

	const std::vector<std::string>& GetInAttachments() const { return m_inAttachments; }
	const std::vector<std::string>& GetOutAttachments() const { return m_outAttachments; }
	//TODO debug value
	int TaskIndex;

private:
	//TODO! The type for attachments (now string) is a placeholder type. This should be changed
	std::vector<std::string>		m_inAttachments; //could be empty
	std::vector<std::string>		m_outAttachments; //should not be empty. Every render task will write at least one framebuffer attachment

	std::function<void(int)>		m_prepareFunc;
};

/////////////////////////////////////
//Render task group
//////////////////////////////////////

class RenderTaskGroup : public TaskGroup<RenderTask>
{
public:
	RenderTaskGroup(const std::string& groupName);
	void Init();
	void PostInit();

private:
	//in this method, the inputs and outputs will be defined based on the task list. So, this method should be called only when all the tasks that is wanted to be executed by the group, will be inserted
	void DefineGroupIO();
	//in this method we detect the task dependecies that will be translate later in subpass dependencies for render pass creation. Task will be executed in the order in which they were added to the task group. A task it's equivalent with a subpass in a render pass
	void DetectTaskDependecies();

	bool PrecedeTask(RenderTask* task) override;
	void AddExternalDependecyToTask(RenderTask* task) override;

public: //TODO change private
	//TODO! The type for attachments (now string) is a placeholder type. This should be changed
	std::vector<std::string> m_Inputs;
	std::vector<std::string> m_Outputs;

	//TODO Add render pass that will be created in the init method
	//TODO Add a finish event
};

/////////////////////////////////////
//Render graph attempt
//////////////////////////////////////

class RenderGraph : TaskGraph<RenderTaskGroup>
{
public:
	void Prepare();
	void UnitTest();
};

//=========================TODO delete

class TestTaskGraph : public TaskGraph<TaskGroup<Task>>
{
public:
	void UnitTest();
};