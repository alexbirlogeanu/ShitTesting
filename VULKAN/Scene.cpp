#include "Scene.h"

#include "Object.h"
#include <random>

//////////////////////////////////////////////////////////////////////////
//CScene
//////////////////////////////////////////////////////////////////////////
std::unordered_set<Object*> CScene::ms_sceneObjects; //i got the objects in 2 places: here and in ObjectFactory. BUt fuck it. not important
BoundingBox3D CScene::ms_sceneBoundingBox;
const glm::uvec2 CScene::TerrainGridSize (64, 64);
const glm::vec2 CScene::TerrainSize (20.0f, 20.0f);
const glm::vec3 CScene::TerrainTranslate = glm::vec3(0.0f, -5.0f, -3.0f); //lel

void CScene::AddObject(Object* obj)
{
	auto result = ms_sceneObjects.insert(obj);
	TRAP(result.second == true);

	CScene::UpdateBoundingBox();
}

void CScene::UpdateBoundingBox()
{
	ms_sceneBoundingBox.Max = ms_sceneBoundingBox.Min = glm::vec3();
	if (ms_sceneObjects.empty())
		return;

	glm::vec3 maxLimits(std::numeric_limits<float>::min());
	glm::vec3 minLimits(std::numeric_limits<float>::max());

	for (auto o = ms_sceneObjects.begin(); o != ms_sceneObjects.end(); ++o)
	{
		BoundingBox3D bb = (*o)->GetBoundingBox();
		std::vector<glm::vec3> bbPoints;
		bb.Transform((*o)->GetModelMatrix(), bbPoints);
		for (unsigned int i = 0; i < bbPoints.size(); ++i)
		{
			maxLimits = glm::max(maxLimits, bbPoints[i]);
			minLimits = glm::min(minLimits, bbPoints[i]);
		}
	}

	ms_sceneBoundingBox.Max = maxLimits;
	ms_sceneBoundingBox.Min = minLimits;
}

void CScene::CalculatePlantsPositions(glm::uvec2 vegetationGridSize, const std::vector<uint32_t>& plantsPerCell, std::vector<glm::vec3>& outPositions)
{
	unsigned int seed = 986923;
	std::mt19937 generator(seed);
	std::uniform_real_distribution<float> distribution(-0.4f, 0.4f);

	glm::vec2 cellScale = TerrainGridSize / vegetationGridSize;
	glm::vec2 cellLength = TerrainSize / glm::vec2(vegetationGridSize);

	SImageData heightMap;
	Read2DTextureData(heightMap, std::string(TEXTDIR) + "terrain3.png", false); //TODO centralize this

	for (uint32_t x = 0; x < vegetationGridSize.x; ++x)
	{
		for (uint32_t y = 0; y < vegetationGridSize.y; ++y)
		{
			uint32_t index = y * vegetationGridSize.x + x;
			uint32_t plants = plantsPerCell[index];
			//we place the plants in a grid so 2 dimensions
			uint32_t plantsPerRow = uint32_t(std::ceil(std::sqrt(plants)));

			if (plantsPerRow == 0)
				continue;

			glm::vec2 distanceBetweenPlants = cellLength / float(plantsPerRow);
			glm::vec2 cellStart = cellLength * glm::vec2(x, y);
			glm::vec2 cellCenter = cellStart + cellLength / 2.0f;
			glm::vec2 displacement = cellCenter - (2.0f * cellStart + (glm::vec2(plantsPerRow - 1) * distanceBetweenPlants)) / 2.0f; //u'll probably will forget about it

			for (uint32_t i = 0; i < plantsPerRow; ++i)
				for (uint32_t j = 0; j < plantsPerRow; ++j)
				{
					glm::vec2 pos = cellStart + glm::vec2(i + distribution(generator), j + distribution(generator)) * distanceBetweenPlants; //give random offsets
					pos += displacement;

					glm::vec2 heightPixel = pos / TerrainSize * glm::vec2(heightMap.width, heightMap.height);

					float height = heightMap.GetRed(uint32_t(heightPixel.x), uint32_t(heightPixel.y)) * 7.0f; //the max height of the terrain. can be found in terrainrenderer. TODO should be centralized

					outPositions.push_back(glm::vec3(pos.x, height, pos.y) + CScene::TerrainTranslate - glm::vec3(TerrainSize.x / 2.0f, 1.0f, TerrainSize.y / 2.0f)); //set height 0. it will be computed later based on heightmap
				}
		}
	}

	delete[] heightMap.data;
}
