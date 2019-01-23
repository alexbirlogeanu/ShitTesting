#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) out vec4 ssrOutput;
layout(location=1) out vec4 debug;

layout(location=0) in vec4 uv;

layout(set=0, binding=0) uniform SSRConstants_U
{
	vec4 ViewDirection;
	mat4 ViewMatrix;
	mat4 ProjMatrix;
	//TODO add InvProjMatrix
	//TODO add ViewSpaceToScreenSpace
};

layout(set=0, binding=1) uniform sampler2D Normals_T;
layout(set=0, binding=2) uniform sampler2D Positions_T;
layout(set=0, binding=3) uniform sampler2D Color_T;
layout(set=0, binding=4) uniform sampler2D Depth_T;

mat4 InvProjMatrix;

float LiniarizeDepth(float z)
{
	float near = 0.01;
	float far = 75.0f;
	return (2 * near) / (far + near - z * (far - near));
}

float InvLiniarizeDepth(float z)
{
	float near = 0.01;
	float far = 75.0f;
	
	return (far + near) / (far - near) - (2 * near) / (z * (far - near));
}

bool OutsideOfScreenSpace(float x)
{
	return x < 0.0f || x > 1.0f;
}

float distanceSquared(vec2 A, vec2 B) 
{
    A -= B;
    return dot(A, A);
}

vec4 GetPointInViewSpace(vec2 coords)
{
	vec4 viewSpacePoint = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	if (any(lessThan(coords, vec2(0.0f, 0.0f))) || any(greaterThan(coords, vec2(1280.0f, 720.0f))))
		return viewSpacePoint;
	float depth = InvLiniarizeDepth(texelFetch(Depth_T, ivec2(coords), 0).r);
	coords /= vec2(1280.0f, 720.0f);
	coords = coords * 2.0f - 1.0f;
	viewSpacePoint = InvProjMatrix * vec4(coords, depth, 1.0f);
	viewSpacePoint /= viewSpacePoint.w;
	return viewSpacePoint;
}

bool IntersectDepthBuffer(float z, float zmin, float zmax)
{
	float thickness = 0.05f;
	return zmax >= (z - thickness) && zmin <= (z );
}

bool TraceViewSpaceRay(	vec3 rayOrigin,
						vec3 rayDir,
						float maxSteps,
						float maxDistance,
						out vec2 hitCoords
						)
{
	hitCoords = vec2(-1.0f, -1.0f);
	float deltaStep = maxDistance / maxSteps;
	
	vec3 pointVS = rayOrigin + deltaStep * rayDir;
	
	float stepCount = 1.0f;
	for (; stepCount < maxSteps; stepCount += 1.0f)
	{
		vec4 ndc = ProjMatrix * vec4(pointVS, 1.0f);
		ndc /= ndc.w;
		ndc.xy = (ndc.xy + 1.0f) / 2.0f;
		
		ndc.z = LiniarizeDepth(ndc.z);
		float d = texture(Depth_T, ndc.xy).r;
		
		if (ndc.z > d)
		{
			hitCoords = ndc.xy;
			break;
		}
		pointVS += deltaStep * rayDir;
	}
	debug = vec4(hitCoords, stepCount, 1.0f);
	
	return !(OutsideOfScreenSpace(hitCoords.x) || OutsideOfScreenSpace(hitCoords.y));
}

bool TraceScreenSpaceRay
   (vec3 rayOrigin,
	vec3 rayDir,
	mat4 ViewToScreenSpace,
	float stride,
	float jitter,
	float maxSteps,
	float maxDistance,
	out vec2 hitCoords
   )
{
	float nearPlane = -0.01f;
	
	float rayLength = (rayOrigin.z + maxDistance * rayDir.z > nearPlane) ? (nearPlane - rayOrigin.z) / rayDir.z : maxDistance;
	vec3 endPoint = rayOrigin + rayLength * rayDir;
	
	vec4 H0 = ViewToScreenSpace * vec4(rayOrigin, 1.0f);
	vec4 H1 = ViewToScreenSpace * vec4(endPoint, 1.0f);
	
	float k0 = 1.0f / H0.w;
	float k1 = 1.0f / H1.w;
	
	vec3 Q0 = rayOrigin * k0;
	vec3 Q1 = endPoint * k1;
	
	vec2 P0 = H0.xy * k0;
	vec2 P1 = H1.xy * k1;
	
	hitCoords = vec2(-1.0f, -1.0f);
	P1 += vec2(distanceSquared(P0, P1) < 0.0001f ? 0.01f : 0.0f);
	
	vec2 delta = P1 - P0;
	
	bool permute = false;
	
	if (abs(delta.x) < abs(delta.y))
	{
		permute = true;
		
		delta = delta.yx;
		P1 = P1.yx;
		P0 = P0.yx;
	}
	
	float stepDirection = sign(delta.x);
	float invdx = stepDirection / delta.x;
	vec2 dP = vec2(stepDirection, delta.y * invdx);
	
	vec3 dQ = (Q1 - Q0) * invdx;
	float dK = (k1 - k0) * invdx;
	
	dP *= stride;
	dQ *= stride;
	dK *= stride;
	
	P0 += dP * jitter;
	Q0 += dQ * jitter;
	k0 += dK * jitter;
	
	vec3 Q = Q0;
	float k = k0;
	
	float prevZMaxEstimate = rayOrigin.z;
	float stepCount = 0.0f;
	float rayZMax = prevZMaxEstimate;
	float rayZMin = prevZMaxEstimate;
	float sceneZMax = rayZMax;
	float startRayZMax = rayZMax;
	float startRayZMin = rayZMin;
	
	float end = P1.x * stepDirection;
	vec2 P = P0 + dP;
	
	for (; (P.x * stepDirection < end) && (stepCount <= maxSteps); P += dP, k += dK, Q += dQ, stepCount += 1.0f)
	{
		
		hitCoords = permute? P.yx : P;
		
		// The depth range that the ray covers within this loop
        // iteration.  Assume that the ray is moving in increasing z
        // and swap if backwards.  Because one end of the interval is
        // shared between adjacent iterations, we track the previous
        // value and then swap as needed to ensure correct ordering
		rayZMin = prevZMaxEstimate;
		
		rayZMax = (dQ.z * 0.5f + Q.z) / (dK * 0.5f + k);
		prevZMaxEstimate = rayZMax;
		
		if (rayZMin > rayZMax)
		{
			float t = rayZMin;
			rayZMin = rayZMax;
			rayZMax = t;
		}
		
		sceneZMax = GetPointInViewSpace(hitCoords).z;
		
		if (IntersectDepthBuffer(sceneZMax, rayZMin, rayZMax))
			break;
	}
	//debug = vec4(hitCoords, stepCount, stepCount);	
	
	debug = vec4(rayDir, stepCount);
	//return !(OutsideOfScreenSpace(hitCoords.x) || OutsideOfScreenSpace(hitCoords.y));
	return IntersectDepthBuffer(sceneZMax, rayZMin, rayZMax);
}

void main()
{
	InvProjMatrix = inverse(ProjMatrix);
	float maxSteps = 64.0f;
	float maxDistance = 7.5f;
	
	vec3 normalWS = texelFetch(Normals_T, ivec2(gl_FragCoord.xy), 0).xyz;
	if (all(equal(normalWS, vec3(0.0f, 0.0f, 0.0f))))
		discard;
		
	vec3 posVS = (ViewMatrix * texelFetch(Positions_T, ivec2(gl_FragCoord.xy), 0)).xyz;
	vec3 normalVS = normalize(vec3(ViewMatrix * vec4(normalWS, 0.0f)));
	
	vec3 rayDir = normalize(reflect(normalize(posVS), normalVS));
	
	vec2 hitCoords;
	//bool hit = TraceViewSpaceRay(posVS, rayDir, maxSteps, maxDistance, hitCoords);
	float stride = 2.0f;
	float jitter = 1.0f;
	float width = 1280.0f;
	float height = 720.0f;
	mat4 screenSpace = mat4(vec4(width * 0.5f, 0.0f, 0.0f, 0.0f),
							vec4(0.0f, height * 0.5f, 0.0f, 0.0f),
							vec4(0.0f, 0.0f, 1.0f, 0.0f),
							vec4(width * 0.5, height * 0.5f, 0.0f, 1.0f));
	
	mat4 viewToScreenSpace = screenSpace * ProjMatrix;
	bool hit = TraceScreenSpaceRay(posVS, rayDir, viewToScreenSpace, stride, jitter, maxSteps, maxDistance, hitCoords);

	ssrOutput = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	if (hit)
		ssrOutput = texelFetch(Color_T, ivec2(hitCoords), 0);
	//debug = vec4(normalVS, 0.0f);
}