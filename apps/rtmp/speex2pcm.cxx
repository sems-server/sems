#include <string.h>

#include <speex/speex.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>
#include <stdint.h>

#define FRAME_SIZE 320

int main(int argc, char** argv)
{
  if(argc != 3){
    fprintf(stderr,"error: missing argument (%i)\n",argc);
    fprintf(stderr,"%s input_file output_file\n",argv[0]);
    return -1;
  }
  
  int in,out;
  in = open(argv[1],O_RDONLY);
  if(in<0){
    fprintf(stderr,"could not open input file '%s': %s\n",
	    argv[1],strerror(errno));
    return -1;
  }
  
  out = open(argv[2],O_WRONLY|O_CREAT|O_TRUNC,
	     S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
  if(out<0){
    fprintf(stderr,"could not open output file '%s': %s\n",
	    argv[2],strerror(errno));
    close(in);
    return -1;
  }

  int ret=0,read;
  unsigned int in_len=0,out_len=0;
  uint32_t pkg_len;
  char buffer[512];
  spx_int16_t output[FRAME_SIZE];

  SpeexBits bits;
  void *dec_state;

  speex_bits_init(&bits);
  dec_state = speex_decoder_init(&speex_wb_mode);

  while(true){
    read = ::read(in,&pkg_len,sizeof(uint32_t));
    if(read < 0){
      fprintf(stderr,"could not read packet size: %s\n",strerror(errno));
      goto error;
    }
    else if(read == 0) {
      // EOF reached
      break;
    }
    else if(read != sizeof(uint32_t)) {
      fprintf(stderr,"incomplete packet size read (%i)\n",read);
      goto error;
    }

    if(pkg_len >= 512){
      fprintf(stderr,"buffer too small!!! (pkg_len=%u)\n",pkg_len);
      goto error;
    }

    read = ::read(in,buffer,pkg_len);
    if(read < 0){
      fprintf(stderr,"could not read packet: %s\n",strerror(errno));
      goto error;
    }
    else if((uint32_t)read != pkg_len) {
      fprintf(stderr,"incomplete packet read (%i != %u)\n",read,pkg_len);
      goto error;
    }

    in_len += pkg_len;
    speex_bits_read_from(&bits, buffer, pkg_len);
    int err = speex_decode_int(dec_state, &bits, output);
    
    switch(err){
    case 0:
      // no error
      write(out,output,sizeof(output));
      out_len += sizeof(output);
      break;

    case -1:
      fprintf(stderr,"End-of-stream\n");
      goto loop_exit;

    case -2:
      fprintf(stderr,"Corrupt stream\n");
      goto loop_exit;
    }
  }
  
 loop_exit:
  printf("in:\t%8u bytes\n",in_len);
  printf("out:\t%8u bytes\n",out_len);
  goto end;

 error:
  ret = -1;

 end:
  close(in);
  close(out);

  return ret;
}


