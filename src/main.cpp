#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <stdio.h>

#include "globals.hpp"
#include "setup.hpp"
#include "world_gen.hpp"
#include "input.hpp"
#include "voxel_data/tetrahexa_tree.hpp"
#include "voxel_data/voxel_allocator.hpp"
#include "ray_caster.hpp"


extern "C" {
    __declspec(dllexport) unsigned int NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

const char* debugFrameTypeString[] {
    "Null",
    "ray hit pos", // 1
    "ray dir", // 2
    "ray ratios Y/X, Y/Z X/Y", // 3
    "ray ratios X/Z, Z/X Z/Y", // 4
    "ray deltas", // 5
    "ray origin", // 6
    "camDir", // 7
    "proj_pln_inter", // 8
    "x vec", // 9
    "camOrigin", // 0
};

void dumpPixelData() {
    float buffer[4];

    glReadPixels(mouse.x,mouse.y,1,1,GL_RGBA,GL_FLOAT,&buffer);

    checkGlError(fullResProgram,"glReadPixels");

    printf("\rdebug frame: %s: %f %f %f %f (mouse pos: %lf, %lf)\n",debugFrameTypeString[sendDebugFrame],buffer[0],buffer[1],buffer[2],buffer[3],mouse.x,mouse.y);
}

u32 getMortonPos(glm::vec3 pos) {
    glm::ivec3 p = glm::ivec3(glm::floor(pos));

    int n = p.x;
    n = (n | (n << 16)) & 0x030000FF;
    n = (n | (n <<  8)) & 0x0300F00F;
    n = (n | (n <<  4)) & 0x030C30C3;
    u32 x = n;

    n = p.y;
    n = (n | (n << 16)) & 0x030000FF;
    n = (n | (n <<  8)) & 0x0300F00F;
    n = (n | (n <<  4)) & 0x030C30C3;
    u32 y = n;

    n = p.z;
    n = (n | (n << 16)) & 0x030000FF;
    n = (n | (n <<  8)) & 0x0300F00F;
    n = (n | (n <<  4)) & 0x030C30C3;

    return (n << 4) | (y << 2) | x;
}

void render() {

    glClearColor(1,1,1,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(lowResProgram);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);//secondaryRaysFBO);
    if (sendDebugFrame) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pixelsDataFBO);
    }

    DEBUG_LEVEL = WARN;
    RayResult result = RAY_CASTER::castRayFromCam(30);
    DEBUG_LEVEL = DEBUG;
    glUniform3f(glGetUniformLocation(lowResProgram, "trueOrigin"), cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform3f(glGetUniformLocation(lowResProgram, "cameraDir"), cameraDir.x, cameraDir.y, cameraDir.z);
    glUniform2f(glGetUniformLocation(lowResProgram, "mousePos"), mouse.x, mouse.y);
    glUniform1ui(glGetUniformLocation(lowResProgram, "originMortonPos"), getMortonPos(cameraPos));
    glUniform1i(glGetUniformLocation(lowResProgram, "renderPosData"), sendDebugFrame);
    glUniform3f(glGetUniformLocation(lowResProgram, "sunDir"), sun.x, sun.y, sun.z);
    glUniform3i(glGetUniformLocation(lowResProgram, "lookingAtBlock"),result.pos.x, result.pos.y, result.pos.z);
    glUniform1f(glGetUniformLocation(lowResProgram, "deltaTime"), (float)glfwGetTime());

    if (dimensionsChanged) {
        glUniform2ui(glGetUniformLocation(lowResProgram, "resolution"), width, height);
        glUniform2f(glGetUniformLocation(lowResProgram, "projPlaneSize"), glm::tan(glm::radians(45.0)), glm::tan(glm::radians(45.0)) * ((float)height)/width);
    }

    if (sendDebugFrame) {  
        dumpPixelData();
        sendDebugFrame = 0;
        return;
    }

    //glBindTexture(GL_TEXTURE_2D, waterNormalsTex);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    DEBUG(DEBUG) checkGlError(lowResProgram,"glDrawArrays1");
    return;

    
    // glUseProgram(lightScatteringProgram);

    // glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // if (dimensionsChanged) {
    //     glUniform2ui(glGetUniformLocation(lightScatteringProgram, "resolution"), width, height);
    //     dimensionsChanged = false;
    // }

    // glUniform3f(glGetUniformLocation(lightScatteringProgram, "origin"), cameraPos.x,cameraPos.y,cameraPos.z);


    // glBindTexture(GL_TEXTURE_2D, colorBufferTex);
    // glActiveTexture(GL_TEXTURE1);
    // glBindTexture(GL_TEXTURE_2D, posTex);
    // glActiveTexture(GL_TEXTURE2);
    // glBindTexture(GL_TEXTURE_2D, normalTex);
    // glActiveTexture(GL_TEXTURE0);

    // glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    // DEBUG(DEBUG) checkGlError(lightScatteringProgram, "glDrawArrays2");

    // glUseProgram(midResProgram);

    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);//midResPassFBO);

    // glUniform3f(glGetUniformLocation(midResProgram, "origin"), cameraPos.x,cameraPos.y,cameraPos.z);
    // glUniform3f(glGetUniformLocation(midResProgram, "cameraDir"), cameraDir.x,cameraDir.y,cameraDir.z);
    // glUniform2f(glGetUniformLocation(midResProgram, "mousePos"), mouse.x, mouse.y);

    // glBindTexture(GL_TEXTURE_2D, lowResPassTex);
    // glActiveTexture(GL_TEXTURE1);
    // glBindTexture(GL_TEXTURE_2D, lowResPassTex2);
    // glActiveTexture(GL_TEXTURE0);
    // glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // return;
    // glUseProgram(fullResProgram);

    // glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // if (sendDebugFrame) {
    //     glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pixelsDataFBO);
    // }

    // glUniform3f(glGetUniformLocation(fullResProgram, "origin"), cameraPos.x,cameraPos.y,cameraPos.z);
    // glUniform3f(glGetUniformLocation(fullResProgram, "cameraDir"), cameraDir.x,cameraDir.y,cameraDir.z);
    // glUniform2f(glGetUniformLocation(fullResProgram, "mousePos"), mouse.x, mouse.y);
    // glUniform1i(glGetUniformLocation(fullResProgram, "renderPosData"), sendDebugFrame);

    // glBindTexture(GL_TEXTURE_2D, midResPassTex);
    // glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    // DEBUG(3) checkGlError(lowResProgram,"glDrawArrays3");

    // if (sendDebugFrame) {  
    //     dumpPixelData();
    //     glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //     sendDebugFrame = 0;
    // }
}

int main() {
    printf("Hello world!\n");

    createWindow();

    if (!setupOpenGl())
        exit(1);

    glBindVertexArray(VAO);
    initTetraHexaTree();
    initVoxelDataAllocator();
    checkGlError(0, "createSsbo");

    printf("generating world...\n");
    double start = glfwGetTime();

    DEBUG_LEVEL = 1;
    genWorld();
    DEBUG_LEVEL = 3;

    printf("generated chunk! time: %lf  \n", glfwGetTime() - start);

    cameraDir = glm::normalize(cameraDir);

    glfwSetKeyCallback(window, glfwCharCallback);
    glfwSetCursorPosCallback(window, glfwMousePosCallback);
    glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);
    glfwSetScrollCallback(window, glfwScrollCallback);

    glfwSwapInterval(1);

    const int averageSize = 100;
    double times[averageSize]{};
    int i = 0;

    while (!glfwWindowShouldClose(window)) {

        start = glfwGetTime();

        updateSsboData();

        render();

        glfwSwapBuffers(window);

        glfwPollEvents();
        doInputUpdates(glfwGetTime() - start);

        times[i++] = glfwGetTime()-start;
        i %= averageSize;
        double out = 0;
        for (int n = 0; n < averageSize && times[n]; n++) out += times[n];
        printf("\rrender time: %dms (%d fps) rotationXY: %lf, %lf camPos: %lf, %lf, %lf    ",
                (int)(out/(double)averageSize*1000),
                (int)(1000.0/(double)((out/(double)averageSize) * 1000)),
                rotationY,
                rotationX,
                cameraPos.x,
                cameraPos.y,
                cameraPos.z);

    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

