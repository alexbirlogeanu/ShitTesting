#include "ResourceLoader.h"

#include "Texture.h"
#include "Mesh.h"

ResourceLoader::ResourceLoader()
{

}

ResourceLoader::~ResourceLoader()
{

}

void ResourceLoader::LoadTexture(CTexture** pText)
{
	const std::string filename = (*pText)->GetFilename();
	auto it = m_texturesMap.find(filename);

	if (it != m_texturesMap.end())
	{
		delete *pText;
		*pText = it->second;
		return;
	}

	SImageData imgData;
	Read2DTextureData(imgData, std::string(TEXTDIR) + filename, (*pText)->GetIsSRGB());
	(*pText)->CreateTexture(imgData, true);//hmmmmmmmmmmmmmmmmm
	m_texturesMap.emplace(filename, (*pText));
}

void ResourceLoader::LoadMesh(Mesh** mesh)
{
	const std::string filename = (*mesh)->GetFilename();
	auto it = m_meshMap.find(filename);

	if (it != m_meshMap.end())
	{
		delete *mesh;
		*mesh = it->second;
		return;
	}

	(*mesh)->LoadFromFile(filename); //hmmmmmmmmmmmmmmmmm
	m_meshMap.emplace(filename, *mesh);
}