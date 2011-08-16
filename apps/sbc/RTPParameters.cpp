/*
 * Copyright (C) 2011 Stefan Sayer
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "RTPParameters.h"

const  iana_rtp_payload IANA_RTP_PAYLOADS[IANA_RTP_PAYLOADS_SIZE] = {
  // from http://www.iana.org/assignments/rtp-parameters
  { "PCMU",  true,   8000, 1},   // 0         PCMU            A                  8000             1                 [RFC3551]
  {  "",     true,      0, 0},   // 1         Reserved
  {  "",     true,      0, 0},   // 2         Reserved
  {  "GSM",  true,   8000, 1},   // 3         GSM             A                  8000             1                 [RFC3551]
  {  "G723", true,   8000, 1},   // 4         G723            A                  8000             1                 [Kumar][RFC3551]
  {  "DVI4", true,   8000, 1},   // 5         DVI4            A                  8000             1                 [RFC3551]
  {  "DVI4", true,  16000, 1},   // 6         DVI4            A                  16000            1                 [RFC3551]
  {  "LPC",  true,   8000, 1},   // 7         LPC             A                  8000             1                 [RFC3551]
  {  "PCMA", true,   8000, 1},   // 8         PCMA            A                  8000             1                 [RFC3551]
  {  "G722", true,   8000, 1},   // 9         G722            A                  8000             1                 [RFC3551]
  {  "L16",  true,  44100, 2},   // 10        L16             A                  44100            2                 [RFC3551]
  {  "L16",  true,  44100, 1},   // 11        L16             A                  44100            1                 [RFC3551]
  {  "QCELP",true,   8000, 1},   // 12        QCELP           A                  8000             1                 [RFC3551]
  {  "CN",   true,   8000, 1},   // 13        CN              A                  8000             1                 [RFC3389]
  {  "MPA",  true,  90000, 0},   // 14        MPA             A                  90000                              [RFC3551][RFC2250]
  {  "G728", true,   8000, 1},   // 15        G728            A                  8000             1                 [RFC3551]
  {  "DVI4", true,  11025, 1},   // 16        DVI4            A                  11025            1                 [DiPol]
  {  "DVI4", true,  22050, 1},   // 17        DVI4            A                  22050            1                 [DiPol]
  {  "G729", true,   8000, 1},   // 18        G729            A                  8000             1                 [RFC3551]
  {  "",     true,      0, 0},   // 19        Reserved        A
  {  "",     true,      0, 0},   // 20        Unassigned      A
  {  "",     true,      0, 0},   // 21        Unassigned      A
  {  "",     true,      0, 0},   // 22        Unassigned      A
  {  "",     true,      0, 0},   // 23        Unassigned      A
  {  "",     true,      0, 0},   // 24        Unassigned      V
  {  "CelB", false, 90000, 0},   // 25        CelB            V                  90000                              [RFC2029]
  {  "JPEG", false, 90000, 0},   // 26        JPEG            V                  90000                              [RFC2435]
  {  "",     false,     0, 0},   // 27        Unassigned      V
  {  "nv",   false, 90000, 0},   // 28        nv              V                  90000                              [RFC3551]
  {  "",     false,     0, 0},   // 29        Unassigned      V
  {  "",     false,     0, 0},   // 30        Unassigned      V
  {  "H261", false, 90000, 0},   // 31        H261            V                  90000                              [RFC4587]
  {  "MPV",  false, 90000, 0},   // 32        MPV             V                  90000                              [RFC2250]
  {  "MP2T", false, 90000, 0},   // 33        MP2T            AV                 90000                              [RFC2250]
  {  "H263", false, 90000, 0}    // 34        H263            V                  90000                              [Zhu]
                                 // 35-71     Unassigned      ?
                                 // 72-76     Reserved for RTCP conflict avoidance                                  [RFC3551]
                                 // 77-95     Unassigned      ?
                                 // 96-127    dynamic         ?                                                     [RFC3551] 
};
