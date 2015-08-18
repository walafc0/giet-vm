/*************************************************************************/
/*                                                                       */
/*  Copyright (c) 1994 Stanford University                               */
/*                                                                       */
/*  All rights reserved.                                                 */
/*                                                                       */
/*  Permission is given to use, copy, and modify this software for any   */
/*  non-commercial purpose as long as this copyright notice is not       */
/*  removed.  All other uses, including redistribution in whole or in    */
/*  part, are forbidden without prior written permission.                */
/*                                                                       */
/*  This software is provided with absolutely no warranty and no         */
/*  support.                                                             */
/*                                                                       */
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/*  SPLASH Ocean Code                                                    */
/*                                                                       */
/*  This application studies the role of eddy and boundary currents in   */
/*  influencing large-scale ocean movements.  This implementation uses   */
/*  dynamically allocated four-dimensional arrays for grid data storage. */
/*                                                                       */
/*  Command line options:                                                */
/*                                                                       */
/*     -mM : Simulate MxM ocean. M must be (power of 2) +2.              */
/*     -nN : N = number of threads. N must be power of 2.                */
/*     -eE : E = error tolerance for iterative relaxation.               */
/*     -rR : R = distance between grid points in meters.                 */
/*     -tT : T = timestep in seconds.                                    */
/*     -s  : Print timing statistics.                                    */
/*     -o  : Print out relaxation residual values.                       */
/*     -h  : Print out command line options.                             */
/*                                                                       */
/*  Default: OCEAN -m130 -n1 -e1e-7 -r20000.0 -t28800.0                  */
/*                                                                       */
/*  NOTE: This code works under both the FORK and SPROC models.          */
/*                                                                       */
/*************************************************************************/

MAIN_ENV

#define DEFAULT_M        514
#define DEFAULT_N        4
#define DEFAULT_E        1e-7
#define DEFAULT_T    28800.0
#define DEFAULT_R    20000.0
#define UP               0
#define DOWN             1
#define LEFT             2
#define RIGHT            3
#define UPLEFT           4
#define UPRIGHT          5
#define DOWNLEFT         6
#define DOWNRIGHT        7
#define PAGE_SIZE     4096

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "decs.h"

struct multi_struct *multi;
struct global_struct *global;
struct locks_struct *locks;
struct bars_struct *bars;

struct Global_Private *main_gp;
double ****main_psi;
double ****main_psim;
double ***main_psium;
double ***main_psilm;
double ***main_psib;
double ***main_ga;
double ***main_gb;
double ****main_work1;
double ***main_work2;
double ***main_work3;
double ****main_work4;
double ****main_work5;
double ***main_work6;
double ****main_work7;
double ***main_oldga;
double ***main_oldgb;
double ****main_q_multi;
double ****main_rhs_multi;
double ****temparray;
double ***tauz;
long *main_imx;
long *main_jmx;

long nprocs = DEFAULT_N;
const double h1 = 1000.0;
const double h3 = 4000.0;
const double h = 5000.0;
const double lf = -5.12e11;
double res = DEFAULT_R;
double dtau = DEFAULT_T;
const double f0 = 8.3e-5;
const double beta = 2.0e-11;
const double gpr = 0.02;
double ysca;
long oim;
long jmm1;
double tolerance = DEFAULT_E;
const double pi = 3.141592653589793;
const double t0 = 0.5e-4;
const double outday0 = 1.0;
const double outday1 = 2.0;
const double outday2 = 2.0;
const double outday3 = 2.0;
const double maxwork = 10000.0;
double factjacob;
double factlap;

//TODO : répliquer ça :
double *main_lev_res;
double *main_lev_tol;
double *main_i_int_coeff;
double *main_j_int_coeff;
long *main_xpts_per_proc;
long *main_ypts_per_proc;
long main_xprocs;
long main_yprocs;
long main_numlev;
double main_eig2;
long main_im = DEFAULT_M;
long main_jm;

long minlevel;
long do_stats = 1;
long do_output = 0;
long *ids_procs;


__attribute__ ((constructor)) int main(int argc, char *argv[])
{
    long i;
    long j;
    long k;
    long x_part;
    long y_part;
    long d_size;
    long itemp;
    long jtemp;
    double procsqrt;
    long temp = 0;
    double min_total;
    double max_total;
    double avg_total;
    double avg_wait;
    double max_wait;
    double min_wait;
    double min_multi;
    double max_multi;
    double avg_multi;
    double min_frac;
    double max_frac;
    double avg_frac;
    long imax_wait;
    long imin_wait;
    long ch;
    unsigned long long computeend;
    unsigned long long start;
    im = main_im;
    
    CLOCK(start);

    while ((ch = getopt(argc, argv, "m:n:e:r:t:soh")) != -1) {
        switch (ch) {
        case 'm':
            im = atoi(optarg);
            if (log_2(im - 2) == -1) {
                printerr("Grid must be ((power of 2)+2) in each dimension\n");
                exit(-1);
            }
            break;
        case 'n':
            nprocs = atoi(optarg);
            if (nprocs < 1) {
                printerr("N must be >= 1\n");
                exit(-1);
            }
            if (log_2(nprocs) == -1) {
                printerr("N must be a power of 2\n");
                exit(-1);
            }
            break;
        case 'e':
            tolerance = atof(optarg);
            break;
        case 'r':
            res = atof(optarg);
            break;
        case 't':
            dtau = atof(optarg);
            break;
        case 's':
            do_stats = !do_stats;
            break;
        case 'o':
            do_output = !do_output;
            break;
        case 'h':
            printf("Usage: ocean <options>\n\n");
            printf("options:\n");
            printf("  -mM : Simulate MxM ocean.  M must be (power of 2) + 2 (default = %d).\n", DEFAULT_M);
            printf("  -nN : N = number of threads. N must be power of 2 (default = %d).\n", DEFAULT_N);
            printf("  -eE : E = error tolerance for iterative relaxation (default = %f).\n", DEFAULT_E);
            printf("  -rR : R = distance between grid points in meters (default = %f).\n", DEFAULT_R);
            printf("  -tT : T = timestep in seconds (default = %f).\n", DEFAULT_T);
            printf("  -s  : Print timing statistics.\n");
            printf("  -o  : Print out relaxation residual values.\n");
            printf("  -h  : Print out command line options.\n\n");
            exit(0);
            break;
        }
    }

    MAIN_INITENV
    
    jm = im;

    printf("\n");
    printf("Ocean simulation with W-cycle multigrid solver\n");
    printf("    Processors                         : %1ld\n", nprocs);
    printf("    Grid size                          : %1ld x %1ld\n", im, jm);
    printf("    Grid resolution (meters)           : %0.2f\n", res);
    printf("    Time between relaxations (seconds) : %0.0f\n", dtau);
    printf("    Error tolerance                    : %0.7g\n", tolerance);
    printf("\n");

    xprocs = 0;
    yprocs = 0;

    procsqrt = sqrt((double) nprocs);
    j = (long) procsqrt;

    while ((xprocs == 0) && (j > 0)) {
        k = nprocs / j;
        if (k * j == nprocs) {
            if (k > j) {
                xprocs = j;
                yprocs = k;
            } else {
                xprocs = k;
                yprocs = j;
            }
        }
        j--;
    }

    if (xprocs == 0) {
        printerr("Could not find factors for subblocking\n");
        exit(-1);
    }

    minlevel = 0;
    itemp = 1;
    jtemp = 1;
    numlev = 0;
    minlevel = 0;

    while (itemp < (im - 2)) {
        itemp = itemp * 2;
        jtemp = jtemp * 2;
        if ((itemp / yprocs > 1) && (jtemp / xprocs > 1)) {
            numlev++;
        }
    }

    if (numlev == 0) {
        printerr("Must have at least 2 grid points per processor in each dimension\n");
        exit(-1);
    }

    main_imx = (long *) G_MALLOC(numlev * sizeof(long), 0);
    main_jmx = (long *) G_MALLOC(numlev * sizeof(long), 0);
    main_lev_res = (double *) G_MALLOC(numlev * sizeof(double), 0);
    main_lev_tol = (double *) G_MALLOC(numlev * sizeof(double), 0);
    main_i_int_coeff = (double *) G_MALLOC(numlev * sizeof(double), 0);
    main_j_int_coeff = (double *) G_MALLOC(numlev * sizeof(double), 0);
    main_xpts_per_proc = (long *) G_MALLOC(numlev * sizeof(long), 0);
    main_ypts_per_proc = (long *) G_MALLOC(numlev * sizeof(long), 0);
    ids_procs = (long *) G_MALLOC(nprocs * sizeof(long), 0);
    
    imx = main_imx;
    jmx = main_jmx;
    lev_res = main_lev_res;
    lev_tol = main_lev_tol;
    i_int_coeff = main_i_int_coeff;
    j_int_coeff = main_j_int_coeff;
    xpts_per_proc = main_xpts_per_proc;
    ypts_per_proc = main_ypts_per_proc;

    for (i = 0; i < nprocs; i++) {
        ids_procs[i] = i;
    }

    imx[numlev - 1] = im;
    jmx[numlev - 1] = jm;
    lev_res[numlev - 1] = res;
    lev_tol[numlev - 1] = tolerance;

    for (i = numlev - 2; i >= 0; i--) {
        imx[i] = ((imx[i + 1] - 2) / 2) + 2;
        jmx[i] = ((jmx[i + 1] - 2) / 2) + 2;
        lev_res[i] = lev_res[i + 1] * 2;
    }

    for (i = 0; i < numlev; i++) {
        xpts_per_proc[i] = (jmx[i] - 2) / xprocs;
        ypts_per_proc[i] = (imx[i] - 2) / yprocs;
    }
    for (i = numlev - 1; i >= 0; i--) {
        if ((xpts_per_proc[i] < 2) || (ypts_per_proc[i] < 2)) {
            minlevel = i + 1;
            break;
        }
    }

    for (i = 0; i < numlev; i++) {
        temp += imx[i];
    }
    temp = 0;
    j = 0;
    for (k = 0; k < numlev; k++) {
        for (i = 0; i < imx[k]; i++) {
            j++;
            temp += jmx[k];
        }
    }

    d_size = nprocs * sizeof(double ***);
    main_psi = (double ****) G_MALLOC(d_size, 0);
    main_psim = (double ****) G_MALLOC(d_size, 0);
    main_work1 = (double ****) G_MALLOC(d_size, 0);
    main_work4 = (double ****) G_MALLOC(d_size, 0);
    main_work5 = (double ****) G_MALLOC(d_size, 0);
    main_work7 = (double ****) G_MALLOC(d_size, 0);
    temparray = (double ****) G_MALLOC(d_size, -1);

    psi = main_psi;
    psim = main_psim;
    work1 = main_work1;
    work4 = main_work4;
    work5 = main_work5;
    work7 = main_work7;

    d_size = 2 * sizeof(double **);
    for (i = 0; i < nprocs; i++) {
        psi[i] = (double ***) G_MALLOC(d_size, i);
        psim[i] = (double ***) G_MALLOC(d_size, i);
        work1[i] = (double ***) G_MALLOC(d_size, i);
        work4[i] = (double ***) G_MALLOC(d_size, i);
        work5[i] = (double ***) G_MALLOC(d_size, i);
        work7[i] = (double ***) G_MALLOC(d_size, i);
        temparray[i] = (double ***) G_MALLOC(d_size, i);
    }

    d_size = nprocs * sizeof(double **);
    main_psium = (double ***) G_MALLOC(d_size, 0);
    main_psilm = (double ***) G_MALLOC(d_size, 0);
    main_psib = (double ***) G_MALLOC(d_size, 0);
    main_ga = (double ***) G_MALLOC(d_size, 0);
    main_gb = (double ***) G_MALLOC(d_size, 0);
    main_work2 = (double ***) G_MALLOC(d_size, 0);
    main_work3 = (double ***) G_MALLOC(d_size, 0);
    main_work6 = (double ***) G_MALLOC(d_size, 0);
    tauz = (double ***) G_MALLOC(d_size, 0);
    main_oldga = (double ***) G_MALLOC(d_size, 0);
    main_oldgb = (double ***) G_MALLOC(d_size, 0);

    psium = main_psium;
    psilm = main_psilm;
    psib = main_psib;
    ga = main_ga;
    gb = main_gb;
    work2 = main_work2;
    work3 = main_work3;
    work6 = main_work6;
    oldga = main_oldga;
    oldgb = main_oldgb;

    main_gp = (struct Global_Private *) G_MALLOC((nprocs + 1) * sizeof(struct Global_Private), -1);
    gp = main_gp;

    for (i = 0; i < nprocs; i++) {
        gp[i].pad = (char *) G_MALLOC(PAGE_SIZE * sizeof(char), i);
        gp[i].rel_num_x = (long *) G_MALLOC(numlev * sizeof(long), i);
        gp[i].rel_num_y = (long *) G_MALLOC(numlev * sizeof(long), i);
        gp[i].eist = (long *) G_MALLOC(numlev * sizeof(long), i);
        gp[i].ejst = (long *) G_MALLOC(numlev * sizeof(long), i);
        gp[i].oist = (long *) G_MALLOC(numlev * sizeof(long), i);
        gp[i].ojst = (long *) G_MALLOC(numlev * sizeof(long), i);
        gp[i].rlist = (long *) G_MALLOC(numlev * sizeof(long), i);
        gp[i].rljst = (long *) G_MALLOC(numlev * sizeof(long), i);
        gp[i].rlien = (long *) G_MALLOC(numlev * sizeof(long), i);
        gp[i].rljen = (long *) G_MALLOC(numlev * sizeof(long), i);
        gp[i].neighbors = (long *) G_MALLOC(8 * sizeof(long), i);
        gp[i].rownum = (long *) G_MALLOC(sizeof(long), i);
        gp[i].colnum = (long *) G_MALLOC(sizeof(long), i);
        gp[i].lpid = (long *) G_MALLOC(sizeof(long), i);
        gp[i].multi_time = (double *) G_MALLOC(sizeof(double), i);
        gp[i].total_time = (double *) G_MALLOC(sizeof(double), i);
        gp[i].sync_time = (double *) G_MALLOC(sizeof(double), i);
        gp[i].process_time = (double *) G_MALLOC(sizeof(double), i);
        gp[i].step_start = (double *) G_MALLOC(sizeof(double), i);
        gp[i].steps_time = (double *) G_MALLOC(10 * sizeof(double), i);
        *gp[i].multi_time = 0;
        *gp[i].total_time = 0;
        *gp[i].sync_time = 0;
        *gp[i].process_time = 0;
        *gp[i].lpid = i;
    }

    subblock();

    x_part = (jm - 2) / xprocs + 2;
    y_part = (im - 2) / yprocs + 2;

    d_size = x_part * y_part * sizeof(double) + y_part * sizeof(double *);

    global = (struct global_struct *) G_MALLOC(sizeof(struct global_struct), -1);

    for (i = 0; i < nprocs; i++) {
        psi[i][0] = (double **) G_MALLOC(d_size, i);
        psi[i][1] = (double **) G_MALLOC(d_size, i);
        psim[i][0] = (double **) G_MALLOC(d_size, i);
        psim[i][1] = (double **) G_MALLOC(d_size, i);
        psium[i] = (double **) G_MALLOC(d_size, i);
        psilm[i] = (double **) G_MALLOC(d_size, i);
        psib[i] = (double **) G_MALLOC(d_size, i);
        ga[i] = (double **) G_MALLOC(d_size, i);
        gb[i] = (double **) G_MALLOC(d_size, i);
        work1[i][0] = (double **) G_MALLOC(d_size, i);
        work1[i][1] = (double **) G_MALLOC(d_size, i);
        work2[i] = (double **) G_MALLOC(d_size, i);
        work3[i] = (double **) G_MALLOC(d_size, i);
        work4[i][0] = (double **) G_MALLOC(d_size, i);
        work4[i][1] = (double **) G_MALLOC(d_size, i);
        work5[i][0] = (double **) G_MALLOC(d_size, i);
        work5[i][1] = (double **) G_MALLOC(d_size, i);
        work6[i] = (double **) G_MALLOC(d_size, i);
        work7[i][0] = (double **) G_MALLOC(d_size, i);
        work7[i][1] = (double **) G_MALLOC(d_size, i);
        temparray[i][0] = (double **) G_MALLOC(d_size, i);
        temparray[i][1] = (double **) G_MALLOC(d_size, i);
        tauz[i] = (double **) G_MALLOC(d_size, i);
        oldga[i] = (double **) G_MALLOC(d_size, i);
        oldgb[i] = (double **) G_MALLOC(d_size, i);
    }

    oim = im;
    //f = (double *) G_MALLOC(oim*sizeof(double), 0);
    multi = (struct multi_struct *) G_MALLOC(sizeof(struct multi_struct), -1);

    d_size = numlev * sizeof(double **);
    if (numlev % 2 == 1) {      /* To make sure that the actual data
                                   starts double word aligned, add an extra
                                   pointer */
        d_size += sizeof(double **);
    }
    for (i = 0; i < numlev; i++) {
        d_size += ((imx[i] - 2) / yprocs + 2) * ((jmx[i] - 2) / xprocs + 2) * sizeof(double) + ((imx[i] - 2) / yprocs + 2) * sizeof(double *);
    }

    d_size *= nprocs;

    if (nprocs % 2 == 1) {      /* To make sure that the actual data
                                   starts double word aligned, add an extra
                                   pointer */
        d_size += sizeof(double ***);
    }

    d_size += nprocs * sizeof(double ***);
    main_q_multi = (double ****) G_MALLOC(d_size, -1);
    main_rhs_multi = (double ****) G_MALLOC(d_size, -1);
    q_multi = main_q_multi;
    rhs_multi = main_rhs_multi;


    locks = (struct locks_struct *) G_MALLOC(sizeof(struct locks_struct), -1);
    bars = (struct bars_struct *) G_MALLOC(sizeof(struct bars_struct), -1);

    LOCKINIT(locks->idlock)
    LOCKINIT(locks->psiailock)
    LOCKINIT(locks->psibilock)
    LOCKINIT(locks->donelock)
    LOCKINIT(locks->error_lock)
    LOCKINIT(locks->bar_lock)
#if defined(MULTIPLE_BARRIERS)
    BARINIT(bars->iteration, nprocs)
    BARINIT(bars->gsudn, nprocs)
    BARINIT(bars->p_setup, nprocs)
    BARINIT(bars->p_redph, nprocs)
    BARINIT(bars->p_soln, nprocs)
    BARINIT(bars->p_subph, nprocs)
    BARINIT(bars->sl_prini, nprocs)
    BARINIT(bars->sl_psini, nprocs)
    BARINIT(bars->sl_onetime, nprocs)
    BARINIT(bars->sl_phase_1, nprocs)
    BARINIT(bars->sl_phase_2, nprocs)
    BARINIT(bars->sl_phase_3, nprocs)
    BARINIT(bars->sl_phase_4, nprocs)
    BARINIT(bars->sl_phase_5, nprocs)
    BARINIT(bars->sl_phase_6, nprocs)
    BARINIT(bars->sl_phase_7, nprocs)
    BARINIT(bars->sl_phase_8, nprocs)
    BARINIT(bars->sl_phase_9, nprocs)
    BARINIT(bars->sl_phase_10, nprocs)
    BARINIT(bars->error_barrier, nprocs)
#else
    BARINIT(bars->barrier, nprocs)
#endif
    link_all();

    multi->err_multi = 0.0;
    i_int_coeff[0] = 0.0;
    j_int_coeff[0] = 0.0;

    for (i = 0; i < numlev; i++) {
        i_int_coeff[i] = 1.0 / (imx[i] - 1);
        j_int_coeff[i] = 1.0 / (jmx[i] - 1);
    }

    /*
       initialize constants and variables

       id is a global shared variable that has fetch-and-add operations
       performed on it by processes to obtain their pids.   
     */

    //global->id = 0;
    global->trackstart = 0;
    global->psibi = 0.0;

    factjacob = -1. / (12. * res * res);
    factlap = 1. / (res * res);
    eig2 = -h * f0 * f0 / (h1 * h3 * gpr);

    jmm1 = jm - 1;
    ysca = ((double) jmm1) * res;
    im = (imx[numlev - 1] - 2) / yprocs + 2;
    jm = (jmx[numlev - 1] - 2) / xprocs + 2;
    
    main_im = im;
    main_jm = jm;
    main_numlev = numlev;
    main_xprocs = xprocs;
    main_yprocs = yprocs;
    main_eig2 = eig2;

    if (do_output) {
        printf("              MULTIGRID OUTPUTS\n");
    }

    CREATE(slave, nprocs);
    WAIT_FOR_END(nprocs);
    CLOCK(computeend);

    printf("\n");
    printf("                PROCESS STATISTICS\n");
    printf("                  Total          Multigrid         Multigrid\n");
    printf(" Proc             Time             Time            Fraction\n");
    printf("    0   %15.0f    %15.0f        %10.3f\n", (*gp[0].total_time), (*gp[0].multi_time), (*gp[0].multi_time) / (*gp[0].total_time));

    if (do_stats) {
        double phase_time;
        min_total = max_total = avg_total = (*gp[0].total_time);
        min_multi = max_multi = avg_multi = (*gp[0].multi_time);
        min_frac = max_frac = avg_frac = (*gp[0].multi_time) / (*gp[0].total_time);
        avg_wait = *gp[0].sync_time;
        max_wait = *gp[0].sync_time;
        min_wait = *gp[0].sync_time;
        imax_wait = 0;
        imin_wait = 0;

        for (i = 1; i < nprocs; i++) {
            if ((*gp[i].total_time) > max_total) {
                max_total = (*gp[i].total_time);
            }
            if ((*gp[i].total_time) < min_total) {
                min_total = (*gp[i].total_time);
            }
            if ((*gp[i].multi_time) > max_multi) {
                max_multi = (*gp[i].multi_time);
            }
            if ((*gp[i].multi_time) < min_multi) {
                min_multi = (*gp[i].multi_time);
            }
            if ((*gp[i].multi_time) / (*gp[i].total_time) > max_frac) {
                max_frac = (*gp[i].multi_time) / (*gp[i].total_time);
            }
            if ((*gp[i].multi_time) / (*gp[i].total_time) < min_frac) {
                min_frac = (*gp[i].multi_time) / (*gp[i].total_time);
            }
            avg_total += (*gp[i].total_time);
            avg_multi += (*gp[i].multi_time);
            avg_frac += (*gp[i].multi_time) / (*gp[i].total_time);
            avg_wait += (*gp[i].sync_time);
            if (max_wait < (*gp[i].sync_time)) {
                max_wait = (*gp[i].sync_time);
                imax_wait = i;
            }
            if (min_wait > (*gp[i].sync_time)) {
                min_wait = (*gp[i].sync_time);
                imin_wait = i;
            }
        }
        avg_total = avg_total / nprocs;
        avg_multi = avg_multi / nprocs;
        avg_frac = avg_frac / nprocs;
        avg_wait = avg_wait / nprocs;
        for (i = 1; i < nprocs; i++) {
            printf("  %3ld   %15.0f    %15.0f        %10.3f\n", i, (*gp[i].total_time), (*gp[i].multi_time), (*gp[i].multi_time) / (*gp[i].total_time));
        }
        printf("  Avg   %15.0f    %15.0f        %10.3f\n", avg_total, avg_multi, avg_frac);
        printf("  Min   %15.0f    %15.0f        %10.3f\n", min_total, min_multi, min_frac);
        printf("  Max   %15.0f    %15.0f        %10.3f\n", max_total, max_multi, max_frac);
        
        printf("\n\n                  Sync\n");
        printf(" Proc      Time        Fraction\n");
        for (i = 0; i < nprocs; i++) {
            printf("  %ld        %u      %f\n", i, (unsigned int)*gp[i].sync_time, *gp[i].sync_time / ((long)(*gp[i].total_time)));
        }

        printf("  Avg   %f   %f\n", avg_wait, (double) avg_wait / (long) (computeend - global->trackstart));
        printf("  Min   %f   %f\n", min_wait, (double) min_wait / (long) (*gp[imin_wait].total_time));
        printf("  Max   %f   %f\n", max_wait, (double) max_wait / (long) (*gp[imax_wait].total_time));

        printf("\nPhases Avg :\n\n");
        for (i = 0; i < 10; i++) {
            phase_time = 0;
            for (j = 0; j < nprocs; j++) {
                phase_time += gp[j].steps_time[i];
            }
            phase_time /= (double) nprocs;
            printf("  %d = %f (fraction %f)\n", i + 1, phase_time, phase_time / (long) (computeend - global->trackstart));
        }
    }
    printf("\n");

    global->starttime = start;
    printf("                       TIMING INFORMATION\n");
    printf("[NPROCS]           : %16ld\n", nprocs);
    printf("[START1]           : %16llu\n", global->starttime);
    printf("[START2]           : %16llu\n", global->trackstart);
    printf("[END]              : %16llu\n", computeend);
    printf("[TOTAL]            : %16llu\n", computeend - global->starttime);    // With init
    printf("[PARALLEL_COMPUTE] : %16llu\n", computeend - global->trackstart);   // Without init
    printf("(excludes first timestep)\n");
    printf("\n");

    MAIN_END
    
}

long log_2(long number)
{
    long cumulative = 1;
    long out = 0;
    long done = 0;

    while ((cumulative < number) && (!done) && (out < 50)) {
        if (cumulative == number) {
            done = 1;
        } else {
            cumulative = cumulative * 2;
            out++;
        }
    }

    if (cumulative == number) {
        return (out);
    } else {
        return (-1);
    }
}

void printerr(char *s)
{
    fprintf(stderr, "ERROR: %s\n", s);
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
