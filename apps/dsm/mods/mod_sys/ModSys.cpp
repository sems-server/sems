/*
 * Copyright (C) 2008 iptego GmbH
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
#include "ModSys.h"
#include "log.h"
#include "AmUtils.h"

#include "DSMSession.h"
#include "AmSession.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

SC_EXPORT(MOD_CLS_NAME);

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {

  DEF_CMD("sys.mkdir", SCMkDirAction);
  DEF_CMD("sys.mkdirRecursive", SCMkDirRecursiveAction);
  DEF_CMD("sys.rename", SCRenameAction);
  DEF_CMD("sys.unlink", SCUnlinkAction);
  DEF_CMD("sys.unlinkArray", SCUnlinkArrayAction);
  DEF_CMD("sys.tmpnam", SCTmpNamAction);
  DEF_CMD("sys.popen", SCPopenAction);

  DEF_CMD("sys.getTimestamp", SCSysGetTimestampAction);
  DEF_CMD("sys.subTimestamp", SCSysSubTimestampAction);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_BEGIN(MOD_CLS_NAME) {

  if (cmd == "sys.file_exists") {
    return new FileExistsCondition(params, false);
  }

  if (cmd == "sys.file_not_exists") {
    return new FileExistsCondition(params, true);
  }

} MOD_CONDITIONEXPORT_END;

MATCH_CONDITION_START(FileExistsCondition) {
  DBG("checking file '%s'\n", arg.c_str());
  string fname = resolveVars(arg, sess, sc_sess, event_params);
  bool ex =  file_exists(fname);
  DBG("file '%s' %s\n", fname.c_str(), ex?"exists":"does not exist");
  if (inv) {
    DBG("returning %s\n", (!ex)?"true":"false");
    return !ex;
  }  else {
    DBG("returning %s\n", (ex)?"true":"false");
    return ex;
  }
} MATCH_CONDITION_END;

bool sys_mkdir(const char* p) {
  if (mkdir(p,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
    ERROR("mkdir failed for '%s': %s\n", 
	  p, strerror(errno));
    return false;
  }
  return true;
}

EXEC_ACTION_START(SCMkDirAction) {
  string d = resolveVars(arg, sess, sc_sess, event_params);
  DBG("mkdir '%s'\n", d.c_str());
  if (sys_mkdir(d.c_str())) {
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);    
  } else {
    sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
  }
} EXEC_ACTION_END;


bool sys_get_parent_dir(const char* path, char* parentPath) {

  const char* ptr = strrchr(path, '/'); // search char from end reverse
  if (ptr == NULL) {
    ptr = strrchr(path, '\\'); // search char from end reverse
    if (ptr == NULL) {
      return false;
    }
  }
  
  // copy the parent substring to parentPath
  unsigned int i;
  for (i = 0; &(path[i+1]) != ptr; i++) {
    parentPath[i] = path[i];
  }
  parentPath[i] = '\0';
  
  return true;
}

bool sys_mkdir_recursive(const char* p) {
  if (!file_exists(p)) {
    char* parent_dir = new char[strlen(p)+1];
    bool has_parent = sys_get_parent_dir(p, parent_dir);
    if (has_parent) {
      bool parent_exists = sys_mkdir_recursive(parent_dir);
      if (parent_exists) {
	bool ret = sys_mkdir(p);
	delete [] parent_dir;
	return ret;
      }
    }
    delete [] parent_dir;
    return false;
  }
  return true;
}

EXEC_ACTION_START(SCMkDirRecursiveAction) {
  string d = resolveVars(arg, sess, sc_sess, event_params);
  DBG("mkdir recursive '%s'\n", d.c_str());
  if (sys_mkdir_recursive(d.c_str())) {
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);    
  } else {
    sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
  }
} EXEC_ACTION_END;

// copies ifp to ofp, blockwise
void filecopy(FILE* ifp, FILE* ofp) {
  size_t nread;
  char buf[1024];
  
  rewind(ifp);
  while (!feof(ifp)) {
    nread = fread(buf, 1, 1024, ifp);
    if (fwrite(buf, 1, nread, ofp) != nread)
      break;
  }
}

CONST_ACTION_2P(SCRenameAction, ',', true);
EXEC_ACTION_START(SCRenameAction) {
  string src = resolveVars(par1, sess, sc_sess, event_params);
  string dst = resolveVars(par2, sess, sc_sess, event_params);

  int rres = rename(src.c_str(), dst.c_str());
  if (!rres) {
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);    
  } else if (rres == EXDEV) {
    FILE* f1 = fopen(src.c_str(), "r");
    if (NULL == f1) {
      WARN("opening source file '%s' for copying failed: '%s'\n", 
	   src.c_str(), strerror(errno));
      sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
      return false;
    }

    FILE* f2 = fopen(dst.c_str(), "w");
    if (NULL == f2) {
      WARN("opening destination file '%s' for copying failed: '%s'\n", 
	   dst.c_str(), strerror(errno));
      sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
      return false;
    }

    filecopy(f1, f2);
    
    fclose(f1);
    fclose(f2);
    
    if (unlink(src.c_str())) {
      WARN("unlinking source file '%s' for copying failed: '%s'\n", 
	   src.c_str(), strerror(errno));
      sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
      return false;
    } 

    sc_sess->SET_ERRNO(DSM_ERRNO_OK);
  } else {
    WARN("renaming '%s' to '%s' failed: '%s'\n", 
	 src.c_str(), dst.c_str(), strerror(errno));
    sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
  }

} EXEC_ACTION_END;

EXEC_ACTION_START(SCUnlinkAction) {
  string fname = resolveVars(arg, sess, sc_sess, event_params);
  if (fname.empty())
    return false;

  if (!unlink(fname.c_str())) {
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);    
  } else {
    WARN("unlink '%s' failed: '%s'\n", 
	fname.c_str(), strerror(errno));
    sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCUnlinkArrayAction, ',', true);
EXEC_ACTION_START(SCUnlinkArrayAction) {
  string fname = resolveVars(par1, sess, sc_sess, event_params);
  if (fname.empty())
    return false;
  string prefix = resolveVars(par2, sess, sc_sess, event_params);

  unsigned int arr_size = 0;
  if (str2i(sc_sess->var[fname + "_size"], arr_size)) {
    ERROR("_size not present/parseable '$%s'\n", sc_sess->var[fname + "_size"].c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    return false;
  }

  sc_sess->SET_ERRNO(DSM_ERRNO_OK);    
  for (unsigned int i=0;i<arr_size;i++)  {
    
    string file_fullname  = prefix + '/' + sc_sess->var[fname + "_"+int2str(i)];
    DBG("unlinking '%s'\n", file_fullname.c_str());
    if (unlink(file_fullname.c_str())) {
      DBG("unlink '%s' failed: '%s'\n", 
	  file_fullname.c_str(), strerror(errno));
      sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
    }
  }
} EXEC_ACTION_END;

EXEC_ACTION_START(SCTmpNamAction) {
  string varname = resolveVars(arg, sess, sc_sess, event_params);
  char fname[L_tmpnam];
  if (!tmpnam(fname)) {
    ERROR("unique name cannot be generated\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_FILE);
  } else {
    sc_sess->var[varname] = fname;
    sc_sess->SET_ERRNO(DSM_ERRNO_OK);
  }
} EXEC_ACTION_END;

CONST_ACTION_2P(SCPopenAction, '=', false);
EXEC_ACTION_START(SCPopenAction) {
  string dst_var = par1;
  if (dst_var.length() && dst_var[0]=='$')
    dst_var = dst_var.substr(1);

  string cmd = resolveVars(par2, sess, sc_sess, event_params);
  
  DBG("executing '%s' while saving output to $%s\n", 
      cmd.c_str(), dst_var.c_str());

  char buf[100];
  string res;
  FILE* fp = popen(cmd.c_str(), "r");
  if (fp==NULL) {
    throw DSMException("sys", "type", "popen", "cause", strerror(errno));
  }

  size_t rlen;

  while (true) {    
    rlen = fread(buf, 1, 100, fp);
    if (rlen < 100) {
      if (rlen)
	res += string(buf, rlen);
      break;
    }

    res += string(buf, rlen);
  }

  sc_sess->var[dst_var] = res;
  
  int status = pclose(fp);
  if (status==-1) {
    throw DSMException("sys", "type", "pclose", "cause", strerror(errno));
  }
  sc_sess->var[dst_var+".status"] = int2str(status);
  DBG("child process returned status %d\n", status);

} EXEC_ACTION_END;

EXEC_ACTION_START(SCSysGetTimestampAction) {
  string varname = resolveVars(arg, sess, sc_sess, event_params);
  struct timeval tv;
  gettimeofday(&tv, NULL);

  // long unsigned msecs = tv.tv_sec * 1000 + tv.tv_usec / 1000;

  char ms_buf[40];
  snprintf(ms_buf, 40, "%li", tv.tv_sec);
  sc_sess->var[varname+".tv_sec"] = ms_buf;

  snprintf(ms_buf, 40, "%li", (long int)tv.tv_usec);
  sc_sess->var[varname+".tv_usec"] = ms_buf;

  DBG("got timestamp $%s=%s, $%s=%s, \n",
      (varname+".tv_sec").c_str(), sc_sess->var[varname+".tv_sec"].c_str(),
      (varname+".tv_usec").c_str(), sc_sess->var[varname+".tv_usec"].c_str()
      );

} EXEC_ACTION_END;

CONST_ACTION_2P(SCSysSubTimestampAction, ',', false);
EXEC_ACTION_START(SCSysSubTimestampAction) {
  string t1 = resolveVars(par1, sess, sc_sess, event_params);
  string t2 = resolveVars(par2, sess, sc_sess, event_params);

  struct timeval tv1;
  struct timeval tv2;

  tv1.tv_sec = atol(sc_sess->var[t1+".tv_sec"].c_str());
  tv1.tv_usec = atol(sc_sess->var[t1+".tv_usec"].c_str());

  tv2.tv_sec = atol(sc_sess->var[t2+".tv_sec"].c_str());
  tv2.tv_usec = atol(sc_sess->var[t2+".tv_usec"].c_str());

  struct timeval diff;
  timersub(&tv1,&tv2,&diff);

  char ms_buf[40];
  snprintf(ms_buf, 40, "%li", diff.tv_sec);
  sc_sess->var[t1+".tv_sec"] = ms_buf;

  snprintf(ms_buf, 40, "%li", (long int)diff.tv_usec);
  sc_sess->var[t1+".tv_usec"] = ms_buf;

  // may be overflowing - use only if timestamps known
  snprintf(ms_buf, 40, "%lu", diff.tv_sec * 1000 + diff.tv_usec / 1000);
  sc_sess->var[t1+".msec"] = ms_buf;

  DBG("sub $%s = %s,  $%s = %s,  $%s = %s\n",
      (t1+".tv_sec").c_str(), sc_sess->var[t1+".tv_sec"].c_str(),
      (t1+".tv_usec").c_str(), sc_sess->var[t1+".tv_usec"].c_str(),
      (t1+".msec").c_str(), sc_sess->var[t1+".msec"].c_str()
      );
} EXEC_ACTION_END;
