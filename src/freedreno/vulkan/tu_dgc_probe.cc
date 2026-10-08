/*
 * Copyright 2026 Starfield DGC development.
 * SPDX-License-Identifier: MIT
 *
 * DGC (VK_EXT_device_generated_commands) development probe, M1 spike.
 *
 * Emits CP_MEM_WRITE / CP_WAIT_MEM_WRITES packets into the command
 * buffer's draw_cs sub-stream, which turnip executes via
 * CP_INDIRECT_BUFFER.  This validates the core assumption of the
 * device-generated-commands bridge: that a PM4 stream executed inside
 * an indirect buffer can write memory and that a subsequent draw in
 * the same stream observes the written data.
 *
 * Exposed as a non-standard device entry point (not part of any
 * extension); the spike test program resolves it via dlsym().
 */

#include "util/macros.h"

#include "tu_buffer.h"
#include "tu_cmd_buffer.h"
#include "tu_cs.h"

extern "C" {

VKAPI_ATTR void VKAPI_CALL
vkCmdTuDgcProbeWriteEXT(VkCommandBuffer commandBuffer,
                        VkBuffer buffer,
                        VkDeviceSize offset,
                        uint32_t value,
                        uint32_t nwrites);

/* M2.0: диспатч внутреннего probe compute-шейдера, пишущего PM4-payload
 * в буфер, затем CP_INDIRECT_BUFFER на этот payload (см. tu_cmd_buffer.cc). */
VKAPI_ATTR void VKAPI_CALL
vkCmdTuDgcProbeDispatchEXT(VkCommandBuffer commandBuffer,
                           VkBuffer buffer,
                           VkDeviceSize offset,
                           VkBuffer markerBuffer,
                           VkDeviceSize markerOffset,
                           uint32_t value);

}

/* PUBLIC: default visibility so the version-script global entry in
 * src/vulkan/vulkan.sym actually exports it (build uses
 * -fvisibility=hidden); retain: keep the symbol despite
 * -Wl,--gc-sections (nothing references it internally; the spike test
 * resolves it via dlsym) */
extern "C" PUBLIC VKAPI_ATTR void
   __attribute__((used, retain)) VKAPI_CALL
vkCmdTuDgcProbeWriteEXT(VkCommandBuffer commandBuffer,
                        VkBuffer buffer,
                        VkDeviceSize offset,
                        uint32_t value,
                        uint32_t nwrites)
{
   VK_FROM_HANDLE(tu_cmd_buffer, cmd, commandBuffer);
   VK_FROM_HANDLE(tu_buffer, buf, buffer);
   struct tu_cs *cs = &cmd->draw_cs;
   uint64_t addr = buf->bo->iova + offset;

   if (nwrites == 0 || nwrites > 64 || (offset & 3) != 0)
      return;

   for (uint32_t i = 0; i < nwrites; i++) {
      /* one dword of payload: CP_MEM_WRITE = 2 + 1 */
      tu_cs_emit_pkt7(cs, CP_MEM_WRITE, 3);
      tu_cs_emit_qw(cs, addr + i * 4);
      tu_cs_emit(cs, value + i * 0x01010101);
   }

   tu_cs_emit_pkt7(cs, CP_WAIT_MEM_WRITES, 0);
}

/* M2.0: см. док-комментарий в объявлении выше и в tu_cmd_buffer.cc. */
extern "C" PUBLIC VKAPI_ATTR void
   __attribute__((used, retain)) VKAPI_CALL
vkCmdTuDgcProbeDispatchEXT(VkCommandBuffer commandBuffer,
                           VkBuffer buffer,
                           VkDeviceSize offset,
                           VkBuffer markerBuffer,
                           VkDeviceSize markerOffset,
                           uint32_t value)
{
   VK_FROM_HANDLE(tu_cmd_buffer, cmd, commandBuffer);
   VK_FROM_HANDLE(tu_buffer, buf, buffer);
   VK_FROM_HANDLE(tu_buffer, marker_buf, markerBuffer);

   if (!buf || !marker_buf)
      return;

   tu_dgc_probe_dispatch(cmd, buf, offset, marker_buf, markerOffset, value);
}
