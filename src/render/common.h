//
// Created by ekaterina on 17.12.2021.
//

#ifndef VK_GRAPHICS_RT_COMMON_H
#define VK_GRAPHICS_RT_COMMON_H

#include "LiteMath.h"
using namespace LiteMath;

struct MaterialData_pbrMR
{
    float4 baseColor;

    float metallic;
    float roughness;
    int baseColorTexId;
    int metallicRoughnessTexId;

    float3 emissionColor;
    int emissionTexId;

    int normalTexId;
    int occlusionTexId;
    float alphaCutoff;
    int alphaMode;
};

struct LightInfo{
    float4 pos_dir;
    float4 color;
    uint32_t instance_id;
    int type; // 0 - point, 1 - env, 2 - mesh;
};

struct vertex {
    float4 pos_norm;
    float4 tex_tan;
};

#endif //VK_GRAPHICS_RT_COMMON_H
