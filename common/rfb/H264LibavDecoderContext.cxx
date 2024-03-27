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


extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/version.h>
}

#include <rfb/Exception.h>
#include <rfb/LogWriter.h>
#include <rfb/PixelBuffer.h>
#include <rfb/H264LibavDecoderContext.h>

using namespace rfb;

static LogWriter vlog("H264LibavDecoderContext");

// static int frame_cnt = 0;
// static FILE* h_file = NULL;
// static FILE* yuv_file = NULL;
// static FILE* rgb_file = NULL;

bool H264LibavDecoderContext::initCodec() {
  os::AutoMutex lock(&mutex);

  swsBuffer = NULL;
  h264WorkBuffer = NULL;
  h264WorkBufferLength = 0;

  const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec)
  {
    vlog.error("Codec not found");
    return false;
  }

  parser = av_parser_init(codec->id);
  if (!parser)
  {
    vlog.error("Could not create H264 parser");
    return false;
  }

  avctx = avcodec_alloc_context3(codec);
  if (!avctx)
  {
    av_parser_close(parser);
    vlog.error("Could not allocate video codec context");
    return false;
  }

  frame = av_frame_alloc();
  if (!frame)
  {
    av_parser_close(parser);
    avcodec_free_context(&avctx);
    vlog.error("Could not allocate video frame");
    return false;
  }

  if (avcodec_open2(avctx, codec, NULL) < 0)
  {
    av_parser_close(parser);
    avcodec_free_context(&avctx);
    av_frame_free(&frame);
    vlog.error("Could not open codec");
    return false;
  }

  int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB32, rect.width(), rect.height(), 1);
  vlog.debug("initCodec width(%d), height(%d), numBytes(%d)", rect.width(), rect.height(), numBytes);
  swsBuffer = new uint8_t[numBytes];

  initialized = true;
  return true;
}

void H264LibavDecoderContext::freeCodec() {
  os::AutoMutex lock(&mutex);
  if (!initialized)
    return;
  av_parser_close(parser);
  avcodec_free_context(&avctx);
  av_frame_free(&frame);
  delete[] swsBuffer;
  vlog.debug("freeCodec h264WorkBuffer(%p)", h264WorkBuffer);
  free(h264WorkBuffer);
  // initialized = false;
  // if(h_file){
  //     fflush(h_file);
  //     fclose(h_file);
  //     h_file = NULL;
  // }
  // if(yuv_file){
  //     fflush(yuv_file);
  //     fclose(yuv_file);
  //     yuv_file = NULL;
  // }
  // if(rgb_file){
  //     fflush(rgb_file);
  //     fclose(rgb_file);
  //     rgb_file = NULL;
  // }
}

// We need to reallocate buffer because AVPacket uses non-const pointer.
// We don't want to const_cast our buffer somewhere. So we would rather to maintain context's own buffer
// Also avcodec requires a right padded buffer
uint8_t* H264LibavDecoderContext::makeH264WorkBuffer(const uint8_t* buffer, uint32_t len)
{
  uint32_t reserve_len = len + AV_INPUT_BUFFER_PADDING_SIZE;

  if (!h264WorkBuffer || reserve_len > h264WorkBufferLength)
  {
    h264WorkBuffer = (uint8_t*)realloc(h264WorkBuffer, reserve_len);
    if (h264WorkBuffer == NULL) {
      throw Exception("H264LibavDecoderContext: Unable to allocate memory");
    }
    h264WorkBufferLength = reserve_len;
  }
  vlog.debug("makeH264WorkBuffer h264WorkBuffer(%p)", h264WorkBuffer);
  memcpy(h264WorkBuffer, buffer, len);
  memset(h264WorkBuffer + len, 0, AV_INPUT_BUFFER_PADDING_SIZE);
  return h264WorkBuffer;
}

void H264LibavDecoderContext::h264_decode(AVCodecContext* dec_ctx, AVFrame* frame, AVPacket* pkt, ModifiablePixelBuffer* pb){
  int ret;
  if(pkt) {
    vlog.debug("handle decode pkt size(%d)", pkt->size);
  }
  ret = avcodec_send_packet(dec_ctx, pkt);
  if (ret < 0) {
      fprintf(stderr, "Error sending a packet for decoding\n");
      return;
  }

  while (ret >= 0) {
      ret = avcodec_receive_frame(dec_ctx, frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
        char szMsg[256] ={0};
        sprintf(szMsg, "err:%s\n", av_err2str(ret));
        vlog.debug("avcodec_receive_frame ret(%d), szMsg(%s)", ret, szMsg);
        return ;
      } else if (ret < 0) {
          vlog.debug("Error during decoding");
          return;
      }

      vlog.debug("decode saving frame %d, ret(%d), rect.width(%d), rect.height(%d)", (int)dec_ctx->frame_num, ret, rect.width(), rect.height());

      // int h=frame->height,w=frame->width;
      // for (int i = 0; i < h; i++) {
      //     fwrite(frame->data[0] + i*frame->linesize[0], 1, w, yuv_file);
      // }
      // for (int i = 0; i < h/2; i++) {
      //     fwrite(frame->data[1] + i * frame->linesize[1], 1, w / 2, yuv_file);
      // }
      // for (int i = 0; i < h / 2; i++) {
      //     fwrite(frame->data[2] + i * frame->linesize[2], 1, w / 2, yuv_file);
      // }


      //transformate to rgb 
      SwsContext* swsCtx = NULL;
      swsCtx = sws_getCachedContext(swsCtx,
            frame->width, frame->height, dec_ctx->pix_fmt,
            rect.width(), rect.height(), AV_PIX_FMT_RGB32,
            SWS_BICUBIC, NULL, NULL, NULL);

      int stride;
      pb->getBuffer(rect, &stride);
      int dst_linesize = stride * pb->getPF().bpp / 8;  // stride is in pixels, linesize is in bytes (stride x4). We need bytes
      int line_bytes = rect.width() * pb->getPF().bpp / 8;
      int numBytes = rect.width() * rect.height() * pb->getPF().bpp / 8;

      vlog.debug("decode: frame->linesize(%d), width(%d), height(%d),frames_type(%d),avctx->pix_fmt(%d), dst_linesize(%d), stride(%d),numBytes(%d),line_bytes(%d)", 
                  frame->linesize[0], frame->width, frame->height,frame->pict_type, dec_ctx->pix_fmt, dst_linesize, stride, numBytes, line_bytes);


      AVFrame* rgbFrame = av_frame_alloc();
      rgbFrame->format = AV_PIX_FMT_RGB32;
      rgbFrame->width = rect.width();
      rgbFrame->height = rect.height();
      ret = av_frame_get_buffer(rgbFrame, 0);

      int ret = sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height,
            rgbFrame->data, rgbFrame->linesize);
      if (ret < 0) {
          char szMsg[256] = { 0 };
          sprintf(szMsg, "err:%s\n", av_err2str(ret));
          vlog.debug("Error during encoder, ret %d, error %s", ret, szMsg);
          return;
      }
      
      if (rgbFrame->linesize[0] == line_bytes) {
          memcpy(swsBuffer, rgbFrame->data[0], rect.height() * line_bytes);
      }
      else {
          uint8_t* tmpFrame = rgbFrame->data[0];
          uint8_t* tmpBuf = swsBuffer;
          int h = rect.height();
          while (h--) {
              memcpy(tmpBuf, tmpFrame, line_bytes);
              tmpFrame += rgbFrame->linesize[0];
              tmpBuf += line_bytes;
          }
      }

      // fwrite(swsBuffer, numBytes, 1, rgb_file);
      pb->imageRect(rect, swsBuffer, rect.width());
      av_frame_free(&rgbFrame);
  }
}

void H264LibavDecoderContext::decode(const uint8_t* h264_in_buffer,
                                     uint32_t len,
                                     ModifiablePixelBuffer* pb) {
  os::AutoMutex lock(&mutex);
  if (!initialized)
    return;
  uint8_t* h264_work_buffer = makeH264WorkBuffer(h264_in_buffer, len);

  AVPacket *packet = av_packet_alloc();
  
  vlog.debug("H264LibavDecoderContext::decode start, pb->width(%d), pb->height(%d), len(%d), h264_in_buffer(%p)", pb->width(), pb->height(), len, h264_in_buffer);
  int ret;

  // frame_cnt++;
  // char hPath[256] ={0};
  // sprintf(hPath, "out_h264_%d.h264", frame_cnt);
  // if(h_file == NULL){
  //     h_file = fopen(hPath, "wb");
  //     vlog.debug("fopen h264 new file h264Path(%s)", hPath);
  // }

  // char fPath[256] ={0};
  // sprintf(fPath, "out_yuv_%d.yuv", frame_cnt);
  // if(yuv_file == NULL){
  //     yuv_file = fopen(fPath, "wb");
  //     vlog.debug("fopen yuv new file yuvPath(%s)", fPath);
  // }

  // char rgbPath[256] ={0};
  // sprintf(rgbPath, "out_rgb_%d.rgb", frame_cnt);
  // if(rgb_file == NULL){
  //     rgb_file = fopen(rgbPath, "wb");
  //     vlog.debug("fopen rgb new file rgbPath(%s)", rgbPath);
  // }

  // fwrite(h264_work_buffer, len, 1, h_file);

  int eof;
  uint8_t* h264_data;
  do {
    eof = !len;
    h264_data = h264_work_buffer;
    vlog.debug("[decode] before while eof(%d), len(%d), h264_work_buffer(%p)", eof, len, h264_work_buffer);
    while(len > 0 || eof){
      ret = av_parser_parse2(parser, avctx, &packet->data, &packet->size, h264_data, len, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
      vlog.debug("decode: av_parser_parse2 ret(%d)", ret);
      if (ret < 0){
        vlog.debug("Error while parsing");
        return;
      }
      h264_data += ret;
      len -= ret;

      if(packet->size){
        vlog.debug("h264_decode before packet->size(%d)",packet->size);
        // fwrite(packet->data, packet->size, 1, h_file);
        h264_decode(avctx, frame, packet, pb);
      } else if (eof) {
        vlog.debug("eof is not 0");
        break;
      }
    }
  } while(!eof);

  /* flush the decoder */
  h264_decode(avctx, frame, NULL, pb);

  packet->size = 0;
  packet->data = NULL;
  av_packet_free(&packet);

  //AV_PICTURE_TYPE_I 
  vlog.debug("decode: commit rect success");
}
