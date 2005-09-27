/*
 * $Id: SemsConfiguration.cpp,v 1.3 2003/12/04 10:31:14 ullstar Exp $
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

#include "SemsConfiguration.h"
#include "log.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>

/** Globally available instance of SemsConfiguration */
SemsConfiguration semsConfig;
		
/** Constructor. The configuration is not read here yet. Use reloadFile() or 
 * reloadModuleConfig() to actually initialize the configuration */
SemsConfiguration::SemsConfiguration() {
	m_status = 0; /* not initialized yet */
	m_inlined = 0; /* no inline config */
	m_plist = NULL; /* no params yet */
	m_modulename[0] = '\0'; /* no module */
}

/** (Re-)loads the configuration for a plugin module.
 * This method can be used to load the configuration initially 
 * or to reload the configuration while sems keeps running.
 * The configuration for this module must be inlined in or referenced from the
 * (already initialized!) main configuration.
 * @return 1 on success
 * @return 0 if there are errors in configuration, configuration is not
 * in a valid state if this occurs.
 */
int SemsConfiguration::reloadModuleConfig(char* modulename) {
	assert(modulename != NULL);
	assert(strlen(modulename) < MAX_CONFIGFILE_LINE_LENGTH - 10);
	strcpy(m_modulename, modulename);
	/* first look for a module configuration entry in the main config */
	char parameter[MAX_CONFIGFILE_LINE_LENGTH]; 
	strcpy(parameter, MODULE_CONFIG_PREFIX);
	strcat(parameter, modulename);
	char* value = semsConfig.getValueForKey(parameter);
	if (value == NULL) {
		WARN("no configuration found for module %s, maybe you want to "
		 "specify %s in config file.\n", modulename, parameter);
		return KEY_NOT_FOUND;
	}	else {
		if (strcmp(value, MODULE_CONFIG_INLINE) == 0) {
			DBG("using inline configuration for module %s\n", modulename);
			return reloadFile(semsConfig.getFilename(), modulename);
		} else {
			DBG("reading configuration for module %s from %s\n", modulename, value);
			return reloadFile(value);
		}	
	}
}

/** (Re-)loads the configuration file. This method can be used to
 * load the configuration initially or to reload the configuration
 * while sems keeps running.
 * @return 1 on success
 * @return 0 if there are errors in configuration, configuration is not
 * in a valid state if this occurs.
 */
int SemsConfiguration::reloadFile(const char* filename, char* modulename) {
	FILE* file;		   /* file pointer for config file */
	char line_buffer[MAX_CONFIGFILE_LINE_LENGTH+1];		/* holds the current line */
	char c;          /* last read character */
	bool readequal;  /* did we read = already ? */
	char* local_pointer; /* current position in line */
	char* value;     /* holds the current value string */
	int line;        /* current line number */
	char inModule[MAX_CONFIGFILE_LINE_LENGTH]; /* the module currently in(line) */
	bool amInModule; /* inside a module def? */
	
	// start in global context
	inModule[0] = '\0';
	amInModule = false;

	// open configuration file
	file = fopen(filename, "r");
	if (!file) {
	        ERROR("could not read config file '%s': %s\n", filename, strerror(errno));
	        return 0;
	}

	if (strlen(filename) > MAX_FILENAME_LENGTH) {
		ERROR("filename too long\n");
		return 0;
	}	
	strcpy(m_filename, filename);
	
	/* free the parameter list, NULL is ok too */
	freeList(m_plist);
	m_plist = NULL;

	#define cfail(reason) { ERROR("configuration error in " \
			"%s:%i, %s\n", filename, line, reason); exit (1); }
	
	while (get_line(file, line_buffer, line)) {
		local_pointer = line_buffer;
		/* no "=" yet */
		readequal = false;
		while ((c = *(local_pointer++)) != '\0') {
			/* read = twice ? */
			if ((c == '=') && readequal) cfail("must be key=value");
			if  (c == '=') {
				readequal = true;
				*(local_pointer-1) = '\0';
				value = local_pointer;
			}
		}	
		if (!readequal) cfail("must be key=value");  /* no "=" in line -> error */
//		DBG("module: %s key: >>%s<< value: >>%s<<\n", inModule, line_buffer, value);

#define addParameter(required) \
		{ \
			if (getParameterForKey(line_buffer) != NULL) { \
			ERROR("%s:%i %s configuration error: duplicate declaration for %s\n", \
				m_filename, line, m_modulename, line_buffer); \
			reportConfigError(line_buffer, "this is the previous location", true); \
		}	\
		addToParameterList(line_buffer, value, line, required); }
	
		// check if this is a inline module configuration
		if ((strncmp(line_buffer, MODULE_CONFIG_PREFIX, MODULE_PREFIX_LEN) == 0) 
				&& (strcmp(value, MODULE_CONFIG_INLINE) == 0)) {
			if (amInModule)
				reportConfigError(line, "inline module definitions may not be nested", true);
			// inline config start
			strcpy(inModule, line_buffer+MODULE_PREFIX_LEN);
			amInModule = true;
			addParameter(false);
//			DBG("entered inline module configuration for %s\n", inModule);
			continue; // don't add this "pseudo"-option to the list
		}	

		// check if this is the end of an inline module configuration
		if ((strncmp(line_buffer, MODULE_CONFIG_PREFIX, MODULE_PREFIX_LEN) == 0) 
				&& (strcmp(value, MODULE_CONFIG_INLINE_END) == 0)) {
			// inline config ends
			if (!amInModule)
				reportConfigError(line, "cannot end module inline definition in global context", true);
			if (strcmp(inModule, line_buffer+MODULE_PREFIX_LEN) == 0) {
				// end of current inline definition
//				DBG("leaving inline module configuration for %s\n", inModule);
				inModule[0] = '\0';
				amInModule = false;
				continue; // skip this pseudo option
			}	else {
				reportConfigError(line, "module inline configuration end does not match start", true);
			}	
		}	
		
		/* global context && global context requested */
		if ( (!amInModule) && (modulename == NULL)) {
			addParameter(true);
		} else if (modulename != NULL) {
			if ((inModule != NULL) && (strcmp(inModule, modulename) == 0)) {
					// yep, that parameter is wanted 
					addParameter(true);
			}	
		}	
	}	
	
	m_status = 1;
	fclose (file);

	return check();
}

/** append a parameter to the parameter list. */
void SemsConfiguration::addToParameterList(char* key, char* value, int lineno, bool required) {
	/* create a parameter to insert */
	struct paramlist* param = (struct paramlist*) malloc( sizeof(struct paramlist) );
	if (param == NULL) {
		ERROR("out of memory\n");
		exit(ENOMEM);
	}
	param->key = (char *) malloc( strlen(key) + 1 );
	param->val = (char *) malloc( strlen(value) + 1 );
	if  ( (param->key == NULL) || (param->val == NULL) ) {
		ERROR("out of memory\n");
		exit(ENOMEM);
	}
	strcpy(param->key, key);
	strcpy(param->val, value);
	param->lin = lineno;
	param->next = NULL;
	param->asked = !required;
	
	/* find end of list and append param */
	if (m_plist == NULL) {
		m_plist = param;
	} else {
		struct paramlist* cur = m_plist;
		while (cur->next != NULL) 
			cur = cur->next;
		cur->next = param;
	}	
}	

/** Get the value for specified key. A reference to the stored value is returned. */
char* SemsConfiguration::getValueForKey(char* key) {
	struct paramlist* cur = getParameterForKey(key);
	if (cur == NULL) 
		return NULL;
	else {
		cur->asked = true;
		return cur->val;
	}	
}		
	
/** Get parameter for specified key. A reference to the stored value is returned. */
struct SemsConfiguration::paramlist* SemsConfiguration::getParameterForKey(char* key) {
	/* go through list and compare */
	struct paramlist* cur = m_plist;
	while (cur != NULL) {
		if ((cur->key != NULL) && (strcmp(cur->key, key) == 0))
			return cur;
		cur = cur->next;
	}	
	return NULL;		
}	

/** Report an error on a specific configuration key or (if key=NULL) a generic configuration
 * error.
 */
void SemsConfiguration::reportConfigError(char* key, char* message, bool fatal) {
	struct paramlist* cur = NULL;
	int l = -1;
	if (key != NULL) {
		cur = getParameterForKey(key);
		if (cur == NULL) 
			ERROR("internal error: could not find location of key %s\n", key);
	}	
	if (cur != NULL)
		l = cur->lin;
	reportConfigError(l, message, fatal);
}	

/** Report an error on a line number.
 */
void SemsConfiguration::reportConfigError(int line, char* message, bool fatal) {
	if (line == -1) {
		if (fatal) {
			ERROR("%s %s configuration: %s\n", m_filename, m_modulename, message);
		} else {
			WARN("%s %s configuration: %s\n", m_filename, m_modulename, message);
		}	
	}	else {
		if (fatal) {
			ERROR("%s:%i %s configuration: %s\n", m_filename, line, m_modulename, message);
		} else {
			WARN("%s:%i %s configuration: %s\n", m_filename, line, m_modulename, message);
		}	
	}
	if (fatal) exit(-1);
}

	
/** Completely frees the parameter list using recursion
 * @param plist the start of the list to be freed
 */
void SemsConfiguration::freeList (struct paramlist* plist) {
	if (!plist)
		return;
	freeList(plist->next);
	if (plist->key)
		free (plist->key);
	if (plist->val)
		free (plist->val);
	free(plist);
}	

/** return the filename of the current config file */
char* SemsConfiguration::getFilename() {
	return m_filename;
}	

/** Print a warning for every parameter, that has not been polled
 * with getValueForKey() yet.
 */
void SemsConfiguration::warnUnknownParams () {
	struct paramlist* cur = m_plist;
	while (cur) {
		if ((!cur->asked) && 
			( strncmp(cur->key, MODULE_CONFIG_PREFIX, MODULE_PREFIX_LEN) != 0)) {
			char buf[MAX_CONFIGFILE_LINE_LENGTH + 30];
			sprintf(buf,"ignoring unknown parameter %s", cur->key);
			reportConfigError(cur->key, buf, false);
		}
		cur = cur->next;
	}	
}	

/** Get next line in configuration. Blank lines, whitespaces and comments (#)
 * are skipped. 
 * @param file file descriptor of open config file
 * @param buffer buffer to hold the line 
 * @param linenr current line number
 * @return true if line is in buffer 
 * @return false if EOF is reached, no new line is in buffer 
 */ 
bool SemsConfiguration::get_line(FILE* file, char* buffer, int &linenr) {
	char* local_pointer;	/* local pointer to the line buffer */
	bool ignore;          /* we have already read a hash mark (comment) */
	bool have_line;       /* is there something valuable in this line ? */
	char c;               /* single character read from config file */
	static int line_number = 0; /* current line number */

	ignore = false;
	have_line = false;
	c = 'A';              /* for outer while (just to make shure) */
	
	while ( (c != EOF) && (!have_line) ) {  /* run until we have a line or EOF */
		local_pointer = buffer;               /* start to write at buffer start */
		while ((c = fgetc(file)) != EOF) {
			if (c == '#')  /* ignore comments */
				ignore = true;
			if (c == '\n') {
				line_number++;
				break;		/* line done */
			}	
			/* ignore comments and strip whitespaces */	
			if (ignore || (c == ' ') || (c == '	'))
				continue; /* continue reading without adding to line */
			if (local_pointer - buffer > MAX_CONFIGFILE_LINE_LENGTH) {
				ERROR("configuration error (line longer than %i)\n", MAX_CONFIGFILE_LINE_LENGTH);
				exit (1);
			}	
			*(local_pointer++) = c;
			have_line = true;  /* we have something useful in this line */
		}
		ignore = false;        /* comments end at EOL */
		*local_pointer = '\0';  /* terminate string buffer */
	}	
	linenr = line_number;
	if (c == EOF) line_number = 0;
	return have_line;
}	

/** Check if parameters are correct. This method must be overwritten by modules
 * configuration implementations. Perform all possible checks on the loaded 
 * configuration, like:
 * - all needed parameters exist
 * - parameter values are within range
 */
int SemsConfiguration::check() {

	return 1;
}
