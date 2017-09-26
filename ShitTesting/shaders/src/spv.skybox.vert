#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 in_position;
//layout(location=1) in vec2 in_uv; //not used
//layout(location=2) in vec3 in_normal; //not used

layout(set=0, binding=0) uniform Skybox
{
	vec4   CameraDir;
	vec4   CameraRight;
	vec4   CameraUp;
	vec4   Frustrum; //x - fov, y - aspect ratio, z - far plane
	vec4   DirLightColor;
};

layout(location=0) out vec4 ViewPosition;
layout(location=1) out vec4 LightColor;
void main()
{
	float hFov = radians(120); //Frustrum.x;
	float aspectRatio = Frustrum.y;
	float farPlane = Frustrum.z;
	
	float halfH = tan(hFov ) * farPlane / 2.0f; 
	float halfW = aspectRatio * halfH;
	
	ViewPosition.xyz = CameraDir.xyz * farPlane;
	ViewPosition.xyz += -in_position.x * halfW * CameraRight.xyz; //quad model right model space is left view space
	ViewPosition.xyz += in_position.y * halfH * CameraUp.xyz;
	
	LightColor = DirLightColor;
	gl_Position = vec4(in_position.xy, 1.0f, 1.0f ); //use proj matrix to be correct
}
