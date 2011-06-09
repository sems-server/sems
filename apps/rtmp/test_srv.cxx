#include <string.h>

#include "RtmpServer.h"
#include "librtmp/log.h"

void print_usage(char* prog)
{
  fprintf(stderr,"%s [-z]\n",prog);
}


int main(int argc, char** argv)
{
  RTMP_LogSetLevel(RTMP_LOGDEBUG);

  if(argc>2){
    fprintf(stderr,"Too many arguments");
    goto error;
  }
  else if(argc>1){
    if((strlen(argv[1]) != sizeof("-z")-1) ||
       strncmp(argv[1],"-z",sizeof("-z")-1)) {
      fprintf(stderr,"Unknown argument '%s'\n",argv[1]);
      goto error;
    }
    RTMP_LogSetLevel(RTMP_LOGALL);
  }

  RtmpServer::instance()->start();
  RtmpServer::instance()->join();

  return 0;

 error:
  print_usage(argv[0]);
  return -1;
}
