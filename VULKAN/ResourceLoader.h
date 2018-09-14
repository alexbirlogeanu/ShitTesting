#pragma once

#include "Singleton.h"

#include <unordered_map>

class Mesh;
class CTexture;

class ResourceLoader : public Singleton<ResourceLoader>
{
	friend class Singleton<ResourceLoader>;
public:
	void LoadTexture(CTexture** pText);
	void LoadMesh(Mesh** mesh);
private:
	ResourceLoader();
	virtual ~ResourceLoader();

	std::unordered_map<std::string, CTexture*>      m_texturesMap;
	std::unordered_map<std::string, Mesh*>          m_meshMap;
};