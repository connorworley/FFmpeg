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

#include "libavutil/frame.h"

#include "avcodec.h"
#include "dxv.h"

int ff_dxv_prepare_ctx(AVCodecContext *avctx, DXVContext *ctx)
{
    int ret;
    int bytes_per_pixel;

    switch (ctx->tex_fmt) {
    case DXV_FMT_DXT1:
        ctx->texdspctx.tex_funct = ctx->texdsp.dxt1_block;
        ctx->texdspctx.tex_ratio = 8;
        ctx->texdspctx.raw_ratio = 16;
        bytes_per_pixel          = 4;
        break;
    case DXV_FMT_DXT5:
        ctx->texdspctx.tex_funct = ctx->texdsp.dxt5_block;
        ctx->texdspctx.tex_ratio = 16;
        ctx->texdspctx.raw_ratio = 16;
        bytes_per_pixel          = 4;
        break;
    case DXV_FMT_YCG6:
        ctx->texdspctx.tex_funct  = ctx->texdsp.rgtc1u_gray_block;
        ctx->texdspctx.tex_ratio  = 8;
        ctx->texdspctx.raw_ratio  = 4;
        ctx->ctexdspctx.tex_funct = ctx->texdsp.rgtc1u_gray_block;
        ctx->ctexdspctx.tex_ratio = 16;
        ctx->ctexdspctx.raw_ratio = 4;
        bytes_per_pixel           = 1;
        break;
    case DXV_FMT_YG10:
        ctx->texdspctx.tex_funct  = ctx->texdsp.rgtc1u_gray_block;
        ctx->texdspctx.tex_ratio  = 16;
        ctx->texdspctx.raw_ratio  = 4;
        ctx->ctexdspctx.tex_funct = ctx->texdsp.rgtc1u_gray_block;
        ctx->ctexdspctx.tex_ratio = 16;
        ctx->ctexdspctx.raw_ratio = 4;
        bytes_per_pixel           = 1;
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Unsupported texture format (0x%08X)\n", ctx->tex_fmt); 
        return AVERROR_INVALIDDATA;
    }

    ctx->texdspctx.slice_count  = av_clip(avctx->thread_count, 1,
                                          avctx->coded_height / TEXTURE_BLOCK_H);
    ctx->ctexdspctx.slice_count = av_clip(avctx->thread_count, 1,
                                          avctx->coded_height / 2 / TEXTURE_BLOCK_H);

    ctx->tex_size = avctx->coded_width  / ctx->texdspctx.raw_ratio *
                    avctx->coded_height / TEXTURE_BLOCK_H *
                    ctx->texdspctx.tex_ratio *
                    bytes_per_pixel;
    ret = av_reallocp(&ctx->tex_data, ctx->tex_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (ret < 0)
        return ret;

    if (bytes_per_pixel == 1) {
        ctx->ctex_size = avctx->coded_width  / 2 / ctx->ctexdspctx.raw_ratio *
                         avctx->coded_height / 2 / TEXTURE_BLOCK_H *
                         ctx->ctexdspctx.tex_ratio;

        ret = av_reallocp(&ctx->ctex_data, ctx->ctex_size + AV_INPUT_BUFFER_PADDING_SIZE);
        if (ret < 0)
            return ret;
    }

    return 0;
}

void ff_dxv_free_ctx(DXVContext *ctx) {
    av_freep(&ctx->tex_data);
    av_freep(&ctx->ctex_data);
}

#define INTERMEDIATE_TEX_FUNC(func_name, tex_dir, frame_dir, dsp_func)        \
int func_name(AVCodecContext *avctx, DXVContext *ctx, AVFrame *frame)         \
{                                                                             \
    AVCodecContext avctx_cocg;                                                \
                                                                              \
    switch (ctx->tex_fmt) {                                                   \
    case DXV_FMT_YG10:                                                        \
        /* Alpha */                                                           \
        ctx->texdspctx.tex_data.tex_dir     = ctx->tex_data +                 \
                                              ctx->texdspctx.tex_ratio / 2;   \
        ctx->texdspctx.frame_data.frame_dir = frame->data[3];                 \
        ctx->texdspctx.stride               = frame->linesize[3];             \
        avctx->execute2(avctx,                                                \
                        dsp_func,                                             \
                        (void*)&ctx->texdspctx,                               \
                        NULL,                                                 \
                        ctx->texdspctx.slice_count);                          \
        /* fallthrough */                                                     \
    case DXV_FMT_YCG6:                                                        \
        avctx_cocg.coded_height = avctx->coded_height / 2;                    \
        avctx_cocg.coded_width  = avctx->coded_width  / 2;                    \
        /* Co */                                                              \
        ctx->ctexdspctx.tex_data.tex_dir     = ctx->ctex_data;                \
        ctx->ctexdspctx.frame_data.frame_dir = frame->data[2];                \
        ctx->ctexdspctx.stride               = frame->linesize[2];            \
        avctx->execute2(&avctx_cocg,                                          \
                        dsp_func,                                             \
                        (void*)&ctx->ctexdspctx,                              \
                        NULL,                                                 \
                        ctx->ctexdspctx.slice_count);                         \
        /* Cg */                                                              \
        ctx->ctexdspctx.tex_data.tex_dir     = ctx->ctex_data +               \
                                               ctx->ctexdspctx.tex_ratio / 2; \
        ctx->ctexdspctx.frame_data.frame_dir = frame->data[1];                \
        ctx->ctexdspctx.stride               = frame->linesize[1];            \
        avctx->execute2(&avctx_cocg,                                          \
                        dsp_func,                                             \
                        (void*)&ctx->ctexdspctx,                              \
                        NULL,                                                 \
                        ctx->ctexdspctx.slice_count);                         \
        /* fallthrough */                                                     \
    case DXV_FMT_DXT1:                                                        \
    case DXV_FMT_DXT5:                                                        \
        /* RGBA or Y */                                                       \
        ctx->texdspctx.tex_data.tex_dir     = ctx->tex_data;                  \
        ctx->texdspctx.frame_data.frame_dir = frame->data[0];                 \
        ctx->texdspctx.stride               = frame->linesize[0];             \
        avctx->execute2(avctx,                                                \
                        dsp_func,                                             \
                        (void*)&ctx->texdspctx,                               \
                        NULL,                                                 \
                        ctx->texdspctx.slice_count);                          \
        break;                                                                \
    default:                                                                  \
        av_log(avctx,                                                         \
              AV_LOG_ERROR,                                                   \
              "Unsupported texture format (0x%08X)\n",                        \
              ctx->tex_fmt);                                                  \
        return AVERROR_INVALIDDATA;                                           \
    }                                                                         \
    return 0;                                                                 \
}

INTERMEDIATE_TEX_FUNC(ff_dxv_compress_inner_tex,
                      in, out,
                      ff_texturedsp_compress_thread);
INTERMEDIATE_TEX_FUNC(ff_dxv_decompress_inner_tex,
                      out, in,
                      ff_texturedsp_decompress_thread);