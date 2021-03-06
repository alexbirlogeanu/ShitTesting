#pragma once

#include <vector>

class GenericEntry
{
public:
    GenericEntry()
        : m_ptr(nullptr)
    {
    }

    virtual ~GenericEntry()
    {}

    inline void* Get() const  { return m_ptr; }
protected:
    inline void Set(void* ptr)
    {
        m_ptr = ptr;
    }
private:
    void*   m_ptr;
};

template<typename T>
class Entry : public GenericEntry
{
public:
    Entry(const T* ptr)
    {
        Set((void*)ptr);
    }

    virtual ~Entry()
    {}
};

enum EResourceType
{
	EResourceType_FinalImage,
	EResourceType_AlbedoImage,
	EResourceType_SpecularImage,
	EResourceType_NormalsImage,
	EResourceType_PositionsImage,
	EResourceType_DepthBufferImage,
	EResourceType_ShadowMapImage,
	EResourceType_AOBufferImage,
	EResourceType_ResolvedShadowImage,
	EResourceType_SunImage,
	EResourceType_AfterPostProcessImage,
	EResourceType_VolumetricImage,
	EResourceType_ShadowProjViewMat,
	EResourceType_ShadowMapSplits,
	EResourceType_ShadowRenderPipeline, //this is not good here
	EResourceType_ShadowRenderPass,
	EResourceType_Count
};

class ResourceTable
{
public:
    ResourceTable()
    {
        m_resources.resize(EResourceType_Count);
    }

    template<typename T>
    void SetAs(const T* ptr, EResourceType type)
    {
        TRAP(type < EResourceType_Count && "Invalid type");
        TRAP(!m_resources[type] && "Resource already registered");
        TRAP(ptr && "Handle is NULL");
        m_resources[type] = new Entry<T>(ptr);
    }

    template<typename T>
    T GetAs(EResourceType type)
    {
        TRAP(type < EResourceType_Count && "Invalid type");
        TRAP(m_resources[type] && "Resource not registered");
        Entry<T>* entry = dynamic_cast<Entry<T>*>(m_resources[type]);
        TRAP(entry && "Incorrect type of resource");
        T* ptr = (T*)entry->Get();
        TRAP(ptr && "Handle is NULL");
        return *ptr;
    }

	template<typename T>
	T* GetAsPtr(EResourceType type)
	{
		TRAP(type < EResourceType_Count && "Invalid type");
		TRAP(m_resources[type] && "Resource not registered");
		Entry<T>* entry = dynamic_cast<Entry<T>*>(m_resources[type]);
		TRAP(entry && "Incorrect type of resource");
		T* ptr = (T*)entry->Get();
		TRAP(ptr && "Handle is NULL");
		return ptr;
	}
private:
    void Validate(EResourceType type)
    {
        TRAP(type < EResourceType_Count && "Invalid type");
        TRAP(!m_resources[type] && "Resource already registered");
    }
    std::vector<GenericEntry*>  m_resources;
};

extern ResourceTable   g_commonResources;