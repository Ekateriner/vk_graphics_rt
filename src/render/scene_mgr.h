#ifndef CHIMERA_SCENE_MGR_H
#define CHIMERA_SCENE_MGR_H

#include <vector>

#include <geom/vk_mesh.h>
#include <ray_tracing/vk_rt_utils.h>
#include "LiteMath.h"
#include <vk_copy.h>
#include <vk_images.h>

#include "../loader_utils/hydraxml.h"
#include "../loader_utils/image_loader.h"
#include "tiny_gltf.h"
#include "../resources/shaders/common.h"
#include "common.h"

struct InstanceInfo
{
  uint32_t inst_id = 0u;
  uint32_t mesh_id = 0u;
  VkDeviceSize instBufOffset = 0u;
  bool renderMark = false;
};

enum class BVH_BUILDER_TYPE
{
  RTX,
  // CPU,
  // ...
};

enum class MATERIAL_FORMAT
{
  METALLIC_ROUGHNESS,
  // HYDRA_CLASSIC
  // PRINCIPLED
  // etc.
};

enum class MATERIAL_LOAD_MODE
{
  NONE,
  MATERIALS_ONLY,
  MATERIALS_AND_TEXTURES
};

struct LoaderConfig
{
  bool load_geometry = true;
  MATERIAL_LOAD_MODE load_materials = MATERIAL_LOAD_MODE::MATERIALS_AND_TEXTURES;
  bool build_acc_structs = false;
  bool build_acc_structs_while_loading_scene = false;
  bool instance_matrix_as_vertex_attribute = false;
  bool debug_output = false;
  BVH_BUILDER_TYPE builder_type = BVH_BUILDER_TYPE::RTX;
  MATERIAL_FORMAT material_format = MATERIAL_FORMAT::METALLIC_ROUGHNESS;
};

struct SceneManager
{
  SceneManager(VkDevice a_device, VkPhysicalDevice a_physDevice, uint32_t a_graphicsQId,
    std::shared_ptr<vk_utils::ICopyEngine> a_pCopyHelper, LoaderConfig a_config = {});
  ~SceneManager();

  bool LoadSceneXML(const std::string &scenePath, bool transpose = true);
  bool LoadSceneGLTF(const std::string &scenePath);
  bool LoadScene(const std::string &scenePath); // guess scene type by extension
//  void LoadSingleTriangle(); // TODO: rework

  bool InitEmptyScene(uint32_t maxMeshes, uint32_t maxTotalVertices, uint32_t maxTotalPrimitives, uint32_t maxPrimitivesPerMesh);

  uint32_t AddMeshFromFile(const std::string& meshPath);
  uint32_t AddMeshFromData(cmesh::SimpleMesh &meshData);

  uint32_t InstanceMesh(uint32_t meshId, const LiteMath::float4x4 &matrix, bool markForRender = true);

  void MarkInstance(uint32_t instId);
  void UnmarkInstance(uint32_t instId);

  void DrawMarkedInstances();

  void DestroyScene();

  VkPipelineVertexInputStateCreateInfo GetPipelineVertexInputStateCreateInfo();

  VkBuffer GetLightsBuffer()       const { return m_lightsBuf; }
  VkBuffer GetVertexBuffer()       const { return m_geoVertBuf; }
  VkBuffer GetIndexBuffer()        const { return m_geoIdxBuf; }
  VkBuffer GetMeshInfoBuffer()     const { return m_meshInfoBuf; }
  VkBuffer GetInstanceMatBuffer()  const { return m_instMatricesBuf; }
  VkBuffer GetMaterialsBuffer()    const { return m_materialBuf; }
  VkBuffer GetMaterialIDsBuffer()  const { return m_matIdsBuf; }

  std::vector<LightInfo> GetLightsVector() const {
      return m_lights;
  }

  std::vector<vertex> GetVertexVector() const {
      std::vector<vertex> vert;
      vert.resize(m_pMeshData->VertexDataSize() / m_pMeshData->SingleVertexSize());
      memcpy(vert.data(), m_pMeshData->VertexData(), m_pMeshData->VertexDataSize());
      return vert; }

  std::vector<uint32_t> GetIndexVector() const {
        std::vector<uint32_t> ind;
        ind.resize(m_pMeshData->IndexDataSize());
        memcpy(ind.data(), m_pMeshData->IndexData(), m_pMeshData->IndexDataSize());
        return ind; }

  std::vector<LiteMath::uint2> GetMeshInfoVector() const {
      std::vector<LiteMath::uint2> mesh_info_tmp;
      for(const auto& m : m_meshInfos)
      {
          mesh_info_tmp.emplace_back(m.m_indexOffset, m.m_vertexOffset);
      }
      return mesh_info_tmp;
  }

  std::vector<LiteMath::float4x4> GetInstanceMatVector() const {
      return m_instanceMatrices;
  }

  std::vector<MaterialData_pbrMR> GetMaterialsVector() const {
      return m_materials;
  }

  std::vector<uint32_t> GetMaterialIDsVector() const {
      return m_matIDs;
  }

  std::vector<VkSampler> GetTextureSamplers() const { return m_samplers; }
  std::vector<VkImageView>  GetTextureViews() const { return m_textureViews; }

  std::shared_ptr<IMeshData> GetMeshData() {return m_pMeshData; }

  uint32_t MeshesNum()    const {return m_meshInfos.size();}
  uint32_t InstancesNum() const {return m_instanceInfos.size();}

  hydra_xml::Camera GetCamera(uint32_t camId) const;
  MeshInfo GetMeshInfo(uint32_t meshId) const {assert(meshId < m_meshInfos.size()); return m_meshInfos[meshId];}
  InstanceInfo GetInstanceInfo(uint32_t instId) const {assert(instId < m_instanceInfos.size()); return m_instanceInfos[instId];}
  LiteMath::float4x4 GetInstanceMatrix(uint32_t instId) const {assert(instId < m_instanceMatrices.size()); return m_instanceMatrices[instId];}

//  void DestroyAS();

  VkAccelerationStructureKHR GetTLAS() const { return m_pBuilderV2->GetTLAS(); }
  void BuildAllBLAS();
  void BuildTLAS();

private:
  const std::string missingTextureImgPath = "../resources/data/missing_texture.png";

  vk_utils::VulkanImageMem LoadSpecialTexture();
  void InitGeoBuffersGPU(uint32_t a_meshNum, uint32_t a_totalVertNum, uint32_t a_totalIndicesNum);
  void LoadOneMeshOnGPU(uint32_t meshIdx);
  void LoadCommonGeoDataOnGPU();
  void LoadInstanceDataOnGPU();
  void LoadMaterialDataOnGPU();

  void AddBLAS(uint32_t meshIdx);

  void LoadGLTFNodesRecursive(const tinygltf::Model &a_model, const tinygltf::Node& a_node, const LiteMath::float4x4& a_parentMatrix,
    std::unordered_map<int, uint32_t> &a_loadedMeshesToMeshId);

  std::vector<MeshInfo> m_meshInfos = {};
  std::shared_ptr<IMeshData> m_pMeshData = nullptr;

  std::vector<InstanceInfo> m_instanceInfos = {};
  std::vector<LiteMath::float4x4> m_instanceMatrices = {};

  std::vector<hydra_xml::Camera> m_sceneCameras = {};

  uint32_t m_totalVertices = 0u;
  uint32_t m_totalIndices  = 0u;

  VkBuffer m_geoVertBuf        = VK_NULL_HANDLE;
  VkBuffer m_geoIdxBuf         = VK_NULL_HANDLE;
  VkBuffer m_meshInfoBuf       = VK_NULL_HANDLE;
  VkBuffer m_matIdsBuf         = VK_NULL_HANDLE;
  VkDeviceMemory m_geoMemAlloc = VK_NULL_HANDLE;

  VkBuffer m_instMatricesBuf    = VK_NULL_HANDLE;
  VkDeviceMemory m_instMemAlloc = VK_NULL_HANDLE;

  std::vector<LightInfo> m_lights = {};
  VkBuffer m_lightsBuf = VK_NULL_HANDLE;
  VkDeviceMemory m_lightsMemAlloc = VK_NULL_HANDLE;

  VkDeviceSize m_loadedVertices = 0;
  VkDeviceSize m_loadedIndices  = 0;

  std::vector<uint32_t> m_matIDs;

  std::vector<MaterialData_pbrMR> m_materials;
  std::vector<ImageFileInfo> m_textureInfos;
  VkBuffer m_materialBuf  = VK_NULL_HANDLE;
  VkDeviceMemory m_matMemAlloc = VK_NULL_HANDLE;
  std::vector<vk_utils::VulkanImageMem> m_textures;
  std::unordered_map<uint32_t, vk_utils::VulkanImageMem&> m_texturesById;
  VkDeviceMemory m_texturesMemAlloc = VK_NULL_HANDLE;
  std::vector<VkSampler> m_samplers;
  std::vector<VkImageView> m_textureViews;

  VkDevice m_device = VK_NULL_HANDLE;
  VkPhysicalDevice m_physDevice = VK_NULL_HANDLE;
  VkCommandPool m_pool = VK_NULL_HANDLE;

  uint32_t m_graphicsQId = UINT32_MAX;
  VkQueue  m_graphicsQ   = VK_NULL_HANDLE;
  std::shared_ptr<vk_utils::ICopyEngine> m_pCopyHelper;

  std::unique_ptr<vk_rt_utils::AccelStructureBuilderV2> m_pBuilderV2;

  std::vector<vk_rt_utils::BLASBuildInput> m_blasData;

  LoaderConfig m_config;
  bool m_useRTX = false;
};

#endif//CHIMERA_SCENE_MGR_H
