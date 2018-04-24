#pragma once

#include <vector>

#include "assimp/Importer.hpp"
#include "SVertex.h"

struct aiScene;
struct aiMesh;
class MeshLoader 
{
public:
    typedef std::vector<SVertex> VertexContainer;
    typedef std::vector<unsigned int> IndexContainer;

    MeshLoader();
    virtual ~MeshLoader();

    void LoadInto(const std::string filename, VertexContainer* outVertexes, IndexContainer* outIndexes);

private:
    void CountMeshElements();
    void ConvertScene();

    void ConvertMesh(aiMesh* currMesh, unsigned int vertexOffset);
private:
    VertexContainer*    m_pVertexes;
    IndexContainer*     m_pIndexes;    

    unsigned int        m_vertexCount;
    unsigned int        m_indexCount;

    Assimp::Importer    m_importer;
    const aiScene*      m_scene;
};
