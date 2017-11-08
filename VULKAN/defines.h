#pragma once
#include <intrin.h>

#define TRAP(cond) { \
        if(!(cond)) \
        { \
            __debugbreak(); \
        } \
    }while(0);


#define VULKAN_ASSERT(func) { VkResult res = func; TRAP(res >= VK_SUCCESS); }
#define cleanStructure(var) memset(&var, 0, sizeof(var))

#define WIDTH 1280
#define HEIGHT 720

#define SHADOWW 2 * WIDTH
#define SHADOWH 2 * HEIGHT

#define MAXUIITEMS  36

//window section
#define WNDCLASSNAME "vulkanWindow"
#define WNDNAME "Vulkan"

//for shaderCompiler
#define MSGSHADERCOMPILED 1