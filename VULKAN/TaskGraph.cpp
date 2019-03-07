#include "TaskGraph.h"

#include <cstdint>
#include <unordered_map>

////////////////////////////////////////////////////////////////////
//Task
////////////////////////////////////////////////////////////////////

Task::Task(const std::function<void(void)>& func)
	: m_task(func)
{

}

Task::~Task()
{

}

void Task::Execute()
{
	m_task();
}

/////////////////////////////////////
//Render task attempt
//////////////////////////////////////

RenderTask::RenderTask(std::vector<std::string>& inAttachments, std::vector<std::string>& outAttachments, const std::function<void(void)>& func)
	: Task(func)
	, m_inAttachments(inAttachments)
	, m_outAttachments(outAttachments)
{
}

RenderTask::RenderTask(std::vector<std::string> inAttachments, std::vector<std::string> outAttachments, const std::function<void(void)>& func)
	: Task(func)
	, m_inAttachments(inAttachments)
	, m_outAttachments(outAttachments)
{
}

/////////////////////////////////////
//Render task group
//////////////////////////////////////

RenderTaskGroup::RenderTaskGroup(const std::string& groupName)
	: TaskGroup<RenderTask>(groupName)
{

}

void RenderTaskGroup::Init()
{
	DefineGroupIO();
}

void RenderTaskGroup::PostInit()
{
	DetectTaskDependecies();
}

void RenderTaskGroup::DefineGroupIO()
{
	std::unordered_set<std::string>	inputs;
	std::unordered_set<std::string> outputs;
	for (auto task : m_tasks)
	{
		for (const auto& in : task->GetInAttachments())
			inputs.insert(in);

		for (const auto& out : task->GetOutAttachments())
			outputs.insert(out);

	}


	std::copy(inputs.begin(), inputs.end(), std::back_inserter(m_Inputs));
	std::copy(outputs.begin(), outputs.end(), std::back_inserter(m_Outputs));
}

void RenderTaskGroup::DetectTaskDependecies()
{
	std::cout << "=======Group: " << m_groupName << std::endl;

	for (uint32_t i = 0; i < m_tasks.size(); ++i)
	{
		std::cout << "Task " << i << ":" << std::endl;
		std::cout << "\t Inputs: { ";
		for (const auto& in : m_tasks[i]->GetInAttachments())
			std::cout << in << ", ";
		std::cout << " } " << std::endl;

		std::cout << "\t Outputs: { ";
		for (const auto& out : m_tasks[i]->GetOutAttachments())
			std::cout << out << ", ";
		std::cout << " } " << std::endl << std::endl;
	}

	std::unordered_map<std::string, std::unordered_set<RenderTask*>> outputList;
	
	std::cout << "Dependecies: " << std::endl;

	for (auto task : m_tasks)
		for (const auto& out : task->GetOutAttachments())
			outputList[out].insert(task);

	for (uint32_t i = 0; i < m_tasks.size(); ++i)
	{
		auto task = m_tasks[i];
		for (const auto& in : task->GetInAttachments())
		{
			auto it = outputList.find(in);
			if (it != outputList.end())
			{
				for (auto taskDep : it->second)
				{
					auto secondTaskIt = std::find(m_tasks.begin(), m_tasks.end(), taskDep);
					TRAP(secondTaskIt != m_tasks.end());
					std::cout << "Task " << secondTaskIt - m_tasks.begin() << " -> ";
					std::cout << "Task " << i << std::endl;
					//TODO! Here we add to the dependecies list
				}
			}
		}

		if (task->GetInAttachments().empty())
			std::cout << "Task " << i << " no deps" << std::endl;
		else
		{
			for (const auto& deps : m_dependencies)
			{
				task->TaskIndex = i;
				if (deps->PrecedeTask(task))
					deps->AddExternalDependecyToTask(task);
			}
		}

	}
}

bool RenderTaskGroup::PrecedeTask(RenderTask* task)
{
	for (const auto& in : task->GetInAttachments())
		if (std::find(m_Outputs.begin(), m_Outputs.end(), in) != m_Outputs.end())
			return true;

	return false;
}

void RenderTaskGroup::AddExternalDependecyToTask(RenderTask* task)
{
	std::cout << "External deps: " << GetName() <<  " -> " << task->TaskIndex << std::endl;
}

/////////////////////////////////////
//Render gragph
//////////////////////////////////////
void  RenderGraph::Prepare()
{
	for (auto group : m_groups)
		group->Init();

	std::unordered_map<std::string, std::unordered_set<RenderTaskGroup*>> outputList;

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
				std::vector<TaskGroup<RenderTask>*> deps;

				std::copy(it->second.begin(), it->second.end(), std::back_inserter(deps));
				auto selfIt = std::find(deps.begin(), deps.end(), group);
				if (selfIt != deps.end())
					deps.erase(selfIt);

				group->AddDependencies(deps);
			}
		}
	}

	for (auto group : m_groups)
		group->PostInit();
}

void PrintString(const std::string& str)
{
	std::cout << "\t" << str << std::endl;
}

void  RenderGraph::UnitTest()
{
	RenderTaskGroup* shadowMap = new RenderTaskGroup("ShadowMap");
	RenderTaskGroup* gbuffer = new RenderTaskGroup("GBuffer");
	RenderTaskGroup* preLight = new RenderTaskGroup("PreLighting");
	RenderTaskGroup* lighting = new RenderTaskGroup("Lighting");

	shadowMap->AddTask(new RenderTask({}, { "shadowMapAtt" }, std::bind(&PrintString, "CreateShadowMap")));
	gbuffer->AddTask(new RenderTask({}, { "AlbedoAtt", "NormalAtt", "SpecularAtt", "DepthAtt"}, std::bind(&PrintString, "CreateGBuffer")));
	preLight->AddTask(new RenderTask({ "shadowMapAtt" }, { "shadowResolveAtt" }, std::bind(&PrintString, "ResolveShadows")));
	preLight->AddTask(new RenderTask({ "DepthAtt" }, { "ambientOcclussionAtt" }, std::bind(&PrintString, "AmbientOcclussion")));
	lighting->AddTask(new RenderTask({ "AlbedoAtt", "NormalAtt", "SpecularAtt", "shadowResolveAtt", "ambientOcclussionAtt", "DepthAtt" }, { "LightAtt" }, std::bind(&PrintString, "DirrectionalLight")));
	lighting->AddTask(new RenderTask({ "AlbedoAtt", "NormalAtt", "SpecularAtt", "shadowResolveAtt", "ambientOcclussionAtt", "LightAtt", "DepthAtt" }, { "FinalLightAtt" }, std::bind(&PrintString, "DefferedTileRendering")));

	AddTaskGroup(shadowMap);
	AddTaskGroup(preLight);
	AddTaskGroup(gbuffer);
	AddTaskGroup(lighting);

	Prepare();

	TopologicalSort();
	
	Execute();
}

////////////////////////////////TODO DELETE================

class A
{
public:
	A(int i)
		: m_i(i)
	{}

	void print()
	{
		std::cout << m_i << std::endl;
	}
private:
	int m_i;
};


void TestTaskGraph::UnitTest()
{
	//TaskGroup* high = new TaskGroup("high");
	//TaskGroup* normal = new TaskGroup("normal");
	//TaskGroup* normal2 = new TaskGroup("normal2");
	//TaskGroup* low = new TaskGroup("low");

	//const int n = 15;
	//std::vector<A*> ass;
	//ass.resize(n);

	//for (int i = 0; i < n; ++i)
	//	ass[i] = new A(i);

	//std::vector<int> highIndexes = { 0 };
	//std::vector<int> normalIndexes = { 1, 3, 6, 7, 9 };
	//std::vector<int> normalIndexes2 = { 10, 11, 12};
	//std::vector<int> lowIndexes = { 4, 8 };

	//auto init = [&](TaskGroup* group, const std::vector<int>& indexes)
	//{
	//	for (auto i : indexes)
	//	{
	//		group->AddTask(new Task(std::bind(&A::print, ass[i])));
	//	}
	//};

	//init(high, highIndexes);
	//init(normal, normalIndexes);
	//init(normal2, normalIndexes2);
	//init(low, lowIndexes);

	//normal->AddDependencies({ high });
	//normal2->AddDependencies({ high });
	//low->AddDependencies({normal, normal2});

	//AddTaskGroup(high);
	//AddTaskGroup(normal);
	//AddTaskGroup(normal2);
	//AddTaskGroup(low);

	TaskGroup<Task>* A = new TaskGroup<Task>("A");
	TaskGroup<Task>* B = new TaskGroup<Task>("B");
	TaskGroup<Task>* C = new TaskGroup<Task>("C");
	TaskGroup<Task>* D = new TaskGroup<Task>("D");
	TaskGroup<Task>* E = new TaskGroup<Task>("E");
	TaskGroup<Task>* F = new TaskGroup<Task>("F");

	B->AddDependencies({A});
	C->AddDependencies({ A, B });
	D->AddDependencies({ C });
	E->AddDependencies({ B });
	F->AddDependencies({ B, C });

	AddTaskGroup(A);
	AddTaskGroup(B);
	AddTaskGroup(C);
	AddTaskGroup(D);
	AddTaskGroup(E);
	AddTaskGroup(F);

	TopologicalSort();
	Execute();
}