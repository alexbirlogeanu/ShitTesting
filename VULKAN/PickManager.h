#pragma once

#include "VulkanLoader.h"

#include <vector>
#include <bitset>

#include "Renderer.h"
#include "defines.h"
#include "Utils.h"
#include "Singleton.h"

#define MAXPICKABLES 16
#include "Mesh.h"

class BufferHandle;
class CPickable
{
public:
    CPickable();
    virtual ~CPickable();

    unsigned int GetId() const { return m_id; }
    virtual glm::mat4 GetModelMatrix()=0;
    virtual BoundingBox3D GetBoundingBox() const =0;
    virtual void Render()=0;
    virtual void GetPickableDescription(std::vector<std::string>& texts)=0;
    virtual bool ChangePickableProperties(unsigned int key)=0;
private:
    static unsigned int ms_nextId;
    unsigned int        m_id;
};

struct SPickParams
{
    glm::mat4   MVPMatrix;
    glm::uvec4  ID;
    glm::vec4   BBMin;
    glm::vec4   BBMax;
};

class CPickRenderer : public CRenderer
{
public:
    CPickRenderer(VkRenderPass renderPass);
    virtual ~CPickRenderer();

    virtual void Init() override;
    virtual void Render() override;
	virtual void PreRender() override;

    void AddNode(CPickable* p);
    void RemoveNode(CPickable* p);

    void SetCoords(glm::uvec2 coords) { m_mouseCoords = coords; };
    unsigned int GetSelectedID() { return m_selectedId; }
protected:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>&, unsigned int& maxSets) override;
    VkDescriptorSet AllocateDescSet();

    void UpdateObjects();
    void CreateBBMesh();
    void GetPickedId();

    bool AreCoordsValid(glm::uvec2 coords);

    struct SNode
    {
        CPickable*      Pickable;
        VkDescriptorSet Set;
        BufferHandle*   buffer;
    };
    friend class CPickManager;

private:
    VkDescriptorSetLayout   m_descriptorSetLayout;
    std::vector<SNode>  m_nodes;

    SNode*              m_pickedNode;

    CGraphicPipeline           m_idPipeline; //lol
    CGraphicPipeline           m_bbPipeline; //lol

    glm::uvec2          m_mouseCoords;
    glm::mat4           m_projection;
    Mesh*               m_bbMesh;
    unsigned int        m_selectedId;

    VkBuffer            m_copyBuffer;
    VkDeviceMemory      m_copyMemory;

    //memory management
    std::bitset<MAXPICKABLES>   m_memoryPool;
    BufferHandle*               m_uniformBuffer;
};
class CUIManager;
class CUIText;
//class CUITextContainer;
class CPickManager : public Singleton<CPickManager>
{
	friend class Singleton<CPickManager>;
public:
    void Register(CPickable* p);
    void Unregister(CPickable* p);

    void RegisterPick(glm::uvec2 coords);
    void RegisterKey(unsigned int key);
    void Update();

    void Setup();
    void CreateDebug();

    void ToggleEditMode();
private:
    CPickManager();
    virtual ~CPickManager();

    void SetupRenderpass(const FramebufferDescription& fbDesc);
    void UpdateEditInfo();

private:
    bool                        m_editMode;
    bool                        m_needRefresh;

    std::vector<CPickable*>     m_pickableObjects;
    std::vector<CPickable*>     m_tempPickableObjects;

    unsigned int                m_selectedID;
    CPickable*                  m_pickedObject;

    VkRenderPass                m_renderPass;
    CPickRenderer*              m_pickRenderer;

    //debug
    enum 
    {
        Title = 0,
        Selected,
        SlotStart = 2,
        SlotsEnd = 4,
        Count = SlotsEnd + 1
    };
    //CUITextContainer*           m_editModeInfo;
};

CPickManager* GetPickManager();
