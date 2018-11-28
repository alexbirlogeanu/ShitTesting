#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/constants.hpp"

class KeyInput;

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
    const glm::mat4&    GetViewMatrix() { return m_viewMatrix; }
    const CFrustrum&    GetFrustrum() const { return m_frustrum; }
    glm::vec3           GetFrontVector() const { return m_front; }
    glm::vec3           GetUpVector() const { return m_up; }
    glm::vec3           GetRightVector() const { return m_right; }
    float               GetFar() const { return m_far; }
    float               GetNear() const { return m_near; }
    float               GetFOV() const { return m_fov; }
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

    CFrustrum  m_frustrum;
};