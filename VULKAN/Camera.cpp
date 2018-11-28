#include "Camera.h"

#include "Input.h"

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
//////////////////////////////////////////////////////////////////////////////////////
//CCamera
//////////////////////////////////////////////////////////////////////////////////////

CCamera::CCamera()
    : m_position(0.0f, 0.5f, 0.0)  
    , m_angles(0.0f, 0.0f, 0.0f)
    , m_dirty(true)
    , m_angularSpeed(glm::pi<float>() / 30.f)
    , m_translateSpeed(120.0f)
    , m_fov(glm::radians(75.0f))
    , m_far(25.0f)
    , m_near(0.01f)
    , m_frustrum(m_near, m_far)
{
    //UpdateViewMatrix();
}

CCamera::~CCamera()
{
}

void CCamera::Update()
{
    if (!m_dirty)
        return;

    UpdateViewMatrix();
    m_frustrum.Update(m_position, m_front, m_up, m_right, m_fov);
    m_dirty = false;
}

void CCamera::Rotate(float x, float y)
{
    float acx = glm::pi<float>() / 2.0f - acos(x);
    float acy = glm::pi<float>() / 2.0f - acos(y);
    m_angles[AnglesType_Yaw] += acx;
    m_angles[AnglesType_Pitch] += acy;
    m_dirty = true;
}

void CCamera::Translate(glm::vec3 translateUnits)
{
    float dt = 1.0f / 1000.0f;
    float fwdDistance = translateUnits[2] * m_translateSpeed * dt;
    float rgtDistance = translateUnits[0] * m_translateSpeed * dt;
    m_position += m_front * fwdDistance;
    m_position += m_right * rgtDistance;
    m_dirty = true;
}

void CCamera::Reset() 
{ 
    m_angles = glm::vec3(0.0f);
    m_dirty = true;
}

void CCamera::UpdateViewMatrix()
{
    glm::vec3 j = glm::vec3(.0f, 1.0f, 0.0f);
    m_front = glm::rotate(glm::vec3(.0f, .0f, -1.0f), m_angles[AnglesType_Yaw], j);
    m_right = glm::cross(m_front, j);
    m_front = glm::rotate(m_front, m_angles[AnglesType_Pitch], m_right);
    m_up = glm::rotate(j, m_angles[AnglesType_Pitch], m_right);
    m_front = glm::normalize(m_front);
    m_right = glm::normalize(m_right);
    m_up = glm::normalize(m_up);

    m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
}

bool CCamera::OnCameraKeyPressed(const KeyInput& key)
{
	if (key.IsKeyPressed('A'))
		Translate(glm::vec3(-1, 0.0f, 0.0f));
	else if (key.IsKeyPressed('D'))
		Translate(glm::vec3(1, 0.0f, 0.0f));
	else if (key.IsKeyPressed('W'))
		Translate(glm::vec3(0.0f, 0.0f, 1));
	else if (key.IsKeyPressed('S'))
		Translate(glm::vec3(0.0f, 0.0f, -1));
	else if (key.IsKeyPressed(VK_SPACE))
		Reset();

	return true;
}
