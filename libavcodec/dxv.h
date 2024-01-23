/*
 * Resolume DXV common code
 * Copyright (C) 2024 Connor Worley <connorbworley@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_DXV_H
#define AVCODEC_DXV_H

#include "libavutil/macros.h"

#include "bytestream.h"
#include "texturedsp.h"

typedef enum DXVTextureFormat {
    DXV_FMT_DXT1 = MKBETAG('D', 'X', 'T', '1'),
    DXV_FMT_DXT5 = MKBETAG('D', 'X', 'T', '5'),
    DXV_FMT_YCG6 = MKBETAG('Y', 'C', 'G', '6'),
    DXV_FMT_YG10 = MKBETAG('Y', 'G', '1', '0'),
} DXVTextureFormat;

typedef struct DXVContext {
    DXVTextureFormat tex_fmt;

    uint8_t *tex_data;   // Compressed texture
    int64_t tex_size;    // Compressed texture size

    uint8_t *ctex_data;  // Compressed chroma texture
    int64_t ctex_size;   // Compressed chroma texture size    

    TextureDSPContext texdsp;
    TextureDSPThreadContext texdspctx;
    TextureDSPThreadContext ctexdspctx; // Chroma

    int (*process_outer_tex)(AVCodecContext *avctx);
} DXVContext;

void ff_dxv_free_ctx(DXVContext *ctx);

int ff_dxv_prepare_ctx(AVCodecContext *avctx, DXVContext *ctx);

int ff_dxv_compress_inner_tex  (AVCodecContext *avctx, DXVContext *ctx, AVFrame *frame);
int ff_dxv_decompress_inner_tex(AVCodecContext *avctx, DXVContext *ctx, AVFrame *frame);

#endif