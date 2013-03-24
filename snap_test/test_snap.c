#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "snapshot.h"


int main(int argc, char **argv){
	fprintf(stderr, "calling snapshot\n");

	int i;
	int n = 8;
	enum snap_event events[n];
	int device[n];
	enum snap_trig triggers[n];

	for(i = 0; i < n; i++){
		events[i] = SNAP_EVENT_CBS_SCHED;
		device[i] = 0;
		triggers[i] = SNAP_TRIG_BEDGE;
	}

	for(i = 0; i < n; i += 2){
		triggers[i] = SNAP_TRIG_AEDGE;
	}

	if(snapshot(events, device, triggers, n) != 0){
		perror("SNAPERROR:");
	}
	snap_join();
	fprintf(stderr, "exiting\n");
	return 0;
}