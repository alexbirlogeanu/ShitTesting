#pragma once

#include "glm/glm.hpp"

#include <vector>

enum class CollisionResult
{
	Outside,
	Intersect,
	Inside
};

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

	glm::vec2 GetCenter() const
	{
		return (Max + Min) / 2.0f;
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

	glm::vec3 GetPositiveVertex(const glm::vec3& dir)
	{
		glm::vec3 p = Min;
		if (dir.x >= 0.0f)
			p.x = Max.x;
		if (dir.y >= 0.0f)
			p.y = Max.y;
		if (dir.z >= 0.0f)
			p.z = Max.z;

		return p;
	}

	glm::vec3 GetNegativeVertex(const glm::vec3& dir)
	{
		glm::vec3 n = Max;
		if (dir.x >= 0.0f)
			n.x = Min.x;
		if (dir.y >= 0.0f)
			n.y = Min.y;
		if (dir.z >= 0.0f)
			n.z = Min.z;

		return n;
	}
};

struct Plane
{
	glm::vec3 Normal;
	glm::vec3 Point;

	Plane()
		: Normal(0.0f, 1.0f, 0.0f)
		, Point(0.0f, 0.0f, 0.0f)
	{}

	Plane(const glm::vec3& normal, const glm::vec3& point)
		: Normal(normal)
		, Point(point)
	{}

	float GetDistance(const glm::vec3& point) const
	{
		return glm::dot(Normal, point) - glm::dot(Normal, Point);
	}
};

class CFrustum
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

	enum FrustrumPlane : uint32_t
	{
		Near = 0,
		Far,
		Right,
		Left,
		Top,
		Bottom,
		PLCount
	};

	CFrustum() {}
	CFrustum(float N, float F);
	virtual ~CFrustum();

	void Update(glm::vec3 p, glm::vec3 dir, glm::vec3 up, glm::vec3 right, float fov);
	glm::vec3 GetPoint(unsigned int p) const { return m_points[p]; }

	CollisionResult Collision(const BoundingBox3D& bb) const;
private:
	void Construct(glm::vec3 p, glm::vec3 dir, glm::vec3 up, glm::vec3 right, float fov);
	void GetPointsFromPlane(glm::vec3 start, glm::vec3 dir, glm::vec3 right, glm::vec3 up, float dist, float fov, glm::vec3 poitns[4]);
	void ConstructPlanes(const glm::vec3& viewDir);
private:
	glm::vec3       m_points[FPCount];
	Plane			m_planes[PLCount];

	float           m_near;
	float           m_far;
};


namespace Geometry
{
	glm::vec3 ComputeNormal(const glm::vec3& center, const glm::vec3& p1, const glm::vec3& p2);
};
