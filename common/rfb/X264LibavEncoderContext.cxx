/* Copyright (C) 2021 Vladimir Sukhonosov <xornet@xornet.org>
 * Copyright (C) 2021 Martins Mozeiko <martins.mozeiko@gmail.com>
 * All Rights Reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavcodec/version.h>
}

#include <rfb/Exception.h>
#include <rfb/LogWriter.h>
#include <rfb/PixelBuffer.h>
#include <rfb/X264LibavEncoderContext.h>

using namespace rfb;

static LogWriter vlog("X264LibavEncoderContext");

bool X264LibavEncoderContext::initCodec(int width, int height)
{
    os::AutoMutex lock(&mutex);

    pPic_in = (x264_picture_t *)malloc(sizeof(x264_picture_t));
    pPic_out = (x264_picture_t *)malloc(sizeof(x264_picture_t));
    x264_param_t *pParam = (x264_param_t *)malloc(sizeof(x264_param_t));

    x264_param_default(pParam);
    pParam->i_width = width;
    pParam->i_height = height;
    pParam->i_csp = X264_CSP_BGRA;
    pParam->i_fps_den = 1;
    pParam->i_fps_num = 25;
    pParam->i_threads = 1;

    x264_param_apply_profile(pParam, x264_profile_names[5]);

    x264_picture_init(pPic_out);
    x264_picture_alloc(pPic_in, X264_CSP_I420, pParam->i_width, pParam->i_height);

    encoder = x264_encoder_open(pParam);

    // get the scaling context
    sws = sws_getContext(width, height, AV_PIX_FMT_BGR32, width, height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (!sws)
    {
        vlog.error("Cannot create SWS context");
        return false;
    }

    vlog.debug("initCodec success");
    initialized = true;
    return true;
}

void X264LibavEncoderContext::freeCodec()
{
    os::AutoMutex lock(&mutex);

    if (!initialized)
        return;

    if(encoder) {
        x264_picture_clean(pPic_in);
        free(pPic_in);
        free(pPic_out);

        x264_encoder_close(encoder);
        encoder = NULL;
    }

    if(sws) {
        sws_freeContext(sws);
        sws = NULL;
    }

    initialized = false;
}

void X264LibavEncoderContext::encode(rdr::OutStream *os, const PixelBuffer *pb)
{
    os::AutoMutex lock(&mutex);
    if (!initialized)
        return;

    const uint8_t *buffer;
    int stride;
    buffer = pb->getBuffer(pb->getRect(), &stride); // TODO

    vlog.debug("X264LibavEncoderContext::encode start buffer(%p)", buffer);
    AVFrame *rgbFrame = av_frame_alloc();
    // avpicture_fill((AVPicture *)rgbFrame, buffer, AV_PIX_FMT_BGR32, pb->width(), pb->height());
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer, AV_PIX_FMT_BGR32, width, height, 1);

                         
    int scaleheight = sws_scale(sws, rgbFrame->data, rgbFrame->linesize, 0, pb->height(), pPic_in->img.plane, pPic_in->img.i_stride);
    vlog.debug("encode:rgbFrame->data(%p), rgbFrame->linesize(%d), frame->data(%p), frame->linesize(%d)", rgbFrame->data, rgbFrame->linesize[0], pPic_in->img.plane, pPic_in->img.i_stride[0]);
    if(scaleheight != pb->height()){
        vlog.error("scale failed: scaleheight(%d), outheight(%d)", scaleheight, pb->height());
        return ;
    }
    
    int frame_size = x264_encoder_encode(encoder, &nals, &nalcount, pPic_in, pPic_out);
    if(frame_size < 0){
        vlog.error("Error while encoding");
        return ;
    }
    if(frame_size){
        os->writeU32(frame_size);                 // write length
        os->writeU32(1U);                           // write flags   TODO
        os->writeBytes(nals[0].p_payload, frame_size); // TODO
    }

    vlog.debug("X264LibavEncoderContext::encode end  package_size(%d), package_data(%p)", frame_size, nals[0].p_payload);
    av_frame_free(&rgbFrame);
}
