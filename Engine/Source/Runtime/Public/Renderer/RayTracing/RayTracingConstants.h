#pragma once
#include "Math/MathGLM.h"

struct FLightConstants {
    FVec3 direction;
    int lightType;
    FVec3 position;
    float radius;
    FVec3 color;
    float intensity;
    float angularSizeOrInvRange;
    float innerAngle;
    float outerAngle;
    float outOfBoundsShadow;
    glm::ivec4 shadowCascades;
    glm::ivec4 perObjectShadows;
    glm::ivec4 shadowChannel;
};

struct FViewConstants {
    FMat4 matWorldToView;
    FMat4 matViewToClip;
    FMat4 matClipToView;
    FMat4 matClipToWorld;
    FVec4 viewportOrigin;
    FVec4 viewportSize;
    FVec4 viewportSizeInv;
    FVec4 cameraDirectionOrPosition;
};

struct FLightingConstants {
    FVec4 ambientColor;
    FLightConstants light;
    FViewConstants view;
};

