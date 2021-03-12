/* Copyright (C) 2002-2003 Fhg Fokus
 * Copyright (C) 2006 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef IvrUAC_h
#define IvrUAC_h

// Python stuff
#include <Python.h>
#include "structmember.h"

/** \brief python IVR wrapper for AmUAC functions */
typedef struct {
    
  PyObject_HEAD
  //    AmSession* session;

} IvrUAC;

extern PyTypeObject IvrUACType;

#endif
