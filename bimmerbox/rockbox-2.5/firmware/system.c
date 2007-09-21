/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: system.c,v 1.2 2007/09/21 21:59:46 duke4d Exp $
 *
 * Copyright (C) 2002 by Alan Korr
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <stdio.h>
#include "config.h"
#include <stdbool.h>
#include "lcd.h"
#include "font.h"
#include "system.h"
#include "kernel.h"

#ifndef SIMULATOR
long cpu_frequency = CPU_FREQ;
#endif

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
int boost_counter = 0;
bool cpu_idle = false;
void cpu_boost(bool on_off)
{
    if(on_off)
    {
        /* Boost the frequency if not already boosted */
        if(boost_counter++ == 0)
        {
            set_cpu_frequency(CPUFREQ_MAX);
        }
    }
    else
    {
        /* Lower the frequency if the counter reaches 0 */
        if(--boost_counter == 0)
        {
            if(cpu_idle)
                set_cpu_frequency(CPUFREQ_DEFAULT);
            else
                set_cpu_frequency(CPUFREQ_NORMAL);
        }

        /* Safety measure */
        if(boost_counter < 0)
            boost_counter = 0;
    }
}

void cpu_idle_mode(bool on_off)
{
    cpu_idle = on_off;

    /* We need to adjust the frequency immediately if the CPU
       isn't boosted */
    if(boost_counter == 0)
    {
        if(cpu_idle)
            set_cpu_frequency(CPUFREQ_DEFAULT);
        else
            set_cpu_frequency(CPUFREQ_NORMAL);
    }
}

#endif

#if CONFIG_CPU == TCC730

void* volatile interrupt_vector[16]  __attribute__ ((section(".idata")));

static void ddma_wait_idle(void)  __attribute__ ((section (".icode")));
static void ddma_wait_idle(void)
{
    /* TODO: power saving trick: set the CPU freq to 22MHz
       while doing the busy wait after a disk dma access.
       (Used by Archos) */
    do {
    } while ((DDMACOM & 3) != 0);
}

void ddma_transfer(int dir, int mem, void* intAddr, long extAddr, int num)
    __attribute__ ((section (".icode")));
void ddma_transfer(int dir, int mem, void* intAddr, long extAddr, int num) {
    int irq = set_irq_level(1);
    ddma_wait_idle();
    long externalAddress = (long) extAddr;
    long internalAddress = ((long) intAddr) & 0xFFFF;
    /* HW wants those two in word units. */
    num /= 2;
    externalAddress /= 2;
    
    DDMACFG = (dir << 1) | (mem << 2);
    DDMAIADR = internalAddress;
    DDMAEADR = externalAddress;
    DDMANUM = num;
    DDMACOM |= 0x4; /* start */
    
    ddma_wait_idle(); /* wait for completion */
    set_irq_level(irq);
}

static void ddma_wait_idle_noicode(void)
{
    do {
    } while ((DDMACOM & 3) != 0);
}

static void ddma_transfer_noicode(int dir, int mem, long intAddr, long extAddr, int num) {
    int irq = set_irq_level(1);
    ddma_wait_idle_noicode();
    long externalAddress = (long) extAddr;
    long internalAddress = (long) intAddr;
    /* HW wants those two in word units. */
    num /= 2;
    externalAddress /= 2;
    
    DDMACFG = (dir << 1) | (mem << 2);
    DDMAIADR = internalAddress;
    DDMAEADR = externalAddress;
    DDMANUM = num;
    DDMACOM |= 0x4; /* start */
    
    ddma_wait_idle_noicode(); /* wait for completion */
    set_irq_level(irq);
}

/* Some linker-defined symbols */
extern int icodecopy;
extern int icodesize;
extern int icodestart;

/* change the a PLL frequency */
void set_pll_freq(int pll_index, long freq_out) {
    volatile unsigned int* plldata;
    volatile unsigned char* pllcon;
    if (pll_index == 0) {
        plldata = &PLL0DATA;
        pllcon = &PLL0CON;
    } else {
        plldata = &PLL1DATA;
        pllcon = &PLL1CON;
    }
    /* VC0 is 32768 Hz */
#define VC0FREQ (32768L)
    unsigned m = (freq_out / VC0FREQ) - 2;
    /* TODO: if m is too small here, use the divider bits [0,1] */
    *plldata = m << 2;
    *pllcon |= 0x1; /* activate */
    do {
    } while ((*pllcon & 0x2) == 0); /*  wait for stabilization */
}

int smsc_version(void) {
    int v;
    int* smsc_ver_addr = (int*)0x4C20;
    __asm__ ("ldc %0, @%1" : "=r"(v) : "a"(smsc_ver_addr));
    v &= 0xFF;
    if (v < 4 || v == 0xFF) {
        return 3;
    }
    return v;
}



void smsc_delay() {
    int i;
    /* FIXME: tune the delay.
       Delay doesn't depend on CPU speed in Archos' firmware.
    */
    for (i = 0; i < 100; i++) {
        
    }
}

static void extra_init(void) {
    /* Power on stuff */
    P1 |= 0x07;
    P1CON |= 0x1f;
    
    /* P5 conf
     * lines 0, 1 & 4 are digital, other analog. : 
     */
    P5CON = 0xec;

    P6CON = 0x19;

    /* P7 conf
       nothing to do: all are inputs
       (reset value of the register is good)
    */

    /* SMSC chip config (?) */
    P10CON |= 0x20;
    P6 &= 0xF7;
    P10 &= 0x20;
    smsc_delay();
    if (smsc_version() < 4) {
        P6 |= 0x08;
        P10 |= 0x20;
    }
}

void set_cpu_frequency(long frequency) {
    /* Enable SDRAM refresh, at least 15MHz */
    if (frequency < cpu_frequency)
        MIUDCNT = 0x800 | (frequency * 15/1000000L - 1);

    set_pll_freq(0, frequency);
    PLL0CON |= 0x4; /* use as CPU clock */

    cpu_frequency = frequency;
    /* wait states and such not changed by Archos. (!?) */

    /* Enable SDRAM refresh, 15MHz. */
    MIUDCNT = 0x800 | (frequency * 15/1000000L - 1);

    tick_start(1000/HZ);
    /* TODO: when uart is done; sync uart freq */
}

/* called by crt0 */
void system_init(void)
{
    /* Disable watchdog */
    WDTEN = 0xA5;

    /****************
     * GPIO ports
     */

    /* keep alive (?) -- clear the bit to prevent crash at start (??) */
    P8 = 0x00;
    P8CON = 0x01;

    /* smsc chip init (?) */
    P10 = 0x20;
    P6 = 0x08;

    P10CON = 0x20;
    P6CON = 0x08;

    /********
     * CPU
     */
    
    
    /* PLL0 (cpu osc. frequency) */
    /* set_cpu_frequency(CPU_FREQ); */


    /*******************
     * configure S(D)RAM 
     */

    /************************
     * Copy .icode section to icram
     */
    ddma_transfer_noicode(0, 0, 0x40, (long)&icodecopy, (int)&icodesize);
   

    /***************************
     * Interrupts
     */

    /* priorities ? */

    /* mask */
    IMR0 = 0;
    IMR1 = 0;


/*  IRQ0            BT INT */
/*  IRQ1           RTC INT */
/*  IRQ2            TA INT */
/*  IRQ3          TAOV INT */
/*  IRQ4            TB INT */
/*  IRQ5          TBOV INT */
/*  IRQ6            TC INT */
/*  IRQ7          TCOV INT */
/*  IRQ8           USB INT */
/*  IRQ9          PPIC INT */
/* IRQ10 UART_Rx/UART_Err/ UART_tx INT */
/* IRQ11            IIC INT */
/* IRQ12           SIO INT */
/* IRQ13           IIS0 INT */
/* IRQ14           IIS1 INT */
/* IRQ15               ­ */

    extra_init();
}



#elif defined(CPU_COLDFIRE)

#define default_interrupt(name) \
  extern __attribute__((weak,alias("UIE"))) void name (void)

static const char* const irqname[] = {
    "", "", "AccessErr","AddrErr","IllInstr", "", "","",
    "PrivVio","Trace","Line-A", "Line-F","Debug","","FormErr","Uninit",
    "","","","","","","","",
    "Spurious","Level1","Level2","Level3","Level4","Level5","Level6","Level7",
    "Trap0","Trap1","Trap2","Trap3","Trap4","Trap5","Trap6","Trap7",
    "Trap8","Trap9","Trap10","Trap11","Trap12","Trap13","Trap14","Trap15",
    "SWT","Timer0","Timer1","I2C","UART1","UART2","DMA0","DMA1",
    "DMA2","DMA3","QSPI","","","","","",
    "PDIR1FULL","PDIR2FULL","EBUTXEMPTY","IIS2TXEMPTY",
    "IIS1TXEMPTY","PDIR3FULL","PDIR3RESYN","UQ2CHANERR",
    "AUDIOTICK","PDIR2RESYN","PDIR2UNOV","PDIR1RESYN",
    "PDIR1UNOV","UQ1CHANERR","IEC2BUFATTEN","IEC2PARERR",
    "IEC2VALNOGOOD","IEC2CNEW","IEC1BUFATTEN","UCHANTXNF",
    "UCHANTXUNDER","UCHANTXEMPTY","PDIR3UNOV","IEC1PARERR",
    "IEC1VALNOGOOD","IEC1CNEW","EBUTXRESYN","EBUTXUNOV",
    "IIS2TXRESYN","IIS2TXUNOV","IIS1TXRESYN","IIS1TXUNOV",
    "GPIO0","GPI1","GPI2","GPI3","GPI4","GPI5","GPI6","GPI7",
    "","","","","","","","SOFTINT0",
    "SOFTINT1","SOFTINT2","SOFTINT3","",
    "","CDROMCRCERR","CDROMNOSYNC","CDROMILSYNC",
    "CDROMNEWBLK","","","","","","","",
    "","","","","","","","",
    "","","","","","","","",
    "","","","","","","","",
    "","","","","","","","",
    "","","","","","","","",
    "","","","","","","","",
    "","","","","","","","",
    "","","","","","","",""
};

default_interrupt (TRAP0); /* Trap #0 */
default_interrupt (TRAP1); /* Trap #1 */
default_interrupt (TRAP2); /* Trap #2 */
default_interrupt (TRAP3); /* Trap #3 */
default_interrupt (TRAP4); /* Trap #4 */
default_interrupt (TRAP5); /* Trap #5 */
default_interrupt (TRAP6); /* Trap #6 */
default_interrupt (TRAP7); /* Trap #7 */
default_interrupt (TRAP8); /* Trap #8 */
default_interrupt (TRAP9); /* Trap #9 */
default_interrupt (TRAP10); /* Trap #10 */
default_interrupt (TRAP11); /* Trap #11 */
default_interrupt (TRAP12); /* Trap #12 */
default_interrupt (TRAP13); /* Trap #13 */
default_interrupt (TRAP14); /* Trap #14 */
default_interrupt (TRAP15); /* Trap #15 */
default_interrupt (SWT); /* Software Watchdog Timer */
default_interrupt (TIMER0); /* Timer 0 */
default_interrupt (TIMER1); /* Timer 1 */
default_interrupt (I2C); /* I2C */
default_interrupt (UART1); /* UART 1 */
default_interrupt (UART2); /* UART 2 */
default_interrupt (DMA0); /* DMA 0 */
default_interrupt (DMA1); /* DMA 1 */
default_interrupt (DMA2); /* DMA 2 */
default_interrupt (DMA3); /* DMA 3 */
default_interrupt (QSPI); /* QSPI */

default_interrupt (PDIR1FULL); /* Processor data in 1 full */
default_interrupt (PDIR2FULL); /* Processor data in 2 full */
default_interrupt (EBUTXEMPTY); /* EBU transmit FIFO empty */
default_interrupt (IIS2TXEMPTY); /* IIS2 transmit FIFO empty */
default_interrupt (IIS1TXEMPTY); /* IIS1 transmit FIFO empty */
default_interrupt (PDIR3FULL); /* Processor data in 3 full */
default_interrupt (PDIR3RESYN); /* Processor data in 3 resync */
default_interrupt (UQ2CHANERR); /* IEC958-2 Rx U/Q channel error */
default_interrupt (AUDIOTICK); /* "tick" interrupt */
default_interrupt (PDIR2RESYN); /* Processor data in 2 resync */
default_interrupt (PDIR2UNOV); /* Processor data in 2 under/overrun */
default_interrupt (PDIR1RESYN); /* Processor data in 1 resync */
default_interrupt (PDIR1UNOV); /* Processor data in 1 under/overrun */
default_interrupt (UQ1CHANERR); /* IEC958-1 Rx U/Q channel error */
default_interrupt (IEC2BUFATTEN);/* IEC958-2 channel buffer full */
default_interrupt (IEC2PARERR); /* IEC958-2 Rx parity or symbol error */
default_interrupt (IEC2VALNOGOOD);/* IEC958-2 flag not good */
default_interrupt (IEC2CNEW); /* IEC958-2 New C-channel received */
default_interrupt (IEC1BUFATTEN);/* IEC958-1 channel buffer full */
default_interrupt (UCHANTXNF); /* U channel Tx reg next byte is first */
default_interrupt (UCHANTXUNDER);/* U channel Tx reg underrun */
default_interrupt (UCHANTXEMPTY);/* U channel Tx reg is empty */
default_interrupt (PDIR3UNOV); /* Processor data in 3 under/overrun */
default_interrupt (IEC1PARERR); /* IEC958-1 Rx parity or symbol error */
default_interrupt (IEC1VALNOGOOD);/* IEC958-1 flag not good */
default_interrupt (IEC1CNEW); /* IEC958-1 New C-channel received */
default_interrupt (EBUTXRESYN); /* EBU Tx FIFO resync */
default_interrupt (EBUTXUNOV); /* EBU Tx FIFO under/overrun */
default_interrupt (IIS2TXRESYN); /* IIS2 Tx FIFO resync */
default_interrupt (IIS2TXUNOV); /* IIS2 Tx FIFO under/overrun */
default_interrupt (IIS1TXRESYN); /* IIS1 Tx FIFO resync */
default_interrupt (IIS1TXUNOV); /* IIS1 Tx FIFO under/overrun */
default_interrupt (GPI0); /* GPIO interrupt 0 */
default_interrupt (GPI1); /* GPIO interrupt 1 */
default_interrupt (GPI2); /* GPIO interrupt 2 */
default_interrupt (GPI3); /* GPIO interrupt 3 */
default_interrupt (GPI4); /* GPIO interrupt 4 */
default_interrupt (GPI5); /* GPIO interrupt 5 */
default_interrupt (GPI6); /* GPIO interrupt 6 */
default_interrupt (GPI7); /* GPIO interrupt 7 */

default_interrupt (SOFTINT0); /* Software interrupt 0 */
default_interrupt (SOFTINT1); /* Software interrupt 1 */
default_interrupt (SOFTINT2); /* Software interrupt 2 */
default_interrupt (SOFTINT3); /* Software interrupt 3 */

default_interrupt (CDROMCRCERR); /* CD-ROM CRC error */
default_interrupt (CDROMNOSYNC); /* CD-ROM No sync */
default_interrupt (CDROMILSYNC); /* CD-ROM Illegal sync */
default_interrupt (CDROMNEWBLK); /* CD-ROM New block */

void UIE (void) /* Unexpected Interrupt or Exception */
{
    unsigned int format_vector, pc;
    int vector;
    char str[32];
    
    asm volatile ("move.l (52,%%sp),%0": "=r"(format_vector));
    asm volatile ("move.l (56,%%sp),%0": "=r"(pc));

    vector = (format_vector >> 18) & 0xff;
    
    /* clear screen */
    lcd_clear_display ();
#ifdef HAVE_LCD_BITMAP
    lcd_setfont(FONT_SYSFIXED);
#endif
    snprintf(str,sizeof(str),"I%02x:%s",vector,irqname[vector]);
    lcd_puts(0,0,str);
    snprintf(str,sizeof(str),"at %08x",pc);
    lcd_puts(0,1,str);
    lcd_update();
    
    /* set cpu frequency to 11mhz (to prevent overheating) */
    DCR = (DCR & ~0x01ff) | 1;
    PLLCR = 0x00000000;

    while (1)
    {
        /* check for the ON button (and !hold) */
        if ((GPIO1_READ & 0x22) == 0)
            system_reboot();
    }
}

/* reset vectors are handled in crt0.S */
void (* const vbr[]) (void) __attribute__ ((section (".vectors"))) = 
{
    UIE,UIE,UIE,UIE,UIE,UIE,
    UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,
    UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,
    UIE,UIE,UIE,TIMER0,TIMER1,UIE,UIE,UIE,
    /*          lvl 3  lvl 4             */
		
    TRAP0,TRAP1,TRAP2,TRAP3,TRAP4,TRAP5,TRAP6,TRAP7,
    TRAP8,TRAP9,TRAP10,TRAP11,TRAP12,TRAP13,TRAP14,TRAP15,
    
    SWT,UIE,UIE,I2C,UART1,UART2,DMA0,DMA1,
    DMA2,DMA3,QSPI,UIE,UIE,UIE,UIE,UIE,
    PDIR1FULL,PDIR2FULL,EBUTXEMPTY,IIS2TXEMPTY,
    IIS1TXEMPTY,PDIR3FULL,PDIR3RESYN,UQ2CHANERR,
    AUDIOTICK,PDIR2RESYN,PDIR2UNOV,PDIR1RESYN,
    PDIR1UNOV,UQ1CHANERR,IEC2BUFATTEN,IEC2PARERR,
    IEC2VALNOGOOD,IEC2CNEW,IEC1BUFATTEN,UCHANTXNF,
    UCHANTXUNDER,UCHANTXEMPTY,PDIR3UNOV,IEC1PARERR,
    IEC1VALNOGOOD,IEC1CNEW,EBUTXRESYN,EBUTXUNOV,
    IIS2TXRESYN,IIS2TXUNOV,IIS1TXRESYN,IIS1TXUNOV,
    GPI0,GPI1,GPI2,GPI3,GPI4,GPI5,GPI6,GPI7,
    UIE,UIE,UIE,UIE,UIE,UIE,UIE,SOFTINT0,
    SOFTINT1,SOFTINT2,SOFTINT3,UIE,
    UIE,CDROMCRCERR,CDROMNOSYNC,CDROMILSYNC,
    CDROMNEWBLK,UIE,UIE,UIE,UIE,UIE,UIE,UIE,

    UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,
    UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,
    UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,
    UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,
    UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,
    UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,
    UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,
    UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE,UIE
};

void system_init(void)
{
    /* Clear the accumulators. From here on it's the responsibility of
       whoever uses them to clear them after use (use movclr instruction). */
    asm volatile ("movclr.l %%acc0, %%d0\n\t"
                  "movclr.l %%acc1, %%d0\n\t"
                  "movclr.l %%acc2, %%d0\n\t"
                  "movclr.l %%acc3, %%d0\n\t"
                  : : : "d0");
}

#ifdef IRIVER_H100
#define MAX_REFRESH_TIMER 56
#define NORMAL_REFRESH_TIMER 20
#else
#define MAX_REFRESH_TIMER 28
#define NORMAL_REFRESH_TIMER 10
#endif

void set_cpu_frequency (long) __attribute__ ((section (".icode")));
void set_cpu_frequency(long frequency)
{
    switch(frequency)
    {
    case CPUFREQ_MAX:
        DCR = (DCR & ~0x01ff) | 1;   /* Refresh timer for bypass
                                            frequency */
        PLLCR &= ~1;  /* Bypass mode */
        PLLCR = 0x11853005;
        CSCR0 = 0x00000980; /* Flash: 2 wait state */
        CSCR1 = 0x00000980; /* LCD: 2 wait states */
        while(!(PLLCR & 0x80000000)) {}; /* Wait until the PLL has locked.
                                            This may take up to 10ms! */
        DCR = (DCR & ~0x01ff) | MAX_REFRESH_TIMER;   /* Refresh timer */
        cpu_frequency = CPUFREQ_MAX;
        tick_start(1000/HZ);
        IDECONFIG1 = 0x106000 | (5 << 10); /* BUFEN2 enable + CS2Pre/CS2Post */
        IDECONFIG2 = 0x40000 | (1 << 8); /* TA enable + CS2wait */
        break;
        
    case CPUFREQ_NORMAL:
        DCR = (DCR & ~0x01ff) | 1;   /* Refresh timer for bypass
                                            frequency */
        PLLCR &= ~1;  /* Bypass mode */
        PLLCR = 0x10886001;
        CSCR0 = 0x00000180; /* Flash: 0 wait states */
        CSCR1 = 0x00000180; /* LCD: 0 wait states */
        while(!(PLLCR & 0x80000000)) {}; /* Wait until the PLL has locked.
                                            This may take up to 10ms! */
        DCR = (DCR & ~0x01ff) | NORMAL_REFRESH_TIMER;   /* Refresh timer */
        cpu_frequency = CPUFREQ_NORMAL;
        tick_start(1000/HZ);
        IDECONFIG1 = 0x106000 | (5 << 10); /* BUFEN2 enable + CS2Pre/CS2Post */
        IDECONFIG2 = 0x40000 | (0 << 8); /* TA enable + CS2wait */
        break;
    default:
        DCR = (DCR & ~0x01ff) | 1;   /* Refresh timer for bypass
                                            frequency */
        PLLCR = 0x00000000;  /* Bypass mode */
        CSCR0 = 0x00000180; /* Flash: 0 wait states */
        CSCR1 = 0x00000180; /* LCD: 0 wait states */
        cpu_frequency = CPU_FREQ;
        tick_start(1000/HZ);
        IDECONFIG1 = 0x106000 | (1 << 10); /* BUFEN2 enable + CS2Pre/CS2Post */
        IDECONFIG2 = 0x40000 | (0 << 8); /* TA enable + CS2wait */
        break;
    }
}


#elif CONFIG_CPU == SH7034
#include "led.h"
#include "system.h"
#include "rolo.h"

#define default_interrupt(name,number) \
  extern __attribute__((weak,alias("UIE" #number))) void name (void); void UIE##number (void)
#define reserve_interrupt(number) \
  void UIE##number (void)

static const char* const irqname[] = {
    "", "", "", "", "IllInstr", "", "IllSltIn","","",
    "CPUAdrEr", "DMAAdrEr", "NMI", "UserBrk",
    "","","","","","","","","","","","","","","","","","","",
    "Trap32","Trap33","Trap34","Trap35","Trap36","Trap37","Trap38","Trap39",
    "Trap40","Trap41","Trap42","Trap43","Trap44","Trap45","Trap46","Trap47",
    "Trap48","Trap49","Trap50","Trap51","Trap52","Trap53","Trap54","Trap55",
    "Trap56","Trap57","Trap58","Trap59","Trap60","Trap61","Trap62","Trap63",
    "Irq0","Irq1","Irq2","Irq3","Irq4","Irq5","Irq6","Irq7",
    "Dma0","","Dma1","","Dma2","","Dma3","",
    "IMIA0","IMIB0","OVI0","", "IMIA1","IMIB1","OVI1","",
    "IMIA2","IMIB2","OVI2","", "IMIA3","IMIB3","OVI3","",
    "IMIA4","IMIB4","OVI4","",
    "Ser0Err","Ser0Rx","Ser0Tx","Ser0TE",
    "Ser1Err","Ser1Rx","Ser1Tx","Ser1TE",
    "ParityEr","A/D conv","","","Watchdog","DRAMRefr"
};



#define RESERVE_INTERRUPT(number) "\t.long\t_UIE" #number "\n"
#define DEFAULT_INTERRUPT(name, number)  "\t.weak\t_" #name \
                          "\n\t.set\t_" #name ",_UIE" #number \
                          "\n\t.long\t_" #name "\n"

asm (

/* Vector table.
 * Handled in asm because gcc 4.x doesn't allow weak aliases to symbols
 * defined in an asm block -- silly.
 * Reset vectors (0..3) are handled in crt0.S 
 */

    ".section\t.vectors,\"aw\",@progbits\n"
    DEFAULT_INTERRUPT (GII,      4)
    RESERVE_INTERRUPT (          5)
    DEFAULT_INTERRUPT (ISI,      6)
    RESERVE_INTERRUPT (          7)
    RESERVE_INTERRUPT (          8)
    DEFAULT_INTERRUPT (CPUAE,    9)
    DEFAULT_INTERRUPT (DMAAE,   10)
    DEFAULT_INTERRUPT (NMI,     11)
    DEFAULT_INTERRUPT (UB,      12)
    RESERVE_INTERRUPT (         13)
    RESERVE_INTERRUPT (         14)
    RESERVE_INTERRUPT (         15)
    RESERVE_INTERRUPT (         16) /* TCB #0 */
    RESERVE_INTERRUPT (         17) /* TCB #1 */
    RESERVE_INTERRUPT (         18) /* TCB #2 */
    RESERVE_INTERRUPT (         19) /* TCB #3 */
    RESERVE_INTERRUPT (         20) /* TCB #4 */
    RESERVE_INTERRUPT (         21) /* TCB #5 */
    RESERVE_INTERRUPT (         22) /* TCB #6 */
    RESERVE_INTERRUPT (         23) /* TCB #7 */
    RESERVE_INTERRUPT (         24) /* TCB #8 */
    RESERVE_INTERRUPT (         25) /* TCB #9 */
    RESERVE_INTERRUPT (         26) /* TCB #10 */
    RESERVE_INTERRUPT (         27) /* TCB #11 */
    RESERVE_INTERRUPT (         28) /* TCB #12 */
    RESERVE_INTERRUPT (         29) /* TCB #13 */
    RESERVE_INTERRUPT (         30) /* TCB #14 */
    RESERVE_INTERRUPT (         31) /* TCB #15 */
    DEFAULT_INTERRUPT (TRAPA32, 32)
    DEFAULT_INTERRUPT (TRAPA33, 33)
    DEFAULT_INTERRUPT (TRAPA34, 34)
    DEFAULT_INTERRUPT (TRAPA35, 35)
    DEFAULT_INTERRUPT (TRAPA36, 36)
    DEFAULT_INTERRUPT (TRAPA37, 37)
    DEFAULT_INTERRUPT (TRAPA38, 38)
    DEFAULT_INTERRUPT (TRAPA39, 39)
    DEFAULT_INTERRUPT (TRAPA40, 40)
    DEFAULT_INTERRUPT (TRAPA41, 41)
    DEFAULT_INTERRUPT (TRAPA42, 42)
    DEFAULT_INTERRUPT (TRAPA43, 43)
    DEFAULT_INTERRUPT (TRAPA44, 44)
    DEFAULT_INTERRUPT (TRAPA45, 45)
    DEFAULT_INTERRUPT (TRAPA46, 46)
    DEFAULT_INTERRUPT (TRAPA47, 47)
    DEFAULT_INTERRUPT (TRAPA48, 48)
    DEFAULT_INTERRUPT (TRAPA49, 49)
    DEFAULT_INTERRUPT (TRAPA50, 50)
    DEFAULT_INTERRUPT (TRAPA51, 51)
    DEFAULT_INTERRUPT (TRAPA52, 52)
    DEFAULT_INTERRUPT (TRAPA53, 53)
    DEFAULT_INTERRUPT (TRAPA54, 54)
    DEFAULT_INTERRUPT (TRAPA55, 55)
    DEFAULT_INTERRUPT (TRAPA56, 56)
    DEFAULT_INTERRUPT (TRAPA57, 57)
    DEFAULT_INTERRUPT (TRAPA58, 58)
    DEFAULT_INTERRUPT (TRAPA59, 59)
    DEFAULT_INTERRUPT (TRAPA60, 60)
    DEFAULT_INTERRUPT (TRAPA61, 61)
    DEFAULT_INTERRUPT (TRAPA62, 62)
    DEFAULT_INTERRUPT (TRAPA63, 63)
    DEFAULT_INTERRUPT (IRQ0,    64)
    DEFAULT_INTERRUPT (IRQ1,    65)
    DEFAULT_INTERRUPT (IRQ2,    66)
    DEFAULT_INTERRUPT (IRQ3,    67)
    DEFAULT_INTERRUPT (IRQ4,    68)
    DEFAULT_INTERRUPT (IRQ5,    69)
    DEFAULT_INTERRUPT (IRQ6,    70)
    DEFAULT_INTERRUPT (IRQ7,    71)
    DEFAULT_INTERRUPT (DEI0,    72)
    RESERVE_INTERRUPT (         73)
    DEFAULT_INTERRUPT (DEI1,    74)
    RESERVE_INTERRUPT (         75)
    DEFAULT_INTERRUPT (DEI2,    76)
    RESERVE_INTERRUPT (         77)
    DEFAULT_INTERRUPT (DEI3,    78)
    RESERVE_INTERRUPT (         79)
    DEFAULT_INTERRUPT (IMIA0,   80)
    DEFAULT_INTERRUPT (IMIB0,   81)
    DEFAULT_INTERRUPT (OVI0,    82)
    RESERVE_INTERRUPT (         83)
    DEFAULT_INTERRUPT (IMIA1,   84)
    DEFAULT_INTERRUPT (IMIB1,   85)
    DEFAULT_INTERRUPT (OVI1,    86)
    RESERVE_INTERRUPT (         87)
    DEFAULT_INTERRUPT (IMIA2,   88)
    DEFAULT_INTERRUPT (IMIB2,   89)
    DEFAULT_INTERRUPT (OVI2,    90)
    RESERVE_INTERRUPT (         91)
    DEFAULT_INTERRUPT (IMIA3,   92)
    DEFAULT_INTERRUPT (IMIB3,   93)
    DEFAULT_INTERRUPT (OVI3,    94)
    RESERVE_INTERRUPT (         95)
    DEFAULT_INTERRUPT (IMIA4,   96)
    DEFAULT_INTERRUPT (IMIB4,   97)
    DEFAULT_INTERRUPT (OVI4,    98)
    RESERVE_INTERRUPT (         99)
    DEFAULT_INTERRUPT (REI0,   100)
    DEFAULT_INTERRUPT (RXI0,   101)
    DEFAULT_INTERRUPT (TXI0,   102)
    DEFAULT_INTERRUPT (TEI0,   103)
    DEFAULT_INTERRUPT (REI1,   104)
    DEFAULT_INTERRUPT (RXI1,   105)
    DEFAULT_INTERRUPT (TXI1,   106)
    DEFAULT_INTERRUPT (TEI1,   107)
    RESERVE_INTERRUPT (        108)
    DEFAULT_INTERRUPT (ADITI,  109)

/* UIE# block.
 * Must go into the same section as the UIE() handler */

    "\t.text\n"
    "_UIE4:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE5:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE6:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE7:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE8:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE9:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE10:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE11:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE12:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE13:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE14:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE15:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE16:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE17:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE18:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE19:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE20:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE21:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE22:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE23:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE24:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE25:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE26:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE27:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE28:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE29:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE30:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE31:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE32:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE33:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE34:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE35:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE36:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE37:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE38:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE39:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE40:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE41:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE42:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE43:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE44:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE45:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE46:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE47:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE48:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE49:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE50:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE51:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE52:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE53:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE54:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE55:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE56:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE57:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE58:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE59:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE60:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE61:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE62:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE63:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE64:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE65:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE66:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE67:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE68:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE69:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE70:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE71:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE72:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE73:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE74:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE75:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE76:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE77:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE78:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE79:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE80:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE81:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE82:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE83:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE84:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE85:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE86:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE87:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE88:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE89:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE90:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE91:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE92:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE93:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE94:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE95:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE96:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE97:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE98:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE99:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE100:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE101:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE102:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE103:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE104:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE105:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE106:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE107:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE108:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"
    "_UIE109:\tbsr\t_UIE\n\tmov.l\t@r15+,r4\n"

);



void system_reboot (void)
{
    set_irq_level(HIGHEST_IRQ_LEVEL);

    asm volatile ("ldc\t%0,vbr" : : "r"(0));

    PACR2 |= 0x4000; /* for coldstart detection */
    IPRA = 0;
    IPRB = 0;
    IPRC = 0;
    IPRD = 0;
    IPRE = 0;
    ICR = 0;

    asm volatile ("jmp @%0; mov.l @%1,r15" : :
                  "r"(*(int*)0),"r"(4));
}

void UIE (unsigned int pc) /* Unexpected Interrupt or Exception */
{
    bool state = true;
    unsigned int n;
    char str[32];

    asm volatile ("sts\tpr,%0" : "=r"(n));
    
    /* clear screen */
    lcd_clear_display ();
#ifdef HAVE_LCD_BITMAP
    lcd_setfont(FONT_SYSFIXED);
#endif
    /* output exception */
    n = 0; // (n - (unsigned)UIE0 - 4)>>2; /* get exception or interrupt number */
    snprintf(str,sizeof(str),"I%02x:%s",n,irqname[n]);
    lcd_puts(0,0,str);
    snprintf(str,sizeof(str),"at %08x",pc);
    lcd_puts(0,1,str);

#ifdef HAVE_LCD_BITMAP
    lcd_update ();
#endif

    while (1)
    {
        volatile int i;
        led (state);
        state = state?false:true;
        
        for (i = 0; i < 240000; ++i);

        /* try to restart firmware if ON is pressed */
#if CONFIG_KEYPAD == PLAYER_PAD
        if (!(PADR & 0x0020))
#elif CONFIG_KEYPAD == RECORDER_PAD
#ifdef HAVE_FMADC
        if (!(PCDR & 0x0008))
#else
        if (!(PBDR & 0x0100))
#endif
#endif
            system_reboot();
    }
}


void system_init(void)
{
    /* Disable all interrupts */
    IPRA = 0;
    IPRB = 0;
    IPRC = 0;
    IPRD = 0;
    IPRE = 0;

    /* NMI level low, falling edge on all interrupts */
    ICR = 0;

    /* Enable burst and RAS down mode on DRAM */
    DCR |= 0x5000;

    /* Activate Warp mode (simultaneous internal and external mem access) */
    BCR |= 0x2000;

    /* Bus state controller initializations. These are only necessary when
       running from flash. */
    WCR1 = 0x40FD; /* Long wait states for CS6 (ATA), short for the rest. */
    WCR3 = 0x8000; /* WAIT is pulled up, 1 state inserted for CS6 */
}

/* Utilize the user break controller to catch invalid memory accesses. */
int system_memory_guard(int newmode)
{
    static const struct {
        unsigned long  addr;
        unsigned long  mask;
        unsigned short bbr;
    } modes[MAXMEMGUARD] = {
        /* catch nothing */
        { 0x00000000, 0x00000000, 0x0000 },
        /* catch writes to area 02 (flash ROM) */
        { 0x02000000, 0x00FFFFFF, 0x00F8 },
        /* catch all accesses to areas 00 (internal ROM) and 01 (free) */
        { 0x00000000, 0x01FFFFFF, 0x00FC }
    };

    int oldmode = MEMGUARD_NONE;
    int i;
    
    /* figure out the old mode from what is in the UBC regs. If the register
       values don't match any mode, assume MEMGUARD_NONE */
    for (i = MEMGUARD_NONE; i < MAXMEMGUARD; i++)
    {
        if (BAR == modes[i].addr && BAMR == modes[i].mask &&
            BBR == modes[i].bbr)
        {
            oldmode = i;
            break;
        }
    }
    
    if (newmode == MEMGUARD_KEEP)
        newmode = oldmode;

    BBR = 0;  /* switch off everything first */

    /* always set the UBC according to the mode, in case the old settings
       didn't match any valid mode */
    BAR  = modes[newmode].addr;
    BAMR = modes[newmode].mask;
    BBR  = modes[newmode].bbr;
    
    return oldmode;
}
#endif

#if CONFIG_CPU != SH7034
/* this does nothing on non-SH systems */
int system_memory_guard(int newmode)
{
    (void)newmode;
    return 0;
}

void system_reboot (void)
{
#ifdef CPU_COLDFIRE
    set_cpu_frequency(0);
    
    asm(" move.w #0x2700,%sr");
    /* Reset the cookie for the crt0 crash check */
    asm(" move.l #0,%d0");
    asm(" move.l %d0,0x10017ffc");
    asm(" movec.l %d0,%vbr");
    asm(" move.l 0,%sp");
    asm(" move.l 4,%a0");
    asm(" jmp (%a0)");
#endif
}
#endif
