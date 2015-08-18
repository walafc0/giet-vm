///////////////////////////////////////////////////////////////////////////////////
// File     : spi_driver.c
// Date     : 31/08/2012
// Author   : cesar fuguet
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////

#include <spi_driver.h>
#include <utils.h>
#include <tty0.h>

///////////////////////////////////////////////////////////////////////////////
//      bswap32()
// This function makes a byte swap for a 4 bytes word 
// Arguments are:
// - x : word
// Returns the word x swapped 
///////////////////////////////////////////////////////////////////////////////
static unsigned int bswap32(unsigned int x)
{
  unsigned int y;
  y =  (x & 0x000000ff) << 24;
  y |= (x & 0x0000ff00) <<  8;
  y |= (x & 0x00ff0000) >>  8;
  y |= (x & 0xff000000) >> 24;
  return y;
}

///////////////////////////////////////////////////////////////////////////////
//      _spi_wait_if_busy()
// This function returns when the SPI controller has finished a transfer 
// Arguments are:
// - spi : initialized pointer to the SPI controller
///////////////////////////////////////////////////////////////////////////////
static void _spi_wait_if_busy(struct spi_dev * spi)
{
    register int delay;

    while(SPI_IS_BUSY(spi))
    {
        for (delay = 0; delay < 100; delay++);
    }
}

///////////////////////////////////////////////////////////////////////////////
//      _spi_init_transfer()
// This function trigger transfer of the tx registers to the selected slaves 
// Arguments are:
// - spi : initialized pointer to the SPI controller
///////////////////////////////////////////////////////////////////////////////
static void _spi_init_transfer(struct spi_dev * spi)
{
    unsigned int spi_ctrl = ioread32(&spi->ctrl);

    iowrite32(&spi->ctrl, spi_ctrl | SPI_CTRL_GO_BSY);
}

///////////////////////////////////////////////////////////////////////////////
//      _spi_calc_divider_value()
// This function computes the value for the divider register in order to obtain
// the SPI desired clock frequency 
// - spi_freq : desired frequency for the generated clock from the SPI
//              controller
// - sys_freq : system clock frequency
// Returns the computed frequency 
///////////////////////////////////////////////////////////////////////////////
static unsigned int _spi_calc_divider_value( unsigned int spi_freq ,
                                             unsigned int sys_freq )
{
    return ((sys_freq / (spi_freq * 2)) - 1);
}

///////////////////////////////////////////////////////////////////////////////
//      spi_put_tx()
// This function writes a byte on the tx register and trigger transfert
// - spi  : initialized pointer to the SPI controller
// - byte : byte to write on tx register
// - index: index of the tx register
///////////////////////////////////////////////////////////////////////////////
void spi_put_tx(struct spi_dev * spi, unsigned char byte, int index)
{
    _spi_wait_if_busy(spi);
    {
        iowrite8(&spi->rx_tx[index % 4], byte);
        _spi_init_transfer(spi);
    }
    _spi_wait_if_busy(spi);
}

///////////////////////////////////////////////////////////////////////////////
//      spi_get_rx()
// This function reads a byte on the rx register
// - spi  : initialized pointer to the SPI controller
// - index: index of the rx register
// Returns the byte read
///////////////////////////////////////////////////////////////////////////////
volatile unsigned char spi_get_rx(struct spi_dev * spi, int index)
{
    return ioread8(&spi->rx_tx[index % 4]);
}

///////////////////////////////////////////////////////////////////////////////
//      spi_get_data()
// This function reads count bytes and store them on a memory buffer
// - spi   : initialized pointer to the SPI controller
// - buffer: physical address of the buffer containing the read data
// - count : number of bytes to get (must be 512 bytes)
// Returns 0 if success and other value when failure
///////////////////////////////////////////////////////////////////////////////
unsigned int spi_get_data( struct spi_dev * spi,
                           paddr_t buffer      ,
                           unsigned int count  )
{
    unsigned int spi_ctrl0; // ctrl value before calling this function
    unsigned int spi_ctrl;
    int i;

    /*
     * Only reading of one block (512 bytes) are supported
     */
    if ( count != 512 ) return 1;

    _spi_wait_if_busy(spi);

    spi_ctrl0 = ioread32(&spi->ctrl);

#if 0
    /*
     * Read data using SPI DMA mechanism
     * Two restrictions:
     *   1. Can transfer only one block (512 bytes).
     *   2. The destination buffer must be aligned to SPI burst size (64 bytes)
     */
    if ((buffer & 0x3f) == 0)
    {
        _puts("spi_get_data(): Starting DMA transfer / count = ");
        _putx(count);
        _puts("\n");

        _puts("spi_get_data(): buffer = ");
        _putx(buffer);
        _puts("\n");

        spi->dma_base  = (buffer      ) & ((1 << 32) - 1); // 32 lsb
        spi->dma_baseh = (buffer >> 32) & ((1 << 8 ) - 1); // 8  msb
        spi->dma_count = count | SPI_DMA_COUNT_READ;

        while ( (spi->dma_count >> 1) );

        goto reset_ctrl;
    }
#endif

    /*
     * Read data without SPI DMA mechanism
     *
     * Switch to 128 bits words
     */

    spi_ctrl = (spi_ctrl0 & ~SPI_CTRL_CHAR_LEN_MASK) | 128;
    iowrite32(&spi->ctrl, spi_ctrl);

    /* 
     * Read data.
     * Four 32 bits words at each iteration (128 bits = 16 bytes)
     */
    for (i = 0; i < count/16; i++)
    {
        iowrite32(&spi->rx_tx[0], 0xffffffff);
        iowrite32(&spi->rx_tx[1], 0xffffffff);
        iowrite32(&spi->rx_tx[2], 0xffffffff);
        iowrite32(&spi->rx_tx[3], 0xffffffff);
        iowrite32(&spi->ctrl,  spi_ctrl | SPI_CTRL_GO_BSY);

        _spi_wait_if_busy(spi);

        _physical_write( buffer, bswap32(ioread32(&spi->rx_tx[3])) ); 
        buffer += 4;
        _physical_write( buffer, bswap32(ioread32(&spi->rx_tx[2])) ); 
        buffer += 4;
        _physical_write( buffer, bswap32(ioread32(&spi->rx_tx[1])) ); 
        buffer += 4;
        _physical_write( buffer, bswap32(ioread32(&spi->rx_tx[0])) ); 
        buffer += 4;
    }

#if 0
reset_ctrl:
#endif

    /* Switch back to original word size */
    iowrite32(&spi->ctrl, spi_ctrl0);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//      spi_ss_assert()
// This function enables a SPI slave
// - spi   : initialized pointer to the SPI controller
// - index : slave index
///////////////////////////////////////////////////////////////////////////////
void spi_ss_assert(struct spi_dev * spi, int index)
{
    unsigned int spi_ss = ioread32(&spi->ss);

    iowrite32(&spi->ss, spi_ss | (1 << index));
}

///////////////////////////////////////////////////////////////////////////////
//      spi_ss_deassert()
// This function disables a SPI slave
// - spi   : initialized pointer to the SPI controller
// - index : slave index
///////////////////////////////////////////////////////////////////////////////
void spi_ss_deassert(struct spi_dev * spi, int index)
{
    unsigned int spi_ss = ioread32(&spi->ss);

    iowrite32(&spi->ss, spi_ss & ~(1 << index));
}

///////////////////////////////////////////////////////////////////////////////
//      _spi_init()
// This function initializes the configuration register of SPI controller
// - spi      : initialized pointer to the SPI controller
// - spi_freq : SPI desired frequency (Hz)
// - sys_freq : system frequency (Hz)
// - char_len : bits per transfer
// - tx_edge  : if 1, transfer on positive edge
// - rx_edge  : if 1, latch received data on negative edge
///////////////////////////////////////////////////////////////////////////////
void _spi_init ( struct spi_dev * spi,
                 int spi_freq        ,
                 int sys_freq        ,
                 int char_len        ,
                 int tx_edge         ,
                 int rx_edge         )
{
    unsigned int spi_ctrl = ioread32(&spi->ctrl);

    if      ( tx_edge == 0 ) spi_ctrl |=  SPI_CTRL_TXN_EN;
    else if ( tx_edge == 1 ) spi_ctrl &= ~SPI_CTRL_TXN_EN;
    if      ( rx_edge == 0 ) spi_ctrl |=  SPI_CTRL_RXN_EN;
    else if ( rx_edge == 1 ) spi_ctrl &= ~SPI_CTRL_RXN_EN;
    if      ( char_len > 0 ) spi_ctrl  = (spi_ctrl & ~SPI_CTRL_CHAR_LEN_MASK) |
                                         (char_len &  SPI_CTRL_CHAR_LEN_MASK);

    iowrite32(&spi->ctrl, spi_ctrl);

    if (spi_freq > 0 && sys_freq > 0)
        iowrite32(&spi->divider, _spi_calc_divider_value(spi_freq, sys_freq));
}


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
