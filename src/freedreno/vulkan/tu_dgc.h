/*
 * Copyright 2026 Starfield DGC development.
 * SPDX-License-Identifier: MIT
 *
 * VK_EXT_device_generated_commands for turnip (M2.1).
 *
 * Layout/execution-set object wrappers around the common mesa
 * vk_indirect_command_layout (src/vulkan/runtime/vk_device_generated_commands.h).
 * The vk member must stay first: vk_indirect_command_layout_create()
 * relies on it (offsetof == 0).
 */

#ifndef TU_DGC_H
#define TU_DGC_H

#include <stdint.h>

#include "vk_object.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "vk_device_generated_commands.h"
#ifdef __cplusplus
}
#endif

struct tu_bo;

/* Must match maxIndirectCommandsTokenCount advertised in M2.1. */
#define TU_DGC_MAX_TOKENS 128

/*
 * M2.3: GPU-side description of one layout token, as read by the internal
 * preprocess shader. Kept 4-byte-aligned and packed: the shader walks
 * tokens[] as dwords at a fixed 16-byte stride.
 */
struct tu_dgc_gpu_token {
   uint32_t type;    /* VkIndirectCommandsLayoutTokenTypeEXT */
   uint32_t src_off; /* bytes: token data offset within a sequence */
   uint32_t dst_off; /* bytes: PC/SI offset within the bound pipeline's PC range */
   uint32_t size;    /* bytes: PC/SI data size */
};

/*
 * M2.3: GPU-side layout descriptor. Built once at layout creation and
 * uploaded to a BO owned by the layout object; the preprocess shader reads
 * it via BDA. Mirrors the CPU-side vk_indirect_command_layout (which has no
 * per-token type list, so the descriptor is built from pTokens directly).
 */
struct tu_dgc_gpu_layout {
   uint32_t stride;
   uint32_t token_count;
   uint32_t dwords_per_seq;
   uint32_t pad;
   struct tu_dgc_gpu_token tokens[TU_DGC_MAX_TOKENS];
};

struct tu_indirect_command_layout {
   struct vk_indirect_command_layout vk;

   /* M2.3: BO holding struct tu_dgc_gpu_layout (see above). */
   struct tu_bo *bo;
   /* PM4 dwords generated per sequence (M2.3 translator). */
   uint32_t dwords_per_seq;
   /* Tokens the M2.3 translator cannot emit (DISPATCH/IB/VB/IES/RT/
    * *_COUNT). Nonzero => vkCmdExecuteGeneratedCommandsEXT is refused
    * loudly; the rest lands with M2.4+. */
   uint32_t unsupported_tokens;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(tu_indirect_command_layout, vk.base,
                                VkIndirectCommandsLayoutEXT,
                                VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_EXT)

struct tu_indirect_execution_set {
   struct vk_object_base base;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(tu_indirect_execution_set, base,
                                VkIndirectExecutionSetEXT,
                                VK_OBJECT_TYPE_INDIRECT_EXECUTION_SET_EXT)

#endif /* TU_DGC_H */
