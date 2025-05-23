/* Copyright (C) 2000-2003 Constantin Kaplinsky.  All Rights Reserved.
 * Copyright (C) 2011 D. R. Commander.  All Rights Reserved.
 * Copyright 2014-2022 Pierre Ossman for Cendio AB
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

#include <assert.h>

#include <rdr/OutStream.h>
#include <rfb/PixelBuffer.h>
#include <rfb/Palette.h>
#include <rfb/encodings.h>
#include <rfb/SConnection.h>
#include <rfb/TightEncoder.h>
#include <rfb/TightConstants.h>
#include <rfb/LogWriter.h>

using namespace rfb;

static LogWriter vlog("TightEncode");

struct TightConf {
  int idxZlibLevel, monoZlibLevel, rawZlibLevel;
};

//
// Compression level stuff. The following array contains zlib
// settings for each of 10 compression levels (0..9).
//
// NOTE: The parameters used in this encoder are the result of painstaking
// research by The VirtualGL Project using RFB session captures from a variety
// of both 2D and 3D applications.  See http://www.VirtualGL.org for the full
// reports.

static const TightConf conf[10] = {
  { 0, 0, 0 }, // 0
  { 1, 1, 1 }, // 1
  { 3, 3, 2 }, // 2
  { 5, 5, 2 }, // 3
  { 6, 7, 3 }, // 4
  { 7, 8, 4 }, // 5
  { 7, 8, 5 }, // 6
  { 8, 9, 6 }, // 7
  { 9, 9, 7 }, // 8
  { 9, 9, 9 }  // 9
};

TightEncoder::TightEncoder(SConnection* conn) :
  Encoder(conn, encodingTight, EncoderPlain, 256)
{
  setCompressLevel(-1);
}

TightEncoder::~TightEncoder()
{
}

bool TightEncoder::isSupported()
{
  return conn->client.supportsEncoding(encodingTight);
}

void TightEncoder::setCompressLevel(int level)
{
  if (level < 0 || level > 9)
    level = 2;

  idxZlibLevel = conf[level].idxZlibLevel;
  monoZlibLevel = conf[level].monoZlibLevel;
  rawZlibLevel = conf[level].rawZlibLevel;
}

void TightEncoder::writeRect(const PixelBuffer* pb, const Palette& palette)
{
  vlog.debug("writeRect pb->width(%d), pb->height(%d), palette.size(%d)", pb->width(), pb->height(), palette.size());
  switch (palette.size()) {
  case 0:
    writeFullColourRect(pb);
    break;
  case 1:
    Encoder::writeSolidRect(pb, palette);
    break;
  case 2:
    writeMonoRect(pb, palette);
    break;
  default:
    writeIndexedRect(pb, palette);
  }
}

void TightEncoder::writeSolidRect(int /*width*/, int /*height*/,
                                  const PixelFormat& pf,
                                  const uint8_t* colour)
{
  rdr::OutStream* os;

  os = conn->getOutStream();

  os->writeU8(tightFill << 4);
  writePixels(colour, pf, 1, os);
}

void TightEncoder::writeMonoRect(const PixelBuffer* pb, const Palette& palette)
{
  const uint8_t* buffer;
  int stride;

  buffer = pb->getBuffer(pb->getRect(), &stride);

  switch (pb->getPF().bpp) {
  case 32:
    writeMonoRect(pb->width(), pb->height(), (uint32_t*)buffer, stride,
                  pb->getPF(), palette);
    break;
  case 16:
    writeMonoRect(pb->width(), pb->height(), (uint16_t*)buffer, stride,
                  pb->getPF(), palette);
    break;
  default:
    writeMonoRect(pb->width(), pb->height(), (uint8_t*)buffer, stride,
                  pb->getPF(), palette);
  }
}

void TightEncoder::writeIndexedRect(const PixelBuffer* pb, const Palette& palette)
{
  const uint8_t* buffer;
  int stride;

  buffer = pb->getBuffer(pb->getRect(), &stride);

  switch (pb->getPF().bpp) {
  case 32:
    writeIndexedRect(pb->width(), pb->height(), (uint32_t*)buffer, stride,
                     pb->getPF(), palette);
    break;
  case 16:
    writeIndexedRect(pb->width(), pb->height(), (uint16_t*)buffer, stride,
                     pb->getPF(), palette);
    break;
  default:
    // It's more efficient to just do raw pixels
    writeFullColourRect(pb);
  }
}

void TightEncoder::writeFullColourRect(const PixelBuffer* pb)
{
  const int streamId = 0;

  rdr::OutStream* os;
  rdr::OutStream* zos;
  int length;

  const uint8_t* buffer;
  int stride, h;

  os = conn->getOutStream();

  os->writeU8(streamId << 4);

  // Set up compression
  if ((pb->getPF().bpp != 32) || !pb->getPF().is888())
    length = pb->getRect().area() * pb->getPF().bpp/8;
  else
    length = pb->getRect().area() * 3;

  zos = getZlibOutStream(streamId, rawZlibLevel, length);

  // And then just dump all the raw pixels
  buffer = pb->getBuffer(pb->getRect(), &stride);
  h = pb->height();

  while (h--) {
    writePixels(buffer, pb->getPF(), pb->width(), zos);
    buffer += stride * pb->getPF().bpp/8;
  }

  // Finish the zlib stream
  flushZlibOutStream(zos);
}

void TightEncoder::writePixels(const uint8_t* buffer, const PixelFormat& pf,
                               unsigned int count, rdr::OutStream* os)
{
  uint8_t rgb[2048];

  if ((pf.bpp != 32) || !pf.is888()) {
    os->writeBytes(buffer, count * pf.bpp/8);
    return;
  }

  while (count) {
    unsigned int iter_count;

    iter_count = sizeof(rgb)/3;
    if (iter_count > count)
      iter_count = count;

    pf.rgbFromBuffer(rgb, buffer, iter_count);
    os->writeBytes(rgb, iter_count * 3);

    buffer += iter_count * pf.bpp/8;
    count -= iter_count;
  }
}

void TightEncoder::writeCompact(rdr::OutStream* os, uint32_t value)
{
  uint8_t b;
  b = value & 0x7F;
  if (value <= 0x7F) {
    os->writeU8(b);
  } else {
    os->writeU8(b | 0x80);
    b = value >> 7 & 0x7F;
    if (value <= 0x3FFF) {
      os->writeU8(b);
    } else {
      os->writeU8(b | 0x80);
      os->writeU8(value >> 14 & 0xFF);
    }
  }
}

rdr::OutStream* TightEncoder::getZlibOutStream(int streamId, int level, size_t length)
{
  // Minimum amount of data to be compressed. This value should not be
  // changed, doing so will break compatibility with existing clients.
  if (length < 12)
    return conn->getOutStream();

  assert(streamId >= 0);
  assert(streamId < 4);

  zlibStreams[streamId].setUnderlying(&memStream);
  zlibStreams[streamId].setCompressionLevel(level);
  zlibStreams[streamId].cork(true);

  return &zlibStreams[streamId];
}

void TightEncoder::flushZlibOutStream(rdr::OutStream* os_)
{
  rdr::OutStream* os;
  rdr::ZlibOutStream* zos;

  zos = dynamic_cast<rdr::ZlibOutStream*>(os_);
  if (zos == NULL)
    return;

  zos->cork(false);
  zos->flush();
  zos->setUnderlying(NULL);

  os = conn->getOutStream();

  writeCompact(os, memStream.length());
  os->writeBytes(memStream.data(), memStream.length());
  memStream.clear();
}

template<class T>
void TightEncoder::writeMonoRect(int width, int height,
                                 const T* buffer, int stride,
                                 const PixelFormat& pf,
                                 const Palette& palette)
{
  rdr::OutStream* os;

  const int streamId = 1;
  T pal[2];

  int length;
  rdr::OutStream* zos;

  assert(palette.size() == 2);

  os = conn->getOutStream();

  os->writeU8((streamId | tightExplicitFilter) << 4);
  os->writeU8(tightFilterPalette);

  // Write the palette
  pal[0] = (T)palette.getColour(0);
  pal[1] = (T)palette.getColour(1);

  os->writeU8(1);
  writePixels((uint8_t*)pal, pf, 2, os);

  // Set up compression
  length = (width + 7)/8 * height;
  zos = getZlibOutStream(streamId, monoZlibLevel, length);

  // Encode the data
  T bg;
  unsigned int value, mask;
  int pad, aligned_width;
  int x, y, bg_bits;

  bg = pal[0];
  aligned_width = width - width % 8;
  pad = stride - width;

  for (y = 0; y < height; y++) {
    for (x = 0; x < aligned_width; x += 8) {
      for (bg_bits = 0; bg_bits < 8; bg_bits++) {
        if (*buffer++ != bg)
          break;
      }
      if (bg_bits == 8) {
        zos->writeU8(0);
        continue;
      }
      mask = 0x80 >> bg_bits;
      value = mask;
      for (bg_bits++; bg_bits < 8; bg_bits++) {
        mask >>= 1;
        if (*buffer++ != bg) {
          value |= mask;
        }
      }
      zos->writeU8(value);
    }

    if (x < width) {
      mask = 0x80;
      value = 0;

      for (; x < width; x++) {
        if (*buffer++ != bg) {
          value |= mask;
        }
        mask >>= 1;
      }
      zos->writeU8(value);
    }

    buffer += pad;
  }

  // Finish the zlib stream
  flushZlibOutStream(zos);
}

template<class T>
void TightEncoder::writeIndexedRect(int width, int height,
                                    const T* buffer, int stride,
                                    const PixelFormat& pf,
                                    const Palette& palette)
{
  rdr::OutStream* os;

  const int streamId = 2;
  T pal[256];

  rdr::OutStream* zos;

  int pad;
  T prevColour;
  unsigned char idx;

  assert(palette.size() > 0);
  assert(palette.size() <= 256);

  os = conn->getOutStream();

  os->writeU8((streamId | tightExplicitFilter) << 4);
  os->writeU8(tightFilterPalette);

  // Write the palette
  for (int i = 0; i < palette.size(); i++)
    pal[i] = (T)palette.getColour(i);

  os->writeU8(palette.size() - 1);
  writePixels((uint8_t*)pal, pf, palette.size(), os);

  // Set up compression
  zos = getZlibOutStream(streamId, idxZlibLevel, width * height);

  // Encode the data
  pad = stride - width;

  prevColour = *buffer;
  idx = palette.lookup(*buffer);

  while (height--) {
    int w = width;
    while (w--) {
      if (*buffer != prevColour) {
        prevColour = *buffer;
        idx = palette.lookup(*buffer);
      }
      zos->writeU8(idx);
      buffer++;
    }
    buffer += pad;
  }

  // Finish the zlib stream
  flushZlibOutStream(zos);
}
