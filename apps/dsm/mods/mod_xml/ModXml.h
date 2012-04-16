/*
 * Copyright (C) 2012 FRAFOS GmbH
 *
 * Development sponsored by Sipwise GmbH.
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
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
#ifndef _MOD_XML_H
#define _MOD_XML_H
#include "DSMModule.h"
#include "DSMSession.h"


#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define MOD_CLS_NAME SCXMLModule

DECLARE_MODULE_BEGIN(MOD_CLS_NAME);
int preload();
DECLARE_MODULE_END;

DEF_ACTION_2P(MODXMLParseSIPMsgBodyAction);
DEF_ACTION_2P(MODXMLParseAction);
DEF_ACTION_2P(MODXMLEvalXPathAction);
DEF_ACTION_2P(MODXMLXPathResultNodeCount);
DEF_ACTION_1P(MODXMLSetLogLevelAction);

class ModXmlDoc 
: public DSMDisposable,
  public AmObject {
 public:
 ModXmlDoc() : doc(NULL) {}
 ModXmlDoc(xmlDocPtr doc) : doc(doc) {}
  ~ModXmlDoc();
  xmlDocPtr doc;
};

class ModXmlXPathObj
: public DSMDisposable,
  public AmObject {
 public:
 ModXmlXPathObj() : xpathObj(NULL), xpathCtx(NULL) {}
 ModXmlXPathObj(xmlXPathObjectPtr xpathObj, xmlXPathContextPtr xpathCtx)
   : xpathObj(xpathObj), xpathCtx(xpathCtx) { }
  ~ModXmlXPathObj();

  xmlXPathObjectPtr xpathObj;
  xmlXPathContextPtr xpathCtx;
};

#endif
