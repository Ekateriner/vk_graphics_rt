#ifndef MAIN_CLASS_DECL_RayTracer_H
#define MAIN_CLASS_DECL_RayTracer_H

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include "vulkan_basics.h"

#include "vk_pipeline.h"
#include "vk_buffers.h"
#include "vk_utils.h"

#include "raytracing.h"

#include "include/RayTracer_ubo.h"

class RayTracer_Generated : public RayTracer
{
public:

  RayTracer_Generated(uint32_t a_width, uint32_t a_height) : RayTracer(a_width, a_height){}
  virtual void InitVulkanObjects(VkDevice a_device, VkPhysicalDevice a_physicalDevice, size_t a_maxThreadsCount);

  virtual void SetVulkanInOutFor_CastSingleRay(
    VkImage     out_colorText,
    VkImageView out_colorView,
    uint32_t dummyArgument = 0)
  {
    CastSingleRay_local.out_colorText   = out_colorText;
    CastSingleRay_local.out_colorView   = out_colorView;
    InitAllGeneratedDescriptorSets_CastSingleRay();
  }

  virtual ~RayTracer_Generated();


  virtual void InitMemberBuffers();

  virtual void UpdateAll(std::shared_ptr<vk_utils::ICopyEngine> a_pCopyEngine)
  {
    UpdatePlainMembers(a_pCopyEngine);
    UpdateVectorMembers(a_pCopyEngine);
    UpdateTextureMembers(a_pCopyEngine);
  }
  
  virtual void UpdatePlainMembers(std::shared_ptr<vk_utils::ICopyEngine> a_pCopyEngine);
  virtual void UpdateVectorMembers(std::shared_ptr<vk_utils::ICopyEngine> a_pCopyEngine);
  virtual void UpdateTextureMembers(std::shared_ptr<vk_utils::ICopyEngine> a_pCopyEngine);

  virtual void CastSingleRayCmd(VkCommandBuffer a_commandBuffer, uint32_t tidX, uint32_t tidY, Texture2D<uint>& out_color);

  virtual void copyKernelFloatCmd(uint32_t length);
  
  virtual void InitEyeRayCmd(uint32_t tidX, uint32_t tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar);
  virtual void RayTraceCmd(uint32_t tidX, uint32_t tidY, const LiteMath::float4* rayPosAndNear, const LiteMath::float4* rayDirAndFar, Texture2D<uint>& out_color);
protected:
  
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice         device         = VK_NULL_HANDLE;

  VkCommandBuffer  m_currCmdBuffer   = VK_NULL_HANDLE;
  uint32_t         m_currThreadFlags = 0;

  std::unique_ptr<vk_utils::ComputePipelineMaker> m_pMaker = nullptr;
  VkPhysicalDeviceProperties m_devProps;

  VkBufferMemoryBarrier BarrierForClearFlags(VkBuffer a_buffer);
  VkBufferMemoryBarrier BarrierForSingleBuffer(VkBuffer a_buffer);
  void BarriersForSeveralBuffers(VkBuffer* a_inBuffers, VkBufferMemoryBarrier* a_outBarriers, uint32_t a_buffersNum);

  virtual void InitHelpers();
  virtual void InitBuffers(size_t a_maxThreadsCount, bool a_tempBuffersOverlay = true);
  virtual void InitKernels(const char* a_filePath);
  virtual void AllocateAllDescriptorSets();

  virtual void InitAllGeneratedDescriptorSets_CastSingleRay();

  virtual void AllocMemoryForInternalBuffers(const std::vector<VkBuffer>& a_buffers);
  virtual void AssignBuffersToMemory(const std::vector<VkBuffer>& a_buffers, VkDeviceMemory a_mem);

  virtual void AllocMemoryForMemberBuffersAndImages(const std::vector<VkBuffer>& a_buffers, const std::vector<VkImage>& a_image);
  
  virtual void FreeMemoryForInternalBuffers();
  virtual void FreeMemoryForMemberBuffersAndImages();
  virtual std::string AlterShaderPath(const char* in_shaderPath) { return std::string(in_shaderPath); }

  
  

  struct CastSingleRay_Data
  {
    VkBuffer rayDirAndFarBuffer = VK_NULL_HANDLE;
    size_t   rayDirAndFarOffset = 0;

    VkBuffer rayPosAndNearBuffer = VK_NULL_HANDLE;
    size_t   rayPosAndNearOffset = 0;

    VkImage     out_colorText = VK_NULL_HANDLE;
    VkImageView out_colorView = VK_NULL_HANDLE;
  } CastSingleRay_local;



  struct MembersDataGPU
  {
    VkDeviceMemory m_vecMem = VK_NULL_HANDLE;
    VkDeviceMemory m_texMem = VK_NULL_HANDLE;
  } m_vdata;

  size_t m_maxThreadCount = 0;
  VkBuffer m_classDataBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_allMem    = VK_NULL_HANDLE;

  VkPipelineLayout      InitEyeRayLayout   = VK_NULL_HANDLE;
  VkPipeline            InitEyeRayPipeline = VK_NULL_HANDLE; 
  VkDescriptorSetLayout InitEyeRayDSLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout CreateInitEyeRayDSLayout();
  void InitKernel_InitEyeRay(const char* a_filePath);
  VkPipelineLayout      RayTraceLayout   = VK_NULL_HANDLE;
  VkPipeline            RayTracePipeline = VK_NULL_HANDLE; 
  VkDescriptorSetLayout RayTraceDSLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout CreateRayTraceDSLayout();
  void InitKernel_RayTrace(const char* a_filePath);


  virtual VkBufferUsageFlags GetAdditionalFlagsForUBO() const;

  VkPipelineLayout      copyKernelFloatLayout   = VK_NULL_HANDLE;
  VkPipeline            copyKernelFloatPipeline = VK_NULL_HANDLE;
  VkDescriptorSetLayout copyKernelFloatDSLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout CreatecopyKernelFloatDSLayout();

  VkDescriptorPool m_dsPool = VK_NULL_HANDLE;
  VkDescriptorSet  m_allGeneratedDS[2];

  RayTracer_UBO_Data m_uboData;
  
  constexpr static uint32_t MEMCPY_BLOCK_SIZE = 256;
  constexpr static uint32_t REDUCTION_BLOCK_SIZE = 256;
};

#endif

