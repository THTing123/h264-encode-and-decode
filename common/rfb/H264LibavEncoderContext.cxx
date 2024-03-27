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
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
}

#include <rfb/Exception.h>
#include <rfb/LogWriter.h>
#include <rfb/PixelBuffer.h>
#include <rfb/H264LibavEncoderContext.h>

using namespace rfb;

static LogWriter vlog("H264LibavEncoderContext");

// static int frame_cnt = 0;
// FILE* yuv_file = NULL;
// FILE* rgb_file = NULL;
// FILE* h_file = NULL;

bool H264LibavEncoderContext::initCodec(int width, int height)
{
    os::AutoMutex lock(&mutex);

    const AVCodec *codec = avcodec_find_encoder_by_name("libx264");//avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        vlog.debug("Cannot find any endcoder");
        return false;
    }

    avctx = avcodec_alloc_context3(codec);
    if (!avctx)
    {
        vlog.debug("Cannot alloc context");
        return false;
    }

    frame = av_frame_alloc();
    if (!frame)
    {
        avcodec_free_context(&avctx);
        vlog.debug("Could not allocate video frame");
        return false;
    }

    // set context
    avctx->bit_rate = 400000;
    avctx->width = ceil((double)width/2)*2;
    avctx->height = ceil((double)height/2)*2;
    avctx->pix_fmt = AV_PIX_FMT_YUV420P;
    avctx->time_base.num = 1;
    avctx->time_base.den = 25;
    avctx->framerate.num = 25;
    avctx->framerate.den = 1;
    avctx->max_b_frames = 0;
    avctx->profile = FF_PROFILE_H264_MAIN;
    avctx->level = 30;

    int ret = avcodec_open2(avctx, codec, NULL);

    if (ret < 0)//avcodec_open2(avctx, codec, &param) < 0
    {
        char szMsg[256] ={0};
        sprintf(szMsg, "err:%s\n", av_err2str(ret));
        avcodec_free_context(&avctx);
        av_frame_free(&frame);
        vlog.error("Could not open codec, ret %d, error(%s)", ret, szMsg);
        return false;
    }

    vlog.debug("initCodec success,ret %d, src_width(%d), src_height(%d), avctx->width(%d), avctx->height(%d)", ret, width, height, avctx->width, avctx->height);
    this->width = width;
    this->height = height;
    initialized = true;
    return true;
}

void H264LibavEncoderContext::freeCodec()
{
    os::AutoMutex lock(&mutex);

    if (!initialized)
        return;
    avcodec_free_context(&avctx);
    av_frame_free(&frame);
    initialized = false;

    // if(yuv_file){
    //     fflush(yuv_file);
    //     fclose(yuv_file);
    //     yuv_file = NULL;
    // }
    // if(h_file){
    //     fflush(h_file);
    //     fclose(h_file);
    //     h_file = NULL;
    // }
    // if(rgb_file){
    //     fflush(rgb_file);
    //     fclose(rgb_file);
    //     rgb_file = NULL;
    // }
}

void H264LibavEncoderContext::getYuvFrame(const PixelBuffer *pb){
    const uint8_t *buffer;
    int stride, ret;
    buffer = pb->getBuffer(pb->getRect(), &stride); // TODO
    int stride_bytes = stride * pb->getPF().bpp/8;
    int h = pb->height();
    
    int line_bytes = pb->width() * pb->getPF().bpp/8;

    vlog.debug("getYuvFrame pb->width(%d), pb->height(%d), bpp(%d), stride(%d)", pb->width(), pb->height(), pb->getPF().bpp, stride);

    uint8_t *tmp = new uint8_t[4 * pb->height() * pb->width()];

    uint8_t *rgbBuffer = tmp;
    while(h--){
        memcpy(tmp, buffer, line_bytes);
        buffer += stride_bytes;
        tmp += line_bytes;
    }
    // fwrite(rgbBuffer, 4 * pb->height() * pb->width(), 1, rgb_file);

    SwsContext* swsCtx = NULL;
    swsCtx = sws_getCachedContext(swsCtx,
        pb->width(), pb->height(), AV_PIX_FMT_RGB32,
        avctx->width, avctx->height, avctx->pix_fmt,
        SWS_BICUBIC,
        NULL, NULL, NULL
    );
    frame->format = avctx->pix_fmt;
    frame->width = avctx->width;
    frame->height = avctx->height;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        vlog.debug("Could not allocate the video yuv frame data");
        return ;
    }

    AVFrame* rgbFrame = av_frame_alloc();
    rgbFrame->format = AV_PIX_FMT_RGB32;
    rgbFrame->width = pb->width();
    rgbFrame->height = pb->height();

    ret = av_frame_get_buffer(rgbFrame, 0);
    if (ret < 0) {
        vlog.debug("Could not allocate the video rgb frame data");
        return ;
    }
    
    if(rgbFrame->linesize[0] == line_bytes){
        memcpy(rgbFrame->data[0], rgbBuffer, pb->height() * line_bytes);
    } else {
        uint8_t* tmpRgbFrame = rgbFrame->data[0];
        uint8_t* tmpRgbBuffer = rgbBuffer;
        h = pb->height();
        while(h--){
            memcpy(tmpRgbFrame, tmpRgbBuffer, line_bytes);
            tmpRgbFrame += rgbFrame->linesize[0];
            tmpRgbBuffer += line_bytes;
        }
    }

    ret = sws_scale(swsCtx, rgbFrame->data, rgbFrame->linesize, 0, pb->height(),
            frame->data, frame->linesize);
    if(ret < 0){
        vlog.debug("sws_scale error, ret %d", ret);
    }
    vlog.debug("GetYuvFrame frame->data[0](%p), [1](%p), [2](%p), linsize[0](%d), [1](%d), [2](%d), format(%d)", frame->data[0],frame->data[1],frame->data[2],
                frame->linesize[0], frame->linesize[1],frame->linesize[2], rgbFrame->format);

    delete[] rgbBuffer;
    av_frame_free(&rgbFrame);
}

void H264LibavEncoderContext::h264_encode(AVFrame* frame, AVPacket* packet, rdr::OutStream *os){
    // char hPath[256] ={0};
    // sprintf(hPath, "out_h264_%d.h264", frame_cnt);
    // if(h_file == NULL){
    //     h_file = fopen(hPath, "wb");
    //     vlog.debug("fopen h264 new file yuvPath(%s)", hPath);
    // }

    if(frame){
        vlog.debug("Send frame ");
    }
    int ret = avcodec_send_frame(avctx, frame);//ret = 0
    if (ret < 0)
    {
        char szMsg[256] ={0};
        sprintf(szMsg, "err:%s\n", av_err2str(ret));
        vlog.debug("Error during encoder, ret %d, error %s", ret, szMsg);
        return;
    }
    
    while(ret >= 0){
        ret = avcodec_receive_packet(avctx, packet);
        if (ret != 0)
        {
            vlog.debug("Error during avcodec_receive_packet, ret != 0, ret %d, error %s", ret, av_err2str(ret));
        } else if (ret < 0){
            vlog.debug("Error during avcodec_receive_packet, ret %d, error %s", ret, av_err2str(ret));
            break;
        }
        // fwrite(packet->data, packet->size, 1, h_file);
        if(packet->size){
            os->writeU32(packet->size);                 // write length
            os->writeU32(1U);                           // write flags   TODO
            os->writeBytes(packet->data, packet->size); // TODO
            // fwrite(tmp, packet->size, 1, h_file);
        }
        
        vlog.debug("H264LibavEncoderContext::encode end  package_size(%d), package_data(%p), ret(%d)", packet->size, packet->data, ret);
        av_packet_unref(packet);
    }
}

void H264LibavEncoderContext::encode(rdr::OutStream *os, const PixelBuffer *pb)
{
    os::AutoMutex lock(&mutex);
    if (!initialized)
        return;

    AVPacket *packet = av_packet_alloc();

    // frame_cnt++;

    // char yuvPath[256] ={0};
    // sprintf(yuvPath, "out_yuv_%d.yuv", frame_cnt);
    // if(yuv_file == NULL){
    //     yuv_file = fopen(yuvPath, "wb");
    //     vlog.debug("fopen yuv new file yuvPath(%s)", yuvPath);
    // }

    // char rgbPath[256] ={0};
    // sprintf(rgbPath, "out_rgb_%d.rgb", frame_cnt);
    // if(rgb_file == NULL){
    //     rgb_file = fopen(rgbPath, "wb");
    //     vlog.debug("fopen h264 new file yuvPath(%s)", rgbPath);
    // }

    getYuvFrame(pb);


    // fwrite(frame->data[0], 1, frame->width * frame->height, yuv_file);
    // fwrite(frame->data[1], 1, frame->width * frame->height/4, yuv_file);
    // fwrite(frame->data[2], 1, frame->width * frame->height/4, yuv_file);
    // int h = frame->height, w = frame->width;
    // for (int i = 0; i < h; i++) {
    //     fwrite(frame->data[0] + i * frame->linesize[0], 1, w, yuv_file);
    // }
    // for (int i = 0; i < h / 2; i++) {
    //     fwrite(frame->data[1] + i * frame->linesize[1], 1, w / 2, yuv_file);
    // }
    // for (int i = 0; i < h / 2; i++) {
    //     fwrite(frame->data[2] + i * frame->linesize[2], 1, w / 2, yuv_file);
    // }


    h264_encode(frame, packet, os);

    h264_encode(NULL, packet, os);

    vlog.debug("encode end ");
    packet->size = 0;
    packet->data = NULL;
    av_packet_free(&packet);
}
