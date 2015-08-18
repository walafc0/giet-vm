#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>



extern volatile int run_threads;
extern volatile long heap_inited;

extern unsigned int nclusters_x;
extern unsigned int nclusters_y;
extern unsigned int procs_per_cluster;


#define printf(...) giet_tty_printf(__VA_ARGS__)

extern const char *optarg;

int getopt (int argc, char *const *argv, const char *optstring);
void exit (int status);
void clusterXY (int tid, unsigned int *cx, unsigned int *cy);
void *ocean_malloc (unsigned long s, int tid);
unsigned int getLpid ();

/** pseudo fprintf **/
typedef char FILE;
extern FILE *stdout;
extern FILE *stderr;
#define fprintf(prologue, ...)	printf("%s", prologue); \
									printf(__VA_ARGS__);

#endif
