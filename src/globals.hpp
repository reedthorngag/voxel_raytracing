#include <glm/glm.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include "types.hpp"

#ifndef _GLOBALS
#define _GLOBALS

#define ERROR 0
#define WARN 1
#define INFO 2
#define DEBUG 3

extern int DEBUG_LEVEL;

#define DEBUG(x) if (x <= DEBUG_LEVEL) 

extern int width;
extern int height;

extern double halfWidth;
extern double halfHeight;

extern bool dimensionsChanged;

extern GLFWwindow* window;

extern GLuint lowResProgram;
extern GLuint midResProgram;
extern GLuint fullResProgram;
extern GLuint lightScatteringProgram;

extern GLuint arraySsbo;
extern GLuint nodeSsbo;

inline void checkGlErrorFunc(GLuint program, const char* id) {
    int error = glGetError();
    if (error != GL_NO_ERROR) {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

        char* infoLog = new char[maxLength];
        glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);
        printf("GL Error: %s: %d: %s\n",id, error, infoLog);
        exit(1);
    }
}

// so it is easy to disable/effectively remove later
#define checkGlError(program, id) checkGlErrorFunc(program, id)



const float moveSpeed = 1000000;

struct Mouse {
    double x = 0;
    double y = 0;
};

extern Mouse mouse;

extern bool mouseLocked;

extern unsigned int sendDebugFrame;

extern glm::vec3 cameraDir;
extern glm::vec3 cameraPos;

extern glm::vec3 sun;

enum properties {
    NONE = 0,
    REFLECTIVE = 0x2,
    REFRACTIVE = 0x4,
    LUMINESCENT = 0x8,
    LIQUID = 0x10
};

struct Block {
    u32 flags;
    u64 color;
    float metadata;
};

extern Block hotbar[];

extern int hotbarLength;
extern int currentSelected;

#endif



