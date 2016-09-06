#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <hiredis.h>
#include "confparse.h"
#include "io.h"

#define NUMPINS 3
#define DEFSTATE "state/default"
#undef HARDSTOP

char *statestr;
unsigned char debug=0;

int imax(int x, int y){
	if(x>y)
		return x;
	return y;
}

int summation(unsigned int start, unsigned int end, int *list){
	int x, acc;
	acc=0;
	for(x=start;x<=end;x++){
		acc += list[x];
	}
	return acc;
}

int setChannelIfOurs(int iofd, int chan, int val, int startaddr, int *pinsperchan, int nchan){
	int i, brkaddr, prevbrk;
	//Does this channel belong to us? If so, update!
	if(chan < startaddr){
		printf("channel %d belongs to a board at lower address.\n", chan);
		return -1;
	}
	prevbrk = startaddr;
	for(i=0; i<nchan; i++){
		brkaddr = summation(0,i, pinsperchan);
		if(chan >= prevbrk && chan < brkaddr){
			if(debug)
				fprintf(stderr,"Setting pin %d, global channel %d, local channel %d to %d\n", i, chan, chan-startaddr+prevbrk, val);  
			if(io_set_pwm(iofd, i, chan-startaddr+prevbrk, val) >= 0)
				return chan;
			else
				return -2;
			prevbrk=brkaddr;
		}
	}
	if(debug)
		fprintf(stderr,"channel %d belongs to a board at higher address.\n",chan);
	return -1;
}


int restoreState(redisContext *c, int iofd, int startaddr, int maxchan, int *pinsperchan, int nchan){
	char chanstr[16];
	char *querystr;
	size_t size;
	int i;
	unsigned char bval;
	redisReply *reply;
	if(debug)
		fprintf(stderr,"Getting state prefix\n");
	reply = redisCommand(c, "GET curstate");
	if(reply && reply->type != REDIS_REPLY_ERROR){
		if(reply->type == REDIS_REPLY_STRING){
			free(statestr);
			size =strlen(reply->str)+1; 
			statestr=malloc(size);
			strcpy(statestr,reply->str);
		}
		freeReplyObject(reply);
	} else {
		fprintf(stderr,"Failed to retreive current state\n");
	}
	if(debug) 
		fprintf(stderr,"Restoring channel state '%s' from %d to %d\n",statestr,startaddr,startaddr+maxchan); 
	for(i=startaddr; i<(startaddr+maxchan); i++){
		snprintf(chanstr,16,"%d",i);
		size = sizeof("HGET  ") + strlen(statestr) + strlen(chanstr);
		querystr = malloc(size);
		strcpy(querystr, "HGET ");
		strcat(querystr, statestr);
		strcat(querystr," ");
		strcat(querystr, chanstr);
		if(debug)
			fprintf(stderr, "Query is %s\n",querystr);
		reply = redisCommand(c, querystr);
		free(querystr);
		if(reply){
			bval=0;
			if(reply->type == REDIS_REPLY_INTEGER){
				bval = reply->integer;
			} else if(reply->type == REDIS_REPLY_STRING){
				bval = atoi(reply->str);
			}
			if(setChannelIfOurs(iofd, i, bval, startaddr, pinsperchan, NUMPINS) == -2){  
				fprintf(stderr,"I/O error setting channel %d\n",i);
				freeReplyObject(reply);
				return -2;
			}

			freeReplyObject(reply);
		} else if(debug) {         
			fprintf(stderr,"No saved state for channel %d\n",i);
		}
	}
	return i;
}

int mainLoop (char *confpath) {
	cfg_t *cfg;
	redisContext *c;
	redisReply *reply;
	char *respcpy, *ascval, *curtok;
	unsigned int chan, val, start;
	int iofd, retval=-1, maxchain, numchan;
	int outbpc[3];
	/* Localize messages & types according to environment, since v2.9 */
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");

	/* Read configuration and initialize to default state*/
	statestr=malloc(sizeof(DEFSTATE)+1);
	strcpy(statestr,DEFSTATE);

	cfg = parse_conf(confpath == NULL ? "/etc/bone-shiftreg.conf" : confpath);
	if (!cfg) {
		fprintf(stderr,"Error parsing configuration file!\n");
		retval=-2;
		goto cleanupnone;
	}
	outbpc[0] = cfg_getint(cfg,"ser0-num-channels");
	outbpc[1] = cfg_getint(cfg,"ser1-num-channels");
	outbpc[2] = cfg_getint(cfg,"ser2-num-channels");
	if(cfg_getbool(cfg,"debug")){
		debug = 1;
	} else {
		debug = 0;
	}
	start = cfg_getint(cfg,"start-address");
	numchan = summation(0,NUMPINS-1, outbpc);
	printf("This board has a total of %d channels\n", numchan);

	/* Open remoteproc device */
	if((iofd = io_init(cfg_getstr(cfg,"pru-remoteproc-file"))) < 0){
		fprintf(stderr,"Failed to open remoteproc device file! Firmware still booting?\n");
		retval = -1;
		goto cleanupconf;
	}
	/* Set output channel count */
	maxchain = imax(imax(outbpc[0],outbpc[1]),outbpc[2]);
	if(maxchain == 0){
		fprintf(stderr,"At least one output chain must have more then zero channels!\n");
		goto cleanupio;
	}

	if(io_set_nchannels(iofd, maxchain) < 0){
		fprintf(stderr,"Failed to set max chain length to %d!\n", maxchain);
		retval = -2;
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
		if(reply)
			freeReplyObject(reply);
	}
	reply = redisCommand(c, "PING");
	if(!reply || reply->type == REDIS_REPLY_ERROR){
		fprintf(stderr,"Unable to execute Redis commands, are you authenticated if necessary?\n");
		if(reply)
			freeReplyObject(reply);
		goto cleanupall;
	} else { 
		freeReplyObject(reply);
	}
	/* If we have gotten here we have initialized successfully */
	/* Restore state */ 
	if(cfg_getbool(cfg,"persistent-state")){
		if(restoreState(c, iofd, start, start+numchan, outbpc, NUMPINS) == -2){
			retval = -2;
			goto cleanupall;
		}
	}
	retval=0;
	/* Message handling loop */
	if(debug)
		fprintf(stderr,"Subscribed...\n");
	reply = redisCommand(c, "SUBSCRIBE changes");
	freeReplyObject(reply);
	while(redisGetReply(c,(void *)&reply) == REDIS_OK) {
		if(debug)
			fprintf(stderr,"Begin message parsing\n");
		// consume message
		if (reply == NULL) 
			continue;
		if (reply->type == REDIS_REPLY_ARRAY) {
			if(reply->elements == 3) {
				if(reply->element[0]->type == REDIS_REPLY_STRING && reply->element[2]->type == REDIS_REPLY_STRING && strcmp("message", reply->element[0]->str) == 0){
					if(debug)  
						fprintf(stderr,"Processing incoming message %s\n", reply->element[2]->str);
					//Message parsing
					respcpy = malloc(strlen(reply->element[2]->str)+1);
					assert(respcpy != NULL);
					strcpy(respcpy, reply->element[2]->str);
					#ifdef HARDSTOP
					if(strstr(respcpy, "@")){
						//Reload requested, cleanup and return 0 (which causes a reload)
						free(respcpy);
						freeReplyObject(reply);
						retval = -2;
						goto cleanupall;
					}
					#endif
					if(strstr(respcpy, "!")){
						//Reload requested, cleanup and return 0 (which causes a reload)
						free(respcpy);
						freeReplyObject(reply);
						goto cleanupall;
					}
					curtok = strtok(respcpy,",");
					while(curtok != NULL){
						ascval = strsep(&curtok, ":");
						if(curtok != NULL){
							chan = atoi(ascval);
							val = atoi(curtok);
							//Does this channel belong to us? If so, update!
							if(setChannelIfOurs(iofd, chan, val, start, outbpc, NUMPINS) == -2){
								/* IO error, bail out! */
								fprintf(stderr, "IO error, bailing!\n");
								freeReplyObject(reply);
								free(respcpy);
								goto cleanupall;
							}
						} else {
							fprintf(stderr, "Malformed message published to channel!\n");
						}
						curtok = strtok(NULL,",");
					}
					free(respcpy);
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
	free(statestr);
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
		fprintf(stderr,"Main loop exited with code %d\n", rv);
		if(rv == -2)
			return 1;
		if(rv < 0)
			usleep(5E6);
	}
	return 0;
}
