#pragma once

#include "glm/glm.hpp"

struct SVertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec3 normal;
	glm::vec3 bitangent;
	glm::vec3 tangent;
	unsigned int color;

	SVertex() {}
	SVertex(glm::vec3 p) : pos(p), uv() {}
	SVertex(glm::vec3 p, glm::vec2 tuv) : pos(p), uv(tuv) {}
	SVertex(glm::vec3 p, glm::vec2 textuv, glm::vec3 n) : pos(p), uv(textuv), normal(n) {}

	void SetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
		color = 0;
		color |= a << 24;
		color |= b << 16;
		color |= g << 8;
		color |= r;
	}
};
