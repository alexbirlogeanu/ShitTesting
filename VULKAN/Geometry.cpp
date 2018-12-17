#include "Geometry.h"


namespace Geometry
{
	glm::vec3 ComputeNormal(const glm::vec3& center, const glm::vec3& p1, const glm::vec3& p2)
	{
		glm::vec3 e1 = p2 - center;
		glm::vec3 e2 = p1 - center;

		glm::vec3 normal = glm::vec3(e1.y * e2.z, e1.z * e2.x, e1.x * e2.y) - glm::vec3(e1.z * e2.y, e1.x * e2.z, e1.y * e2.x);

		return glm::normalize(normal);
	}
};
//////////////////////////////////////////////////////////////////////////////////////
//CFrustum
//////////////////////////////////////////////////////////////////////////////////////

CFrustum::CFrustum(float N, float F)
	: m_near(N)
	, m_far(F)
{
}

CFrustum::~CFrustum()
{
}

void CFrustum::Update(glm::vec3 eye, glm::vec3 dir, glm::vec3 up, glm::vec3 right, float fov)
{
	Construct(eye, dir, up, right, fov);
}

CollisionResult CFrustum::Collision(const BoundingBox3D& bb) const
{
	CollisionResult result = CollisionResult::Inside;

	auto getPositiveVertex = [](const BoundingBox3D& bb, const glm::vec3& normal)
	{
		glm::vec3 p = bb.Min;
		if (normal.x >= 0.0f)
			p.x = bb.Max.x;
		if (normal.y >= 0.0f)
			p.y = bb.Max.y;
		if (normal.z >= 0.0f)
			p.z = bb.Max.z;

		return p;
	};

	auto getNegativeVertex = [](const BoundingBox3D& bb, const glm::vec3& normal)
	{
		glm::vec3 n = bb.Max;
		if (normal.x >= 0.0f)
			n.x = bb.Min.x;
		if (normal.y >= 0.0f)
			n.y = bb.Min.y;
		if (normal.z >= 0.0f)
			n.z = bb.Min.z;

		return n;
	};

	for (uint32_t i = 0; i < FrustrumPlane::PLCount; ++i)
	{
		glm::vec3 p = getPositiveVertex(bb, m_planes[i].Normal);

		if (m_planes[i].GetDistance(p) < 0.0f)
			return CollisionResult::Outside;

		glm::vec3 n = getNegativeVertex(bb, m_planes[i].Normal);

		if (m_planes[i].GetDistance(n) < 0.0f)
			result = CollisionResult::Intersect;
	}

	return result;
}

void CFrustum::Construct(glm::vec3 eye, glm::vec3 dir, glm::vec3 up, glm::vec3 right, float fov)
{
	glm::vec3 nearPoints[4];
	glm::vec3 farPoints[4];

	GetPointsFromPlane(eye, dir, right, up, m_near, fov, nearPoints);
	GetPointsFromPlane(eye, dir, right, up, m_far, fov, farPoints);

	memcpy(m_points, nearPoints, sizeof(nearPoints));
	memcpy(&m_points[4], farPoints, sizeof(farPoints));

	ConstructPlanes(dir);
}

void CFrustum::GetPointsFromPlane(glm::vec3 start, glm::vec3 dir, glm::vec3 right, glm::vec3 up, float dist, float fov, glm::vec3 points[4])
{
	const float aspectRatio = 16.0f / 9.0f;
	glm::vec3 center = start + dir * dist;
	float halfHeight = dist * glm::tan(fov / 2.0f);
	float halfWidth = aspectRatio * halfHeight;

	points[0] = center + up * halfHeight - right * halfWidth; // top left
	points[1] = center + up * halfHeight + right * halfWidth; //top right
	points[2] = center - up * halfHeight + right * halfWidth; //bot right
	points[3] = center - up * halfHeight - right * halfWidth; //bot left
}

void CFrustum::ConstructPlanes(const glm::vec3& viewDir)
{
	m_planes[FrustrumPlane::Near] = Plane(glm::normalize(viewDir), m_points[FrustrumPoints::NTL]);
	m_planes[FrustrumPlane::Far] = Plane(glm::normalize(-viewDir), m_points[FrustrumPoints::FTL]);

	m_planes[FrustrumPlane::Right] = Plane(Geometry::ComputeNormal(m_points[NTR], m_points[NBR], m_points[FTR]), m_points[NTR]);
	m_planes[FrustrumPlane::Left] = Plane(Geometry::ComputeNormal(m_points[NTL], m_points[FTL], m_points[NBL]), m_points[NTL]);

	m_planes[FrustrumPlane::Top] = Plane(Geometry::ComputeNormal(m_points[NTL], m_points[NTR], m_points[FTL]), m_points[NTL]);
	m_planes[FrustrumPlane::Bottom] = Plane(Geometry::ComputeNormal(m_points[NBL], m_points[FBL], m_points[NBR]), m_points[NBL]);

}
