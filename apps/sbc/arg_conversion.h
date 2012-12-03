#ifndef __ARG_CONVERSION
#define __ARG_CONVERSION

#include "AmArg.h"

std::string arg2username(const AmArg &a);
bool username2arg(const std::string &src, AmArg &dst);

#endif
