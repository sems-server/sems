/*
 * $Id: AmAudioEcho.cpp,v 1.5 2003/11/25 16:19:18 rco Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the sems software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * sems is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "AmAudioEcho.h"
#include "log.h"

int AmAudioEcho::read(unsigned int user_ts, unsigned int size)
{
    timed_buffer.get(user_ts,(ShortSample*)((unsigned char*)samples),size);
    return size;
}

int AmAudioEcho::write(unsigned int user_ts, unsigned int size)
{
    timed_buffer.put(user_ts,(ShortSample*)((unsigned char*)samples),size);
    return size;
}

AmAudioEcho::AmAudioEcho()
{
}

AmAudioEcho::~AmAudioEcho()
{
}
