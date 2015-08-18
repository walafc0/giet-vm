//////////////////////////////////////////////////////////////////////////////////
// File    : main.c  (for gameoflife)
// Date    : November 2013 / February 2015
// Authors :  Alexandre Joannou <alexandre.joannou@lip6.fr> november 2013
//            Alain Greiner <alain.greiner@lip6.fr> february 2015
//
// This multi-threaded application is an emulation of the Game of Life automaton.
// The world size is defined by the HEIGHT and WIDTH parameters.
// There is one task per processor.
// Each task compute HEIGHT/nbprocs lines.
// Task running on processor P(0,0,0) initialises the barrier, the TTY terminal,
// and the chained buffer DMA controler.
//
// The number of processors must be a power of 2 not larger than HEIGHT.
//////////////////////////////////////////////////////////////////////////////////

#include "stdio.h"
#include "limits.h"
#include "user_barrier.h"
#include "mapping_info.h"
#include "hard_config.h"

#define WIDTH           128
#define HEIGHT          128
#define NB_ITERATION    1000000000

#define PRINTF(...) ({ if ( proc_id==0) { giet_tty_printf(__VA_ARGS__); } })

typedef unsigned char uint8_t;

uint8_t WORLD[2][HEIGHT][WIDTH] __attribute__((aligned(64)));

uint8_t DISPLAY[2][HEIGHT][WIDTH] __attribute__((aligned(64)));

unsigned int status0[16];
unsigned int status1[16];

giet_sqt_barrier_t barrier;

volatile unsigned int init_ok;

////////////////////////////////////
void init_world( unsigned int phase,
                 unsigned int base_line,
                 unsigned int nb_line )
{
   unsigned int x,y;
   for (y = base_line ; y < base_line + nb_line ; y++)
   {
      for(x = 0 ; x < WIDTH ; x++) 
      {
         WORLD[phase][y][x] = (giet_rand() >> (x % 8)) & 0x1;
      }
   }
}

//////////////////////////////////////////////////////
uint8_t number_of_alive_neighbour( unsigned int phase,
                                   unsigned int x, 
                                   unsigned int y )
{
   uint8_t nb = 0;

   nb += WORLD[phase][(y - 1) % HEIGHT][(x - 1) % WIDTH];
   nb += WORLD[phase][ y              ][(x - 1) % WIDTH];
   nb += WORLD[phase][(y + 1) % HEIGHT][(x - 1) % WIDTH];
   nb += WORLD[phase][(y - 1) % HEIGHT][ x             ];
   nb += WORLD[phase][(y + 1) % HEIGHT][ x             ];
   nb += WORLD[phase][(y - 1) % HEIGHT][(x + 1) % WIDTH];
   nb += WORLD[phase][ y              ][(x + 1) % WIDTH];
   nb += WORLD[phase][(y + 1) % HEIGHT][(x + 1) % WIDTH];

   return nb;
}

/////////////////////////////////////////
uint8_t compute_cell( unsigned int phase,
                      unsigned int x, 
                      unsigned int y )
{
   uint8_t nb_neighbours_alive = number_of_alive_neighbour( phase, x , y );

   if (WORLD[phase][y][x] == 1) 
   {
      if (nb_neighbours_alive == 2 || nb_neighbours_alive == 3)  return 1;
   }
   else 
   {
      if (nb_neighbours_alive == 3) return 1;
      else                          return WORLD[phase][y][x];
   }
   return 0;
}

/////////////////////////////////////////
void compute_new_gen( unsigned int phase,
                      unsigned int base_line, 
                      unsigned int nb_line )
{
   unsigned int x,y;
   for (y = base_line; y < base_line + nb_line; y++)
   {
      for(x = 0; x < WIDTH ; x++) 
      {
         WORLD[phase][y][x] = compute_cell( 1 - phase , x , y );  
      }
   }
}

////////////////////////////////////
void copy_world( unsigned int phase,
                 unsigned int base_line,
                 unsigned int nb_line )
{
   unsigned int x,y;
   for (y = base_line; y < base_line + nb_line; y++)
   {
      for(x = 0; x < WIDTH ; x++) 
      {
         DISPLAY[phase][y][x] = WORLD[phase][y][x]*255;  
      }
   }
}

////////////////////////////////////////
__attribute__((constructor)) void main()
////////////////////////////////////////
{
   // get processor identifier
   unsigned int x;
   unsigned int y;
   unsigned int p;
   giet_proc_xyp( &x, &y, &p );

   // get processors number
   unsigned int x_size;
   unsigned int y_size;
   unsigned int nprocs;
   giet_procs_number( &x_size, &y_size, &nprocs );

   // compute continuous processor index & number of procs
   unsigned int proc_id = (((x * y_size) + y) * nprocs) + p;  
   unsigned int n_global_procs = x_size * y_size * nprocs; 

   unsigned int i;

   unsigned int nb_line       = HEIGHT / n_global_procs;
   unsigned int base_line     = nb_line * proc_id; 
   
   // parameters checking
   giet_assert( (n_global_procs <= HEIGHT),
                " Number or processors larger than world height" );

   giet_assert( ((WIDTH == FBUF_X_SIZE) && (HEIGHT == FBUF_Y_SIZE)),
                "Frame Buffer size does not fit the world size" );
   
   giet_assert( ((x_size == 1) || (x_size == 2) || (x_size == 4) ||
                 (x_size == 8) || (x_size == 16)),
                "x_size must be a power of 2 no larger than 16" );

   giet_assert( ((y_size == 1) || (y_size == 2) || (y_size == 4) ||
                 (y_size == 8) || (y_size == 16)),
                "y_size must be a power of 2 no larger than 16" );

   giet_assert( ((nprocs == 1) || (nprocs == 2) || (nprocs == 4)), 
                "nprocs must be a power of 2 no larger than 4" );

   // P[0,0,0] makes initialisation
   if ( proc_id == 0 )
   {
      // get a private TTY for P[0,0,0]
      giet_tty_alloc( 0 );

      // get a Chained Buffer DMA channel
      giet_fbf_cma_alloc();

      // initializes the source and destination buffers
      giet_fbf_cma_init_buf( &DISPLAY[0][0][0] , 
                             &DISPLAY[1][0][0] , 
                             status0 ,
                             status1 );

      // activates CMA channel
      giet_fbf_cma_start( HEIGHT * WIDTH );

      // initializes distributed heap
      unsigned int cx;
      unsigned int cy;
      for ( cx = 0 ; cx < x_size ; cx++ )
      {
         for ( cx = 0 ; cx < x_size ; cx++ )
         {
            heap_init( cx , cy );
         }
      }

      // initialises barrier
      sqt_barrier_init( &barrier , x_size , y_size , nprocs );

      PRINTF("\n[GAMEOFLIFE] P[0,0,0] completes initialisation at cycle %d\n"
             " nprocs = %d / nlines = %d\n", 
             giet_proctime() , n_global_procs, HEIGHT );

      // activates all other processors
      init_ok = 1;
   }
   else
   {
      while ( init_ok == 0 ) asm volatile("nop\n nop\n nop");
   }

   ///////////// world  initialization ( All processors )

   // All processors initialize WORLD[0]
   init_world( 0 , base_line , nb_line );

   // copy WORLD[0] to DISPLAY[0]
   copy_world( 0 , base_line , nb_line );

   // synchronise with other procs
   sqt_barrier_wait( &barrier );

   // P(0,0,0) displays DISPLAY[0]
   if ( proc_id == 0 ) giet_fbf_cma_display ( 0 );

   PRINTF("\n[GAMEOFLIFE] starts evolution at cycle %d\n", giet_proctime() );
   
   //////////// evolution : 2 steps per iteration 

   for (i = 0 ; i < NB_ITERATION ; i++)
   {
      // compute WORLD[1] from WORLD[0]
      compute_new_gen( 1 , base_line , nb_line );

      // copy WORLD[1] to DISPLAY[1]
      copy_world( 1 , base_line , nb_line );

      // synchronise with other procs
      sqt_barrier_wait( &barrier );

      // P(0,0,0) displays DISPLAY[1]
      if ( proc_id == 0 ) giet_fbf_cma_display ( 1 );
   
      PRINTF(" - step %d\n", 2*i );
   
      // compute WORLD[0] from WORLD[1]
      compute_new_gen( 0 , base_line , nb_line );

      // copy WORLD[0] to DISPLAY[0]
      copy_world( 0 , base_line , nb_line );

      // synchronise with other procs
      sqt_barrier_wait( &barrier );

      // P(0,0,0) displays DISPLAY[0]
      if ( proc_id == 0 ) giet_fbf_cma_display ( 0 );

      PRINTF(" - step %d\n", 2*i + 1 );
   } // end main loop

   PRINTF("\n*** End of main at cycle %d ***\n", giet_proctime());

   giet_exit("Completed");
} // end main()

// Local Variables:
// tab-width: 3
// c-basic-offset: 3
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=3:tabstop=3:softtabstop=3



