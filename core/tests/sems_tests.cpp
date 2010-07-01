
#include "log.h"




int main(int argc, char* argv[])
{
  init_logging();
  log_stderr = true;
  log_level = 3;
  DBG("yeah!\n");
  return 0;
}
