#pragma once
#include "glm/glm.hpp"
#include <array>

class CUIManager;
class KeyInput;
class MouseInput;

class CDirectionalLight
{
public:
    CDirectionalLight();
    virtual ~CDirectionalLight();

    glm::vec4 GetLightIradiance() const;
    glm::vec4 GetDirection() const;
    float GetLightIntensity() const;

    void Rotate(float step);
    void Shift(float step);

    void ChangeLightColor();
    void ChangeLightIntensity(float step);

    void ToggleDebug();
    void CreateDebug();

	bool OnKeyboardPressed(const KeyInput& key);
	bool OnMouseEvent(const MouseInput& mouseInput);
private:
    void Update();
private:
    glm::vec3 m_direction;
    glm::mat3 m_TBN;

    static const unsigned int           ms_colors = 3;
    std::array<glm::vec4, ms_colors>    m_lightsColor;
    unsigned int                        m_lcIndex;//light color index
    float                               m_lightIntensity;

    float m_alpha;
    float m_beta;

    const float m_rotStep;
    //for debug
    glm::vec3       m_normal;
    glm::vec3       m_tangent;
    glm::vec3       m_bitangent;
    glm::vec3       m_debugPosition;
    bool            m_displayDebug;
};