#pragma once
#include <intrin.h>
#include <Windows.h>

#define TRAP(cond) { \
        if(!(cond)) \
        { \
            __debugbreak(); \
        } \
    }while(0);


#define HEAP_ONLY(CLASSNAME)	public: \
								void Destroy() { delete this; } \
								protected: \
								virtual ~##CLASSNAME();

#define VULKAN_ASSERT(func) { VkResult res = func; TRAP(res >= VK_SUCCESS); }
#define cleanStructure(var) memset(&var, 0, sizeof(var))

#define WIDTH 1280
#define HEIGHT 720

#define SHADOWW 2 * WIDTH
#define SHADOWH 2 * HEIGHT
#define SHADOWSPLITS 3

//window section
#define WNDCLASSNAME "vulkanWindow"
#define WNDNAME "Vulkan"

//for shaderCompiler
#define MSGSHADERCOMPILED 1

#define BATCH_MAX_TEXTURE 12
#define DEFAULT_MIPLEVELS 5

#define START_PROFILE_SECTION(section) { \
										DWORD start = GetTickCount(); \
										std::string profileSectionName (section);

#define END_PROFILE_SECTION(section)	TRAP(profileSectionName == section && "START_PROFILE and END_PROFILE section doesn't match");\
										DWORD end = GetTickCount();\
										std::cout << "For section " << profileSectionName << " time elapsed: " << end - start << "ms..." << std::endl;\
										}
