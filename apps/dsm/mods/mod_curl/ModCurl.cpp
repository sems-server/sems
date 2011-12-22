/*
 * Copyright (C) 2009 TelTech Systems Inc.
 * Copyright (C) 2011 Stefan Sayer
 * 
 * This file is part of SEMS, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

#include "ModCurl.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"

#include <curl/curl.h> 
#include <sstream>
#include "AmConfigReader.h"

bool CurlModule::curl_initialized = false;

SC_EXPORT(CurlModule);

CurlModule::CurlModule() {
  if (!curl_initialized) {
    curl_initialized = true;
    if (curl_global_init(CURL_GLOBAL_ALL)) {
      ERROR("Initializing libcurl\n");
      throw string("Initializing libcurl");
    }

    curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
    if (data && data->version >=0) {
      DBG("using libcurl version '%s'\n", 
	  data->version);
      if (data->features & CURL_VERSION_SSL) {
	DBG("libcurl with SSL version '%s'\n", data->ssl_version);
      } else {
	DBG("libcurl without SSL support\n");
      }
    }
  }
}

CurlModule::~CurlModule() {
}


DSMAction* CurlModule::getAction(const string& from_str) {
  string cmd;
  string params;
  splitCmd(from_str, cmd, params);

  DEF_CMD("curl.get", SCJCurlGetAction);
  DEF_CMD("curl.getDiscardResult", SCJCurlGetNoresultAction);
  DEF_CMD("curl.getFile", SCJCurlGetFileAction);
  DEF_CMD("curl.getForm", SCJCurlGetFormAction);
  DEF_CMD("curl.post", SCJCurlPOSTGetResultAction);
  DEF_CMD("curl.postDiscardResult", SCJCurlPOSTAction);

  return NULL;
}

DSMCondition* CurlModule::getCondition(const string& from_str) {
  return NULL;
}

size_t debug_output_func(void  *ptr,  size_t  size,  size_t
			 nmemb,  void  *stream) {
  string data((char*)ptr, size*nmemb);
  DBG("server out: <<%s>>\n", data.c_str());
  return size*nmemb;
}

/** append output to $curl.out */
size_t var_output_func(void  *ptr,  size_t  size,  size_t
			 nmemb,  void  *stream) {
  if (NULL == stream)
    return size*nmemb;

  string data((char*)ptr, size*nmemb);
  DBG("server out: <<%s>>\n", data.c_str());
  DSMSession* sc_sess = reinterpret_cast<DSMSession*>(stream);
  if (sc_sess) {
    sc_sess->var["curl.out"]+=data;
  }
  return size*nmemb;
}

bool curl_run_get(DSMSession* sc_sess, const string& url, 
		  bool get_result) {
  CURL* m_curl_handle = curl_easy_init();
  if (!m_curl_handle) {
    ERROR("getting curl handle\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
    return false;
  }
  
  char* enc_url = curl_easy_escape(m_curl_handle, url.c_str(), url.length());
  if (NULL == enc_url) {
    ERROR("URL-encoding url '%s'\n", url.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    curl_easy_cleanup(m_curl_handle);
    return false;
  }

  if (curl_easy_setopt(m_curl_handle, CURLOPT_URL, url.c_str())
       != CURLE_OK)  {
    ERROR("setting URL '%s'\n", url.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);    
    curl_easy_cleanup(m_curl_handle);
    free(enc_url);
    return false;
  }

  if (!sc_sess->var["curl.timeout"].empty())  {
    unsigned int curl_timeout = 0;
    if (str2i(sc_sess->var["curl.timeout"], curl_timeout)) {
      WARN("curl.timeout '%s' not understood\n", sc_sess->var["curl.timeout"].c_str());
    } else {
      if ((curl_easy_setopt(m_curl_handle, CURLOPT_TIMEOUT, curl_timeout) != CURLE_OK) || 
	  (curl_easy_setopt(m_curl_handle, CURLOPT_NOSIGNAL, 1L) != CURLE_OK)) {
	ERROR("setting timeout '%u'\n", curl_timeout);
	sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
	curl_easy_cleanup(m_curl_handle);
	free(enc_url);
	return false;
      }
    }
  }

  if (!get_result) {
    if (curl_easy_setopt(m_curl_handle, CURLOPT_WRITEFUNCTION, debug_output_func) 
	!= CURLE_OK)  {
      ERROR("setting curl write function\n");
      sc_sess->SET_ERRNO(DSM_ERRNO_FILE);    
      curl_easy_cleanup(m_curl_handle);
      free(enc_url);
      return false;
    }
  } else {
    if ((curl_easy_setopt(m_curl_handle, CURLOPT_WRITEFUNCTION, var_output_func) 
	!= CURLE_OK)||
	(curl_easy_setopt(m_curl_handle, CURLOPT_WRITEDATA, sc_sess) 
	 != CURLE_OK))  {
      ERROR("setting curl write function\n");
      sc_sess->SET_ERRNO(DSM_ERRNO_FILE);    
      curl_easy_cleanup(m_curl_handle);
      free(enc_url);
      return false;
    }
  }

  char curl_err[CURL_ERROR_SIZE];
  curl_err[0]='\0';
  if (curl_easy_setopt(m_curl_handle, CURLOPT_ERRORBUFFER, curl_err)
       != CURLE_OK)  {
    ERROR("setting curl error buffer\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    curl_easy_cleanup(m_curl_handle);
    free(enc_url);
    return false;
  }

  CURLcode rescode = curl_easy_perform(m_curl_handle);

  if (rescode) {
    DBG("Error while trying to retrieve '%s': '%s'\n", 
	url.c_str(), curl_err);
    sc_sess->var["curl.err"] = string(curl_err);
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);    
  } else {
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);    
  }
  
  curl_easy_cleanup(m_curl_handle);
  free(enc_url);
  return false;
}


EXEC_ACTION_START(SCJCurlGetAction) {
  sc_sess->var.erase("curl.out");
  return curl_run_get(sc_sess, resolveVars(arg, sess, sc_sess, event_params), true);
} EXEC_ACTION_END;

EXEC_ACTION_START(SCJCurlGetNoresultAction) {
  return curl_run_get(sc_sess, resolveVars(arg, sess, sc_sess, event_params), false);
} EXEC_ACTION_END;


CONST_ACTION_2P(SCJCurlGetFormAction, ',', true);
EXEC_ACTION_START(SCJCurlGetFormAction) {
  sc_sess->var.erase("curl.out");
  string form_url = resolveVars(par1, sess, sc_sess, event_params);;
  bool url_has_qmark = form_url.find('?')!=string::npos;

  vector<string> p_vars=explode(par2, ";");
  for (vector<string>::iterator it=
	 p_vars.begin();it != p_vars.end();it++) {
    string varname = (it->size() && ((*it)[0]=='$')) ? (it->substr(1)) : (*it);
    DBG("adding '%s' = '%s'\n", varname.c_str(), sc_sess->var[varname].c_str());
    if (!url_has_qmark && it == p_vars.begin()) 
      form_url+= "?";
    else 
      form_url+= "&";
    form_url += varname + "=" + sc_sess->var[varname];
  }

  return curl_run_get(sc_sess, form_url, true);
} EXEC_ACTION_END;

void curl_run_getfile(DSMSession* sc_sess, const string& url, const string& outfile) {
  CURL* m_curl_handle = curl_easy_init();
  if (!m_curl_handle) {
    ERROR("getting curl handle\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_FILE);    
    return;
  }
  
  if (curl_easy_setopt(m_curl_handle, CURLOPT_URL, url.c_str())
       != CURLE_OK)  {
    ERROR("setting URL '%s'\n", url.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);    
    curl_easy_cleanup(m_curl_handle);
    return;
  }

  if (!sc_sess->var["curl.timeout"].empty())  {
    unsigned int curl_timeout = 0;
    if (str2i(sc_sess->var["curl.timeout"], curl_timeout)) {
      WARN("curl.timeout '%s' not understood\n", sc_sess->var["curl.timeout"].c_str());
    } else {
      if ((curl_easy_setopt(m_curl_handle, CURLOPT_TIMEOUT, curl_timeout) != CURLE_OK) || 
	  (curl_easy_setopt(m_curl_handle, CURLOPT_NOSIGNAL, 1L) != CURLE_OK)) {
	ERROR("setting timeout '%u'\n", curl_timeout);
	sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
	curl_easy_cleanup(m_curl_handle);
	return;
      }
    }
  }

  FILE* f = fopen(outfile.c_str(), "wb");
  if (NULL == f) {
    DBG("Error opening file '%s' for writing\n", outfile.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_FILE);    
    return;
  }

  if (curl_easy_setopt(m_curl_handle, CURLOPT_WRITEDATA, f) 
       != CURLE_OK)  {
    ERROR("setting curl data file\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_FILE);    
    fclose(f);
    return;
  }

  char curl_err[CURL_ERROR_SIZE];
  curl_err[0]='\0';
  if (curl_easy_setopt(m_curl_handle, CURLOPT_ERRORBUFFER, curl_err)
       != CURLE_OK)  {
    ERROR("setting URL '%s'\n", url.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    fclose(f);
    return;
  }

  CURLcode rescode = curl_easy_perform(m_curl_handle);

  if (rescode) {
    DBG("Error while trying to retrieve '%s' to '%s': '%s'\n", 
	url.c_str(), outfile.c_str(), curl_err);
    sc_sess->var["curl.err"] = string(curl_err);
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);    
  }else {
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);    
  }

  fclose(f);
  curl_easy_cleanup(m_curl_handle);
}


CONST_ACTION_2P(SCJCurlGetFileAction, ',', true);
EXEC_ACTION_START(SCJCurlGetFileAction) {
  curl_run_getfile(sc_sess, 
		   resolveVars(par1, sess, sc_sess, event_params), 
		   resolveVars(par2, sess, sc_sess, event_params));
} EXEC_ACTION_END;

bool curl_run_post(DSMSession* sc_sess, const string& par1, const string& par2, 
		   bool get_result) {
  CURL* m_curl_handle = curl_easy_init();
  if (!m_curl_handle) {
    ERROR("getting curl handle\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_FILE);    
    return false;
  }
  
  if (curl_easy_setopt(m_curl_handle, CURLOPT_URL, par1.c_str())
       != CURLE_OK)  {
    ERROR("setting URL '%s'\n", par1.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);    
    curl_easy_cleanup(m_curl_handle);
    return false;
  }

  if (!sc_sess->var["curl.timeout"].empty())  {
    unsigned int curl_timeout = 0;
    if (str2i(sc_sess->var["curl.timeout"], curl_timeout)) {
      WARN("curl.timeout '%s' not understood\n", sc_sess->var["curl.timeout"].c_str());
    } else {
      if ((curl_easy_setopt(m_curl_handle, CURLOPT_TIMEOUT, curl_timeout) != CURLE_OK) || 
	  (curl_easy_setopt(m_curl_handle, CURLOPT_NOSIGNAL, 1L) != CURLE_OK)) {
	ERROR("setting timeout '%u'\n", curl_timeout);
	sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
	curl_easy_cleanup(m_curl_handle);
	return false;
      }
    }
  }

  if (!get_result) {
    if (curl_easy_setopt(m_curl_handle, CURLOPT_WRITEFUNCTION, debug_output_func) 
	!= CURLE_OK)  {
      ERROR("setting curl write function\n");
      sc_sess->SET_ERRNO(DSM_ERRNO_FILE);    
      curl_easy_cleanup(m_curl_handle);
      return false;
    }
  } else {
    if ((curl_easy_setopt(m_curl_handle, CURLOPT_WRITEFUNCTION, var_output_func) 
	!= CURLE_OK)||
	(curl_easy_setopt(m_curl_handle, CURLOPT_WRITEDATA, sc_sess) 
	 != CURLE_OK))  {
      ERROR("setting curl write function\n");
      sc_sess->SET_ERRNO(DSM_ERRNO_FILE);    
      curl_easy_cleanup(m_curl_handle);
      return false;
    }    
  }

  char curl_err[CURL_ERROR_SIZE];
  curl_err[0]='\0';
  if (curl_easy_setopt(m_curl_handle, CURLOPT_ERRORBUFFER, curl_err)
       != CURLE_OK)  {
    ERROR("setting curl error buffer\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);
    curl_easy_cleanup(m_curl_handle);
    return false;
  }

  struct curl_httppost *post=NULL;
  struct curl_httppost *last=NULL;
  string post_vars;
  vector<string> p_vars=explode(par2, ";");
  for (vector<string>::iterator it=
	 p_vars.begin();it != p_vars.end();it++) {
    string varname = (it->size() && ((*it)[0]=='$')) ? (it->substr(1)) : (*it);
    DBG("adding '%s' = '%s'\n", varname.c_str(), sc_sess->var[varname].c_str());
    curl_formadd(&post, &last,
		 CURLFORM_COPYNAME, varname.c_str(),
		 CURLFORM_COPYCONTENTS, sc_sess->var[varname].c_str(), CURLFORM_END);
  }

  CURLcode rescode = curl_easy_perform(m_curl_handle);

  bool res = false;
  if (rescode) {
    DBG("Error while trying to POST to '%s': '%s'\n", 
	par1.c_str(), curl_err);
    sc_sess->var["curl.err"] = string(curl_err);
    sc_sess->SET_ERRNO(DSM_ERRNO_GENERAL);    
  } else {
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);    
    res = true;
  }
  curl_formfree(post);
  curl_easy_cleanup(m_curl_handle);
  return false;
}

CONST_ACTION_2P(SCJCurlPOSTAction, ',', true);
EXEC_ACTION_START(SCJCurlPOSTAction) {
  curl_run_post(sc_sess, resolveVars(par1, sess, sc_sess, event_params), 
		par2, false);
  return false;
} EXEC_ACTION_END;

CONST_ACTION_2P(SCJCurlPOSTGetResultAction, ',', true);
EXEC_ACTION_START(SCJCurlPOSTGetResultAction) {
  sc_sess->var.erase("curl.out");
  curl_run_post(sc_sess, resolveVars(par1, sess, sc_sess, event_params), 
		par2, true);
  return false;
} EXEC_ACTION_END;
