#include "raytracing.h"
#include "float.h"

float3 EyeRayDir(float x, float y, float w, float h, float4x4 a_mViewProjInv)
{
  float4 pos = make_float4( 2.0f * (x + 0.5f) / w - 1.0f,
    2.0f * (y + 0.5f) / h - 1.0f,
    0.0f,
    1.0f );

  pos = a_mViewProjInv * pos;
  pos /= pos.w;

  //  pos.y *= (-1.0f);

  return normalize(to_float3(pos));
}

uint EncodeColor(float4 color) {
    return int(color.x * 255 * 0x01000000) + int(color.y * 0x00FF0000) + int(color.z * 0x0000FF00) + int(color.w * 0x000000FF);
}

float3 DecodeNormal(uint a_data)
{
    const uint a_enc_x = (a_data  & 0x0000FFFFu);
    const uint a_enc_y = ((a_data & 0xFFFF0000u) >> 16);
    const float sign   = (a_enc_x & 0x0001u) != 0 ? -1.0f : 1.0f;

    const int usX = int(a_enc_x & 0x0000FFFEu);
    const int usY = int(a_enc_y & 0x0000FFFFu);

    const int sX  = (usX <= 32767) ? usX : usX - 65536;
    const int sY  = (usY <= 32767) ? usY : usY - 65536;

    const float x = sX*(1.0f / 32767.0f);
    const float y = sY*(1.0f / 32767.0f);
    const float z = sign*sqrt(fmax(1.0f - x*x - y*y, 0.0f));

    return float3(x, y, z);
}


void RayTracer::kernel_InitEyeRay(uint32_t tidX, uint32_t tidY, float4* rayPosAndNear, float4* rayDirAndFar)
{
  *rayPosAndNear = m_camPos; // to_float4(m_camPos, 1.0f);

  const float3 rayDir  = EyeRayDir(float(tidX), float(tidY), float(m_width), float(m_height), m_invProjView);
  *rayDirAndFar  = to_float4(rayDir, FLT_MAX);
}

void RayTracer::kernel_RayTrace(uint32_t tidX, uint32_t tidY, const float4* rayPosAndNear, const float4* rayDirAndFar, uint32_t* out_color)
{
  float4 color (0.0f, 0.0f, 0.0f, 0.0f);
  float4 k (1.0f, 1.0f, 1.0f, 1.0f);

  float4 rayPos = *rayPosAndNear;
  float4 rayDir = *rayDirAndFar ;

  for(uint32_t i = 0; i < MAX_DEPTH; i++) {
      CRT_Hit hit = m_pAccelStruct->RayQuery_NearestHit(rayPos, rayDir);

      if(hit.primId == -1) {
          break;
      }

      float4 hit_point = rayPos + rayDir * hit.t;

      for (int j = 0; j < lights.size(); j++) {
          if(lights[j].type == 0) {
              if (!m_pAccelStruct->RayQuery_AnyHit(hit_point, lights[j].pos_dir - hit_point)) {
                  color = k * Shade(hit, lights[j]);
              }
          }
          else if (lights[j].type == 1) {
              if (!m_pAccelStruct->RayQuery_AnyHit(hit_point, -1 * lights[j].pos_dir)) {
                  color = k * Shade(hit, lights[j]);
              }
          }
          else {
              if (m_pAccelStruct->RayQuery_NearestHit(hit_point, lights[j].pos_dir - hit_point).instId == lights[j].instance_id) {
                  color = k * Shade(hit, lights[j]);
              }
          }
      }

      uint32_t mat_ind = mat_indices_buf[meshes[hit.geomId].x / 3 + hit.primId];

      if (materials[mat_ind].metallic <= 10e-4){
          break;
      }

      rayPos = hit_point;
      rayDir = reflect(normalize(rayDir), HitNormal(hit));

      k *= CookTorrance(hit, rayDir);


  }

  out_color[tidY * m_width + tidX] = EncodeColor(color);
  //out_color[tidY * m_width + tidX] = m_palette[hit.instId % palette_size];
}

void RayTracer::CastSingleRay(uint32_t tidX, uint32_t tidY, uint32_t* out_color)
{
    float4 rayPosAndNear, rayDirAndFar;
    kernel_InitEyeRay(tidX, tidY, &rayPosAndNear, &rayDirAndFar);

    kernel_RayTrace(tidX, tidY, &rayPosAndNear, &rayDirAndFar, out_color);
}

float4 RayTracer::HitNormal(CRT_Hit hit) {
    uint32_t ind0 = indices_buf[meshes[hit.geomId].x + 3 * hit.primId + 0];
    uint32_t ind1 = indices_buf[meshes[hit.geomId].x + 3 * hit.primId + 1];
    uint32_t ind2 = indices_buf[meshes[hit.geomId].x + 3 * hit.primId + 2];

    float4 ver0 = vertices_buf[meshes[hit.geomId].y + ind0];
    float4 ver1 = vertices_buf[meshes[hit.geomId].y + ind1];
    float4 ver2 = vertices_buf[meshes[hit.geomId].y + ind2];

    float3 norm0 = DecodeNormal(as_uint32(ver0.w));
    float3 norm1 = DecodeNormal(as_uint32(ver1.w));
    float3 norm2 = DecodeNormal(as_uint32(ver2.w));

    float3 normal = norm0 * hit.coords[0] + norm1 * hit.coords[1] + norm2 * (1 - hit.coords[0] - hit.coords[1]);
    return normalize(transpose(inverse4x4(inst_matrices[hit.instId])) * to_float4(normal, 0.0));
}

float4 RayTracer::HitPos(CRT_Hit hit) {
    uint32_t ind0 = indices_buf[meshes[hit.geomId].x + 3 * hit.primId + 0];
    uint32_t ind1 = indices_buf[meshes[hit.geomId].x + 3 * hit.primId + 1];
    uint32_t ind2 = indices_buf[meshes[hit.geomId].x + 3 * hit.primId + 2];

    float4 ver0 = vertices_buf[meshes[hit.geomId].y + ind0];
    float4 ver1 = vertices_buf[meshes[hit.geomId].y + ind1];
    float4 ver2 = vertices_buf[meshes[hit.geomId].y + ind2];

    float3 pos0 = to_float3(ver0);
    float3 pos1 = to_float3(ver1);
    float3 pos2 = to_float3(ver2);

    float3 position = pos0 * hit.coords[0] + pos1 * hit.coords[1] + pos2 * (1 - hit.coords[0] - hit.coords[1]);
    return inst_matrices[hit.instId] * to_float4(position, 1.0);
}

float4 RayTracer::CookTorrance(CRT_Hit hit, float4 light_dir) {
    uint32_t mat_ind = mat_indices_buf[meshes[hit.geomId].x / 3 + hit.primId];

    float4 p = HitPos(hit);
    float4 n = HitNormal(hit);
    float4 l = normalize(light_dir);
    float4 v = normalize(m_camPos - p);

    float4 h = normalize(l + v);
    float nh = dot (n, h);
    float nv = dot (n, v);
    float nl = dot (n, l);
    float vh = dot (v, h);

    // GGX
    float m = materials[mat_ind].roughness * materials[mat_ind].roughness;
    float m2 = m * m;
    float nh2 = nh * nh;
    float d = (m2 - 1.0f) * nh2 + 1.0f;
    d = m2 / (PI * d * d);

    // Frensel
    float product = clamp(nv, 0.0f, 1.0f);
    float3 f0 = mix(float3(0.04),
                                        to_float3(materials[mat_ind].baseColor),
                                        materials[mat_ind].metallic);
    float3 f = mix(f0, float3(1.0), std::pow(1.0 - product, 5.0));

    // G_default
    float g = std::min(1.0f, std::min(2.0f * nh * nv / vh, 2.0f * nh * nl / vh));

    float3 ct = f * (0.25f * d * g / nv);
    float diff = fmax(nl, 0.0f);
    float ks = 0.5f;

    return to_float4(diff * to_float3(materials[mat_ind].baseColor) + ks * ct, 1.0 );
}

float4 RayTracer::Shade(CRT_Hit hit, LightInfo light) {
    float4 normal = HitNormal(hit);
    float4 position = HitPos(hit);
    uint32_t mat_ind = mat_indices_buf[meshes[hit.geomId].x / 3 + hit.primId];

    float attenuation = 1.0f / (const_attenuation + linear_attenuation * hit.t + quad_attenuation * hit.t * hit.t);

    float4 color = float4(0.0);
    if (light.type == 0 || light.type == 2) {
        color = to_float4(materials[mat_ind].emissionColor, 1.0) + m_ambient_color + \
                (materials[mat_ind].baseColor * dot(normalize(normal), normalize(light.pos_dir - position)) + \
                CookTorrance(hit, light.pos_dir - position)) * attenuation;
    }
    if (light.type == 1) {
        color =  to_float4(materials[mat_ind].emissionColor, 1.0) + m_ambient_color + \
                 (materials[mat_ind].baseColor * dot(normalize(normal), normalize(-1.0f * light.pos_dir)) + \
                 CookTorrance(hit, light.pos_dir)) * attenuation;
    }

    return color;
}

//float4 RayTracer::DecodeColor(uint32_t color) {
//    return float4(float(color / 0x01000000) / 255,
//                            float((color % 0x01000000) / 0x00010000) / 255,
//                            float((color % 0x00010000) / 0x00000100) / 255,
//                            float(color % 0x00000100) / 255);
//}