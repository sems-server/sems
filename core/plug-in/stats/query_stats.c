#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define MSG_BUF_SIZE 256

void print_usage(const char * progname)
{
    fprintf(stderr,
	    "Syntax: %s ip_address port\n",
	    progname
	);
}


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

char* append_str(const char* buf, const char* str, unsigned int str_s)
{
    memcpy(buf,str,str_s);
    buf += str_s;
}

int main(int argc, char** argv)
{
    char msg_buf[MSG_BUF_SIZE];
    char *p_buf;
    int msg_size=10, sd, err;
    struct sockaddr_in addr;

    if(argc != 3){
	print_usage(argv[0]);
	return -1;
    }
    
    if(!inet_aton(argv[1],&addr.sin_addr)){
	fprintf(stderr,"'%s' is an invalid IP address\n",argv[1]);
	return -1;
    }

    if(str2i(argv[2],&addr.sin_port) == -1){
	fprintf(stderr,"'%s' is not a valid integer\n",argv[2]);
	return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(addr.sin_port);

    /*TODO: build msg to send */
    p_buf = msg_buf;
    p_buf = append_str(p_buf,"calls\n",6);

    
    sd = socket(PF_INET,SOCK_DGRAM,0);
    err = sendto(sd,msg_buf,msg_size,0,
		 (const struct sockaddr*)&addr,
		 sizeof(struct sockaddr_in));
    
    if(err == -1){
	fprintf(stderr,"sendto: %s\n",strerror(errno));
    }
    else {
	msg_size = recv(sd,msg_buf,MSG_BUF_SIZE,0);
	if(msg_size == -1)
	    fprintf(stderr,"recv: %s\n",strerror(errno));
	else
	    printf("received:\n%.*s",msg_size-1,msg_buf);
    }
    
    close(sd);
    return 0;
}
