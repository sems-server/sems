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
#include "ModXml.h"
#include "log.h"
#include "AmUtils.h"

SC_EXPORT(MOD_CLS_NAME);

void xml_err_func(void *ctx, const char *msg, ...);
xmlGenericErrorFunc handler = (xmlGenericErrorFunc)xml_err_func;
int xml_log_level = L_ERR;

int MOD_CLS_NAME::preload() {
  DBG("initializing libxml2...\n");
  xmlInitParser();
  initGenericErrorDefaultFunc(&handler);
  handler = (xmlGenericErrorFunc)xml_err_func;
  xmlSetGenericErrorFunc(NULL, &xml_err_func);
  xmlKeepBlanksDefault(0);
  xmlIndentTreeOutput = 1; // doesn't seem to have effect :/
  return 0;
}

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {
  DEF_CMD("xml.parse", MODXMLParseAction);
  DEF_CMD("xml.parseSIPMsgBody", MODXMLParseSIPMsgBodyAction);

  DEF_CMD("xml.evalXPath", MODXMLEvalXPathAction);
  DEF_CMD("xml.XPathResultCount", MODXMLXPathResultNodeCount);
  DEF_CMD("xml.getXPathResult", MODXMLgetXPathResult);
  DEF_CMD("xml.printXPathResult", MODXMLprintXPathResult);
  DEF_CMD("xml.updateXPathResult", MODXMLupdateXPathResult);

  DEF_CMD("xml.docDump", MODXMLdocDump);

  DEF_CMD("xml.setLoglevel", MODXMLSetLogLevelAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);

ModXmlDoc::~ModXmlDoc() {
  if (NULL != doc) {
    DBG("freeing XML document [%p]\n", doc);
    xmlFreeDoc(doc);
  }
}

ModXmlXPathObj::~ModXmlXPathObj() {
  if (NULL != xpathObj) {
    DBG("freeing XML xpath obj [%p]\n", xpathObj);
    xmlXPathFreeObject(xpathObj);
  }
  if (NULL != xpathCtx) {
    DBG("freeing XML xpath ctx [%p]\n", xpathCtx);
    xmlXPathFreeContext(xpathCtx);
  }
}

#define TMP_BUF_SIZE 256
void xml_err_func(void *ctx, const char *msg, ...) {
   char _string[TMP_BUF_SIZE];
   va_list arg_ptr;
   va_start(arg_ptr, msg);
   vsnprintf(_string, TMP_BUF_SIZE, msg, arg_ptr);
   va_end(arg_ptr);

   _LOG(xml_log_level, "%s", _string);
}

CONST_ACTION_2P(MODXMLParseSIPMsgBodyAction, ',', false);
EXEC_ACTION_START(MODXMLParseSIPMsgBodyAction) {
  string msgbody_var = resolveVars(par1, sess, sc_sess, event_params);
  string dstname = resolveVars(par2, sess, sc_sess, event_params);
  AVarMapT::iterator it = sc_sess->avar.find(msgbody_var);
  if (it==sc_sess->avar.end()) {
    DBG("no message body in avar '%s'\n", msgbody_var.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("no message body in avar " + msgbody_var);
    EXEC_ACTION_STOP;
  }
  AmMimeBody* msgbody = dynamic_cast<AmMimeBody*>(it->second.asObject());
  if (NULL == msgbody) {
    DBG("no AmMimeBody in avar '%s'\n", msgbody_var.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("no AmMimeBody in avar " + msgbody_var);
    EXEC_ACTION_STOP;
  }
  const unsigned char* b =  msgbody->getPayload();
  if (b==NULL) {
    DBG("empty AmMimeBody in avar '%s'\n", msgbody_var.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("no AmMimeBody in avar " + msgbody_var);
    EXEC_ACTION_STOP;
  }

  xmlSetGenericErrorFunc(NULL, &xml_err_func);

  xmlDocPtr doc =
    xmlReadMemory((const char*)b, msgbody->getLen(), "noname.xml", NULL, 0);
  if (doc == NULL) {
    DBG("failed parsing XML document from '%s'\n", msgbody_var.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("failed parsing XML document from " + msgbody_var);
    EXEC_ACTION_STOP;
  }

  xmlSetGenericErrorFunc(doc, &xml_err_func);

  ModXmlDoc* xml_doc = new ModXmlDoc(doc);
  sc_sess->avar[dstname] = xml_doc;
  DBG("parsed XML body document to '%s'\n", dstname.c_str());

//  string basedir = resolveVars(par2, sess, sc_sess, event_params);
} EXEC_ACTION_END;

CONST_ACTION_2P(MODXMLParseAction, ',', false);
EXEC_ACTION_START(MODXMLParseAction) {
  string xml_doc = resolveVars(par1, sess, sc_sess, event_params);
  string dstname = resolveVars(par2, sess, sc_sess, event_params);

  xmlSetGenericErrorFunc(NULL, &xml_err_func);

  xmlDocPtr doc =
    xmlReadMemory(xml_doc.c_str(), xml_doc.length(), "noname.xml", NULL, 0);
  if (doc == NULL) {
    DBG("failed parsing XML document from '%s'\n", xml_doc.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("failed parsing XML document from " + xml_doc);
    EXEC_ACTION_STOP;
  }
  xmlSetGenericErrorFunc(doc, &xml_err_func);

  ModXmlDoc* xml_doc_var = new ModXmlDoc(doc);
  sc_sess->avar[dstname] = xml_doc_var;
  DBG("parsed XML body document to '%s'\n", dstname.c_str());
} EXEC_ACTION_END;

template<class T>
T* getXMLElemFromVariable(DSMSession* sc_sess, const string& var_name) {
  AVarMapT::iterator it = sc_sess->avar.find(var_name);
  if (it == sc_sess->avar.end()) {
    DBG("object '%s' not found\n", var_name.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("object '"+var_name+"' not found\n");
    return NULL;
  }

  T* doc = dynamic_cast<T*>(it->second.asObject());
  if (NULL == doc) {
    DBG("object '%s' is not the right type\n", var_name.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("object '"+var_name+"' is not the right type\n");
    return NULL;
  }
  return doc;
}

CONST_ACTION_2P(MODXMLEvalXPathAction, ',', false);
EXEC_ACTION_START(MODXMLEvalXPathAction) {
  string xpath_expr  = resolveVars(par1, sess, sc_sess, event_params);
  string xml_doc_var = resolveVars(par2, sess, sc_sess, event_params);

  xmlSetGenericErrorFunc(NULL, &xml_err_func);

  ModXmlDoc* xml_doc = getXMLElemFromVariable<ModXmlDoc>(sc_sess, xml_doc_var);
  if (NULL == xml_doc)
    EXEC_ACTION_STOP;

  xmlDocPtr doc = xml_doc->doc;
  
  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
  if(xpathCtx == NULL) {
    DBG("unable to create new XPath context\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("unable to create new XPath context");
    EXEC_ACTION_STOP;
  }
  xmlSetGenericErrorFunc(xpathCtx, &xml_err_func);

  string xml_doc_ns = sc_sess->var[xml_doc_var+".ns"];
  vector<string> ns_entries = explode(xml_doc_ns, " ");
  for (vector<string>::iterator it=ns_entries.begin(); it != ns_entries.end(); it++) {
    vector<string> ns = explode(*it, "=");
    if (ns.size() != 2) {
      DBG("script writer error: namespace entry must be prefix=href (got '%s')\n",
	  it->c_str());
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("script writer error: namespace entry must be prefix=href\n");
      xmlXPathFreeContext(xpathCtx);
      EXEC_ACTION_STOP;
    }

    if(xmlXPathRegisterNs(xpathCtx, (const xmlChar*)ns[0].c_str(),
			  (const xmlChar*)ns[1].c_str()) != 0) {
      DBG("unable to register namespace %s=%s\n", ns[0].c_str(), ns[1].c_str());
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("unable to register namespace\n");
      xmlXPathFreeContext(xpathCtx);
      EXEC_ACTION_STOP;
    }
    DBG("registered namespace %s=%s\n", ns[0].c_str(), ns[1].c_str());
  }

  xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar*)xpath_expr.c_str(),
						      xpathCtx);
  if(xpathObj == NULL) {
    DBG("unable to evaluate xpath expression \"%s\"\n", xpath_expr.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("unable to evaluate xpath expression");
    xmlXPathFreeContext(xpathCtx);
    EXEC_ACTION_STOP;
  }

  ModXmlXPathObj* xpath_obj = new ModXmlXPathObj(xpathObj, xpathCtx);
  sc_sess->avar[xml_doc_var+".xpath"] = xpath_obj;
  DBG("evaluated XPath expression on '%s' to '%s'\n",
      xml_doc_var.c_str(), (xml_doc_var+".xpath").c_str());

} EXEC_ACTION_END;

CONST_ACTION_2P(MODXMLXPathResultNodeCount, '=', false);
EXEC_ACTION_START(MODXMLXPathResultNodeCount) {
  string cnt_var  = par1;
  string xpath_res_var = resolveVars(par2, sess, sc_sess, event_params);

  if (cnt_var.size() && cnt_var[0]=='$') {
    cnt_var.erase(0,1);
  }

  ModXmlXPathObj* xpath_obj =
    getXMLElemFromVariable<ModXmlXPathObj>(sc_sess, xpath_res_var);
  if (NULL == xpath_obj){
    DBG("no xpath result found in '%s'\n", xpath_res_var.c_str());
    sc_sess->var[cnt_var] = "0";
    EXEC_ACTION_STOP;
  }

  unsigned int res = (xpath_obj->xpathObj->nodesetval) ? 
    xpath_obj->xpathObj->nodesetval->nodeNr : 0;

  sc_sess->var[cnt_var] = int2str(res);
  DBG("set count $%s=%u\n", cnt_var.c_str(), res);
  
} EXEC_ACTION_END;


CONST_ACTION_2P(MODXMLgetXPathResult, '=', false);
EXEC_ACTION_START(MODXMLgetXPathResult) {
  string cnt_var  = par1;
  string xpath_res_var = resolveVars(par2, sess, sc_sess, event_params);

  if (cnt_var.size() && cnt_var[0]=='$') {
    cnt_var.erase(0,1);
  }

  ModXmlXPathObj* xpath_obj =
    getXMLElemFromVariable<ModXmlXPathObj>(sc_sess, xpath_res_var);
  if (NULL == xpath_obj){
    DBG("no xpath result found in '%s'\n", xpath_res_var.c_str());
    sc_sess->var[cnt_var] = "0";
    EXEC_ACTION_STOP;
  }

  vector<string> res;

  if (NULL == xpath_obj->xpathObj->nodesetval){
    res.push_back(string());
  } else {
    xmlNodeSetPtr nodes = xpath_obj->xpathObj->nodesetval; 
    xmlNodePtr cur;

    for (int i=0;i<xpath_obj->xpathObj->nodesetval->nodeNr;i++) {
      if(nodes->nodeTab[i]->type == XML_NAMESPACE_DECL) {
	xmlNsPtr ns;
	    
	ns = (xmlNsPtr)nodes->nodeTab[i];
	cur = (xmlNodePtr)ns->next;
	res.push_back(string(string((const char*) ns->prefix)+"="+string((const char*) ns->href)));

      } else if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
	cur = nodes->nodeTab[i];
	xmlChar* c = xmlNodeGetContent(cur);
	res.push_back(c ? string((const char*)c) : string());
      } else {
	cur = nodes->nodeTab[i];    
	res.push_back(string((const char*) cur->name)+"\": type "+int2str(cur->type));
      }
    }
  }

  if (res.size() == 1) {
    sc_sess->var[cnt_var] = res[0];
    DBG("set $%s='%s'\n", cnt_var.c_str(), res[0].c_str());
  } else {
    unsigned int p =0;
    for (vector<string>::iterator it = res.begin(); it!= res.end(); it++) {
      sc_sess->var[cnt_var+"["+int2str(p)+"]"] = *it;
      DBG("set $%s='%s'\n", (cnt_var+"["+int2str(p)+"]").c_str(), it->c_str());
      p++;
    }
  }

  
} EXEC_ACTION_END;

CONST_ACTION_2P(MODXMLprintXPathResult, '=', false);
EXEC_ACTION_START(MODXMLprintXPathResult) {
  string cnt_var  = par1;
  string xpath_res_var = resolveVars(par2, sess, sc_sess, event_params);

  if (cnt_var.size() && cnt_var[0]=='$') {
    cnt_var.erase(0,1);
  }

  ModXmlXPathObj* xpath_obj =
    getXMLElemFromVariable<ModXmlXPathObj>(sc_sess, xpath_res_var);
  if (NULL == xpath_obj){
    DBG("no xpath result found in '%s'\n", xpath_res_var.c_str());
    sc_sess->var[cnt_var] = "0";
    EXEC_ACTION_STOP;
  }

  string& res = sc_sess->var[cnt_var];
  if (NULL == xpath_obj->xpathObj->nodesetval){
    res = "";
  } else {
    xmlNodeSetPtr nodes = xpath_obj->xpathObj->nodesetval; 
    xmlNodePtr cur;

    for (int i=0;i<xpath_obj->xpathObj->nodesetval->nodeNr;i++) {
      if(nodes->nodeTab[i]->type == XML_NAMESPACE_DECL) {
	xmlNsPtr ns;
	    
	ns = (xmlNsPtr)nodes->nodeTab[i];
	cur = (xmlNodePtr)ns->next;
	if(cur->ns) { 
	  res += "namespace \""+string((const char*) ns->prefix)+"\"=\""+string((const char*) ns->href)+"\" for node "+
	    string((const char*) cur->ns->href)+":"+string((const char*) cur->name)+"\n";
	} else {
	  res += "namespace \""+ string((const char*) ns->prefix) +"\"=\""+ string((const char*) ns->href) +"\" for node "+
	    string((const char*) cur->name)+"\n";
	}
      } else if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
	cur = nodes->nodeTab[i];
	if(cur->ns) {    	    
	  xmlChar* c = xmlNodeGetContent(cur);
	  res += "element node \""+string((const char*) cur->ns->href)+":"+string((const char*) cur->name)+"\" content: \""+
	    (c?string((const char*)c):string("NULL"))+ "\"\n";
	} else {
	  xmlChar* c = xmlNodeGetContent(cur);
	  res += "element node \""+string((const char*) cur->name)+"\" content: \""+ (c?string((const char*)c):string("NULL"))+"\n"; 
	}
      } else {
	cur = nodes->nodeTab[i];    
	res += "node \""+string((const char*) cur->name)+"\": type "+int2str(cur->type)+"\n";
      }
    }
  }

  DBG("set $%s='%s'\n", cnt_var.c_str(), res.c_str());
  
} EXEC_ACTION_END;


/**
   modified from  http://www.xmlsoft.org/examples/xpath2.c (MIT license)
 * update_xpath_nodes:
 * @nodes:		the nodes set.
 * @value:		the new value for the node(s)
 *
 * Prints the @nodes content to @output.
 */
static void
update_xpath_nodes(xmlNodeSetPtr nodes, const xmlChar* value, int index) {
    int size;
    int i;
    
    assert(value);
    size = (nodes) ? nodes->nodeNr : 0;
    
    if (index < 0) { 
      // update all
      /*
       * NOTE: the nodes are processed in reverse order, i.e. reverse document
       *       order because xmlNodeSetContent can actually free up descendant
       *       of the node and such nodes may have been selected too ! Handling
       *       in reverse order ensure that descendant are accessed first, before
       *       they get removed. Mixing XPath and modifications on a tree must be
       *       done carefully !
       */
      for(i = size - 1; i >= 0; i--) {
	if (NULL == nodes->nodeTab[i])
	  continue;
	
	xmlNodeSetContent(nodes->nodeTab[i], value);
	/*
	 * All the elements returned by an XPath query are pointers to
	 * elements from the tree *except* namespace nodes where the XPath
	 * semantic is different from the implementation in libxml2 tree.
	 * As a result when a returned node set is freed when
	 * xmlXPathFreeObject() is called, that routine must check the
	 * element type. But node from the returned set may have been removed
	 * by xmlNodeSetContent() resulting in access to freed data.
	 * This can be exercised by running
	 *       valgrind xpath2 test3.xml '//discarded' discarded
	 * There is 2 ways around it:
	 *   - make a copy of the pointers to the nodes from the result set 
	 *     then call xmlXPathFreeObject() and then modify the nodes
	 * or
	 *   - remove the reference to the modified nodes from the node set
	 *     as they are processed, if they are not namespace nodes.
	 */
	if (nodes->nodeTab[i]->type != XML_NAMESPACE_DECL)
	  nodes->nodeTab[i] = NULL;
      }
    } else {
      if (index >= size) {
	ERROR("trying to update XML node %d, size is %d\n", index, size);
	return;
      }

      if (NULL == nodes->nodeTab[index]) {
	ERROR("trying to update XML node %d which is NULL\n", index);
      }
	
      xmlNodeSetContent(nodes->nodeTab[index], value);
      /*
       * All the elements returned by an XPath query are pointers to
       * elements from the tree *except* namespace nodes where the XPath
       * semantic is different from the implementation in libxml2 tree.
       * As a result when a returned node set is freed when
       * xmlXPathFreeObject() is called, that routine must check the
       * element type. But node from the returned set may have been removed
       * by xmlNodeSetContent() resulting in access to freed data.
       * This can be exercised by running
       *       valgrind xpath2 test3.xml '//discarded' discarded
       * There is 2 ways around it:
       *   - make a copy of the pointers to the nodes from the result set 
       *     then call xmlXPathFreeObject() and then modify the nodes
       * or
       *   - remove the reference to the modified nodes from the node set
       *     as they are processed, if they are not namespace nodes.
       */
      if (nodes->nodeTab[index]->type != XML_NAMESPACE_DECL)
	nodes->nodeTab[index] = NULL;
    }
}

CONST_ACTION_2P(MODXMLupdateXPathResult, '=', false);
EXEC_ACTION_START(MODXMLupdateXPathResult) {
  string xpath_res_var = resolveVars(par1, sess, sc_sess, event_params);
  string value  = resolveVars(par2, sess, sc_sess, event_params);

  // support index
  int index = -1;
  if (xpath_res_var.size()>2 && xpath_res_var[xpath_res_var.size()-1]==']') {
    size_t p = xpath_res_var.rfind('[');
    if (p != string::npos) {
      str2int(xpath_res_var.substr(p+1, xpath_res_var.size()-p-2), index);
      xpath_res_var.erase(p);
    }
  }

  DBG("index %d, var '%s'\n", index, xpath_res_var.c_str());
  ModXmlXPathObj* xpath_obj =
    getXMLElemFromVariable<ModXmlXPathObj>(sc_sess, xpath_res_var);
  if (NULL == xpath_obj){
    DBG("no xpath result found in '%s'\n", xpath_res_var.c_str());
    EXEC_ACTION_STOP;
  }
  // todo: call xmlEncodeSpecialChars with doc 

  
  update_xpath_nodes(xpath_obj->xpathObj->nodesetval, (const xmlChar*) value.c_str(), index);

} EXEC_ACTION_END;

CONST_ACTION_2P(MODXMLdocDump, '=', false);
EXEC_ACTION_START(MODXMLdocDump) {
  string res_var  = par1;
  string xml_doc_var = resolveVars(par2, sess, sc_sess, event_params);

  if (res_var.size() && res_var[0]=='$') {
    res_var.erase(0,1);
  }

  ModXmlDoc* xml_doc = getXMLElemFromVariable<ModXmlDoc>(sc_sess, xml_doc_var);
  if (NULL == xml_doc) {
    DBG("XML document not found t variable '%s'\n", xml_doc_var.c_str());
    sc_sess->var[res_var] = "";
    EXEC_ACTION_STOP;
  }

  xmlChar* mem;
  int size;
  xmlDocDumpFormatMemory(xml_doc->doc, &mem, &size, /* indent=*/1);
  sc_sess->var[res_var] = string((const char*)mem, size);
  xmlFree(mem);
  DBG("set $%s to XML of size %d\n", res_var.c_str(), size);

} EXEC_ACTION_END;


EXEC_ACTION_START(MODXMLSetLogLevelAction) {
  string xml_log_level_s = resolveVars(arg, sess, sc_sess, event_params);
  if (xml_log_level_s == "error")
    xml_log_level = L_ERR;
  else if (xml_log_level_s == "warn")
    xml_log_level = L_WARN;
  else if (xml_log_level_s == "info")
    xml_log_level = L_INFO;
  else if (xml_log_level_s == "debug")
    xml_log_level = L_DBG;
  else {
    ERROR("script writer error: '%s' is no valid log level (error, warn, info, debug)\n",
	  xml_log_level_s.c_str());
  }
} EXEC_ACTION_END;
