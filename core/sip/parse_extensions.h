#ifndef __PARSE_EXTENSIONS_H__
#define __PARSE_EXTENSIONS_H__

#include <string>

enum {
    SIP_EXTENSION_100REL = 1<<0,
#define SIP_EXTENSION_100REL    SIP_EXTENSION_100REL
    SIP_EXTENSION_TIMER = 1<<1,
#define SIP_EXTENSION_TIMER     SIP_EXTENSION_TIMER
    SIP_EXTENSION_REPLACES = 1<<2,
#define SIP_EXTENSION_REPLACES  SIP_EXTENSION_REPLACES
};

bool parse_extensions(unsigned *extensions, const char *start, int len);

/**
 * Register a SIP extension as supported.
 * Called at startup by core and by plugins in onLoad().
 */
void register_supported_extension(const std::string& option_tag);

/**
 * Check if a SIP extension option-tag is supported.
 */
bool is_extension_supported(const std::string& option_tag);

/**
 * Given a Require header value (comma-separated option-tags),
 * return a comma-separated string of unsupported option-tags.
 * Returns empty string if all are supported.
 */
std::string get_unsupported_extensions(const char* require_value, unsigned int len);

#endif /* __PARSE_EXTENSIONS_H__ */
