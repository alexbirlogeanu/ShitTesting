#pragma once

#include "glm/glm.hpp"

#include <vector>

struct BoundingBox2D
{
	glm::vec2 Min;
	glm::vec2 Max;

	BoundingBox2D(const glm::vec2& minLimits, const glm::vec2& maxLimits)
		: Min(minLimits)
		, Max(maxLimits)
	{}

	bool ContainsPoint(const glm::vec2& point) const
	{
		return Min.x <= point.x && Min.y <= point.y && point.x <= Max.x && point.y <= Max.y;
	}
};

struct BoundingBox3D
{
	glm::vec3 Min;
	glm::vec3 Max;

	BoundingBox3D()
	{}

	BoundingBox3D(const glm::vec3& minLimits, const glm::vec3 maxLimits)
		: Min(minLimits)
		, Max(maxLimits)
	{}

	void Construct(std::vector<glm::vec3>& points)
	{
		points.resize(8);
		points[0] = glm::vec3(Min.x, Max.y, Min.z); //L T F
		points[1] = glm::vec3(Max.x, Max.y, Min.z); // R T F
		points[2] = glm::vec3(Max.x, Min.y, Min.z); // R B F
		points[3] = glm::vec3(Min.x, Min.y, Min.z); // L B F
		points[4] = glm::vec3(Min.x, Max.y, Max.z); // L T B
		points[5] = glm::vec3(Max.x, Max.y, Max.z); // R T B
		points[6] = glm::vec3(Max.x, Min.y, Max.z); // R B B
		points[7] = glm::vec3(Min.x, Min.y, Max.z); //L B B
	}

	void Transform(const glm::mat4& tMatrix, std::vector<glm::vec3>& tPoints)
	{
		Construct(tPoints);
		for (unsigned int i = 0; i < tPoints.size(); ++i)
			tPoints[i] = glm::vec3(tMatrix * glm::vec4(tPoints[i], 1.0f));
	}
};

class CFrustrum
{
public:
	enum FrustrumPoints
	{
		NTL = 0,
		NTR,
		NBR,
		NBL,
		FTL,
		FTR,
		FBR,
		FBL,
		FPCount
	};

	CFrustrum(float N, float F);
	virtual ~CFrustrum();

	void Update(glm::vec3 p, glm::vec3 dir, glm::vec3 up, glm::vec3 right, float fov);
	glm::vec3 GetPoint(unsigned int p) const { return m_points[p]; }
private:
	void Construct(glm::vec3 p, glm::vec3 dir, glm::vec3 up, glm::vec3 right, float fov);
	void GetPointsFromPlane(glm::vec3 start, glm::vec3 dir, glm::vec3 right, glm::vec3 up, float dist, float fov, glm::vec3 poitns[4]);
private:
	glm::vec3       m_points[FPCount];

	float           m_near;
	float           m_far;
};
