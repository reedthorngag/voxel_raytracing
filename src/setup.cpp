
#include "setup.hpp"

#include "shaders/load_shader.hpp"
#include "globals.hpp"

#include <fstream>
#include <openSimplexNoise.h>

float vertices[] = {
    -1, -1, 0.0,
    -1,  1, 0.0,
     1, -1, 0.0,
     1,  1, 0.0,
};

GLuint VAO;
GLuint VBO;

GLuint pixelsDataTex;

GLuint pixelsDataFBO;
GLuint mouseHitPosFBO;

GLuint lowResPassTex;
GLuint lowResPassTex2;
GLuint lowResPassFBO;

GLuint midResPassTex;
GLuint midResPassFBO;

GLuint colorBufferTex;
GLuint posTex;
GLuint normalTex;
GLuint secondaryRaysFBO;

GLuint waterNormalsTex;

void glfwErrorCallback(int errorCode, const char* errorMessage) {
    printf("glfw error: %d %s\n", errorCode, errorMessage);
}

void glfwMonitorCallback(GLFWmonitor* monitor, int event)
{
    if (event == GLFW_CONNECTED)
    {
        printf("Monitor connected!\n");
    }
    else if (event == GLFW_DISCONNECTED)
    {
        printf("Monitor disconnected!\n");
    }
}

void createWindow() {
    if (!glfwInit()) {
        printf("GLFW init failed!\n");
        exit(1);
    }

    glfwSetErrorCallback(glfwErrorCallback);

    glfwSetMonitorCallback(glfwMonitorCallback);

    int count;
    GLFWmonitor** monitors = glfwGetMonitors(&count);//glfwGetPrimaryMonitor();
    GLFWmonitor* monitor = monitors[0];
    DEBUG(1) printf("Number of connected monitors: %d\n",count);

    int x, y;
    glfwGetMonitorPos(monitor, &x, &y);
    DEBUG(1) printf("monitor pos: %d, %d\n",x,y);
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_DOUBLEBUFFER, true);
    glfwWindowHint(GLFW_MAXIMIZED, true);
    glfwWindowHint(GLFW_DECORATED, false);
    glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE);
    //glfwWindowHint(GLFW_FLOATING, true);

    window = glfwCreateWindow(mode->width, mode->height, "Voxel engine v2", monitor, NULL);
    if (!window) {
        printf("Window creation failed!\n");
        exit(1);
    }

    //glfwShowWindow(window);
    //glfwSetWindowSize(window, mode->width,mode->height-50);
    ///glfwSetWindowPos(window,x,y);

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        printf("GLEW init failed!\n");
        exit(1);
    }

    DEBUG(1) printf("OpenGL version: %s\n",glGetString(GL_VERSION));

    glfwGetFramebufferSize(window,&width,&height);
    glViewport(0,0,width,height);

    DEBUG(1) printf("Frame buffer size: %d, %d\n",width, height);

    halfWidth = (double)width/2.0;
    halfHeight = (double)height/2.0;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}

GLuint loadShader(GLuint program, const char* shaderSource, GLuint shaderType)  {
    GLuint shader = loadShader(shaderSource, shaderType);
    if (!shader) return 0;

    glAttachShader(program,shader);

    glDeleteShader(shader); // mark for deletion, only actually deleted after glDetachShader

    return shader;
}

bool linkProgram(GLuint program) {
    glLinkProgram(program);

    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, (int *)&isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

        char* infoLog = new char[maxLength];
        glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);
        printf("Linking failed! Info log: %s\n",infoLog);

        delete[] infoLog;
        glDeleteProgram(program);

        return false;
    }

    glValidateProgram(program);

    GLint isValid = 0;
    glGetProgramiv(program, GL_VALIDATE_STATUS, (int *)&isValid);
    if (isValid == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

        char* infoLog = new char[maxLength];
        glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);
        printf("Linking failed! Info log: %s\n",infoLog);

        delete[] infoLog;
        glDeleteProgram(program);

        return false;;
    }

    return true;
}

bool setupProgram1() {
    lowResProgram = glCreateProgram();
    
    GLuint shader1 = loadShader(lowResProgram, "../src/shaders/low_res.frag", GL_FRAGMENT_SHADER);
    if (!shader1) return false;
    
    GLuint shader2 = loadShader(lowResProgram, "../src/shaders/shader.vert", GL_VERTEX_SHADER);
    if (!shader2) return false;

    if (!linkProgram(lowResProgram))
        return false;

    glDetachShader(lowResProgram, shader1);
    glDetachShader(lowResProgram, shader2);

    DEBUG(3) {
        const size_t MAX_SIZE = 1<<24;
        char* binary = new char[MAX_SIZE];
        GLenum format;
        GLint length;
        glGetProgramBinary(lowResProgram,MAX_SIZE,&length,&format,binary);
        checkGlError(lowResProgram,"getBinary");

        std::ofstream binaryfile("bin.txt");
        binaryfile.write(binary,length);
    }

    return true;
}

bool setupProgram2() {
    midResProgram = glCreateProgram();
    
    GLuint shader1 = loadShader(midResProgram, "../src/shaders/mid_res.frag", GL_FRAGMENT_SHADER);
    if (!shader1) return false;
    
    GLuint shader2 = loadShader(midResProgram, "../src/shaders/shader.vert", GL_VERTEX_SHADER);
    if (!shader2) return false;

    if (!linkProgram(midResProgram))
        return false;

    glDetachShader(midResProgram, shader1);
    glDetachShader(midResProgram, shader2);

    return true;
    
}

bool setupProgram3() {

    fullResProgram = glCreateProgram();
    
    GLuint shader1 = loadShader(fullResProgram, "../src/shaders/full_res.frag", GL_FRAGMENT_SHADER);
    if (!shader1) return false;
    
    GLuint shader2 = loadShader(fullResProgram, "../src/shaders/shader.vert", GL_VERTEX_SHADER);
    if (!shader2) return false;

    if (!linkProgram(fullResProgram))
        return false;

    glDetachShader(fullResProgram, shader1);
    glDetachShader(fullResProgram, shader2);

    return true;
}

bool setupProgram4() {

    lightScatteringProgram = glCreateProgram();
    
    GLuint shader1 = loadShader(lightScatteringProgram, "../src/shaders/light_scattering.frag", GL_FRAGMENT_SHADER);
    if (!shader1) return false;
    
    GLuint shader2 = loadShader(lightScatteringProgram, "../src/shaders/shader.vert", GL_VERTEX_SHADER);
    if (!shader2) return false;

    if (!linkProgram(lightScatteringProgram))
        return false;

    glDetachShader(lightScatteringProgram, shader1);
    glDetachShader(lightScatteringProgram, shader2);

    return true;
}

bool createDependencies() {

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenTextures(1,&colorBufferTex);
    glBindTexture(GL_TEXTURE_2D, colorBufferTex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width, height);

    glGenTextures(1,&posTex);
    glBindTexture(GL_TEXTURE_2D, posTex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, width, height);

    glGenTextures(1,&normalTex);
    glBindTexture(GL_TEXTURE_2D, normalTex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, width, height);

    glGenFramebuffers(1, &secondaryRaysFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, secondaryRaysFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D, colorBufferTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D, posTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2,GL_TEXTURE_2D, normalTex, 0);

    GLenum drawBuffers[]{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, drawBuffers);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // glGenTextures(1,&lowResPassTex);
    // glBindTexture(GL_TEXTURE_2D, lowResPassTex);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width>>2, height>>2);

    // glGenTextures(1,&lowResPassTex2);
    // glBindTexture(GL_TEXTURE_2D, lowResPassTex2);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width>>2, height>>2);

    // glGenFramebuffers(1, &lowResPassFBO);
    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, lowResPassFBO);
    // glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,lowResPassTex,0);
    // glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,lowResPassTex2,0);

    // GLenum drawBuffers[]{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    // glDrawBuffers(2, drawBuffers);
    

    // glGenTextures(1,&midResPassTex);
    // glBindTexture(GL_TEXTURE_2D, midResPassTex);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width>>1, height>>1);

    // glGenFramebuffers(1, &midResPassFBO);
    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, midResPassFBO);
    // glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,midResPassTex,0);


    glGenTextures(1,&pixelsDataTex);
    glBindTexture(GL_TEXTURE_2D, pixelsDataTex);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width, height);

    glGenFramebuffers(1, &pixelsDataFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pixelsDataFBO);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,pixelsDataTex,0);

    
    // glGenTextures(1,&waterNormalsTex);
    // glBindTexture(GL_TEXTURE_2D, waterNormalsTex);

    // int width = 258;
    // int height = 258;

    // glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, width-2, height-2);

    // OpenSimplexNoise::Noise o1(42);

    // double TAU = 6.283185307179586;
    // float* buf = new float[width * height];

    // float aspect_ratio = width / height;
    // glm::vec2 x_offset = glm::vec2(-1.0,1.0);
    // glm::vec2 y_offset = (float)(1.0 / aspect_ratio) * glm::vec2(-1.0,1.0);

    // for (int y = 0; y < height; y++) {
    //     for (int x = 0; x < width; x++) {
    //         // set buffer (torus) resolution and scale
    //         // float s = (float)x / (float)width;
    //         // float t = (float)y / (float)height;
    //         // float dx = x_offset.x - x_offset.y;
    //         // float dy = y_offset.x - y_offset.y;
                
    //         // // calculate position on torus
    //         // float nx = x_offset.x + glm::cos(TAU * s) * dx / TAU;
    //         // float ny = y_offset.x + glm::cos(TAU * t) * dy / TAU;
    //         // float nz = x_offset.x + glm::sin(TAU * s) * dx / TAU;
    //         // float nw = y_offset.x + glm::sin(TAU * t) * dy / TAU;
                
    //         // calculate noise value	
    //         float value = o1.eval(x*0.2,y*0.2);//nx, ny, nz, nw) + 1;
    //         buf[y*width + x] = value;
    //     }
    // }

    // float* outBuf = new float[width * height * 4 * 2];

    // width -= 2;
    // height -= 2;
    // for (int y = 1; y < height+1; y++) {
    //     for (int x = 1; x < width*4 + 1; x += 4) {
    //         float L = buf[y * (width+2) + x - 1];
    //         float R = buf[y * (width+2) + x + 1];
    //         float T = buf[(y-1) * (width+2) + x];
    //         float B = buf[(y+1) * (width+2) + x];
    //         glm::vec3 normal = glm::normalize(glm::vec3(2*(R-L), 2*(B-T), -4));
    //         outBuf[(y-1) * width + x - 1] = normal.x;
    //         outBuf[(y-1) * width + x] = normal.y;
    //         outBuf[(y-1) * width + x + 1] = normal.z;
    //         outBuf[(y-1) * width + x + 2] = 0;
    //     }
    // }

    // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 100, 100, GL_RGBA, GL_FLOAT, buf);
    // checkGlError(0, "waterNormalsTex");

    // delete[] buf;
    // delete[] outBuf;

    return true;
}

bool setupOpenGl() {

    glDisable(GL_DEPTH_TEST);

    if (!setupProgram1()) return false;
    //if (!setupProgram2()) return false;
    //if (!setupProgram3()) return false;
    if (!setupProgram4()) return false;

    if (!createDependencies()) return false;

    return true;
}

void reloadShaders() {
    glUseProgram(0);
    GLuint oldProgram1 = lowResProgram;
    // GLuint oldProgram2 = midResProgram;
    // GLuint oldProgram3 = fullResProgram;
    // GLuint oldProgram4 = lightScatteringProgram;

    if (setupProgram1()){;// && setupProgram4() && setupProgram2() && setupProgram3()) {
        glDeleteProgram(oldProgram1);
        //glDeleteProgram(oldProgram2);
        // glDeleteProgram(oldProgram3);
        // glDeleteProgram(oldProgram4);
        printf("Shaders successfully reloaded!    \n");
    } else {
        lowResProgram = oldProgram1;
        // midResProgram = oldProgram2;
        // fullResProgram = oldProgram3;
        // lightScatteringProgram = oldProgram4;
    }
}

