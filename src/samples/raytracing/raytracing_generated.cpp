#include <vector>
#include <memory>
#include <limits>
#include <cassert>

#include "vulkan_basics.h"
#include "raytracing_generated.h"
#include "include/RayTracer_ubo.h"

#include "CrossRT.h"
ISceneObject* CreateVulkanRTX(VkDevice a_device, VkPhysicalDevice a_physDevice, uint32_t a_transferQId, uint32_t a_graphicsQId);

VkBufferUsageFlags RayTracer_Generated::GetAdditionalFlagsForUBO() const
{
  return 0;
}

static uint32_t ComputeReductionSteps(uint32_t whole_size, uint32_t wg_size)
{
  uint32_t steps = 0;
  while (whole_size > 1)
  {
    steps++;
    whole_size = (whole_size + wg_size - 1) / wg_size;
  }
  return steps;
}

void RayTracer_Generated::InitVulkanObjects(VkDevice a_device, VkPhysicalDevice a_physicalDevice, size_t a_maxThreadsCount) 
{
  physicalDevice = a_physicalDevice;
  device         = a_device;
  InitHelpers();
  InitBuffers(a_maxThreadsCount, true);
  InitKernels(".spv");
  AllocateAllDescriptorSets();

  auto queueAllFID = vk_utils::getQueueFamilyIndex(physicalDevice, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT);
  m_pAccelStruct = std::shared_ptr<ISceneObject>(CreateVulkanRTX(a_device, a_physicalDevice, queueAllFID, queueAllFID), [](ISceneObject *p) { DeleteSceneRT(p); } ); 
}

void RayTracer_Generated::UpdatePlainMembers(std::shared_ptr<vk_utils::ICopyEngine> a_pCopyEngine)
{
  const size_t maxAllowedSize = std::numeric_limits<uint32_t>::max();

  m_uboData.m_invProjView = m_invProjView;
  m_uboData.m_camPos = m_camPos;
  m_uboData.m_height = m_height;
  m_uboData.m_width = m_width;
  m_uboData.meshes_size     = uint32_t( meshes.size() );    assert( meshes.size() < maxAllowedSize );
  m_uboData.meshes_capacity = uint32_t( meshes.capacity() ); assert( meshes.capacity() < maxAllowedSize );
  m_uboData.lights_size     = uint32_t( lights.size() );    assert( lights.size() < maxAllowedSize );
  m_uboData.lights_capacity = uint32_t( lights.capacity() ); assert( lights.capacity() < maxAllowedSize );
  m_uboData.materials_size     = uint32_t( materials.size() );    assert( materials.size() < maxAllowedSize );
  m_uboData.materials_capacity = uint32_t( materials.capacity() ); assert( materials.capacity() < maxAllowedSize );
  m_uboData.mat_indices_buf_size     = uint32_t( mat_indices_buf.size() );    assert( mat_indices_buf.size() < maxAllowedSize );
  m_uboData.mat_indices_buf_capacity = uint32_t( mat_indices_buf.capacity() ); assert( mat_indices_buf.capacity() < maxAllowedSize );
  a_pCopyEngine->UpdateBuffer(m_classDataBuffer, 0, &m_uboData, sizeof(m_uboData));
}

void RayTracer_Generated::UpdateVectorMembers(std::shared_ptr<vk_utils::ICopyEngine> a_pCopyEngine)
{
  if(meshes.size() > 0)
    a_pCopyEngine->UpdateBuffer(m_vdata.meshesBuffer, 0, meshes.data(), meshes.size()*sizeof(struct LiteMath::uint2) );
  if(lights.size() > 0)
    a_pCopyEngine->UpdateBuffer(m_vdata.lightsBuffer, 0, lights.data(), lights.size()*sizeof(struct LightInfo) );
  if(materials.size() > 0)
    a_pCopyEngine->UpdateBuffer(m_vdata.materialsBuffer, 0, materials.data(), materials.size()*sizeof(struct MaterialData_pbrMR) );
  if(mat_indices_buf.size() > 0)
    a_pCopyEngine->UpdateBuffer(m_vdata.mat_indices_bufBuffer, 0, mat_indices_buf.data(), mat_indices_buf.size()*sizeof(unsigned int) );
}

void RayTracer_Generated::UpdateTextureMembers(std::shared_ptr<vk_utils::ICopyEngine> a_pCopyEngine)
{ 
}

void RayTracer_Generated::CastSingleRayMegaCmd(uint32_t tidX, uint32_t tidY, uint32_t* out_color)
{
  uint32_t blockSizeX = 256;
  uint32_t blockSizeY = 1;
  uint32_t blockSizeZ = 1;

  struct KernelArgsPC
  {
    uint32_t m_sizeX;
    uint32_t m_sizeY;
    uint32_t m_sizeZ;
    uint32_t m_tFlags;
  } pcData;
  
  uint32_t sizeX  = uint32_t(tidX);
  uint32_t sizeY  = uint32_t(tidY);
  uint32_t sizeZ  = uint32_t(1);
  
  pcData.m_sizeX  = tidX;
  pcData.m_sizeY  = tidY;
  pcData.m_sizeZ  = 1;
  pcData.m_tFlags = m_currThreadFlags;

  vkCmdPushConstants(m_currCmdBuffer, CastSingleRayMegaLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(KernelArgsPC), &pcData);
  
  vkCmdBindPipeline(m_currCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CastSingleRayMegaPipeline);
  vkCmdDispatch    (m_currCmdBuffer, (sizeX + blockSizeX - 1) / blockSizeX, (sizeY + blockSizeY - 1) / blockSizeY, (sizeZ + blockSizeZ - 1) / blockSizeZ);
 
}


void RayTracer_Generated::copyKernelFloatCmd(uint32_t length)
{
  uint32_t blockSizeX = MEMCPY_BLOCK_SIZE;

  vkCmdBindPipeline(m_currCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, copyKernelFloatPipeline);
  vkCmdPushConstants(m_currCmdBuffer, copyKernelFloatLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &length);
  vkCmdDispatch(m_currCmdBuffer, (length + blockSizeX - 1) / blockSizeX, 1, 1);
}

VkBufferMemoryBarrier RayTracer_Generated::BarrierForClearFlags(VkBuffer a_buffer)
{
  VkBufferMemoryBarrier bar = {};
  bar.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  bar.pNext               = NULL;
  bar.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
  bar.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
  bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.buffer              = a_buffer;
  bar.offset              = 0;
  bar.size                = VK_WHOLE_SIZE;
  return bar;
}

VkBufferMemoryBarrier RayTracer_Generated::BarrierForSingleBuffer(VkBuffer a_buffer)
{
  VkBufferMemoryBarrier bar = {};
  bar.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  bar.pNext               = NULL;
  bar.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
  bar.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
  bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bar.buffer              = a_buffer;
  bar.offset              = 0;
  bar.size                = VK_WHOLE_SIZE;
  return bar;
}

void RayTracer_Generated::BarriersForSeveralBuffers(VkBuffer* a_inBuffers, VkBufferMemoryBarrier* a_outBarriers, uint32_t a_buffersNum)
{
  for(uint32_t i=0; i<a_buffersNum;i++)
  {
    a_outBarriers[i].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    a_outBarriers[i].pNext               = NULL;
    a_outBarriers[i].srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    a_outBarriers[i].dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    a_outBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    a_outBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    a_outBarriers[i].buffer              = a_inBuffers[i];
    a_outBarriers[i].offset              = 0;
    a_outBarriers[i].size                = VK_WHOLE_SIZE;
  }
}

void RayTracer_Generated::CastSingleRayCmd(VkCommandBuffer a_commandBuffer, uint32_t tidX, uint32_t tidY, uint32_t* out_color)
{
  m_currCmdBuffer = a_commandBuffer;
  VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT }; 
  vkCmdBindDescriptorSets(a_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, CastSingleRayMegaLayout, 0, 1, &m_allGeneratedDS[0], 0, nullptr);
  CastSingleRayMegaCmd(tidX, tidY, out_color);
  vkCmdPipelineBarrier(m_currCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr); 
}


