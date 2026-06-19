#version 460 core
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_NV_gpu_shader5 : enable

#define u64 uint64_t
#define i64 int64_t
#define u32 uint
#define u8 uint8_t

out layout (location = 0) vec4 FragColor;
out layout (location = 1) vec3 PosOut;
out layout (location = 2) vec3 NormalOut;
//out layout (location = 0) vec4 FragOut;
//out layout (location = 1) vec4 FragOut2;

uniform vec3 trueOrigin;
uniform vec3 cameraDir;
uniform vec2 mousePos;
uniform int renderPosData;
uniform uint rootNodeIndex;
uniform uint originMortonPos;
uniform vec2 pixelSize;
uniform vec2 projPlaneSize;
uniform uvec2 resolution;
uniform vec3 sunDir;
uniform ivec3 lookingAtBlock;
uniform float deltaTime;

uniform sampler2D waterTex;

in vec4 gl_FragCoord;

layout (std430, binding = 3) readonly buffer layoutNodes {
    u64vec2 nodes[];
};

layout (packed, binding = 2) readonly buffer layoutArrays {
    uint[64] children_array[];
};

vec3 origin;

struct Ray {
    vec3 dir;

    ivec3 step;

    ivec3 sign;

    vec3 ratiosX;
    vec3 ratiosY;
    vec3 ratiosZ;

    mat3 ratios;

    vec3 delta;

    vec3 absDelta;
};

Ray buildRay(vec3 dir) {
    Ray ray;

    ray.dir = dir;

    ray.step = ivec3(1);

    if (dir.x < 0)
        ray.step.x = -1;
    if (dir.y < 0)
        ray.step.y = -1;
    if (dir.z < 0)
        ray.step.z = -1;

    ray.ratiosX = vec3(
        1,
        dir.y/dir.x,
        dir.z/dir.x
    );
    ray.ratiosY = vec3(
        dir.x/dir.y,
        1,
        dir.z/dir.y
    );
    ray.ratiosZ = vec3(
        dir.x/dir.z,
        dir.y/dir.z,
        1
    );

    ray.ratios = mat3(vec3(ray.ratiosX),vec3(ray.ratiosY),vec3(ray.ratiosZ));

    ray.delta = 1/dir;
    ray.absDelta = abs(ray.delta);

    return ray;
}

struct Pos {
    ivec3 round;
    
    vec3 exact;

    vec3 deltaPos;
};

Pos pos;
Ray ray;

const int MAX_DEPTH = 6;

u32 stack[MAX_DEPTH];
uint currentMortonPos = 0;
int depth;
uint mortonPos = 0;

uint lastHit = 0;
uint lastHitFlags = 0;
float lastHitMetadata = 0;

void getBlock();
void nextIntersect(int step);
void nextIntersectDDA();

mat4 rotationMatrix(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

const int BITS_PER_COLOR = 21;
const int COLOR_RANGE = 1 << BITS_PER_COLOR;
const int COLOR_MASK = COLOR_RANGE - 1;
const double SCALED_COLOR = 1.0 / COLOR_RANGE;

vec3 color_int_to_vec3(u64 color) {
    return vec3((color >> (BITS_PER_COLOR * 2)) * SCALED_COLOR,
        ((color >> BITS_PER_COLOR) & COLOR_MASK) * SCALED_COLOR,
        (color & COLOR_MASK) * SCALED_COLOR
    );
}

u64 vec3_to_color_int(vec3 color) {
    return (u64(color.x) << 42) | (u64(color.x) << 21) | u64(color.x);
}

float sigmoid(float x, float scale, float dropOffSteepness) {
    return 1.0/(1+exp(-x*dropOffSteepness))*scale;
}

vec3 genSkyBox() {
    if (ray.dir.y < 0) ray.dir.y *= 1.4;
    float haze = (0.1-abs(clamp(ray.dir.y,-.3,.3))) * 0.8 + 0.1;
    float modifier = clamp(sigmoid(1-(haze*2),1.0,2.0), 0, 1);

    vec3 skyDefaultColor = vec3(0.2,0.4,1);

    float b = distance(ray.dir, sunDir) * 50;
    vec3 sun = vec3(1,1,0) * (sigmoid(1.5-b,1,1.6));

    return (skyDefaultColor + clamp(haze,0,1) * 3) * modifier + sun;
}

vec3 r = vec3(1,0,0);
vec3 g = vec3(0,1,0);
vec3 b = vec3(0,0,1);

vec3 finalColorMod = vec3(1);

bool rayReflected = false;

void reflectRay(u64vec2 block) {

    pos.deltaPos[lastHit] -= ray.absDelta[lastHit];
    // pos.exact[lastHit] += ray.step[lastHit];

    // ray.ratios[0][lastHit] *= -1;
    // ray.ratios[1][lastHit] *= -1;
    // ray.ratios[2][lastHit] *= -1;

    // ray.ratios[lastHit][lastHit] *= -1;

    ray.step[lastHit] *= -1;
    ray.dir[lastHit] *= -1;

    finalColorMod *= 0.94;
    rayReflected = true;
}

float currentRefractiveIndex = 1.0;

vec3 refractRay(vec3 ray, vec3 normal, float n1, float n2) {
    float r = n1 / n2;

    float c1 = dot(normal, ray);
    if (c1 < 0) {
        normal = -normal;
        c1 = dot(normal, ray);
    }

    float c2 = sqrt(1-r*r*(1-c1*c1));

    return r * ray + (r*c1-c2) * normal;
}

void refractRay(float newRefractIndex, uint flags) {

    finalColorMod *= (flags & 0x10) > 0 ? vec3(0.94,0.97,1.0) : vec3(0.95);

    if (currentRefractiveIndex == newRefractIndex) return;

    if (lastHit != 0 && ray.step.x < 0) pos.exact.x += 1;
    if (lastHit != 1 && ray.step.y < 0) pos.exact.y += 1;
    if (lastHit != 2 && ray.step.z < 0) pos.exact.z += 1;

    vec3 normal = vec3(0);
    normal[lastHit] = ray.step[lastHit];

    // water
    if ((flags & 0x10) > 0) {
        normal.x += sin((deltaTime*0.75 + pos.exact.x*0.2 + pos.exact.z*0.08)*10)*0.2;
        normal.x += sin(deltaTime*0.75 + pos.exact.x)*0.1;
        //normal.y += sin(pos.exact.x*0.1);

        normal = normalize(normal);
    }

    ray.dir = refractRay(ray.dir, normal, currentRefractiveIndex, newRefractIndex);

    ray = buildRay(ray.dir);

    pos.exact += min(ray.step, 0);

    pos.deltaPos = ray.absDelta - (pos.exact - pos.round) * ray.delta;

    currentRefractiveIndex = newRefractIndex;
}

bool facingLightDir(vec3 lightDir, uint index) {
    return lightDir[index] * -ray.step[index] > 0;
}

vec3 calcLightIntensity(vec3 color, vec3 lightDir, uint index) {

    float mod = facingLightDir(lightDir,index) ? 0.15 : 0;
    float intensity = min(max(0, lightDir[index] * -ray.step[index]) + 0.4 + mod, 1);

    return color * intensity;
}

u64vec2 block;

void main() {
    origin = trueOrigin;

    if (renderPosData == 0 && distance(gl_FragCoord.xy, mousePos) <= 3) {
        FragColor = vec4(1,1,1,0);
        return;
    }

    vec2 pixelSize = 1.0/resolution;
    vec2 FragCoord = gl_FragCoord.xy * pixelSize;

    stack[0] = 0;
    stack[1] = 0;
    stack[2] = 0;
    stack[3] = 0;
    stack[4] = 0;
    stack[5] = 0;
    depth = 0;

    pos.round = ivec3(floor(origin));

    mortonPos = originMortonPos;

    vec3 camLeft = cross(cameraDir,vec3(0,1,0));
    vec3 camUp = cross(cameraDir,camLeft);
    vec3 projection_plane_center = cameraDir;
    vec3 projection_plane_left = cross(projection_plane_center, vec3(0,1,0));
    vec3 projection_plane_intersect = normalize(projection_plane_center + 
        (projection_plane_left * -(projPlaneSize.x * (FragCoord.x - 0.5))) + 
        cross(projection_plane_center, projection_plane_left) * (-FragCoord.y + 0.5) * projPlaneSize.y);
    

    ray = buildRay(projection_plane_intersect);

    pos.exact = origin;
    
    if (ray.step.x < 0) pos.exact.x -= 1;
    if (ray.step.y < 0) pos.exact.y -= 1;
    if (ray.step.z < 0) pos.exact.z -= 1;

    pos.deltaPos = ray.absDelta - (pos.exact - pos.round) * ray.delta;


    getBlock();
    if (block.x != -1) {
        if ((block.y & 0x4) > 0) {
            currentRefractiveIndex = 1.1;
        } else {
            FragColor = vec4(color_int_to_vec3(block.x),0);
            return;
        }
    }

    uint i = 0;
    const uint numSteps = 300;

    // get first hit
    while (block.x == -1 && i++ < numSteps) {
        nextIntersectDDA();
        getBlock();
    }

    // continue ray path with handling for reflections and refractions
    while (i++ < numSteps) {

        if (block.x != -1) {
            uint flags = uint(block.y) & 0x7;

            if (flags == 0x3) {
                reflectRay(block);

            } else if (flags == 0x5) {
                refractRay(1.1, uint(block.y));

            } else break;
        }

        nextIntersectDDA();
        getBlock();
    }

    if (renderPosData == 1) {
        FragColor = vec4(pos.exact, 0);
        return;
    } else if (renderPosData == 2) {
        FragColor = vec4(pos.round, 0);
        return;
    }

    if (lastHit != 0 && ray.step.x < 0) pos.exact.x += 1;
    if (lastHit != 1 && ray.step.y < 0) pos.exact.y += 1;
    if (lastHit != 2 && ray.step.z < 0) pos.exact.z += 1;

    if (lookingAtBlock == pos.round) {
        FragColor = vec4(color_int_to_vec3(block.x) * 2 + 0.3,0);
        return;
    }

    if (block.x == -1) {
        FragColor = vec4(genSkyBox() * finalColorMod, 0);
        return;
    }
    
    u64vec2 origBlock = block;
    uint origLastHit = lastHit;
    ivec3 origPos = pos.round;

    vec3 color = color_int_to_vec3(block.x);

    FragColor = vec4(calcLightIntensity(color, sunDir, lastHit) * finalColorMod, 0);
    if (rayReflected) {
        return;
    }

    if (!facingLightDir(sunDir, lastHit)) {
        FragColor = vec4(color * 0.3 * finalColorMod, 0);
        return;
    }

    ray = buildRay(sunDir);

    if (ray.step.x < 0) pos.exact.x -= 1;
    if (ray.step.y < 0) pos.exact.y -= 1;
    if (ray.step.z < 0) pos.exact.z -= 1;

    pos.deltaPos = ray.absDelta - (pos.exact - pos.round) * ray.delta;
    pos.deltaPos[lastHit] -= ray.absDelta[lastHit];

    i = 75;
    block = u64vec2(-1);
    while ((block.x == -1 || (uint(block.y) & 0x10) > 0) && i-- != 0) {
        nextIntersectDDA();
        getBlock();
    }

    if (block.x != -1) {
        FragColor = vec4(color * 0.3 * finalColorMod, 0);
    }

}

void nextIntersect(int step) {

    vec3 dst = step - abs(pos.exact - pos.round);
    vec3 x = abs(vec3(dst.x * ray.ratiosX));
    vec3 y = abs(vec3(dst.y * ray.ratiosY));

    if (x.x < x.y && x.x < x.z) {

        pos.exact += x;
        pos.round.x += ray.step.x * step;
        
        uint n = pos.round.x;
        n = (n | (n << 16)) & 0x030000FF;
        n = (n | (n <<  8)) & 0x0300F00F;
        n = (n | (n <<  4)) & 0x030C30C3;
        mortonPos &= 0xFCF3CF3C;
        mortonPos |= n;

        lastHit = 0;

    } else if (y.y < y.z) {
        
        pos.exact += y;
        pos.round.y += ray.step.y * step;

        uint n = pos.round.y;
        n = (n | (n << 16)) & 0x030000FF;
        n = (n | (n <<  8)) & 0x0300F00F;
        n = (n | (n <<  4)) & 0x030C30C3;
        mortonPos &= 0xFCF3CF3C << 2 | 0x3;
        mortonPos |= n << 2;

        lastHit = 1;

    } else {
        pos.exact += vec3(dst.z * ray.ratiosZ);
        pos.round.z += ray.step.z * step;

        uint n = pos.round.z;
        n = (n | (n << 16)) & 0x030000FF;
        n = (n | (n <<  8)) & 0x0300F00F;
        n = (n | (n <<  4)) & 0x030C30C3;
        mortonPos &= 0xFCF3CF3C << 4 | 0xf;
        mortonPos |= n << 4;

        lastHit = 2;
    }

    //pos.deltaPos = ray.absDelta - fract(pos.exact) * ray.delta;
}

void nextIntersectDDA() {

    vec3 dst = ray.step - (pos.exact - pos.round);

    if (pos.deltaPos.x < pos.deltaPos.y && pos.deltaPos.x < pos.deltaPos.z) {
        pos.round.x += ray.step.x;
        pos.deltaPos.x += ray.absDelta.x;

        uint n = pos.round.x;
        n = (n | (n << 16)) & 0x030000FF;
        n = (n | (n <<  8)) & 0x0300F00F;
        n = (n | (n <<  4)) & 0x030C30C3;
        mortonPos &= 0xFCF3CF3C;
        mortonPos |= n;

        lastHit = 0;

    } else if (pos.deltaPos.y < pos.deltaPos.z) {
        pos.round.y += ray.step.y;
        pos.deltaPos.y += ray.absDelta.y;

        uint n = pos.round.y;
        n = (n | (n << 16)) & 0x030000FF;
        n = (n | (n <<  8)) & 0x0300F00F;
        n = (n | (n <<  4)) & 0x030C30C3;
        mortonPos &= 0xFCF3CF3C << 2 | 0x3;
        mortonPos |= n << 2;

        lastHit = 1;

    } else {
        pos.round.z += ray.step.z;
        pos.deltaPos.z += ray.absDelta.z;
        
        uint n = pos.round.z;
        n = (n | (n << 16)) & 0x030000FF;
        n = (n | (n <<  8)) & 0x0300F00F;
        n = (n | (n <<  4)) & 0x030C30C3;
        mortonPos &= 0xFCF3CF3C << 4 | 0xf;
        mortonPos |= n << 4;

        lastHit = 2;
    }

    pos.exact += dst[lastHit] * ray.ratios[lastHit];
}

void getBlock() {
    
    currentMortonPos ^= mortonPos;
    int n = MAX_DEPTH-1;
    while (currentMortonPos != 0) {
        n--;
        currentMortonPos >>= 6;
    }

    currentMortonPos = mortonPos;

    depth = min(n, depth);

    uint posOffset = (MAX_DEPTH-1 - depth) * 6;

    while (depth < MAX_DEPTH) {

        block = nodes[stack[depth]];
        
        uint a = uint(block.y) & 1;
        if (a == 1) {
            return;
        }

        posOffset -= 6;
        uint index = (mortonPos >> posOffset) & 0x3f;

        a = uint(block.x >> index & 1);
        if (a == 0) {
            block = u64vec2(-1, 0);
            return;
        }

        stack[++depth] = children_array[
            uint(block.y >> 32)
        ][index];
    }

    return;
}
