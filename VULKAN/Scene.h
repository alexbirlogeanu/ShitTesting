#pragma once

#include "Mesh.h"

#include <unordered_set>
class Object;

class CScene
{
public:
	const static glm::uvec2 TerrainGridSize;
	const static glm::vec2 TerrainSize;
	const static glm::vec3 TerrainTranslate; //lel

	static void AddObject(Object* obj);
	static BoundingBox GetBoundingBox() { return ms_sceneBoundingBox; };
	static void CalculatePlantsPositions(glm::uvec2 vegetationGridSize, const std::vector<uint32_t>& plantsPerCell, std::vector<glm::vec3>& outPositions);

private:
	static void UpdateBoundingBox();
private:
	static std::unordered_set<Object*>      ms_sceneObjects;
	static BoundingBox                      ms_sceneBoundingBox;

};