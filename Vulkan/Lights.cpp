#include "Lights.h"
#include "UI.h"

CDirectionalLight::CDirectionalLight() 
    : m_alpha(45.0f) //degrees
    , m_beta(270.0f)
    , m_rotStep(10.0f)
    , m_normal(0.0f, -1.0f, 0.0f)
    , m_tangent(1.0f, 0.0f, 0.0f)
    , m_debugDirVector(nullptr)
    , m_debugLightAxisSystem(nullptr)
    , m_debugPosition(0.0f, 0.5f, -1.0f)
    , m_lcIndex(0)
    , m_lightIntensity(10.0f)
    , m_displayDebug(false)
{
    m_bitangent = glm::cross(m_normal, m_tangent);
    m_TBN = glm::mat3(m_tangent, m_bitangent, m_normal);
    Update();

    m_lightsColor[0] = glm::vec4(0.82f ,0.63f, 0.5f, 1.0f);
    m_lightsColor[1] = glm::vec4(1.0f);
    m_lightsColor[2] = glm::vec4(0.0f, 0.2f, 0.4f, 1.0f);
}

CDirectionalLight::~CDirectionalLight()
{

}

void CDirectionalLight::ChangeLightColor()
{
    m_lcIndex = (m_lcIndex + 1) % ms_colors;
}

void CDirectionalLight::ChangeLightIntensity(float step)
{
    m_lightIntensity = glm::max(m_lightIntensity + step, 0.0f);
}

void CDirectionalLight::ToggleDebug()
{
    m_displayDebug = !m_displayDebug;
    UpdateDebug();
}

glm::vec4 CDirectionalLight::GetLightIradiance() const
{
    return m_lightsColor[m_lcIndex] * m_lightIntensity;
}

glm::vec4 CDirectionalLight::GetDirection() const
{
    return glm::vec4(m_direction, 0.0f);
}

float CDirectionalLight::GetLightIntensity() const
{
    return m_lightIntensity;
}

void CDirectionalLight::Rotate(float step)
{
    m_beta += step * m_rotStep;
    Update();
}

void CDirectionalLight::Shift(float step)
{
    m_alpha += step * m_rotStep;
    Update();
}

void CDirectionalLight::Update()
{
    float alpha = glm::radians(m_alpha);
    float beta =  glm::radians(m_beta);

    m_direction = glm::vec3(sin(alpha) * cos(beta), sin(alpha) * sin(beta), cos(alpha));
    m_direction = m_TBN * m_direction;
    m_direction = glm::normalize(m_direction);

    UpdateDebug();
}

void CDirectionalLight::CreateDebug(CUIManager* manager)
{
    m_debugDirVector = manager->CreateVectorItem(m_debugPosition, m_direction / 2.0f, glm::vec4(0.1f, 0.6f, 0.6f, 1.0f));
    m_debugLightAxisSystem = manager->CreateAxisSystemItem(m_debugPosition, m_tangent / 2.0f, m_bitangent / 2.0f, m_normal / 2.0f);

    UpdateDebug();
}

void CDirectionalLight::UpdateDebug()
{
    if (m_debugDirVector && m_debugLightAxisSystem) 
    {
        m_debugDirVector->SetVector(m_direction / 2.0f);
        m_debugDirVector->SetVisible(m_displayDebug);
        m_debugLightAxisSystem->SetVisible(m_displayDebug);
    }
}