/*
 * Copyright © Google, Inc.
 * SPDX-License-Identifier: MIT
 */

#ifndef __FREEDRENO_UBWC_H__
#define __FREEDRENO_UBWC_H__

#include "util/format/u_format.h"

#include "freedreno_dev_info.h"

enum fd6_ubwc_compat_type {
   FD6_UBWC_UNKNOWN_COMPAT,
   FD6_UBWC_R8G8_UNORM,
   FD6_UBWC_R8G8_INT,
   FD6_UBWC_R8G8B8A8_UNORM,
   FD6_UBWC_R8G8B8A8_INT,
   FD6_UBWC_B8G8R8A8_UNORM,
   FD6_UBWC_B8G8R8A8_INT,
   FD6_UBWC_R16G16_UNORM,
   FD6_UBWC_R16G16_INT,
   FD6_UBWC_R16G16B16A16_UNORM,
   FD6_UBWC_R16G16B16A16_INT,
   FD6_UBWC_R32_INT,
   FD6_UBWC_R32G32_INT,
   FD6_UBWC_R32G32B32A32_INT,
   FD6_UBWC_R32_FLOAT,
   FD6_UBWC_R16_INT,
   FD6_UBWC_A2B10G10R10_INT,
};

static inline enum fd6_ubwc_compat_type
fd6_ubwc_compat_mode(const struct fd_dev_info *info, enum pipe_format format)
{
   switch (format) {
   case PIPE_FORMAT_R8G8_UNORM:
   case PIPE_FORMAT_R8G8_SRGB:
      return info->props.ubwc_unorm_snorm_int_compatible ?
         FD6_UBWC_R8G8_INT : FD6_UBWC_R8G8_UNORM;

   case PIPE_FORMAT_R8G8_SNORM:
      return info->props.ubwc_unorm_snorm_int_compatible ?
         FD6_UBWC_R8G8_INT : FD6_UBWC_UNKNOWN_COMPAT;

   case PIPE_FORMAT_R8G8_UINT:
   case PIPE_FORMAT_R8G8_SINT:
      return FD6_UBWC_R8G8_INT;

   case PIPE_FORMAT_R8G8B8A8_UNORM:
   case PIPE_FORMAT_R8G8B8A8_SRGB:
      return info->props.ubwc_unorm_snorm_int_compatible ?
         FD6_UBWC_R8G8B8A8_INT : FD6_UBWC_R8G8B8A8_UNORM;

   case PIPE_FORMAT_R8G8B8A8_SNORM:
      return info->props.ubwc_unorm_snorm_int_compatible ?
         FD6_UBWC_R8G8B8A8_INT : FD6_UBWC_UNKNOWN_COMPAT;

   case PIPE_FORMAT_R8G8B8A8_UINT:
   case PIPE_FORMAT_R8G8B8A8_SINT:
      return FD6_UBWC_R8G8B8A8_INT;

   case PIPE_FORMAT_R16G16_UNORM:
      return info->props.ubwc_unorm_snorm_int_compatible ?
         FD6_UBWC_R16G16_INT : FD6_UBWC_R16G16_UNORM;

   case PIPE_FORMAT_R16G16_SNORM:
      return info->props.ubwc_unorm_snorm_int_compatible ?
         FD6_UBWC_R16G16_INT : FD6_UBWC_UNKNOWN_COMPAT;

   case PIPE_FORMAT_R16G16_UINT:
   case PIPE_FORMAT_R16G16_SINT:
      return FD6_UBWC_R16G16_INT;

   case PIPE_FORMAT_R16G16B16A16_UNORM:
      return info->props.ubwc_unorm_snorm_int_compatible ?
         FD6_UBWC_R16G16B16A16_INT : FD6_UBWC_R16G16B16A16_UNORM;

   case PIPE_FORMAT_R16G16B16A16_SNORM:
      return info->props.ubwc_unorm_snorm_int_compatible ?
         FD6_UBWC_R16G16B16A16_INT : FD6_UBWC_UNKNOWN_COMPAT;

   case PIPE_FORMAT_R16G16B16A16_UINT:
   case PIPE_FORMAT_R16G16B16A16_SINT:
      return FD6_UBWC_R16G16B16A16_INT;

   case PIPE_FORMAT_R16_UNORM:
   case PIPE_FORMAT_R16_SNORM:
   case PIPE_FORMAT_R16_UINT:
   case PIPE_FORMAT_R16_SINT:
   case PIPE_FORMAT_R16_FLOAT:
      /* The blob table has no single-channel
       * 16-bit row at all, so every vkd3d list containing R16 (R16_TYPELESS and
       * three more classes) silently loses UBWC.  Measured on a 740: a lone
       * R16_UNORM image with no format list does get UBWC metadata
       * (memreq 0x82000 vs 0x80000 linear), so the ceiling exists and the gate
       * is what removes it.  All four variants are the same 16 bits read
       * differently, and the tiled layout is a function of the block size
       * alone (fd6_layout.c: layout->cpp = util_format_get_blocksize()).
       */
      return FD6_UBWC_R16_INT;

   /* NB: vk_to_pipe_format() maps VK_FORMAT_A2B10G10R10_UNORM_PACK32 onto
    * PIPE_FORMAT_R10G10B10A2_UNORM, not PIPE_FORMAT_A2B10G10R10_UNORM
    * (src/vulkan/util/vk_format.c:114), so the R10G10B10A2 spelling is the
    * one that actually reaches this switch. */
   case PIPE_FORMAT_R10G10B10A2_UNORM:
   case PIPE_FORMAT_R10G10B10A2_UINT:
      /* Same argument for the packed 10-bit form, and the same measurement:
       * a lone R10G10B10A2_UNORM image gets 0x102000 against 0x100000 linear.
       * Only UNORM and UINT are added, and they are exactly the two with a
       * tiled entry (VTC) in fd6_format_table.c; SNORM and SINT have none.
       */
      return FD6_UBWC_A2B10G10R10_INT;

   case PIPE_FORMAT_R32_UINT:
   case PIPE_FORMAT_R32_SINT:
      return FD6_UBWC_R32_INT;

   case PIPE_FORMAT_R32G32_UINT:
   case PIPE_FORMAT_R32G32_SINT:
      return FD6_UBWC_R32G32_INT;

   case PIPE_FORMAT_R32G32B32A32_UINT:
   case PIPE_FORMAT_R32G32B32A32_SINT:
      return FD6_UBWC_R32G32B32A32_INT;

   case PIPE_FORMAT_R16G16_FLOAT:
      return FD6_UBWC_R16G16_INT;

   case PIPE_FORMAT_R16G16B16A16_FLOAT:
      return FD6_UBWC_R16G16B16A16_INT;

   case PIPE_FORMAT_R32_FLOAT:
      /* Stock leaves this UNKNOWN, with the comment "a630 blob allows these,
       * but not a660" - and gen2 is the a660 family.  The assumption is that
       * the blob's table is about UNORM/SNORM/INT families and simply has no
       * float rows to compare against; the layout argument that justifies the
       * B8G8R8A8 family applies unchanged here, because a float view of an
       * int-written texel is the same bits read differently and the tiled
       * layout is identical.  Measured on a 740: the five vkd3d TYPELESS
       * classes keep UBWC and still read back byte-exact.
       */
      return FD6_UBWC_R32_INT;

   case PIPE_FORMAT_R32G32_FLOAT:
      return FD6_UBWC_R32G32_INT;

   case PIPE_FORMAT_R32G32B32A32_FLOAT:
      return FD6_UBWC_R32G32B32A32_INT;

   case PIPE_FORMAT_Z32_FLOAT:
      /* Z32_FLOAT stays UNKNOWN on purpose: it is not part of any colour
       * typeless family, and depth has its own clear semantics. */
      return FD6_UBWC_UNKNOWN_COMPAT;

   case PIPE_FORMAT_B8G8R8A8_UNORM:
   case PIPE_FORMAT_B8G8R8A8_SRGB:
      /* The blob doesn't list any of the B8G8R8A8 family as compatible, but
       * the family agrees with itself: the formats only differ in how the
       * same 32 bits are interpreted, so they share both the tiled layout
       * (the swap is the same for all of them) and the UBWC encoding.
       * freedreno is happy to cast between them, and zink would really like
       * to.
       */
      return info->props.ubwc_unorm_snorm_int_compatible ?
         FD6_UBWC_B8G8R8A8_INT : FD6_UBWC_B8G8R8A8_UNORM;

   case PIPE_FORMAT_B8G8R8A8_SNORM:
      return info->props.ubwc_unorm_snorm_int_compatible ?
         FD6_UBWC_B8G8R8A8_INT : FD6_UBWC_UNKNOWN_COMPAT;

   case PIPE_FORMAT_B8G8R8A8_UINT:
   case PIPE_FORMAT_B8G8R8A8_SINT:
      return FD6_UBWC_B8G8R8A8_INT;

   default:
      return FD6_UBWC_UNKNOWN_COMPAT;
   }
}


#endif
