/* Définitions des fonctions standard (simplifiées) utilisées par ocean pour GIET */

#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>

EXTERN_ENV

#include "decs.h"
#include "giet_utils.h"

FILE * stdout = "";
FILE *stderr = "STDERR : ";

extern double ****main_q_multi;
extern double ****main_rhs_multi;
extern double ****main_psi;
extern double ****main_psim;
extern double ***main_psium;
extern double ***main_psilm;
extern double ***main_psib;
extern double ***main_ga;
extern double ***main_gb;
extern double ***main_oldga;
extern double ***main_oldgb;
extern double ****main_work1;
extern double ***main_work2;
extern double ***main_work3;
extern double ****main_work4;
extern double ****main_work5;
extern double ***main_work6;
extern double ****main_work7;
extern long *main_imx;
extern long *main_jmx;

extern double *main_lev_res;
extern double *main_lev_tol;
extern double *main_i_int_coeff;
extern double *main_j_int_coeff;
extern long *main_xpts_per_proc;
extern long *main_ypts_per_proc;
extern long main_xprocs;
extern long main_yprocs;
extern long main_numlev;
extern double main_eig2;
extern long main_im;
extern long main_jm;

double ****work1 __attribute__ ((section("seg_ldata")));
double ***work2 __attribute__ ((section("seg_ldata")));
double ***work3 __attribute__ ((section("seg_ldata")));
double ****work4 __attribute__ ((section("seg_ldata")));
double ****work5 __attribute__ ((section("seg_ldata")));
double ***work6 __attribute__ ((section("seg_ldata")));
double ****work7 __attribute__ ((section("seg_ldata")));
double ****psi __attribute__ ((section("seg_ldata")));
double ****psim __attribute__ ((section("seg_ldata")));
double ***psium __attribute__ ((section("seg_ldata")));
double ***psilm __attribute__ ((section("seg_ldata")));
double ***psib __attribute__ ((section("seg_ldata")));
double ***ga __attribute__ ((section("seg_ldata")));
double ***gb __attribute__ ((section("seg_ldata")));
double ***oldga __attribute__ ((section("seg_ldata")));
double ***oldgb __attribute__ ((section("seg_ldata")));
double ****q_multi __attribute__ ((section("seg_ldata")));
double ****rhs_multi __attribute__ ((section("seg_ldata")));
long *imx __attribute__ ((section("seg_ldata")));
long *jmx __attribute__ ((section("seg_ldata")));
double *f __attribute__ ((section("seg_ldata")));
struct Global_Private *gp;

double *lev_res __attribute__ ((section("seg_ldata")));
double *lev_tol __attribute__ ((section("seg_ldata")));
double *i_int_coeff __attribute__ ((section("seg_ldata")));
double *j_int_coeff __attribute__ ((section("seg_ldata")));
long *xpts_per_proc __attribute__ ((section("seg_ldata")));
long *ypts_per_proc __attribute__ ((section("seg_ldata")));
long xprocs __attribute__ ((section("seg_ldata")));
long yprocs __attribute__ ((section("seg_ldata")));
long numlev __attribute__ ((section("seg_ldata")));
double eig2 __attribute__ ((section("seg_ldata")));
long im __attribute__ ((section("seg_ldata")));
long jm __attribute__ ((section("seg_ldata")));

unsigned int nclusters_x __attribute__ ((section("seg_ldata")));
unsigned int nclusters_y __attribute__ ((section("seg_ldata")));
unsigned int procs_per_cluster __attribute__ ((section("seg_ldata")));

volatile long heap_inited = 0;
volatile int run_threads = 0;

//Entry point for all threads (except main)
//  waiting allocs and inits of main then copy read-only tabs in ldata segment (replicated)
//  some read-write tabs are also replicated, but not entirely : only pointers
__attribute__ ((constructor)) void thread()
{
    unsigned long size;
    long id = (long) giet_thread_id();

    unsigned int cx, cy, lp;

    giet_proc_xyp(&cx, &cy, &lp);
    giet_tty_printf("Thread %d (%d:%d.%d) waiting\n", id, cx, cy, lp);

    if (lp == 0) {
        
        giet_procs_number(&nclusters_x, &nclusters_y, &procs_per_cluster);
        heap_init(cx, cy);

        while (heap_inited != id) {
            asm volatile ("nop\r\n");
        }
        heap_inited += procs_per_cluster;
        
        
        size = nprocs * sizeof(double ***);
        rhs_multi = (double ****) G_MALLOC(size, id);
        q_multi = (double ****) G_MALLOC(size, id);
        psi = (double ****) G_MALLOC(size, id);
        psim = (double ****) G_MALLOC(size, id);
        work1 = (double ****) G_MALLOC(size, id);
        work4 = (double ****) G_MALLOC(size, id);
        work5 = (double ****) G_MALLOC(size, id);
        work7 = (double ****) G_MALLOC(size, id);
        
        size = nprocs * sizeof(double **);
        psium = (double ***) G_MALLOC(size, id);
        psilm = (double ***) G_MALLOC(size, id);
        psib = (double ***) G_MALLOC(size, id);
        ga = (double ***) G_MALLOC(size, id);
        gb = (double ***) G_MALLOC(size, id);
        oldga = (double ***) G_MALLOC(size, id);
        oldgb = (double ***) G_MALLOC(size, id);
        work2 = (double ***) G_MALLOC(size, id);
        work3 = (double ***) G_MALLOC(size, id);
        work6 = (double ***) G_MALLOC(size, id);
    }
    
    while (run_threads != 1) {
        asm volatile ("nop\r\n");
    }

    *gp[id].lpid = lp;
        
    if (lp == 0) {
        int i, j, k;
        
        xprocs = main_xprocs;
        yprocs = main_yprocs;
        numlev = main_numlev;
        eig2 = main_eig2;
        im = main_im;
        jm = main_jm;

        size = numlev * sizeof(long);
        imx = (long *) G_MALLOC(size, id);
        jmx = (long *) G_MALLOC(size, id);
        xpts_per_proc = (long *) G_MALLOC(size, id);
        ypts_per_proc = (long *) G_MALLOC(size, id);
        
        size = numlev * sizeof(double);
        lev_res = (double *) G_MALLOC(size, id);
        lev_tol = (double *) G_MALLOC(size, id);
        i_int_coeff = (double *) G_MALLOC(size, id);
        j_int_coeff = (double *) G_MALLOC(size, id);
        
        for(i=0;i<numlev;i++) {
            imx[i] = main_imx[i];
            jmx[i] = main_jmx[i];
            lev_res[i] = main_lev_res[i];
            lev_tol[i] = main_lev_tol[i];
            i_int_coeff[i] = main_i_int_coeff[i];
            j_int_coeff[i] = main_j_int_coeff[i];
            xpts_per_proc[i] = main_xpts_per_proc[i];
            ypts_per_proc[i] = main_ypts_per_proc[i];
        }
        
        size = numlev * sizeof(double **);        
        for (i = 0; i < nprocs; i++) {
            
            q_multi[i] = (double ***) G_MALLOC(size, id);
            rhs_multi[i] = (double ***) G_MALLOC(size, id);

            for (j = 0; j < numlev; j++) {
            
                rhs_multi[i][j] = (double **) G_MALLOC(((imx[j] - 2) / yprocs + 2) * sizeof(double *), id);
                q_multi[i][j] = (double **) G_MALLOC(((imx[j] - 2) / yprocs + 2) * sizeof(double *), id);
                for (k = 0; k < ((imx[j] - 2) / yprocs + 2); k++) {
                    q_multi[i][j][k] = main_q_multi[i][j][k];
                    rhs_multi[i][j][k] = main_rhs_multi[i][j][k];
                }
                
            }
            
            work1[i] = main_work1[i];
            work2[i] = main_work2[i];
            work3[i] = main_work3[i];
            work4[i] = main_work4[i];
            work5[i] = main_work5[i];
            work6[i] = main_work6[i];
            work7[i] = main_work7[i];
            psi[i] = main_psi[i];
            psim[i] = main_psim[i];
            psium[i] = main_psium[i];
            psilm[i] = main_psilm[i];
            psib[i] = main_psib[i];
            ga[i] = main_ga[i];
            gb[i] = main_gb[i];
            oldga[i] = main_oldga[i];
            oldgb[i] = main_oldgb[i];
        }
    }
    giet_tty_printf("Thread %d launched\n", id);

    slave(&id);

    BARRIER(bars->barrier, nprocs)
    
    giet_exit("done.");
}


const char *optarg;

int getopt(int argc, char *const *argv, const char *optstring)
{
    return -1;
}

//give the cluster coordinate by thread number
//  if tid=-1, return the next cluster (round robin)
void clusterXY(int tid, unsigned int *cx, unsigned int *cy)
{
    unsigned int cid;
    static unsigned int x = 0, y = 0;

    cid = tid / procs_per_cluster;

    if (tid != -1) {
        *cx = (cid / nclusters_y);
        *cy = (cid % nclusters_y);
        return;
    }
    
    if (giet_thread_id() != 0) {
        giet_exit("pseudo-random mapped malloc : thread 0 only");
    }

    x++;
    if (x == nclusters_x) {
        x = 0;
        y++;
        if (y == nclusters_y) {
            y = 0;
        }
    }
    *cx = x;
    *cy = y;
}

void *ocean_malloc(unsigned long s, int tid)
{
    void *ptr;
    unsigned int x, y;
    clusterXY(tid, &x, &y);
    ptr = remote_malloc(s, x, y);
    giet_assert (ptr != 0, "Malloc failed");
    return ptr;
}

void exit(int status)
{
    if (status) {
        giet_exit("Done (status != 0)");
    } else {
        giet_exit("Done (ok)");
    }
}
