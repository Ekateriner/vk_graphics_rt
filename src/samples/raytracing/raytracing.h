#ifndef VK_GRAPHICS_RT_RAYTRACING_H
#define VK_GRAPHICS_RT_RAYTRACING_H

#include <cstdint>
#include <memory>
#include <iostream>
#include <vector>
#include "LiteMath.h"
using namespace LiteMath;
#include "render/CrossRT.h"

struct LightInfo{
    float4 pos_dir;
    float4 color;
    uint32_t instance_id;
    int type; // 0 - point, 1 - env, 2 - mesh;
};

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


class RayTracer
{
public:
  RayTracer(uint32_t a_width, uint32_t a_height) : m_width(a_width), m_height(a_height) {}

  void UpdateView(const LiteMath::float3& a_camPos, const LiteMath::float4x4& a_invProjView ) { m_camPos = to_float4(a_camPos, 1.0f); m_invProjView = a_invProjView; }
  void SetScene(std::shared_ptr<ISceneObject> a_pAccelStruct) { m_pAccelStruct = a_pAccelStruct; };

  //uint32_t EncodeColor(LiteMath::float4 color);
  //LiteMath::float3 DecodeNormal(uint32_t a_data);

  void CastSingleRay(uint32_t tidX, uint32_t tidY, uint32_t* out_color);
  void kernel_InitEyeRay(uint32_t tidX, uint32_t tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar);
  void kernel_RayTrace(uint32_t tidX, uint32_t tidY, const LiteMath::float4* rayPosAndNear, const LiteMath::float4* rayDirAndFar, uint32_t* out_color);

  //LiteMath::float4 DecodeColor(uint32_t color);



protected:

  LiteMath::float4 HitNormal(CRT_Hit hit);
  LiteMath::float4 HitPos(CRT_Hit hit);
  LiteMath::float4 CookTorrance(CRT_Hit hit, LiteMath::float4 light_dir);
  LiteMath::float4 Shade(CRT_Hit hit, LightInfo light);

  uint32_t m_width;
  uint32_t m_height;

  LiteMath::float4   m_camPos;
  LiteMath::float4x4 m_invProjView;
  LiteMath::float4   m_ambient_color;

  std::vector<LightInfo> lights;
  std::vector<LiteMath::uint2> meshes;
  std::vector<MaterialData_pbrMR> materials;
  std::vector<LiteMath::float4x4> inst_matrices;
  std::vector<LiteMath::float4> vertices_buf;
  std::vector<uint32_t> indices_buf;
  std::vector<uint32_t> mat_indices_buf;


  std::shared_ptr<ISceneObject> m_pAccelStruct;

  static constexpr uint32_t MAX_DEPTH = 3;
  static constexpr float const_attenuation = 0.0f;
  static constexpr float linear_attenuation = 0.0f;
  static constexpr float quad_attenuation = 1.0f;
  static constexpr float PI = 3.14159265358979323846f;

  static constexpr uint32_t palette_size = 20;
  // color palette to select color for objects based on mesh/instance id
  static constexpr uint32_t m_palette[palette_size] = {
    0xffe6194b, 0xff3cb44b, 0xffffe119, 0xff0082c8,
    0xfff58231, 0xff911eb4, 0xff46f0f0, 0xfff032e6,
    0xffd2f53c, 0xfffabebe, 0xff008080, 0xffe6beff,
    0xffaa6e28, 0xfffffac8, 0xff800000, 0xffaaffc3,
    0xff808000, 0xffffd8b1, 0xff000080, 0xff808080
  };
};

#endif// VK_GRAPHICS_RT_RAYTRACING_H
