/*-----------------------------------------------------------------------------
/ Atmel Xmega32E5 UART bootloader.
/------------------------------------------------------------------------------
/ “THE COFFEEWARE LICENSE” (Revision 1):
/ <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
/ can do whatever you want with this stuff. If we meet some day, and you think
/ this stuff is worth it, you can buy me a coffee in return.
/------------------------------------------------------------------------------
/ v3 - April 2016: Can use b
/ v2 - August 2014
/----------------------------------------------------------------------------*/
#include <avr/io.h> 
#include <util/delay.h>
#include "xmega_digital.h"
#include "sp_driver.h"
/*---------------------------------------------------------------------------*/
int16_t getch();
void init_uart();
void initClock_32Mhz();
void (*funcptr)(void) = 0x0000;
/*---------------------------------------------------------------------------*/
/* SPM_PAGESIZE is 128 for Xmega32E5 */
uint8_t pageBuf[SPM_PAGESIZE];
/*---------------------------------------------------------------------------*/
#define VERSION 2
#define WDT_Reset() asm("wdr")
/*---------------------------------------------------------------------------*/
void (*sendch)(uint8_t);
/*---------------------------------------------------------------------------*/
uint8_t (*getByte)();
/*---------------------------------------------------------------------------*/
uint8_t getByte_portD()
{
  return USARTD0.DATA;
}
/*---------------------------------------------------------------------------*/
uint8_t getByte_portC()
{
  return USARTC0.DATA;
}
/*---------------------------------------------------------------------------*/
uint8_t (*newMessage)(); 
/*---------------------------------------------------------------------------*/
uint8_t newMessage_portD() 
{
  return (USARTD0.STATUS & USART_RXCIF_bm);
}
/*---------------------------------------------------------------------------*/
uint8_t newMessage_portC() 
{
  return (USARTC0.STATUS & USART_RXCIF_bm);
}
/*---------------------------------------------------------------------------*/
void sendch_portD(uint8_t ch)
{
  while(!(USARTD0.STATUS & USART_DREIF_bm));

  USARTD0.DATA = ch;
}
/*---------------------------------------------------------------------------*/
void sendch_portC(uint8_t ch)
{
  while(!(USARTC0.STATUS & USART_DREIF_bm));

  USARTC0.DATA = ch;
}
/*---------------------------------------------------------------------------*/
#define WDT_IsSyncBusy() (WDT.STATUS & WDT_SYNCBUSY_bm)
/*---------------------------------------------------------------------------*/
static void boot_program_page(uint32_t pageOffset, uint8_t *buf)
{
  SP_LoadFlashPage(buf);
  SP_EraseWriteApplicationPage(pageOffset);
  SP_WaitForSPM();
}
/*---------------------------------------------------------------------------*/
int main(void) 
{    
  uint16_t i;
  uint8_t msg;
  uint32_t t32;  
  uint32_t pageOffset;
   
  /* If we are here because of a WDT reset, go directly to the user app. */
  if(RST.STATUS & RST_WDRF_bm)
  {
    /* Clear the flag */        
    RST.STATUS |= RST_WDRF_bm;

    WDT_Reset();

    /* Disable the WDT */
    CCP = CCP_IOREG_gc;
    WDT.CTRL = WDT_CEN_bm;

    /* Go to user app ... */
    funcptr();
  }

  /* Initialize the WDT peripheral */
  CCP = CCP_IOREG_gc;
  WDT.CTRL = WDT_WPER_1KCLK_gc | WDT_ENABLE_bm | WDT_CEN_bm;
  while(WDT_IsSyncBusy());

  /* Onboard LED */
  pinMode(A,5,OUTPUT);
  digitalWrite(A,5,HIGH);

  initClock_32Mhz();
  init_uart();

  /* Wait until a message arrives or timeout */
  while((newMessage_portD() == 0)&&(newMessage_portC() == 0));

  if(newMessage_portD() != 0)
  {
    /* Was it correct message? */
    if(getByte_portD() != 'a')
    {
      /* WDT will eventually jump to the user app. */
      while(1);
    }
    else
    {
      /* Set the functions ... */
      sendch = sendch_portD;
      getByte = getByte_portD;
      newMessage = newMessage_portD;
      
      /* Send ACK */
      sendch('Y');    
    }       
  }
  else
  {
    /* Was it correct message? */
    if(getByte_portC() != 'a')
    {
      /* WDT will eventually jump to the user app. */
      while(1);
    }
    else
    {
      /* Set the functions ... */
      sendch = sendch_portC;
      getByte = getByte_portC;
      newMessage = newMessage_portC;
      
      /* Send ACK */
      sendch('Y');    
    }    
  }

  while(1)
  {        
    while(!newMessage());
    
    msg = getByte();
    
    WDT_Reset();

    togglePin(A,5);
    
    switch(msg)
    {
      /* Ping */
      case 'a':
      {
        /* Send ACK */
        sendch('Y');
        break;
      }
      /* Fill the page buffer */
      case 'b':
      {
        /* Send ACK */
        sendch('Y');

        for(i=0;i<SPM_PAGESIZE;i++)
        {
          while(!newMessage());
          pageBuf[i] = getByte();
        }

        break;
      }
      /* Program the buffer */
      case 'c':
      {
        /* Send ACK */
        sendch('Y');

        while(!newMessage());
        pageOffset = getByte();

        while(!newMessage());
        t32 = getByte();
        t32 = t32 << 8;
        pageOffset += t32;

        while(!newMessage());
        t32 = getByte();
        t32 = t32 << 16;
        pageOffset += t32;

        while(!newMessage());
        t32 = getByte();
        t32 = t32 << 24;
        pageOffset += t32;

        boot_program_page(pageOffset,pageBuf);

        /* Send ACK */
        sendch('Y');
        break;
      }
      /* Delete the pages */
      case 'd':
      {   
        for(t32=0;t32<BOOTSTART;t32+=SPM_PAGESIZE)
        {
          WDT_Reset(); SP_EraseApplicationPage(t32);
          WDT_Reset(); SP_WaitForSPM();
        }                    

        /* Send ACK */
        sendch('Y');
        break;
      }
      /* Version readout */
      case 'v':
      {
        sendch(VERSION);
        break;
      }
      /* Go to user app ... */
      case 'x':
      {
        /* WDT will eventually jump to the user app. */
        while(1);

        break;
      }
    }     
  }
  
  /* WDT will eventually jump to the user app. */
  while(1);

  return 0;
}
/*---------------------------------------------------------------------------*/
void init_uart()
{
  /* Set UART pin driver states */
  pinMode(D,6,INPUT);
  pinMode(D,7,OUTPUT);

  /* Remap the UART pins */
  PORTD.REMAP = PORT_USART0_bm;
  
  USARTD0.CTRLB = USART_RXEN_bm|USART_TXEN_bm;
  USARTD0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc|USART_PMODE_DISABLED_gc|USART_CHSIZE_8BIT_gc;

  /* 115200 baud rate with 32MHz clock */
  USARTD0.BAUDCTRLA = 131; USARTD0.BAUDCTRLB = (-3 << USART_BSCALE_gp);

  /* Set UART pin driver states */
  pinMode(C,2,INPUT);
  pinMode(C,3,OUTPUT);

  USARTC0.CTRLB = USART_RXEN_bm|USART_TXEN_bm;
  USARTC0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc|USART_PMODE_DISABLED_gc|USART_CHSIZE_8BIT_gc;

  /* 115200 baud rate with 32MHz clock */
  USARTC0.BAUDCTRLA = 131; USARTC0.BAUDCTRLB = (-3 << USART_BSCALE_gp);
}
/*---------------------------------------------------------------------------*/
void initClock_32Mhz()
{
  /* Generates 32Mhz clock from internal 2Mhz clock via PLL */
  OSC.PLLCTRL = OSC_PLLSRC_RC2M_gc | 16;
  OSC.CTRL |= OSC_PLLEN_bm ;
  while((OSC.STATUS & OSC_PLLRDY_bm) == 0);
  CCP = CCP_IOREG_gc;
  CLK.CTRL = CLK_SCLKSEL_PLL_gc;
}
/*---------------------------------------------------------------------------*/
