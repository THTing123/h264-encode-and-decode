/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2014 Pierre Ossman for Cendio AB
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <rdr/OutStream.h>
#include <rfb/encodings.h>
#include <rfb/SConnection.h>
#include <rfb/PixelBuffer.h>
#include <rfb/RawEncoder.h>

#include <ctime>
#include <rfb/LogWriter.h>

using namespace rfb;

static LogWriter vlog("RawEncoder");

RawEncoder::RawEncoder(SConnection* conn) :
  Encoder(conn, encodingRaw, EncoderPlain)
{
}

RawEncoder::~RawEncoder()
{
}

bool RawEncoder::isSupported()
{
  // Implicitly required;
  return true;
}

void RawEncoder::writeRect(const PixelBuffer* pb,
                           const Palette& /*palette*/)
{
  const uint8_t* buffer;
  int stride;
  
  rdr::OutStream* os;
  int h, line_bytes, stride_bytes;

  buffer = pb->getBuffer(pb->getRect(), &stride);

  os = conn->getOutStream();

  h = pb->height();
  line_bytes = pb->width() * pb->getPF().bpp/8;
  stride_bytes = stride * pb->getPF().bpp/8;
  while (h--) {
    os->writeBytes(buffer, line_bytes);
    buffer += stride_bytes;
  }

  vlog.debug("writeRect pb->width(%d), pb->height(%d)", pb->width(), pb->height());
}

void RawEncoder::writeSolidRect(int width, int height,
                                const PixelFormat& pf,
                                const uint8_t* colour)
{
  rdr::OutStream* os;
  int pixels, pixel_size;

  os = conn->getOutStream();

  pixels = width*height;
  pixel_size = pf.bpp/8;
  while (pixels--)
    os->writeBytes(colour, pixel_size);
}
