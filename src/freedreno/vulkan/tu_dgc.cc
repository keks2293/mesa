/*
 * Copyright 2026 Starfield DGC development.
 * SPDX-License-Identifier: MIT
 *
 * VK_EXT_device_generated_commands for turnip (M2.1).
 *
 * Extension advertisement is in tu_device.cc; this file provides the
 * device entry points:
 *  - indirect command layout create/destroy use the common mesa layer
 *    (vk_indirect_command_layout_create/destroy) and are fully functional;
 *  - memory requirements report stride * maxSequenceCount (real PM4 size
 *    comes with the M2.3+ command translator);
 *  - preprocess/execute are no-ops for now (M2.4/M2.5 emit the real
 *    PM4 template via the M2.0 probe path);
 *  - indirect execution sets are dummy objects: vkd3d never creates them
 *    (no VKD3D DGC shader objects), they only must exist for the
 *    extension to be usable.
 */

#include "util/macros.h"

#include "tu_dgc.h"
#include "tu_device.h"
#include "vk_device.h"
#include "vk_log.h"
#include "vk_object.h"

VKAPI_ATTR VkResult VKAPI_CALL
tu_CreateIndirectCommandsLayoutEXT(VkDevice _device,
                                   const VkIndirectCommandsLayoutCreateInfoEXT *pCreateInfo,
                                   const VkAllocationCallbacks *pAllocator,
                                   VkIndirectCommandsLayoutEXT *pIndirectCommandsLayout)
{
   VK_FROM_HANDLE(tu_device, device, _device);
   struct tu_indirect_command_layout *layout;

   layout = (struct tu_indirect_command_layout *)
      vk_indirect_command_layout_create(&device->vk, pCreateInfo, pAllocator,
                                        sizeof(*layout));
   if (!layout)
      return vk_error(_device, VK_ERROR_OUT_OF_HOST_MEMORY);

   /* M2.2: v1 scope — push constants + draw/dispatch (+ count/index/vertex
    * buffers, sequence index). Execution sets (shader objects), mesh and
    * ray-tracing tokens are rejected loudly.
    */
   const uint32_t v1_dgc_info =
      BITFIELD_BIT(MESA_VK_DGC_PC) |
      BITFIELD_BIT(MESA_VK_DGC_SI) |
      BITFIELD_BIT(MESA_VK_DGC_IB) |
      BITFIELD_BIT(MESA_VK_DGC_VB) |
      BITFIELD_BIT(MESA_VK_DGC_DRAW) |
      BITFIELD_BIT(MESA_VK_DGC_DRAW_INDEXED) |
      BITFIELD_BIT(MESA_VK_DGC_DISPATCH);
   if ((layout->vk.dgc_info & ~v1_dgc_info) != 0) {
      const VkResult err = vk_errorf(
         _device, VK_ERROR_FEATURE_NOT_PRESENT,
         "DGC layout uses tokens outside the turnip v1 scope (dgc_info=0x%x, "
         "v1=0x%x: execution sets/mesh/RT not supported yet)",
         layout->vk.dgc_info, v1_dgc_info);
      vk_indirect_command_layout_destroy(&device->vk, pAllocator, &layout->vk);
      return err;
   }

   *pIndirectCommandsLayout = tu_indirect_command_layout_to_handle(layout);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
tu_DestroyIndirectCommandsLayoutEXT(VkDevice _device,
                                    VkIndirectCommandsLayoutEXT indirectCommandsLayout,
                                    const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(tu_device, device, _device);
   VK_FROM_HANDLE(tu_indirect_command_layout, layout, indirectCommandsLayout);

   if (!indirectCommandsLayout)
      return;

   vk_indirect_command_layout_destroy(&device->vk, pAllocator, &layout->vk);
}

VKAPI_ATTR void VKAPI_CALL
tu_GetGeneratedCommandsMemoryRequirementsEXT(VkDevice _device,
                                             const VkGeneratedCommandsMemoryRequirementsInfoEXT *pInfo,
                                             VkMemoryRequirements2 *pMemoryRequirements)
{
   VK_FROM_HANDLE(tu_device, device, _device);
   VK_FROM_HANDLE(tu_indirect_command_layout, layout, pInfo->indirectCommandsLayout);
   const uint32_t alignment = 256;

   pMemoryRequirements->memoryRequirements.size =
      align(layout->vk.stride * pInfo->maxSequenceCount, alignment);
   pMemoryRequirements->memoryRequirements.alignment = alignment;
   pMemoryRequirements->memoryRequirements.memoryTypeBits =
      (1u << device->physical_device->memory.non_lazy_type_count) - 1;
}

/* M2.1: no-op; the real CPU-side template build lands with M2.4. */
VKAPI_ATTR void VKAPI_CALL
tu_CmdPreprocessGeneratedCommandsEXT(VkCommandBuffer commandBuffer,
                                     const VkGeneratedCommandsInfoEXT *pGeneratedCommandsInfo,
                                     VkCommandBuffer stateCommandBuffer)
{
}

/* M2.1: no-op; M2.5 dispatches the M2.0 PM4 template instead. */
VKAPI_ATTR void VKAPI_CALL
tu_CmdExecuteGeneratedCommandsEXT(VkCommandBuffer commandBuffer,
                                  VkBool32 isPreprocessed,
                                  const VkGeneratedCommandsInfoEXT *pGeneratedCommandsInfo)
{
}

VKAPI_ATTR VkResult VKAPI_CALL
tu_CreateIndirectExecutionSetEXT(VkDevice _device,
                                 const VkIndirectExecutionSetCreateInfoEXT *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkIndirectExecutionSetEXT *pIndirectExecutionSet)
{
   VK_FROM_HANDLE(tu_device, device, _device);

   struct tu_indirect_execution_set *ies = (struct tu_indirect_execution_set *)
      vk_object_alloc(&device->vk, pAllocator, sizeof(*ies),
                      VK_OBJECT_TYPE_INDIRECT_EXECUTION_SET_EXT);
   if (!ies)
      return vk_error(_device, VK_ERROR_OUT_OF_HOST_MEMORY);

   *pIndirectExecutionSet = tu_indirect_execution_set_to_handle(ies);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
tu_DestroyIndirectExecutionSetEXT(VkDevice _device,
                                  VkIndirectExecutionSetEXT indirectExecutionSet,
                                  const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(tu_device, device, _device);
   VK_FROM_HANDLE(tu_indirect_execution_set, ies, indirectExecutionSet);

   if (!indirectExecutionSet)
      return;

   vk_object_free(&device->vk, pAllocator, ies);
}

VKAPI_ATTR void VKAPI_CALL
tu_UpdateIndirectExecutionSetPipelineEXT(VkDevice _device,
                                         VkIndirectExecutionSetEXT indirectExecutionSet,
                                         uint32_t executionSetWriteCount,
                                         const VkWriteIndirectExecutionSetPipelineEXT *pExecutionSetWrites)
{
}

VKAPI_ATTR void VKAPI_CALL
tu_UpdateIndirectExecutionSetShaderEXT(VkDevice _device,
                                       VkIndirectExecutionSetEXT indirectExecutionSet,
                                       uint32_t executionSetWriteCount,
                                       const VkWriteIndirectExecutionSetShaderEXT *pExecutionSetWrites)
{
}
