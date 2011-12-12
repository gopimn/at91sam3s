/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2009, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */
/**
 *  \page usart_hard_handshaking USART Hardware Handshaking Example
 *
 *  \section Purpose
 *
 *  This example demonstrates the hardware handshaking mode (i.e. RTS/CTS)
 *  provided by the USART peripherals on SAM3S microcontrollers. The practical
 *  use of hardware handshaking is that it allows to stop transfer on the USART
 *  without losing any data in the process. This is very useful for applications
 *  that need to program slow memories for example.
 *
 *  \section Requirements
 *
 *  This example can be used on sam3s-ek. It requires a serial line with
 *  hardware control support( TXD and RXD cross over, RTS and CTS cross over)
 *  to connect the board and pc.
 *
 *  \section Description
 *
 *  The provided program uses hardware handshaking mode to regulate the data
 *  rate of an incoming file transfer. A terminal application, such as
 *  hyperterminal, is used to send a text file to the device (without any
 *  protocol such as X-modem). The device will enforce the configured
 *  bytes per second (bps) rate with its Request To Send (RTS) line.
 *
 *  Whenever the data rate meet or exceed the configurable threshold, the device
 *  stops receiving data on the USART. Since no buffer is provided to the PDC,
 *  this will set the RTS line, telling the computer to stop sending data. Each
 *  second, the current data rate and total number of bytes received are
 *  displayed; the transfer is also restarted.
 *
 *  Note that the device may receive slightly less bytes than the actual file
 *  size, depending on the nature of the file. This does NOT mean that bytes
 *  have been lost: this is simply an issue with how line breaks are transmitted
 *  by the terminal. It is therefore better to use binary files, as they most
 *  often do not contain line breaks. For example, send one of the object files
 *  generated by the compiler.
 *
 *  \section Usage
 *
 *  -# Build the program and download it inside the evaluation board. Please
 *     refer to the <a href="http://www.atmel.com/dyn/resources/prod_documents/doc6224.pdf">SAM-BA User Guide</a>,
 *     the <a href="http://www.atmel.com/dyn/resources/prod_documents/doc6310.pdf">GNU-Based Software Development</a>
 *     application note or to the <a href="ftp://ftp.iar.se/WWWfiles/arm/Guides/EWARM_UserGuide.ENU.pdf">IAR EWARM User Guide</a>,
 *     depending on your chosen solution.
 *  -# Connect a serial cable to the USART0 port on the evaluation kit. It will
 *     most often be labeled "RS232 COM port".
 *  -# On the computer, open and configure a terminal application (e.g.
 *     HyperTerminal on Microsoft Windows) with these settings:
 *        - 115200 bauds
 *        - 8 data bits
 *        - No parity
 *        - 1 stop bit
 *        - Hardware flow control (RTS/CTS)
 *  -# Start the application. The following traces shall appear on the terminal:
 *     \code
 *     -- USART Hardware Handshaking Example xxx --
 *     -- xxxxxx-xx
 *     -- Compiled: xxx xx xxxx xx:xx:xx --
 *     Bps:    0; Tot:      0
 *     \endcode
 *  -# Send a file in text format to the device. On HyperTerminal, this is done
 *     by selecting "Transfer -> Send Text File" (this does not prevent you from
 *     sending binary files). The transfer will start and the device will update
 *     the bps and total counts on the terminal.
 *  -# Whenever the transfer is complete, the total number of bytes received
 *     should match the size of the sent file (unless it is a text file, see
 *     explanation in description section).
 *
 *  \section References
 *  - usart_hard_handshaking/main.c
 *  - pio.h
 *  - tc.h
 *  - usart.h
 */

/** \file
 *
 *  This file contains all the specific code for the usart_hard_handshaking.
 */

/*------------------------------------------------------------------------------
 *         Headers
 *------------------------------------------------------------------------------*/

#include "board.h"

#include <stdio.h>

/*------------------------------------------------------------------------------
 *         Local definition
 *------------------------------------------------------------------------------*/


/** Maximum Bytes Per Second (BPS) rate that will be forced using the CTS pin.*/
#define MAX_BPS             500

/** Size of the receive buffer used by the PDC, in bytes.*/
#define BUFFER_SIZE         1


/*------------------------------------------------------------------------------
 *          Local variables
 * ------------------------------------------------------------------------------*/

/**  Pins to configure for the application.*/
const Pin pins[] = {
    BOARD_PIN_USART_RXD,
    BOARD_PIN_USART_TXD,
    BOARD_PIN_USART_CTS,
    BOARD_PIN_USART_RTS,
    BOARD_PIN_USART_EN,


};

/**  Number of bytes received between two timer ticks.*/
volatile uint32_t bytesReceived = 0;

/**  Receive buffer.*/
uint8_t pBuffer[BUFFER_SIZE];

/**  String buffer.*/
char pString[24];

/*------------------------------------------------------------------------------
 *         Local functions
 *------------------------------------------------------------------------------*/

/**
 *  \brief Interrupt handler for USART.
 *
 * Increments the number of bytes received in the
 * current second and starts another transfer if the desired bps has not been
 * met yet.
 */
void USART1_IrqHandler(void)
{
    uint32_t status;

    /* Read USART status*/
    status = BOARD_USART_BASE->US_CSR;

    /* Receive buffer is full*/
    if ((status & US_CSR_RXBUFF) == US_CSR_RXBUFF) {

        bytesReceived += BUFFER_SIZE;

        /* Restart transfer if BPS is not high enough*/
        if (bytesReceived < MAX_BPS) {

            USART_ReadBuffer(BOARD_USART_BASE, pBuffer, BUFFER_SIZE);
        }
        /* Otherwise disable interrupt*/
        else {

            BOARD_USART_BASE->US_IDR = US_IDR_RXBUFF;
        }
    }
}

/**
 *  \brief Interrupt handler for TC0.
 *
 * Displays the number of bytes received during the
 * last second and the total number of bytes received, then restarts a read
 * transfer on the USART if it was stopped.
 */
void TC0_IrqHandler( void )

{
    uint32_t dwStatus ;
    static uint32_t bytesTotal = 0 ;

    /* Read TC0 status*/
    dwStatus = TC0->TC_CHANNEL[0].TC_SR;

    /* RC compare*/
    if ( (dwStatus & TC_SR_CPCS) == TC_SR_CPCS )
    {
        /* Display info*/
        bytesTotal += bytesReceived ;
        sprintf( pString, "Bps: %4u; Tot: %6u\n\r", (unsigned int)bytesReceived, (unsigned int)bytesTotal ) ;
        USART_WriteBuffer( BOARD_USART_BASE, pString, sizeof( pString ) ) ;
        bytesReceived = 0 ;

        /* Resume transfer if needed*/
        if ( BOARD_USART_BASE->US_RCR == 0 )
        {
            USART_ReadBuffer( BOARD_USART_BASE, pBuffer, BUFFER_SIZE ) ;
            BOARD_USART_BASE->US_IER = US_IER_RXBUFF ;
        }
    }
}
/**
 *  \brief USART hardware handshaking configuration
 *
 * Configures USART in hardware handshaking mode, asynchronous, 8 bits, 1 stop
 * bit, no parity, 115200 bauds and enables its transmitter and receiver.
 */
static void _ConfigureUsart( void )
{
    uint32_t mode = US_MR_USART_MODE_HW_HANDSHAKING
                        | US_MR_USCLKS_MCK
                        | US_MR_CHRL_8_BIT
                        | US_MR_PAR_NO
                        | US_MR_NBSTOP_1_BIT
                        | US_MR_CHMODE_NORMAL ;

    /* Enable the peripheral clock in the PMC*/
    PMC->PMC_PCER0 = 1 << BOARD_ID_USART ;

    /* Configure the USART in the desired mode @115200 bauds*/
    USART_Configure( BOARD_USART_BASE, mode, 115200, BOARD_MCK ) ;

    /* Configure the RXBUFF interrupt*/
    NVIC_EnableIRQ( USART1_IRQn ) ;

    /* Enable receiver & transmitter*/
    USART_SetTransmitterEnabled( BOARD_USART_BASE, 1 ) ;
    USART_SetReceiverEnabled( BOARD_USART_BASE, 1 ) ;
}

/**
 *  \brief TC0 configuration
 *
 * Configures Timer Counter 0 (TC0) to generate an interrupt every second. This
 * interrupt will be used to display the number of bytes received on the USART.
 */

static void _ConfigureTc0( void )
{
    /* Enable TC0 peripheral clock*/
    PMC_EnablePeripheral( ID_TC0 ) ;

    /* Configure TC for a 1s (= 1Hz) tick*/
    TC_Configure( TC0, 0, 0x4 | TC_CMR_CPCTRG ) ;

    TC0->TC_CHANNEL[0].TC_RC = 32768 ;

    /* Configure interrupt on RC compare*/
    TC0->TC_CHANNEL[0].TC_IER = TC_SR_CPCS ;

    NVIC_EnableIRQ( TC0_IRQn ) ;

}

/*------------------------------------------------------------------------------*/
/*         Global functions*/
/*------------------------------------------------------------------------------*/

/**
 *  \brief usart-hard-handshaking Application entry point..
 *
 *  Configures USART0 in hardware handshaking mode and
 *  Timer Counter 0 to generate an interrupt every second. Then, starts the first
 *  transfer on the USART and wait in an endless loop.
 *
 *  \return Unused (ANSI-C compatibility).
 *  \callgraph
 */
extern int main( void )
{
    /* Disable watchdog*/
    WDT_Disable( WDT ) ;

    /* Configure pins*/
    PIO_Configure( pins, PIO_LISTSIZE( pins ) ) ;

    /* Configure USART and display startup trace*/
    _ConfigureUsart() ;

    printf( "-- USART Hardware Handshaking Example %s --\n\r", SOFTPACK_VERSION ) ;
    printf( "-- %s\n\r", BOARD_NAME ) ;
    printf( "-- Compiled: %s %s --\n\r", __DATE__, __TIME__ ) ;

    /* Configure TC0 to generate a 1s tick*/
    _ConfigureTc0() ;

    /* Start receiving data and start timer*/
    USART_ReadBuffer( BOARD_USART_BASE, pBuffer, BUFFER_SIZE ) ;
    BOARD_USART_BASE->US_IER = US_IER_RXBUFF ;
    TC_Start( TC0, 0 ) ;

    /* Infinite loop*/
    while ( 1 ) ;
}
