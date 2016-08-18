#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <hiredis.h>
#include "confparse.h"

int main (int argc, char **argv) {
  cfg_t *cfg;
  redisContext *c ;
  redisReply *reply;
  /* Localize messages & types according to environment, since v2.9 */
  setlocale(LC_MESSAGES, "");
  setlocale(LC_CTYPE, "");

  /* Read configuration */
  cfg = parse_conf(argc > 1 ? argv[1] : "bone-shiftout.conf");
  if (!cfg) {
    fprintf(stderr,"Error parsing configuration file!\n");
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
  printf("subscribed...\n");
  reply = redisCommand(c, "SUBSCRIBE changes");
  freeReplyObject(reply);
  while(redisGetReply(c,(void *)&reply) == REDIS_OK) {
    printf("subscribe returned\n");
    // consume message
    if (reply == NULL) 
      continue;
    printf("reply ok %u \n", reply->type);
    if (reply->type == REDIS_REPLY_ARRAY) {
      for (int j = 0; j < reply->elements; j++) {
        printf("%u) %s\n", j, reply->element[j]->str);
      }
    }
    freeReplyObject(reply);
  }
  cfg_free(cfg);
}
