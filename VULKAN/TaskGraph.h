#pragma once


#include "defines.h"

#include <functional>
#include <vector>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <iostream>

class Task
{
	HEAP_ONLY(Task);
public:
	Task(const std::function<void(void)>& func);
	virtual void Execute();
	
protected:
	std::function<void(void)> m_task;
};

template<class T>
class Base
{
public:
	T* GetThis() { return this; }
};

////////////////////////////////////////////////////////////////////
//TaskGroup<T, Derived>
////////////////////////////////////////////////////////////////////

template<class TaskType, class Derived>
class TaskGroupBase
{
	HEAP_ONLY(TaskGroupBase);
public:
	TaskGroupBase(const std::string& groupName);
	
	void AddDependencies(const std::vector<Derived*> deps);
	virtual void AddTask(TaskType* task);

	const std::unordered_set<Derived*>& GetDependecies() const { return m_dependencies; }
	const std::string& GetName() const { return m_groupName; }
	virtual void Execute();

protected:
	std::unordered_set<Derived*>					m_dependencies;
	std::vector<TaskType*>							m_tasks;

	std::string										m_groupName;
};


////////////////////////////////////////////////////////////////////
//TaskGroupBase template definition
////////////////////////////////////////////////////////////////////

template<class TaskType, class Derived>
TaskGroupBase<TaskType, Derived>::TaskGroupBase(const std::string& groupName)
	: m_groupName(groupName)
{

}

template<class TaskType, class Derived>
TaskGroupBase<TaskType, Derived>::~TaskGroupBase()
{
	for (auto task : m_tasks)
		task->Destroy();
}

template<class TaskType, class Derived>
void TaskGroupBase<TaskType, Derived>::AddDependencies(const std::vector<Derived*> deps)
{
	//TODO check if some deps are already in the vector / set
	std::copy(deps.begin(), deps.end(), std::inserter(m_dependencies, m_dependencies.begin()));
}

template<class TaskType, class Derived>
void TaskGroupBase<TaskType, Derived>::AddTask(TaskType* task)
{
	//TODO check if some deps are already in the vector / set
	m_tasks.push_back(task);
}

template<class TaskType, class Derived>
void TaskGroupBase<TaskType, Derived>::Execute()
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
