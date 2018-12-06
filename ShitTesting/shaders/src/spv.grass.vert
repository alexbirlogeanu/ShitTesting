#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 in_position;
layout(location=1) in vec2 in_uv;

struct Plant
{
	vec4 Position; //world space
	vec4 Properties; //x - width, y - height of the billboard, z - current angular speed of the billboard, comes from "simulation", w - texture index
};

layout(set = 0, binding = 0) buffer Buffer
{
	Plant Plants[];
};


layout(push_constant) uniform Globals
{
	mat4 ProjViewMatrix;
	vec4 CameraPosition;
};

layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 materialProps;
layout(location = 2) out vec4 worldPos;
layout(location = 3) out vec4 normal;

mat4 GetRotationMatrix(vec3 axis, float angle) 
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

void main()
{
	Plant plant = Plants[gl_InstanceIndex];
	uv = in_uv;
	materialProps = vec4(0.90, 0.1, 0.5, plant.Properties.w);
	
	vec2 size = plant.Properties.xy;	
	
	vec4 center = plant.Position + vec4(0.0f, size.y / 2.0f, 0.0f, 0.0f);

	//vec4 position
	mat4 translate = mat4(vec4(1.0f, 0.0f, 0.0f, 0.0f),
					vec4(0.0f, 1.0f, 0.0f, 0.0f),
					vec4(0.0f, 0.0f, 1.0f, 0.0f),
					center);
	
	mat4 scale = mat4(vec4(size.x / 2.0f, 0.0f, 0.0f, 0.0f),
					vec4(0.0f, size.y / 2.0f, 0.0f, 0.0f),
					vec4(0.0f, 0.0f, 1.0f, 0.0f),
					vec4(0.0f, 0.0f, 0.0f, 1.0f));
	
	
	vec3 front = center.xyz - CameraPosition.xyz;
	front.y = 0.0f; //in XoZ plane
	front = normalize(front);
	normal = vec4(-front, 0.0f);
	
	float angle = atan(front.x, front.z);
	
	mat4 rotate = GetRotationMatrix(vec3(0.0f, 1.0f, 0.0f), -angle);
	
	worldPos = translate * rotate * scale * vec4(in_position, 1.0f);
	
	gl_Position = ProjViewMatrix * worldPos;
}
