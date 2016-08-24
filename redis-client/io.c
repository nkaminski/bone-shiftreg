#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <commands.h>

int io_init(const char *devname){
  int fd;
  fd = open(devname, O_RDWR);
  if(fd == -1)
  {
    perror("Open Failed");
  }
  return fd;
}
int io_set_nchannels(int fd, uint16_t channels){
  char cmd[COMMAND_LEN];
  ssize_t rv;
  cmd[0] = SET_NBITS;
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    cmd[1] = (channels);
    cmd[2] = (channels >> 8);
  #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    cmd[2] = (channels);
    cmd[1] = (channels >> 8);
  #else
  #error "Unsupported hardware byte order"
  #endif
  rv = write(fd, cmd, COMMAND_LEN);
  
  if(rv == -1)
  {
    perror("Write failed while setting number of channels");
  }
return rv;
}

int io_set_pwm(int fd, uint8_t chainaddr, uint8_t pinaddr, uint8_t value){
  char cmd[COMMAND_LEN];
  ssize_t rv;
  switch(chainaddr){
  case 0:
    cmd[0] = SET_PWM0;
    break;
  case 1:
    cmd[0] = SET_PWM1;
    break;
  case 2:
    cmd[0] = SET_PWM2;
    break;
  default:
    fprintf(stderr, "Invalid chain address provided to io_set_pwm(), Ignoring!\n");
    return 0;
  }
  cmd[1] = pinaddr;
  cmd[2] = value;
  
  rv = write(fd, cmd, COMMAND_LEN);
  
  if(rv == -1)
  {
    perror("Write failed while setting channel value");
  }
return rv;
}

void io_close(int fd){
  if(fd > 2){
    close(fd);
  }
}    
