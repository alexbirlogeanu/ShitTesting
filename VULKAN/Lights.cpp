#include "Lights.h"
#include "UI.h"
#include "Input.h"

CDirectionalLight::CDirectionalLight() 
    : m_alpha(45.0f) //degrees
    , m_beta(270.0f)
    , m_rotStep(10.0f)
    , m_normal(0.0f, -1.0f, 0.0f)
    , m_tangent(1.0f, 0.0f, 0.0f)
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
}

void CDirectionalLight::CreateDebug(CUIManager* manager)
{
}

bool CDirectionalLight::OnKeyboardPressed(const KeyInput& keyInput)
{
	const float lightMoveFactor = 0.5f;
	WPARAM key = keyInput.GetKeyPressed();

	switch (key)
	{
		case VK_UP:
			Shift(lightMoveFactor);
			break;
		case VK_DOWN:
			Shift(-lightMoveFactor);
			break;
		case VK_RIGHT:
			Rotate(lightMoveFactor);
			break;
		case VK_LEFT:
			Rotate(-lightMoveFactor);
			break;
		case 'P':
			ChangeLightColor();
			break;
		case VK_OEM_PLUS:
			ChangeLightIntensity(0.5f);
			break;
		case VK_OEM_MINUS:
			ChangeLightIntensity(-0.5f);
			break;
		default:
			return false;
	}

	return true;
}

bool CDirectionalLight::OnMouseEvent(const MouseInput& mouseInput)
{
	if (mouseInput.IsButtonDown(MouseInput::Middle))
	{
		ToggleDebug();
		return true;
	}
	return false;
}
