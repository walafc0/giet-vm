///////////////////////////////////////////////////////////////////////////////////
// File     : spi_driver.h
// Date     : 31/08/2012
// Author   : cesar fuguet
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////
#ifndef _GIET_SPI_DRIVER_H_
#define _GIET_SPI_DRIVER_H_

#include <io.h>
#include <mapping_info.h>

///////////////////////////////////////////////////////////////////////////////
// SPI structure definition
///////////////////////////////////////////////////////////////////////////////
struct spi_dev
{
    // RX/TX registers of the SPI controller 
    unsigned int rx_tx[4];

    // control register of the SPI controller
    unsigned int ctrl;

    // divider register for the SPI controller generated clock signal
    unsigned int divider;

    // slave select register of the SPI controller
    unsigned int ss;

    // SPI-DMA registers
    unsigned int dma_base;
    unsigned int dma_baseh;
    unsigned int dma_count;
};

void spi_put_tx(struct spi_dev * spi, unsigned char byte, int index);

inline volatile unsigned char spi_get_rx(struct spi_dev * spi, int index);

unsigned int spi_get_data(struct spi_dev * spi, paddr_t buffer, unsigned int count);

inline void spi_ss_assert(struct spi_dev * spi, int index);

inline void spi_ss_deassert(struct spi_dev * spi, int index);

void _spi_init ( struct spi_dev * spi,
                 int spi_freq        ,
                 int sys_freq        ,
                 int char_len        ,
                 int tx_edge         ,
                 int rx_edge         );

///////////////////////////////////////////////////////////////////////////////
// SPI macros and constants
///////////////////////////////////////////////////////////////////////////////
#define SPI_TX_POSEDGE         1           // MOSI is changed on neg edge
#define SPI_TX_NEGEDGE         0           // MOSI is changed on pos edge
#define SPI_RX_POSEDGE         1           // MISO is latched on pos edge
#define SPI_RX_NEGEDGE         0           // MISO is latched on neg edge

#define SPI_CTRL_ASS_EN        ( 1 << 13 ) // Auto Slave Sel Assertion
#define SPI_CTRL_IE_EN         ( 1 << 12 ) // Interrupt Enable
#define SPI_CTRL_LSB_EN        ( 1 << 11 ) // LSB are sent first
#define SPI_CTRL_TXN_EN        ( 1 << 10 ) // MOSI is changed on neg edge
#define SPI_CTRL_RXN_EN        ( 1 << 9  ) // MISO is latched on neg edge
#define SPI_CTRL_GO_BSY        ( 1 << 8  ) // Start the transfer
#define SPI_CTRL_DMA_BSY       ( 1 << 16 ) // DMA in progress
#define SPI_CTRL_CHAR_LEN_MASK (  0xFF   ) // Bits transmited in 1 transfer
#define SPI_RXTX_MASK          (  0xFF   ) // Mask for the an RX/TX value

#define SPI_DMA_COUNT_READ     ( 1 << 0  ) // operation is a read (else write)

///////////////////////////////////////////////////////////////////////////////
//      SPI_IS_BUSY()
// This macro checks the GO_BSY and DMA_BSY bits of the SPI controller which
// indicates an ongoing transfer.
//
// Returns 1 if there is an unfinished transfer
///////////////////////////////////////////////////////////////////////////////
#define SPI_IS_BUSY(x) \
    ((ioread32(&x->ctrl) & (SPI_CTRL_GO_BSY|SPI_CTRL_DMA_BSY)) != 0) ? 1 : 0

#endif

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
