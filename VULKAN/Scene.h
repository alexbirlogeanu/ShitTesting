#pragma once

#include "Geometry.h"
#include "Singleton.h"

#include <unordered_set>
class Object;
class KeyInput;
class DebugBoundingBox;
class Scene : public Singleton<Scene>
{
	friend class Singleton<Scene>;
public:
	//keep this shit for now
	const static glm::uvec2 TerrainGridSize;
	const static glm::vec2 TerrainSize;
	const static glm::vec3 TerrainTranslate; //lel

	void AddObject(Object* obj);
	BoundingBox3D GetBoundingBox() { return m_sceneBoundingBox; };
	void CalculatePlantsPositions(glm::uvec2 vegetationGridSize, const std::vector<uint32_t>& plantsPerCell, std::vector<glm::vec3>& outPositions);

	void Update(float dt);

	bool OnDebugKey(const KeyInput&);
private:
	Scene();
	virtual ~Scene();

	void UpdateBoundingBox();
	void FrustumCulling();
private:
	std::unordered_set<Object*>			m_sceneObjects;
	BoundingBox3D						m_sceneBoundingBox;


	//debug
	std::vector<DebugBoundingBox*>		m_debugBoundigBoxes;
};