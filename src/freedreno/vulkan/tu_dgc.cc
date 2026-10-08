/*
 * Copyright 2026 Starfield DGC development.
 * SPDX-License-Identifier: MIT
 *
 * VK_EXT_device_generated_commands for turnip (M2.1/M2.3).
 *
 * Extension advertisement is in tu_device.cc; this file provides the
 * device entry points:
 *  - indirect command layout create/destroy use the common mesa layer
 *    (vk_indirect_command_layout_create/destroy) and are fully functional;
 *    M2.3 additionally uploads a GPU-side descriptor (tu_dgc_gpu_layout)
 *    to a BO owned by the layout object;
 *  - memory requirements report dwords_per_seq * maxSequenceCount (the
 *    exact PM4 size the M2.3 command translator generates);
 *  - preprocess is a no-op (M2.3 does everything at Execute time);
 *  - execute (M2.3): dispatches an internal preprocess shader that
 *    translates the sequences into a PM4 stream, then emits the M2.0
 *    barrier and the CP_INDIRECT_BUFFER. Scope: PUSH_CONSTANT /
 *    SEQUENCE_INDEX / DRAW / DRAW_INDEXED tokens; DISPATCH and the rest
 *    land with M2.4+;
 *  - indirect execution sets are dummy objects: vkd3d never creates them
 *    (no VKD3D DGC shader objects), they only must exist for the
 *    extension to be usable.
 */

#include "util/macros.h"

#include "tu_cmd_buffer.h"
#include "tu_dgc.h"
#include "tu_device.h"
#include "tu_knl.h"
#include "vk_device.h"
#include "vk_log.h"
#include "vk_object.h"

/* PM4 size of one generated sequence, in dwords (M2.3 scope). */
static uint32_t
tu_dgc_token_dwords(uint32_t size_B)
{
   /* PC/SI: PKT4 header + data. */
   return 1 + size_B / 4;
}

static void
tu_dgc_build_gpu_layout(const VkIndirectCommandsLayoutCreateInfoEXT *ci,
                        struct tu_dgc_gpu_layout *desc,
                        uint32_t *unsupported_tokens)
{
   desc->stride = ci->indirectStride;
   desc->token_count = ci->tokenCount;

   for (uint32_t i = 0; i < ci->tokenCount; i++) {
      const VkIndirectCommandsLayoutTokenEXT *tok = &ci->pTokens[i];

      desc->tokens[i].type = (uint32_t)tok->type;
      desc->tokens[i].src_off = tok->offset;
      desc->tokens[i].dst_off = 0;
      desc->tokens[i].size = 0;

      switch (tok->type) {
      case VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT:
      case VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_DATA_EXT:
         desc->tokens[i].dst_off = tok->data.pPushConstant->updateRange.offset;
         desc->tokens[i].size = tok->data.pPushConstant->updateRange.size;
         if (desc->tokens[i].size == 0 ||
             desc->tokens[i].size % 4 != 0) {
            /* A PKT4 with cnt = size/4 == 0 is invalid PM4; refuse the
             * layout at Execute time (loudly). */
            (*unsupported_tokens)++;
            break;
         }
         desc->dwords_per_seq += tu_dgc_token_dwords(desc->tokens[i].size);
         if (getenv("DGC_DUMP"))
            fprintf(stderr, "dgc_tok[%u]: type=%u size=%u dst=%u src=%u -> +=%u total=%u\n",
                    i, tok->type, desc->tokens[i].size,
                    desc->tokens[i].dst_off, tok->offset,
                    tu_dgc_token_dwords(desc->tokens[i].size),
                    desc->dwords_per_seq);
         break;
      case VK_INDIRECT_COMMANDS_TOKEN_TYPE_SEQUENCE_INDEX_EXT:
      case VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_DATA_SEQUENCE_INDEX_EXT:
         desc->tokens[i].dst_off = tok->data.pPushConstant->updateRange.offset;
         desc->tokens[i].size = tok->data.pPushConstant->updateRange.size;
         if (desc->tokens[i].size != 4) {
            (*unsupported_tokens)++;
            break;
         }
         desc->dwords_per_seq += 1 + 1; /* header + the index itself */
         break;
      case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_EXT:
      case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_EXT: {
         /* CP_DRAW_INDIRECT_MULTI: header + payload. Payload — как у
          * штатных tu_CmdDraw{Indexed}Indirect: DRAW — 6 dword'ов,
          * DRAW_INDEXED — 9 (index VA + max). */
         const uint32_t dws =
            tok->type == VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_EXT ?
               1 + 9 :
               1 + 6;
         desc->dwords_per_seq += dws;
         if (getenv("DGC_DUMP"))
            fprintf(stderr, "dgc_tok[%u]: type=%u src=%u -> +=%u total=%u\n",
                    i, tok->type, tok->offset, dws, desc->dwords_per_seq);
         break;
      }
      default:
         /* DISPATCH / IB / VB / IES / RT / *_COUNT: not translatable in
          * M2.3; the layout is refused at Execute time (loudly). */
         (*unsupported_tokens)++;
         break;
      }
   }
}

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

   /* M2.3: upload the GPU-side descriptor. */
   struct tu_dgc_gpu_layout desc = {};
   uint32_t unsupported_tokens = 0;
   tu_dgc_build_gpu_layout(pCreateInfo, &desc, &unsupported_tokens);
   layout->dwords_per_seq = desc.dwords_per_seq;
   layout->unsupported_tokens = unsupported_tokens;

   VkResult result =
      tu_bo_init_new(device, NULL, &layout->bo, sizeof(desc), TU_BO_ALLOC_NO_FLAGS,
                     "dgc layout descriptor");
   if (result != VK_SUCCESS) {
      vk_indirect_command_layout_destroy(&device->vk, pAllocator, &layout->vk);
      return vk_error(_device, result);
   }
   if (tu_bo_map(device, layout->bo, NULL) != VK_SUCCESS) {
      tu_bo_finish(device, layout->bo);
      layout->bo = NULL;
      vk_indirect_command_layout_destroy(&device->vk, pAllocator, &layout->vk);
      return vk_error(_device, VK_ERROR_OUT_OF_HOST_MEMORY);
   }
   memcpy(layout->bo->map, &desc, sizeof(desc));
   (void)tu_bo_unmap(device, layout->bo, false);

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

   if (layout->bo)
      tu_bo_finish(device, layout->bo);

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

   /* M2.3: the generated PM4 stream is dwords_per_seq dwords per sequence. */
   pMemoryRequirements->memoryRequirements.size =
      align(layout->vk.stride * pInfo->maxSequenceCount, alignment);
   pMemoryRequirements->memoryRequirements.alignment = alignment;
   pMemoryRequirements->memoryRequirements.memoryTypeBits =
      (1u << device->physical_device->memory.non_lazy_type_count) - 1;
}

/* M2.1: no-op; M2.3 does everything at Execute time. */
VKAPI_ATTR void VKAPI_CALL
tu_CmdPreprocessGeneratedCommandsEXT(VkCommandBuffer commandBuffer,
                                     const VkGeneratedCommandsInfoEXT *pGeneratedCommandsInfo,
                                     VkCommandBuffer stateCommandBuffer)
{
}

/* M2.3: dispatch the internal preprocess shader (sequences -> PM4 stream),
 * emit the M2.0 barrier and the CP_INDIRECT_BUFFER. The chip-specific body
 * lives in tu_cmd_buffer.cc (tu_dgc_execute_generated_commands). */
VKAPI_ATTR void VKAPI_CALL
tu_CmdExecuteGeneratedCommandsEXT(VkCommandBuffer commandBuffer,
                                  VkBool32 isPreprocessed,
                                  const VkGeneratedCommandsInfoEXT *pGeneratedCommandsInfo)
{
   VK_FROM_HANDLE(tu_cmd_buffer, cmd, commandBuffer);
   tu_dgc_execute_generated_commands(cmd, isPreprocessed, pGeneratedCommandsInfo);
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
