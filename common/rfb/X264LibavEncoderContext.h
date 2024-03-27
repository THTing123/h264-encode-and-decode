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

#ifndef __RFB_H264LIBAVEnCODER_H__
#define __RFB_H264LIBAVEnCODER_H__

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <x264.h>
}

#include <rfb/H264EncoderContext.h>
#include <rdr/OutStream.h>

namespace rfb {
  class X264LibavEncoderContext : public H264EncoderContext {
    public:
      X264LibavEncoderContext() : H264EncoderContext() {
          nals = NULL;
          nalcount = 0;
      }
      ~X264LibavEncoderContext() { freeCodec(); }

      virtual void encode(rdr::OutStream* os, const PixelBuffer* pb);

    protected:
      virtual bool initCodec(int width, int height);
      virtual void freeCodec();

    private:
      SwsContext* sws;
      int width;
      int height;
      x264_t *encoder;
      x264_picture_t *pPic_in;
      x264_picture_t *pPic_out;
      x264_nal_t *nals;           // NAL array
      int nalcount;               // Number of NALs
  };
}

#endif
