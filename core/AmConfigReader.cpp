#include "AmConfigReader.h"
#include "log.h"
#include "AmUtils.h"

#include <errno.h>

#define IS_SPACE(c) ((c == ' ') || (c == '\t'))

#define IS_EOL(c) ((c == '\0')||(c == '#'))

#define TRIM(s) \
          do{ \
              while( IS_SPACE(*s) ) s++; \
          }while(false)

int  AmConfigReader::loadFile(const string& path)
{
    FILE* fp = fopen(path.c_str(),"r");
    if(!fp){
	ERROR("could not open configuration file '%s': %s\n",
	      path.c_str(),strerror(errno));
	return -1;
    }

    int  lc = 0;
    int  ls = 0;
    char lb[MAX_CONFIG_LINE] = {'\0'};

    char *c,*key_beg,*key_end,*val_beg,*val_end;

    c=key_beg=key_end=val_beg=val_end=0;
    while(!feof(fp) && ((ls = fifo_get_line(fp, lb, MAX_CONFIG_LINE)) != -1)){
	
	c=key_beg=key_end=val_beg=val_end=0;
	lc++;

	c = lb;
	TRIM(c);

	if(IS_EOL(*c)) continue;

	key_beg = c;
	while( (*c != '=') && !IS_SPACE(*c) ) c++;
	
	key_end = c;
	if(IS_SPACE(*c))
	    TRIM(c);
	else if( !(c - key_beg) )
	    goto syntax_error;

	if(*c != '=')
	    goto syntax_error;

	c++;
	TRIM(c);

	if(*c == '"'){

	    val_beg = ++c;

	    while( (*c != '"') && (*c != '\0') ) c++;

	    if(*c == '\0')
		goto syntax_error;

	    val_end = c;
	}
	else {
	    val_beg = c;

	    while( !IS_EOL(*c) && !IS_SPACE(*c) ) c++;

	    val_end = c;
	}

	if((key_beg < key_end) && (val_beg < val_end))
	    keys[string(key_beg,key_end-key_beg)] = 
		string(val_beg,val_end-val_beg);
	else
	    goto syntax_error;
    }

    fclose(fp);
    return 0;

 syntax_error:
    ERROR("syntax error line %i in %s\n",lc,path.c_str());
    fclose(fp);
    return -1;
}

bool AmConfigReader::hasParameter(const string& param)
{
    return (keys.find(param) != keys.end());
}

const string& AmConfigReader::getParameter(const string& param, const string& defval)
{
    map<string,string>::iterator it = keys.find(param);
    if(it == keys.end())
	return defval;
    else
	return it->second;
}

unsigned int AmConfigReader::getParameterInt(const string& param, unsigned int defval)
{
    unsigned int result=0;
    if(str2i(getParameter(param),result))
	return defval;
    else
	return result;
}
