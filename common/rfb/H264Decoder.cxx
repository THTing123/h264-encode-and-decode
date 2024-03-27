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

#define MAX_H264_INSTANCES 64

#include <deque>

#include <rdr/MemInStream.h>
#include <rdr/InStream.h>
#include <rdr/OutStream.h>
#include <rfb/LogWriter.h>
#include <rfb/Exception.h>
#include <rfb/H264Decoder.h>
#include <rfb/H264DecoderContext.h>

using namespace rfb;

static LogWriter vlog("H264Decoder");
// static int readCnt = 0;
// static FILE* tmpFile = NULL;

enum rectFlags {
  resetContext       = 0x1,
  resetAllContexts   = 0x2,
};

H264Decoder::H264Decoder() : Decoder(DecoderOrdered)
{
}

H264Decoder::~H264Decoder()
{
  resetContexts();
}

void H264Decoder::resetContexts()
{
  os::AutoMutex lock(&mutex);
  for (std::deque<H264DecoderContext*>::iterator it = contexts.begin(); it != contexts.end(); it++)
    delete *it;
  contexts.clear();
}

H264DecoderContext* H264Decoder::findContext(const Rect& r)
{
  os::AutoMutex m(&mutex);
  for (std::deque<H264DecoderContext*>::iterator it = contexts.begin(); it != contexts.end(); it++)
    if ((*it)->isEqualRect(r))
      return *it;
  return NULL;
}

// void H264Decoder::SaveH264Packet(const uint8_t* ptr, uint32_t len){
//   readCnt++;
//   char tmpPath[256] ={0};
//   sprintf(tmpPath, "tmp_read_%d.h264", readCnt);
//   if(tmpFile == NULL){
//       tmpFile = fopen(tmpPath, "wb");
//       vlog.debug("fopen yuv new file yuvPath(%s)", tmpPath);
//   }
//   fwrite(ptr, len, 1, tmpFile);
//   fclose(tmpFile);
//   tmpFile = NULL;
// }

bool H264Decoder::readRect(const Rect& /*r*/,
                           rdr::InStream* is,
                           const ServerParams& /*server*/,
                           rdr::OutStream* os)
{
  uint32_t len;

  if (!is->hasData(8))
    return false;

  is->setRestorePoint();

  len = is->readU32();
  os->writeU32(len);
  uint32_t flags = is->readU32();

  vlog.debug("H264Decoder::readRect len(%d), flag(%d), ptr(%p), end(%p)", 
              len, flags, is->GetPtrPointer(), is->GetEndPointer());
  os->writeU32(flags);

  if (!is->hasDataOrRestore(len)){
    vlog.debug("[H264Decoder.readRect] hasDataOrRestore false");
    return false;
  }

  // const uint8_t* tmp = is->GetPtrPointer();
  // SaveH264Packet(is->GetPtrPointer(), len);//debug

  is->clearRestorePoint();

  os->copyBytes(is, len);

  return true;
}

void H264Decoder::decodeRect(const Rect& r, const uint8_t* buffer,
                             size_t buflen,
                             const ServerParams& /*server*/,
                             ModifiablePixelBuffer* pb)
{
  rdr::MemInStream is(buffer, buflen);
  uint32_t len = is.readU32();
  uint32_t flags = is.readU32();

  vlog.debug("H264Decoder::decodeRect len(%d), flag(%d), rect.tl(%d,%d), rect.br(%d,%d), ctx_size(%d)", len, flags, r.tl.x, r.tl.y, r.br.x, r.br.y, (int)contexts.size());
  H264DecoderContext* ctx = NULL;
  // if (flags & resetAllContexts)
  // {
  //   resetContexts();
  //   if (!len)
  //     return;
  //   flags &= ~(resetContext | resetAllContexts);
  // } else {
  //   ctx = findContext(r);
  //   vlog.debug("findContext success, ctx(%p)", ctx);
  // }

  // if (!ctx)
  // {
  //   os::AutoMutex lock(&mutex);
  //   vlog.debug("H264Decoder::decodeRect mutex after");
  //   if (contexts.size() >= MAX_H264_INSTANCES)
  //   {
  //     H264DecoderContext* excess_ctx = contexts.front();
  //     delete excess_ctx;
  //     contexts.pop_front();
  //   }
  //   ctx = H264DecoderContext::createContext(r);
  //   if (!ctx)
  //     throw Exception("H264Decoder: Context not be created");
  //   contexts.push_back(ctx);
  //   vlog.debug("H264Decoder::decodeRect add new ctx(%p)", ctx);
  // }

  ctx = H264DecoderContext::createContext(r);
  if (!ctx)
    throw Exception("H264Decoder: Context not be created");

  if (!ctx->isReady())
    throw Exception("H264Decoder: Context is not ready");

  if (!len)
    return;
  vlog.debug("H264Decoder::decodeRect before, MemInStream is start(%p),ptr(%p),end(%p)", is.GetStart(), is.GetPtrPointer(),is.GetEndPointer());
  ctx->decode(is.getptr(len), len, pb);
  ctx->reset();
}
