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

#ifndef __RFB_H264ENCODERCONTEXT_H__
#define __RFB_H264ENCODERCONTEXT_H__

#include <stdint.h>

#include <os/Mutex.h>
#include <rfb/Rect.h>
#include <rfb/Encoder.h>
#include <rdr/OutStream.h>

namespace rfb {
  class H264EncoderContext {
    public:
      static H264EncoderContext *createContext(int width, int height);

      virtual ~H264EncoderContext() = 0;

      virtual void encode(rdr::OutStream* os, const PixelBuffer* pb) {(void)os;(void)pb;}
      void reset();

      bool isReady();

    protected:
      os::Mutex mutex;
      bool initialized;

      H264EncoderContext()  { initialized = false; }

      virtual bool initCodec(int width, int height) { (void)width; (void)height; return false; }
      virtual void freeCodec() {}
  };
}



#endif