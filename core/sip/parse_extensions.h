#ifndef __PARSE_EXTENSIONS_H__
#define __PARSE_EXTENSIONS_H__


enum {
    SIP_EXTENSION_100REL = 1<<0,
#define SIP_EXTENSION_100REL    SIP_EXTENSION_100REL
};

bool parse_extensions(unsigned *extensions, const char *start, int len);

#endif /* __PARSE_EXTENSIONS_H__ */
