#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/constants.hpp"
#include "Geometry.h"

class KeyInput;


class CCamera
{
public:
    enum AnglesTypes
    {
        AnglesType_Roll = 0,
        AnglesType_Yaw = 1,
        AnglesType_Pitch = 2
    };

    CCamera();
    virtual ~CCamera();

    void Update();

    //Getters
    const glm::vec3&    GetPos() const {return m_position; }
    const glm::mat4&    GetViewMatrix() const { return m_viewMatrix; }
    const CFrustum&		GetFrustum() const { return m_frustrum; }
    glm::vec3           GetFrontVector() const { return m_front; }
    glm::vec3           GetUpVector() const { return m_up; }
    glm::vec3           GetRightVector() const { return m_right; }
    float               GetFar() const { return m_far; }
    float               GetNear() const { return m_near; }
    float               GetFOV() const { return m_fov; }
	bool				GetIsDirty() const { return m_dirty; }
    // End Getters

    void Rotate(float x, float y);
    void Translate(glm::vec3 translateUnits);
    void Reset();

	bool OnCameraKeyPressed(const KeyInput& key);
private:
    void UpdateViewMatrix();
private:
    bool m_dirty;
    glm::vec3 m_angles;
    glm::vec3 m_front;
    glm::vec3 m_right;
    glm::vec3 m_up;
    glm::vec3 m_position;

    float m_angularSpeed;
    float m_translateSpeed;

    glm::mat4 m_viewMatrix;

    float m_far;
    float m_near;
    float m_fov; //radians

    CFrustum  m_frustrum;
};