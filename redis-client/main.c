#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <hiredis.h>
#include "confparse.h"
#include "io.h"
#define max(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })

int summation(unsigned int start, unsigned int end, int *list){
  int x, acc;
  acc=0;
  for(x=start;x<=end;x++){
    acc += list[x];
  }
  return acc;
}

int setChannelIfOurs(int iofd, int chan, int val, int startaddr, int *pinsperchan, int nchan){
  int i, brkaddr;
  //Does this channel belong to us? If so, update!
  if(chan < startaddr){
    printf("channel %d belongs to a board at lower address.\n", chan);
    return -1;
  }

  for(i=nchan-1; i>=0; i++){
    brkaddr = summation(0,i,pinsperchan);
    if(chan < (startaddr + brkaddr)){
      printf("Setting pin %d, global channel %d, local channel %d to %d\n", i, chan, chan-startaddr-brkaddr, val);  
      if(io_set_pwm(iofd, i, chan-startaddr-brkaddr, val) >= 0)
        return chan;
      else
        return -2;
    }
  }
  printf("channel %d belongs to a board at higher address.\n",chan);
  return -1;
}

int mainLoop (char *confpath) {
  cfg_t *cfg;
  redisContext *c ;
  redisReply *reply;
  int iofd, retval=-1;
  int outbpc[3];
  /* Localize messages & types according to environment, since v2.9 */
  setlocale(LC_MESSAGES, "");
  setlocale(LC_CTYPE, "");

  /* Read configuration */
  cfg = parse_conf(confpath == NULL ? "bone-shiftout.conf" : confpath);
  if (!cfg) {
    fprintf(stderr,"Error parsing configuration file!\n");
    goto cleanupnone;
  }
  outbpc[0] = cfg_getint(cfg,"ser0-num-channels");
  outbpc[1] = cfg_getint(cfg,"ser1-num-channels");
  outbpc[2] = cfg_getint(cfg,"ser2-num-channels");

  /* Open remoteproc device */
  if((iofd = io_init(cfg_getstr(cfg,"pru-remoteproc-file"))) < 0){
    fprintf(stderr,"Failed to open remoteproc device file\n");
    goto cleanupconf;
  }
  /* Set output channel count */
  if(io_set_nchannels(iofd, max(cfg_getint(cfg,"ser0-num-channels"),max(cfg_getint(cfg,"ser1-num-channels"),cfg_getint(cfg,"ser2-num-channels"))) < 0)){
    goto cleanupio;
  }
  /* Establish Redis connection */
  printf("Connecting to redis at %s, port %ld\n", cfg_getstr(cfg,"redis-host"), cfg_getint(cfg,"redis-port"));
  c = redisConnect(cfg_getstr(cfg,"redis-host"), cfg_getint(cfg,"redis-port"));
  /* Handle Redis connection errors */
  if (c == NULL || c->err) {
    if (c) {
      fprintf(stderr,"Error: %s\n", c->errstr);
      goto cleanupall;
    } else {
      fprintf(stderr,"Can't allocate redis context\n");
      goto cleanupio;
    }
  }
  /* Authenticate if needed */
  if(cfg_getbool(cfg,"redis-authrequired")){
    const char authpref[] = "AUTH ";
    char *authstr = malloc(sizeof(authpref) + strlen(cfg_getstr(cfg,"redis-password")) + 1);
    memcpy(authstr, authpref, sizeof(authpref));
    strcat(authstr, cfg_getstr(cfg,"redis-password"));
    reply = redisCommand(c, authstr);
    free(authstr);
    if(!reply || reply->type == REDIS_REPLY_ERROR){
      fprintf(stderr,"authentication failed\n");
      goto cleanupall;
    }
  }
  /* If we have gotten here we have initialized successfully */
  retval=0;
  /* Message handling loop */
  char *respcpy, *ascval;
  unsigned int chan, val, start;
  start = cfg_getint(cfg,"start-address");
  printf("Subscribed...\n");
  reply = redisCommand(c, "SUBSCRIBE changes");
  freeReplyObject(reply);
  while(redisGetReply(c,(void *)&reply) == REDIS_OK) {
    printf("subscribe returned\n");
    // consume message
    if (reply == NULL) 
      continue;
    if (reply->type == REDIS_REPLY_ARRAY) {
      if(reply->elements == 3) {
        if(reply->element[0]->type == REDIS_REPLY_STRING && reply->element[2]->type == REDIS_REPLY_STRING && strcmp("message", reply->element[0]->str) == 0){
          printf("Processing incoming message %s\n", reply->element[2]->str);
          //Message parsing
          respcpy = malloc(strlen(reply->element[2]->str)+1);
          assert(respcpy != NULL);
          strcpy(respcpy, reply->element[2]->str);
          ascval = strsep(&respcpy, ":");
          if(respcpy != NULL){
            chan = atoi(ascval);
            val = atoi(respcpy);
            //Does this channel belong to us? If so, update!
            if(setChannelIfOurs(iofd, chan, val, start, outbpc, 3) == -2){
              /* IO error, bail out! */
              fprintf(stderr, "IO error, bailing!\n");
              freeReplyObject(reply);
              free(ascval);
              goto cleanupall;
            }
          } else {
            fprintf(stderr, "Malformed message published to channel!\n");
          }
          free(ascval);
        }
      } else {
        fprintf(stderr, "Invalid number of elements in reply array!\n");
      }
    }
    freeReplyObject(reply);
  }
  fprintf(stderr, "Lost connection to redis server!\n");
cleanupall:
  redisFree(c);
cleanupio:
  io_close(iofd);
cleanupconf:
  cfg_free(cfg);
cleanupnone:
  return retval;
}

int main (int argc, char **argv) {
  char *confpath=NULL;
  int rv;
  if(argc > 2){
    confpath = argv[1];
  }
  while(1){
    printf("Beaglebone PWM shift register redis client starting...\n");
    rv = mainLoop(confpath); 
    fprintf(stderr,"Main loop exited with code %d, restarting in 5 seconds\n", rv);
    usleep(5E6);
  }
  return 0;
}
