#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <hiredis.h>
#include "confparse.h"
#include "io.h"

int main (int argc, char **argv) {
  cfg_t *cfg;
  redisContext *c ;
  redisReply *reply;
  int iofd;
  /* Localize messages & types according to environment, since v2.9 */
  setlocale(LC_MESSAGES, "");
  setlocale(LC_CTYPE, "");

  /* Read configuration */
  cfg = parse_conf(argc > 1 ? argv[1] : "bone-shiftout.conf");
  if (!cfg) {
    fprintf(stderr,"Error parsing configuration file!\n");
    return 1;
  }
  /* Open remoteproc device */
  if((iofd = io_init(cfg_getstr(cfg,"pru-remoteproc-file"))) < 0){
    fprintf(stderr,"Failed to open remoteproc device file\n");
    return 1;
  }
  /* Set output channel count */
  if(io_set_nchannels(iofd, cfg_getint(cfg,"num-channels")) < 0){
    io_close(iofd);
    return 1;
  }
  /* Establish Redis connection */
  printf("Connecting to redis at %s, port %ld\n", cfg_getstr(cfg,"redis-host"), cfg_getint(cfg,"redis-port"));
  c = redisConnect(cfg_getstr(cfg,"redis-host"), cfg_getint(cfg,"redis-port"));
  /* Handle Redis connection errors */
  if (c == NULL || c->err) {
    if (c) {
      fprintf(stderr,"Error: %s\n", c->errstr);
    } else {
      fprintf(stderr,"Can't allocate redis context\n");
    }
    return 1;
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
    return 0;
  }
  }
  /* Message handling loop */
  char *respcpy, *ascval;
  unsigned int chan, val, start, nchan;
  start = cfg_getint(cfg,"start-address");
  nchan = cfg_getint(cfg,"num-channels");
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
            if(chan >= start && chan < (start + nchan)){
              printf("COMMAND: channel %d -> %d\n", chan, val);
              printf("IO Command: address %d -> %d\n", chan - start, val);
              //TODO Error handling and || output
              io_set_pwm(iofd, 0, chan-start, val);
            } else {
              printf("Channel %d is not on our board, ignoring.\n", chan);
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
  io_close(iofd);
  cfg_free(cfg);
}
