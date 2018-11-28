#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec3 position;
layout(location=1) in vec2 in_uv;
layout(location=2) in vec3 in_normal; //not used

struct Glyph
{
	vec4	TextCoords; //uv - start uv from Font texture, st - delta uv of the glyph (width, height ) / font_texture_size.xy
	vec4	ScreenPosition;  //xy - screen space start pos of the glyph in pixels, zw - glyph width height
};

layout(set = 0, binding = 0) uniform Globals
{
	vec4 ScreenSize;
};

layout(set=1, binding=0) buffer TextParams
{
	Glyph UIText[];
};

vec2 vertexOffsets[] = {
	vec2(0.0f, 0.0f),
	vec2(1.0f, 0.0f),
	vec2(1.0f, 1.0f),
	vec2(0.0f, 1.0f)
};

vec2 uvOffsets[] = {
	vec2(0.0f, 0.0f),
	vec2(1.0f, 0.0f),
	vec2(1.0f, -1.0f),
	vec2(0.0f, -1.0f)
};

layout(location=0) out vec4 uv;

void main()
{
	Glyph curr_glyph = UIText[gl_InstanceIndex];
	vec2 screenPosition = curr_glyph.ScreenPosition.xy + vertexOffsets[gl_VertexIndex] * curr_glyph.ScreenPosition.zw;
	vec2 normalizedPos =  (screenPosition.xy ) / ScreenSize.xy; //in 0 - 1
	//now we need to transform from 0-1 to -1 - 1
	vec2 ndcPos = normalizedPos * 2 - vec2(1.0, 1.0);
	uv.xy = curr_glyph.TextCoords.xy + uvOffsets[gl_VertexIndex] * curr_glyph.TextCoords.zw;
	gl_Position = vec4(ndcPos, 0.0f, 1.0f);
}