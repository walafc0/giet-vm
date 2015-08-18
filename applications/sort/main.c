///////////////////////////////////////////////////////////////////////////////
// File   :  main.c
// Date   :  November 2013
// Author :  Cesar Fuguet Tortolero <cesar.fuguet-tortolero@lip6.fr>
///////////////////////////////////////////////////////////////////////////////
// This multi-threaded application implement a multi-stage sort application.
// The various stages are separated by synchronisation barriers.
// There is one thread per physical processors. Computation is organised as
// a binary tree: All threads contribute to the first stage of parallel sort
// but, the number of participating threads is divided by 2 at each next stage.
//       Number_of_stages = number of barriers = log2(Number_of_threads)
//
// Constraints :
// - It supports up to 1024 processors and the number of processors
//   must be a power of 2.
// _ The array of values to be sorted (ARRAY_LENGTH) must be power of 2 
//   larger than the number of processors.
// - This application uses a private TTY terminal, shared by all threads,
//   that is protectted by an user-level SQT lock.
///////////////////////////////////////////////////////////////////////////////

#include "stdio.h"
#include "mapping_info.h"
#include "user_barrier.h"
#include "user_lock.h"

#define ARRAY_LENGTH    0x400
#define IPT             (ARRAY_LENGTH / threads) // ITEMS PER THREAD

// macro to use a shared TTY
#define printf(...)     lock_acquire( &tty_lock ); \
                        giet_tty_printf(__VA_ARGS__);  \
                        lock_release( &tty_lock )

int              array0[ARRAY_LENGTH];
int              array1[ARRAY_LENGTH];

volatile int     init_ok = 0;
giet_barrier_t   barrier[10];
user_lock_t      tty_lock;   

void bubbleSort(
        int * array,
        unsigned int length,
        unsigned int init_pos);

void merge(
        int * array,
        int * result,
        int length,
        int init_pos_a,
        int init_pos_b,
        int init_pos_result);

//////////////////////////////////////////
__attribute__ ((constructor)) void main()
//////////////////////////////////////////
{
    int * src_array = NULL;
    int * dst_array = NULL;
    int i;
    unsigned int x_size;
    unsigned int y_size;
    unsigned int nprocs;
    unsigned int threads;

    // each thread gets its thread_id
    int thread_id = giet_thread_id();
    
    unsigned int time_start = giet_proctime();
    unsigned int time_end;   

    // each thread compute number of threads (one thread per proc)
    giet_procs_number( &x_size , &y_size , &nprocs );
    threads = x_size * y_size * nprocs;

    // thread 0 makes TTY and barrier initialisations
    // other threads wait initialisation completion.
    if ( thread_id == 0 )
    {
        // request a shared TTY used by all threads
        giet_tty_alloc(1);
        
        // TTY lock initialisation
        lock_init( &tty_lock );

        printf("\n[ SORT T0 ] Starting sort application with %d threads "
                 "at cycle %d\n", threads, time_start);

        // Barriers Initialization
        for (i = 0; i < __builtin_ctz( threads ); i++)
        {
            barrier_init( &barrier[i], threads >> i );
        }

        init_ok = 1;
    }
    else
    {
        while( !init_ok );
    }

    // each thread checks number of tasks
    if ( (threads != 1)   && (threads != 2)   && (threads != 4)   && 
         (threads != 8)   && (threads != 16 ) && (threads != 32)  && 
         (threads != 64)  && (threads != 128) && (threads != 256) && 
         (threads != 512) && (threads != 1024) )
    {
        giet_exit("error : number of processors must be power of 2");
    }


    // Each thread contribute to Array Initialization
    for (i = IPT * thread_id; i < IPT * (thread_id + 1); i++)
    {
        array0[i] = giet_rand();
    }

    // all threads contribute to the first stage of parallel sort
    printf("[ SORT T%d ] Stage 0: Sorting...\n\r", thread_id);

    bubbleSort(array0, IPT, IPT * thread_id);

    printf("[ SORT T%d ] Finishing Stage 0\n\r", thread_id);

    // the number of threads is divided by 2 at each next stage
    for (i = 0; i < __builtin_ctz( threads ); i++)
    {
        barrier_wait( &barrier[i] );

        if((thread_id % (2 << i)) != 0)
        {
            printf("[ SORT T%d ] Quit\n\r", thread_id );
            giet_exit("Completed");
        }

        printf("[ SORT T%d ] Stage %d: Sorting...\n\r", thread_id, i+1);

        if((i % 2) == 0)
        {
            src_array = &array0[0];
            dst_array = &array1[0];
        }
        else
        {
            src_array = &array1[0];
            dst_array = &array0[0];
        }

        merge(src_array, dst_array
                , IPT << i
                , IPT * thread_id
                , IPT * (thread_id + (1 << i))
                , IPT * thread_id
                );

        printf("[ SORT T%d ] Finishing Stage %d\n\r", thread_id, i + 1);
    }

    int success;
    int failure_index;

    // Verify the resulting array
    if(thread_id != 0)
    {
        giet_exit("error: only thread 0 should get here");
    }

    success = 1;
    for(i=0; i<(ARRAY_LENGTH-1); i++)
    {
        if(dst_array[i] > dst_array[i+1])
        {
            success = 0;
            failure_index = i;
            break;
        }
    }

    time_end = giet_proctime();

    printf("[ SORT T0 ] Finishing sort application at cycle %d\n"
           "[ SORT T0 ] Time elapsed = %d\n",
            time_end, (time_end - time_start) );

    if (success)
    {
        giet_exit("!!! Success !!!");
    }
    else
    {
        printf("[ SORT T0 ] Failure!! Incorrect element: %d\n\r", 
               failure_index);
        for(i=0; i<ARRAY_LENGTH; i++)
        {
            printf("array[%d] = %d\n", i, dst_array[i]);
        }
        giet_exit("!!!  Failure !!!");
    }

    giet_exit("Completed");
}

////////////////////////////////////
void bubbleSort( int *        array,
                 unsigned int length,
                 unsigned int init_pos )
{
    int i;
    int j;
    int aux;

    for(i = 0; i < length; i++)
    {
        for(j = init_pos; j < (init_pos + length - i - 1); j++)
        {
            if(array[j] > array[j + 1])
            {
                aux          = array[j + 1];
                array[j + 1] = array[j];
                array[j]     = aux;
            }
        }
    }
}

/////////////
void merge(
        int * array,
        int * result,
        int length,
        int init_pos_a,
        int init_pos_b,
        int init_pos_result)
{
    int i;
    int j;
    int k;

    i = 0;
    j = 0;
    k = init_pos_result;

    while((i < length) || (j < length))
    {
        if((i < length) && (j < length))
        {
            if(array[init_pos_a + i] < array[init_pos_b + j])
            {
                result[k++] = array[init_pos_a + i];
                i++;
            }
            else
            {
                result[k++] = array[init_pos_b + j];
                j++;
            }
        }
        else if(i < length)
        {
            result[k++] = array[init_pos_a + i];
            i++;
        }
        else
        {
            result[k++] = array[init_pos_b + j];
            j++;
        }
    }
}

/* vim: tabstop=4 : shiftwidth=4 : expandtab
*/
