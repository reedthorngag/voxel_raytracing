#include <glm/glm.hpp>
#include "globals.hpp"

int DEBUG_LEVEL = DEBUG;

int width;
int height;

double halfWidth;
double halfHeight;

bool dimensionsChanged = true;

Mouse mouse;

bool mouseLocked = false;

unsigned int sendDebugFrame = 0;

glm::vec3 cameraDir(1,0,1);
glm::vec3 cameraPos(35,50,35);

glm::vec3 sun = glm::normalize(glm::vec3(2.0,1.0,4.0));

GLFWwindow* window;

GLuint lowResProgram;
GLuint midResProgram;
GLuint fullResProgram;
GLuint lightScatteringProgram;

GLuint arraySsbo;
GLuint nodeSsbo;

// bad code warning, its "temporary" though (yea right)
Block hotbar[] {
    Block{
        NONE,
        RGB_TO_U64(255,0,0),
        0
    },
    Block{
        NONE,
        RGB_TO_U64(0,255,0),
        0
    },
    Block{
        REFLECTIVE,
        RGB_TO_U64(255,0,0),
        0.94
    },
    Block{
        REFLECTIVE,
        RGB_TO_U64(255,255,255),
        0.94
    },
    Block{
        REFRACTIVE,
        RGB_TO_U64(0,0,0),
        1.5
    },
};

int hotbarLength = sizeof(hotbar)/sizeof(Block);
int currentSelected;
