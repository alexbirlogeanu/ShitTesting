#include "MeshLoader.h"

#include "defines.h"
#include "assimp/scene.h"           // Output data structure
#include "assimp/postprocess.h"     // Post processing flags

MeshLoader::MeshLoader()
	: m_scene(nullptr)
	, m_pVertexes(nullptr)
	, m_pIndexes(nullptr)
	, m_vertexCount(0)
	, m_indexCount(0)
{

}

MeshLoader::~MeshLoader()
{

}

void MeshLoader::LoadInto(const std::string filename, VertexContainer* outVertexes, IndexContainer* outIndexes)
{
	m_pVertexes = outVertexes;
	m_pIndexes = outIndexes;

	m_scene = m_importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);
	TRAP(m_scene);
	TRAP(m_scene->HasMeshes());

	CountMeshElements();
	//alloc indexes
	m_pVertexes->reserve(m_vertexCount);
	m_pIndexes->reserve(m_indexCount);

	ConvertScene();
}

void MeshLoader::CountMeshElements()
{
	unsigned int nMeshes = m_scene->mNumMeshes;
	aiMesh** meshes = m_scene->mMeshes;
	for (unsigned int i = 0; i < nMeshes; ++i)
	{
		aiMesh* currMesh = meshes[i];
		TRAP(currMesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE);
		m_vertexCount += currMesh->mNumVertices;
		m_indexCount += (currMesh->mNumFaces * 3);
	}
}

void MeshLoader::ConvertScene()
{
	unsigned int nMeshes = m_scene->mNumMeshes;
	aiMesh** meshes = m_scene->mMeshes;
	unsigned int vertexOffset = 0;
	for (unsigned int i = 0; i < nMeshes; ++i)
	{
		aiMesh* currMesh = meshes[i];
		ConvertMesh(currMesh, vertexOffset);
		vertexOffset += currMesh->mNumVertices;
	}
}

void MeshLoader::ConvertMesh(aiMesh* currMesh, unsigned int vertexOffset)
{
	aiVector3D* vPositions = currMesh->mVertices;
	glm::vec3 pos;
	for (unsigned int v = 0; v < currMesh->mNumVertices; ++v)
	{
		pos = glm::vec3(vPositions[v].x, vPositions[v].y, vPositions[v].z);
		m_pVertexes->push_back(SVertex(pos));
	}
	if (currMesh->HasNormals())
	{
		aiVector3D* vNormals = currMesh->mNormals;
		for (unsigned int n = 0; n < currMesh->mNumVertices; ++n)
			(*m_pVertexes)[n + vertexOffset].normal = glm::vec3(vNormals[n].x, vNormals[n].y, vNormals[n].z);
	}

	if (currMesh->HasTextureCoords(0))
	{
		aiVector3D** vTextCoords = currMesh->mTextureCoords;
		for (unsigned int u = 0; u < currMesh->mNumVertices; ++u)
			(*m_pVertexes)[u + vertexOffset].uv = glm::vec2(vTextCoords[0][u].x, vTextCoords[0][u].y);
	}

	if (currMesh->HasTangentsAndBitangents())
	{
		aiVector3D* vBitangent = currMesh->mBitangents;
		aiVector3D* vTangent = currMesh->mTangents;

		for (unsigned int bt = 0; bt < currMesh->mNumVertices; ++bt)
		{
			(*m_pVertexes)[bt + vertexOffset].bitangent = glm::vec3(vBitangent[bt].x, vBitangent[bt].y, vBitangent[bt].z);
			(*m_pVertexes)[bt + vertexOffset].tangent = glm::vec3(vTangent[bt].x, vTangent[bt].y, vTangent[bt].z);
		}
	}

	aiFace* faces = currMesh->mFaces;
	for (unsigned int f = 0; f < currMesh->mNumFaces; ++f)
	{
		aiFace face = faces[f];
		for (unsigned int v = 0; v < 3; ++v)
			m_pIndexes->push_back(face.mIndices[v] + vertexOffset);
	}
}
