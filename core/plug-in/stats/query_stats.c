
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <map>
#include <string>
using std::string;

#define MSG_BUF_SIZE 2048

void print_usage(const char * progname)
{
  fprintf(stderr,
	  "Syntax: %s [<options>]\n"
	  "\n"
	  "where <options>: \n"
	  " -s <server>  : server name|ip (default: 127.0.0.1)\n"
	  " -p <port>    : server port (default: 5040)\n"
	  " -c <cmd>     : command (default: calls)\n"
	  "\n"
	  "Tips: \n"
	  " o quote the command if it has arguments (e.g. %s -c \"set_loglevel 1\")\n"
	  " o \"which\" prints available commands\n"
	  ,
	  progname, progname
	  );
}

static int parse_args(int argc, char* argv[], const string& flags,
		      const string& options, std::map<char,string>& args);


/* returns non-zero if error occured */
int str2i(const char* str, unsigned short* result)
{
  int i=0;
  unsigned short ret=0;
  const char* init=str;

  for(; (*str != '\0') && (*str == ' '); str++);

  for(; *str != '\0';str++){
    if ( (*str <= '9' ) && (*str >= '0') ){
      ret=ret*10+*str-'0';
      i++;
      if (i>10) goto error_digits;
    } 
    else
      goto error_char;
  }

  *result = ret;
  return 0;

 error_digits:
  fprintf(stderr,"str2i: too many letters in [%s]\n", init);
  return -1;
 error_char:
  fprintf(stderr,"str2i: unexpected char %c in %s\n", *str, init);
  return -1;
}

int main(int argc, char** argv)
{
  char rcv_buf[MSG_BUF_SIZE];
  string  msg_buf;
  int sd, err;
  struct sockaddr_in addr;
    
  std::map<char,string> args;

  if(parse_args(argc, argv, "h","spc", args)){
    print_usage(argv[0]);
    return -1;
  }

  string server="127.0.0.1";
  string port="5040";
  string cmd="calls";

  for(std::map<char,string>::iterator it = args.begin(); 
      it != args.end(); ++it){
		
    if(it->second.empty())
      continue;
		
    switch( it->first ){
    case 'h': { print_usage(argv[0]); exit(1); } break;
    case 's': { server=it->second; } break;
    case 'p': { port=it->second; } break;
    case 'c': { cmd=it->second; } break;
    }
  }

  if(!inet_aton(server.c_str(),&addr.sin_addr)){
    fprintf(stderr,"server '%s' is an invalid IP address\n",server.c_str());
    return -1;
  }

  if(str2i(port.c_str(),&addr.sin_port) == -1){
    fprintf(stderr,"port '%s' is not a valid integer\n",port.c_str());
    return -1;
  }


  addr.sin_family = AF_INET;
  addr.sin_port = htons(addr.sin_port);

  msg_buf = cmd;    
  printf("sending '%s\\n' to %s:%s\n", msg_buf.c_str(), 
	 server.c_str(), port.c_str());

  msg_buf += "\n";

  sd = socket(PF_INET,SOCK_DGRAM,0);
  err = sendto(sd,msg_buf.c_str(),msg_buf.length(),0,
	       (const struct sockaddr*)&addr,
	       sizeof(struct sockaddr_in));
    
  if(err == -1){
    fprintf(stderr,"sendto: %s\n",strerror(errno));
  }
  else {
    int msg_size = recv(sd,rcv_buf,MSG_BUF_SIZE,0);
    if(msg_size == -1)
      fprintf(stderr,"recv: %s\n",strerror(errno));
    else
      printf("received:\n%.*s",msg_size-1,rcv_buf);
  }
    
  close(sd);
  return 0;
}


static int parse_args(int argc, char* argv[],
		      const string& flags,
		      const string& options,
		      std::map<char,string>& args)
{
  for(int i=1; i<argc; i++){

    char* arg = argv[i];

    if( (*arg != '-') || !*(++arg) ) { 
      fprintf(stderr,"%s: invalid parameter: '%s'\n",argv[0],argv[i]);
      return -1;
    }    

    if( flags.find(*arg) != string::npos ) {
	    
      args[*arg] = "yes";
    }
    else if(options.find(*arg) != string::npos) {

      if(!argv[++i]){
	fprintf(stderr,"%s: missing argument for parameter '-%c'\n",argv[0],*arg);
	return -1;
      }
	    
      args[*arg] = argv[i];
    }
    else {
      fprintf(stderr,"%s: unknown parameter '-%c'\n",argv[0],arg[1]);
      return -1;
    }
  }
  return 0;
}
