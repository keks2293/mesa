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

#include "vk_object.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "vk_device_generated_commands.h"
#ifdef __cplusplus
}
#endif

struct tu_indirect_command_layout {
   struct vk_indirect_command_layout vk;
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
