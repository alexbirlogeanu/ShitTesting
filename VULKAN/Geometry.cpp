#include "Geometry.h"

//////////////////////////////////////////////////////////////////////////////////////
//CFrustrum
//////////////////////////////////////////////////////////////////////////////////////

CFrustrum::CFrustrum(float N, float F)
	: m_near(N)
	, m_far(F)
{
}

CFrustrum::~CFrustrum()
{
}

void CFrustrum::Update(glm::vec3 eye, glm::vec3 dir, glm::vec3 up, glm::vec3 right, float fov)
{
	Construct(eye, dir, up, right, fov);
}

void CFrustrum::Construct(glm::vec3 eye, glm::vec3 dir, glm::vec3 up, glm::vec3 right, float fov)
{
	glm::vec3 nearPoints[4];
	glm::vec3 farPoints[4];

	GetPointsFromPlane(eye, dir, right, up, m_near, fov, nearPoints);
	GetPointsFromPlane(eye, dir, right, up, m_far, fov, farPoints);

	memcpy(m_points, nearPoints, sizeof(nearPoints));
	memcpy(&m_points[4], farPoints, sizeof(farPoints));
}

void CFrustrum::GetPointsFromPlane(glm::vec3 start, glm::vec3 dir, glm::vec3 right, glm::vec3 up, float dist, float fov, glm::vec3 points[4])
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