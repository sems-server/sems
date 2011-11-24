#ifndef _MSG_STORAGE_API_H
#define _MSG_STORAGE_API_H

#include <stdio.h>

#define MSG_OK                  0
#define MSG_EMSGEXISTS          1
#define MSG_EUSRNOTFOUND        2
#define MSG_EMSGNOTFOUND        3
#define MSG_EALREADYCLOSED      4
#define MSG_EREADERROR          5
#define MSG_ENOSPC              6
#define MSG_ESTORAGE            7

#include "AmArg.h"

class MessageDataFile 
: public AmObject {
 public:
  FILE* fp;
  MessageDataFile(FILE* fp) 
    : fp(fp) { }
};

#endif
