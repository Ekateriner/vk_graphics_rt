/////////////////////////////////////////////////////////////////////
/////////////  Required  Shader Features ////////////////////////////
/////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
/////////////////// include files ///////////////////////////////////
/////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
/////////////////// declarations in class ///////////////////////////
/////////////////////////////////////////////////////////////////////
#ifndef uint32_t
#define uint32_t uint
#endif
#define MAXFLOAT 1e37f
#define MINFLOAT 1e37f
#define FLT_MAX 1e37f
#define FLT_MIN -1e37f
#define FLT_EPSILON 1e-6f;
struct CRT_Hit 
{
  float    t;         ///< intersection distance from ray origin to object
  uint primId; 
  uint instId;
  uint geomId;    ///< use 4 most significant bits for geometry type; thay are zero for triangles 
  float    coords[4]; ///< custom intersection data; for triangles coords[0] and coords[1] stores baricentric coords (u,v)
};
struct MaterialData_pbrMR
{
    vec4 baseColor;

    float metallic;
    float roughness;
    int baseColorTexId;
    int metallicRoughnessTexId;

    vec3 emissionColor;
    int emissionTexId;

    int normalTexId;
    int occlusionTexId;
    float alphaCutoff;
    int alphaMode;
};
struct LightInfo{
    vec4 pos_dir;
    vec4 color;
    uint instance_id;
    int type; // 0 - point, 1 - env, 2 - mesh;
};
struct vertex {
    vec4 pos_norm;
    vec4 tex_tan;
};
const uint MAX_DEPTH = 2;
const float const_attenuation = 0.0f;
const float linear_attenuation = 0.5f;
const float quad_attenuation = 1.0f;
const vec4 m_ambient_color = vec4(0.2f, 0.2f, 0.2f, 0.2f);
const uint palette_size = 20;
const uint m_palette[20] = {
    0xffe6194b, 0xff3cb44b, 0xffffe119, 0xff0082c8,
    0xfff58231, 0xff911eb4, 0xff46f0f0, 0xfff032e6,
    0xffd2f53c, 0xfffabebe, 0xff008080, 0xffe6beff,
    0xffaa6e28, 0xfffffac8, 0xff800000, 0xffaaffc3,
    0xff808000, 0xffffd8b1, 0xff000080, 0xff808080
  };

#include "include/RayTracer_ubo.h"

/////////////////////////////////////////////////////////////////////
/////////////////// local functions /////////////////////////////////
/////////////////////////////////////////////////////////////////////

uint color_pack_rgba(vec4 rel_col)
{
//    vec4 const_255 = vec4(255.0f);
//    int4 rgba = floatBitsToInt(rel_col*const_255);
//    return (rgba[3] << 24) | (rgba[2] << 16) | (rgba[1] << 8) | rgba[0];
    return int(rel_col.x * 255 * 0x01000000) + int(rel_col.y * 0x00FF0000) + int(rel_col.z * 0x0000FF00) + int(rel_col.w * 0x000000FF);
}

vec3 DecodeNormal(uint a_data) {
    const uint a_enc_x = (a_data  & 0x0000FFFFu);
    const uint a_enc_y = ((a_data & 0xFFFF0000u) >> 16);
    const float sign   = (a_enc_x & 0x0001u) != 0 ? -1.0f : 1.0f;

    const int usX = int(a_enc_x & 0x0000FFFEu);
    const int usY = int(a_enc_y & 0x0000FFFFu);

    const int sX  = (usX <= 32767) ? usX : usX - 65536;
    const int sY  = (usY <= 32767) ? usY : usY - 65536;

    const float x = float(sX)*(1.0f / 32767.0f);
    const float y = float(sY)*(1.0f / 32767.0f);
    const float z = sign*sqrt(max(1.0f - x*x - y*y, 0.0f));

    return vec3(x,y,z);
}

vec3 EyeRayDir(float x, float y, float w, float h, mat4 a_mViewProjInv) {
  vec4 pos = vec4(2.0f * (x + 0.5f) / w - 1.0f, 2.0f * (y + 0.5f) / h - 1.0f, 0.0f, 1.0f);

  pos = a_mViewProjInv * pos;
  pos /= pos.w;

  //  pos.y *= (-1.0f);

  return normalize(pos.xyz);
}

uint fakeOffset(uint x, uint y, uint pitch) { return y*pitch + x; }  // RTV pattern, for 2D threading

#define KGEN_FLAG_RETURN            1
#define KGEN_FLAG_BREAK             2
#define KGEN_FLAG_DONT_SET_EXIT     4
#define KGEN_FLAG_SET_EXIT_NEGATIVE 8
#define KGEN_REDUCTION_LAST_STEP    16

