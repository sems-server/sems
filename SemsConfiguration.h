/*
 * $Id: SemsConfiguration.h,v 1.2 2003/09/06 11:59:51 ullstar Exp $
 *
 * Copyright (C) 2002-2003 Fhg Fokus
 *
 * This file is part of sems, a free SIP media server.
 *
 * sems is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
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

#ifndef _SEMSCONFIGURATION_H_
#define _SEMSCONFIGURATION_H_

#include <stdio.h>   /* for FILE in get_line */
#include <string>

/** a line in the configuration file may not be longer than this */
#define MAX_CONFIGFILE_LINE_LENGTH 1000

/** configuration file names may be no longer than this */
#define MAX_FILENAME_LENGTH 255

/** module specific configuration prefix */
#define MODULE_CONFIG_PREFIX "config."

/** module specific configuration prefix string length */
#define MODULE_PREFIX_LEN 7

/** indicates module specific configuration is inline, use in config file */
#define MODULE_CONFIG_INLINE "inline"

/** indicates end of inline module specific configuration, use in config file */
#define MODULE_CONFIG_INLINE_END "end"

/** error code for key not found */
#define KEY_NOT_FOUND 2



/** Configuration class for sems. Loads and parses a configuration 
 * file consisting of key value pairs.
 * Any module can inherit SemsConfiguration and overwrite the method check().
 * The subclass should then call reloadModuleConfig(_app_name), after that it
 * can access any module specific parameter with getValueForKey().
 * It is comfortable to "ask" for any valid parameter with this function at
 * least once and the call warnUnknownParams() to warn the user on any 
 * unrecognized or misspelled option.
 * Errors in configuration are best reported with reportConfigError().
 */
class SemsConfiguration
{
	private:
	/** status of the configuration instance, 0=uninit 1=ok */
	int m_status;
	/** 1=the configuration for this module is inlined in master file */
	int m_inlined;

	/* store some facts for later error reporting */
	char m_filename[MAX_FILENAME_LENGTH];
	char m_modulename[MAX_CONFIGFILE_LINE_LENGTH];

	/** struct to store parameters (linked list) */
	struct paramlist {
		char* key;
		char* val;
		int lin; /* line number */
		bool asked;
		struct paramlist* next;
	};
	
	/** list of all parameters from this file */
	struct paramlist* m_plist;

	/** free the parameter list */
	void freeList(struct paramlist* plist);

	/** append a parameter to the parameter list. */
	void addToParameterList(char* key, char* value, int lineno, bool required);

	bool get_line(FILE* configfile, char* line_buffer, int &linenr);

	struct paramlist* getParameterForKey(char* key);

	protected:
	/** Generate an error message for the specified line number */
	void reportConfigError(int line, char *message, bool fatal = true);

	public:
  SemsConfiguration();

	/* (re)load a specific config file or the default config file or inlined specs */
	int reloadFile(const char *filename, char* modulename = NULL);

	/* load module config, return 0=ERROR, 1=SUCCESS or KEY_NOT_FOUND=no config found */
	int reloadModuleConfig(char *modulename);

	/** return the name of the file used */
	char* getFilename();

	/** get key-specific value (original reference), if specified, else NULL */
	char* getValueForKey(char *key);

	/** have all values been asked for (with getValueForKey()) - if not drop warnings */
	void warnUnknownParams();

	/** Generate an error message for the specified key, if fatal, end program */
	void reportConfigError(char *key, char *message, bool fatal = true);

	/** check if configuaration has errors. Inherited classes should overwrite
	 * this method
	 */
	virtual int check();
};
	
/** Globally available SemsConfiguration instance, should be used
 * to access global configuation values.
 */
extern SemsConfiguration semsConfig;

#endif
