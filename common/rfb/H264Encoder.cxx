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
#include <rfb/encodings.h>
#include <rfb/SConnection.h>
#include <rfb/H264Encoder.h>
#include <rfb/H264EncoderContext.h>
#include <rdr/InStream.h>
#include <rdr/OutStream.h>
#include <rfb/Exception.h>
#include <rfb/LogWriter.h>

using namespace rfb;

static LogWriter vlog("H264Encoder");

H264Encoder::H264Encoder(SConnection *conn):Encoder(conn, encodingH264, EncoderPlain)
{
}

H264Encoder::~H264Encoder()
{
}

bool H264Encoder::isSupported()
{
    vlog.debug("isSupported");
    return conn->client.supportsEncoding(encodingH264);
}

void H264Encoder::writeRect(const PixelBuffer *pb,
                            const Palette & /*palette*/)
{
    rdr::OutStream* os = conn->getOutStream();
    vlog.debug("writeRect width(%d), height(%d), bpp(%d)", pb->width(), pb->height(), pb->getPF().bpp);
    H264EncoderContext* ctx =  H264EncoderContext::createContext(pb->width(), pb->height());
    if (!ctx)
        throw Exception("H264Encoder: Context not be created");
    if (!ctx->isReady())
        throw Exception("H264Encoder: Context is not ready");

    
    ctx->encode(os, pb);
    vlog.debug("writeRect ctx->encode run encode");
    ctx->reset();//TODO
}

void H264Encoder::writeSolidRect(int width, int height,
                                 const PixelFormat &pf,
                                 const uint8_t *colour)

{
    (void)colour;
    vlog.info("H264Encoder writeSolidRect, width %d, height %d, pf.bpp %d", width, height, pf.bpp);
}
