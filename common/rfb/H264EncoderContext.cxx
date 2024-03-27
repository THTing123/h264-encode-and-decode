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

#include <os/Mutex.h>
#include <rfb/Exception.h>
#include <rfb/LogWriter.h>
#include <rfb/H264EncoderContext.h>
#include <rfb/H264LibavEncoderContext.h>

using namespace rfb;

static LogWriter vlog("H264EncoderContext");

H264EncoderContext *H264EncoderContext::createContext(int width, int height)
{
  H264EncoderContext *ret = new H264LibavEncoderContext();
  if (!ret->initCodec(width, height))
  {
    throw Exception("H264DecoderContext: Unable to create context");
  }

  return ret;
}

H264EncoderContext::~H264EncoderContext()
{
}

bool H264EncoderContext::isReady()
{
  os::AutoMutex lock(&mutex);
  return initialized;
}

void H264EncoderContext::reset()
{
  freeCodec();
  //initCodec(width, height);
}
