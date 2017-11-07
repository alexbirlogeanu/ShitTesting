#pragma once
#include "glm/glm.hpp"
#include <array>

class CUIVector;
class CUIAxisSystem;
class CUIManager;
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
    void CreateDebug(CUIManager* manager);
private:
    void Update();
    void UpdateDebug();
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
    CUIVector*      m_debugDirVector;
    CUIAxisSystem*  m_debugLightAxisSystem;
    glm::vec3       m_debugPosition;
    bool            m_displayDebug;
};