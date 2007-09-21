/****************** imports ******************/

#include "sh7034.h"
#include "system.h"
#include "kernel.h"
#include "string.h"
#include "stdio.h"
#include "lcd.h"
#include "stdlib.h"

#include "ibus.h"
#include "ibus_cdc.h"

// Function prototypes not meant to be visible outside this file
void transmit_isr(void); // 2nd level ISR for I-Bus transmission
void timer4_isr(void) __attribute__((interrupt_handler)); // IMIA4 ISR
void uart_err_isr(void) __attribute__((interrupt_handler)); // ERI1 ISR
void uart_rx_isr(void) __attribute__((interrupt_handler)); // RXI1 ISR
void receive_timeout_isr(void); // 2nd level ISR for receiver timeout


/****************** constants ******************/

// bit time on the I-Bus is 0.10416 ms = 9600 Hz

//#define IBUS_BAUDRATE (9600*10)
//#define IBUS_BIT_FREQ (IBUS_BAUDRATE/10) // the true bit frequency again
//#define IBUS_STEP_FREQ (IBUS_BAUDRATE/5) // 2 steps per bit

#define IBUS_STEPS_PER_BIT	2
#define IBUS_BAUDRATE	9600
#define IBUS_BIT_FREQ	IBUS_BAUDRATE
#define IBUS_STEP_FREQ	(IBUS_BIT_FREQ*IBUS_STEPS_PER_BIT)

#define IBUS_RX_TIMEOUT	   180


#define IBUS_RCV_QUEUESIZE  8 // how many packets can be queued by receiver

#define IMIA4 (*((volatile unsigned long*)0x09000180)) // timer 4
#define ERI1  (*((volatile unsigned long*)0x090001A0)) // RX error
#define RXI1  (*((volatile unsigned long*)0x090001A4)) // RX

//#define _USE_SERIAL_MOD

#define PB10 0x0400
#define PB11 0x0800

#ifdef _USE_SERIAL_MOD
#define TXBIT	PB11
#else
#define TXBIT	PB10	// Use PB10 to make RX/TX on the same pin
#endif

#define TXMASK	(TXBIT>>8)


// timer operation mode
#define TM_OFF        0 // not in use
#define TM_TRANSMIT   1 // periodic timer to transmit
#define TM_RX_TIMEOUT 2 // single shot for receive timeout




/****************** data types ******************/

// one entry in the receive queue
typedef struct
{
    unsigned char buf[IBUS_MAX_SIZE]; // message buffer
    unsigned size; // length of data in the buffer
    unsigned error; // error code from reception
} t_rcv_queue_entry;


/****************** globals ******************/

t_gRcvBuffer gRcvBuffer;

// information owned by the timer transmit ISR
struct 
{
    unsigned char send_buf[IBUS_MAX_SIZE]; // I-Bus message
    unsigned send_size; // current length of data in the buffer
    unsigned index; // index for which byte to send
    unsigned char byte; // which byte to send
    unsigned int bitmask; // which bit to send (bit = startbit, bit 1=byte's bit 0, ..., bit 9 = bit 8, bit 10 = parity bit, ...
    int step; // where in the pulse are we
    bool bit; // currently sent bit
	bool parity; // true if we need to send a parity bit
    bool collision; // set if a collision happened
    bool busy; // flag if in transmission
} gSendIRQ;


// information owned by the UART receive ISR
struct 
{
    t_rcv_queue_entry queue[IBUS_RCV_QUEUESIZE]; // I-Bus message queue
    unsigned buf_read; // readout maintained by the user application
    unsigned buf_write; // writing maintained by ISR
    bool overflow; // indicate queue overflow
    unsigned byte; // currently assembled byte
} gRcvIRQ;


// information owned by the timer
struct
{
    unsigned mode; // off, transmit, receive timout
    unsigned gra_transmit; // value for transmit
    unsigned gra_timeout; // value for receive timeout
} gTimer;

extern int idle_time;


// helper function for debugging purposes
// it dumps a packet into a string of hex bytes separated by spaces
void dump_packet(char* dest, int dst_size, char* src, int n)
{
    int i;
    int len = MIN(dst_size/3, n);
	unsigned char hi, lo;

    for (i=0; i<len; i++)
    {   // convert to hex digits
		hi = (src[i] & 0xF0) >> 4;
		lo = src[i] & 0x0F;
        dest[i*3] = hi < 10 ? '0' + hi : 'A' + hi - 10;
        dest[i*3+1] = lo < 10 ? '0' + lo : 'A' + lo - 10;
		dest[i*3+2] = ' ';
    }
    dest[i*3] = '\0'; // zero terminate string
}



/****************** implementation ******************/
void timer_init(unsigned hz, unsigned to)
{
    memset(&gTimer, 0, sizeof(gTimer));
    
    and_b(~0x10, &TSTR); // Stop the timer 4
    and_b(~0x10, &TSNC); // No synchronization
    and_b(~0x10, &TMDR); // Operate normally

    IMIA4 = (unsigned long)timer4_isr; // install ISR

    gTimer.gra_transmit = FREQ / hz - 1; // time for bit transitions
    gTimer.gra_timeout = FREQ / to - 1; // time for receive timeout

    TSR4 &= ~0x01;
    TIER4 = 0xF9; // Enable GRA match interrupt
}

// define for what the timer should be used right now
void timer_set_mode(int mode)
{
	// I guess we should stop the timer first... (FPB)
//	and_b(~0x10, &TSTR);

    TCNT4 = 0; // start counting at 0
    gTimer.mode = mode; // store the mode

    if (mode == TM_RX_TIMEOUT)
    {
        GRA4 = gTimer.gra_timeout;
        TCR4 = 0x00; // no clear at GRA match, sysclock/1
        IPRD = (IPRD & 0xFF0F) | 0x00B0; // interrupt priority 11
        or_b(0x10, &TSTR); // start timer 4
    }
    else if (mode == TM_TRANSMIT)
    {
        GRA4 = gTimer.gra_transmit;
        TCR4 = 0x20; // clear at GRA match, sysclock/1
        //IPRD = (IPRD & 0xFF0F) | 0x0020; // interrupt priority 14
        IPRD = (IPRD & 0xFF0F) | 0x00F0; // interrupt priority 14
        or_b(0x10, &TSTR); // start timer 4
    }
    else
    {
        and_b(~0x10, &TSTR); // stop the timer 4
        IPRD = (IPRD & 0xFF0F); // disable interrupt
    }
}


void timer4_isr(void) // IMIA4
{
    TSR4 &= ~0x01; // clear the interrupt
    switch (gTimer.mode)
    {   // distribute the interrupt
    case TM_TRANSMIT:
        transmit_isr();
        break;
    case TM_RX_TIMEOUT:
        receive_timeout_isr();
        break;
    default:
        timer_set_mode(TM_OFF); // spurious interrupt
    }
}

static unsigned char even_map[] = { 
	/* 0000 */ 0,
	/* 0001 */ 1,
	/* 0010 */ 1,
	/* 0011 */ 0,
	/* 0100 */ 1,
	/* 0101 */ 0,
	/* 0110 */ 0,
	/* 0111 */ 1,
	/* 1000 */ 1,
	/* 1001 */ 0,
	/* 1010 */ 0,
	/* 1011 */ 1,
	/* 1100 */ 0,
	/* 1101 */ 1,
	/* 1110 */ 1,
	/* 1111 */ 0
};

bool compute_even(unsigned char data)
{
	return (even_map[data&0x0F] ^ even_map[(data >> 4)&0x0F])!=0;
}


// 2nd level ISR for I-Bus transmission
void transmit_isr(void)
{
    bool exit = false;
    TSR4 &= ~0x01; // clear the interrupt

    switch(gSendIRQ.step++)
    {	
	case -3:
		// We are at the beggining of transmission
		// we need to make sure the line is free for 1.5 bits long
		and_b(~TXMASK, &PBIORH); // float PB10/PB11 (input);
/*	case -8:
	case -7:
	case -6:
	case -5:
	case -4:
	case -3:
*/	case -2:
	case -1:
        if ((PBDR & PB10) == 0)
            gSendIRQ.collision = true;
		break;

 	case 0:
		// we are at the beggining of a new bit
		// Let's see what we need to send

		if(gSendIRQ.bitmask == 0x001) {
			// we need to send a startbit = 0
			gSendIRQ.bit = false;
		}

		else if(gSendIRQ.bitmask == 0x200 ) {
			// we need to send the parity bit
			if(gSendIRQ.parity) gSendIRQ.bit = compute_even(gSendIRQ.byte);
			else // we don't need to send a parity bit after all...
				gSendIRQ.bitmask <<= 1; // Let's shift to the stopbit instead
		}

		if(gSendIRQ.bitmask == 0x400) {
			// stopbit is always one ('high')
			gSendIRQ.bit = true;
		}

		// This is where we really send the bits...

        if (gSendIRQ.bit) {// sending "one"? 
			and_b(~TXMASK, &PBIORH); // float PB10/PB11 (input);

		}
		else {
			and_b(~TXMASK, &PBDRH); // low on PB10/PB11
			or_b(TXMASK, &PBIORH); // drive PB10/PB11 low (output)
		}
       break;
//  case 1: 
// 	case 2:
//  case 3: 
//	case 4:
//	case 5:
//	case 6:
//	case 7:
//	case 8:
//          if (gSendIRQ.bit && ((PBDR & PB10) == 0))
//              gSendIRQ.collision = true;
//  		break;
	default:
        if (gSendIRQ.bit && ((PBDR & PB10) == 0))
            gSendIRQ.collision = true;

          // prepare next round
        gSendIRQ.step = 0;

		gSendIRQ.bitmask <<= 1;

		if (gSendIRQ.bitmask > 0x001 && gSendIRQ.bitmask < 0x200)
		{   // new bit of the current byte
			gSendIRQ.bit = (gSendIRQ.byte & (gSendIRQ.bitmask>>1));
		}
		else if(gSendIRQ.bitmask == 0x800)
		{   // new byte
			if (++gSendIRQ.index < gSendIRQ.send_size)
			{
				gSendIRQ.bitmask = 0x001;
				gSendIRQ.parity = true;
				gSendIRQ.byte = gSendIRQ.send_buf[gSendIRQ.index];
			}
			else
				exit = true; // done
		}
		break;
    }

    if (exit || gSendIRQ.collision)
    {   // stop transmission
        or_b(0x20, PBCR1_ADDR+1); // RxD1 again for PB10
        timer_set_mode(TM_OFF); // stop the timer
        gSendIRQ.busy = false; // do this last, to avoid race conditions
    }
}

void uart_init(unsigned baudrate)
{
    RXI1 = (unsigned long)uart_rx_isr; // install ISR
    ERI1 = (unsigned long)uart_err_isr; // install ISR

    SCR1 = 0x00; // disable everything; select async mode with SCK pin as I/O
    SMR1 = 0x20; // async, 8N1, NoMultiProc, sysclock/1
    BRR1 = ((FREQ/(32*baudrate))-1);
                 
    IPRE = (IPRE & ~0xf000) | 0xc000; // interrupt on level 12

    sleep(HZ/50); // hardware needs to settle for at least one bit interval

#ifdef _USE_SERIAL_MOD
	and_b(~0xC0, PBCR1_ADDR+1);	// GPIO for PB11
#endif

    and_b(~(SCI_RDRF | SCI_ORER | SCI_FER | SCI_PER), &SSR1); // clear any receiver flag
    or_b(SCI_RE | SCI_RIE , &SCR1); // enable the receiver with interrupt
}

void uart_rx_isr(void) // RXI1
{
    unsigned char data;
    t_rcv_queue_entry* p_entry = &gRcvIRQ.queue[gRcvIRQ.buf_write]; // short cut
    
    data = RDR1; // get data
    
    and_b(~SCI_RDRF, &SSR1); // clear data received flag

    if (gTimer.mode == TM_TRANSMIT)
        p_entry->error = RX_OVERLAP; // oops, we're also transmitting, stop
    else
        timer_set_mode(TM_RX_TIMEOUT); // (re)spawn timeout

    if (p_entry->error != RX_BUSY)
        return;

	gRcvIRQ.byte = data;

    if (p_entry->error == RX_BUSY)
    {
	   // byte completed
       if (p_entry->size >= sizeof(p_entry->buf))
       {
           p_entry->error = RX_OVERFLOW; // buffer full
       }
       else
       {
            p_entry->buf[p_entry->size] = gRcvIRQ.byte;
            gRcvIRQ.byte = 0;
            p_entry->size++;
       }
    }
}


void uart_err_isr(void) // ERI1
{
    t_rcv_queue_entry* p_entry = &gRcvIRQ.queue[gRcvIRQ.buf_write]; // short cut

    if (p_entry->error == RX_BUSY)
    {   // terminate reception in case of error
        if (SSR1 & SCI_FER) 
            p_entry->error = RX_FRAMING;
        else if (SSR1 & SCI_ORER)
            p_entry->error = RX_OVERRUN;
        else if (SSR1 & SCI_PER)
            p_entry->error = RX_PARITY;
    }

    // clear any receiver flag
    and_b(~(SCI_RDRF | SCI_ORER | SCI_FER | SCI_PER), &SSR1);
}

// 2nd level ISR for receiver timeout, this finalizes reception
void receive_timeout_isr(void)
{
    t_rcv_queue_entry* p_entry = &gRcvIRQ.queue[gRcvIRQ.buf_write]; // short cut

    timer_set_mode(TM_OFF); // single shot

    if (p_entry->error == RX_BUSY) // everthing OK so far?
        p_entry->error = RX_RECEIVED; // end with valid data

    // move to next queue entry
    gRcvIRQ.buf_write++;
    if (gRcvIRQ.buf_write >= IBUS_RCV_QUEUESIZE)
        gRcvIRQ.buf_write = 0;
    p_entry = &gRcvIRQ.queue[gRcvIRQ.buf_write];

    if (gRcvIRQ.buf_write == gRcvIRQ.buf_read)
    {   // queue overflow
        gRcvIRQ.overflow = true;
        // what can I do? Continuing overwrites the oldest.
    }

    gRcvIRQ.byte = 0;
    p_entry->size = 0;
    p_entry->error = RX_BUSY; // enable receive on new entry
}



// generate the checksum
unsigned char compute_checksum(unsigned char *msg, unsigned char len)
{
	int chk = 0;
	int i;

	for(i=0; i<len; i++)
		chk ^= msg[i];

	return chk;
}


// General send/receive routines for IBUS communication

void ibus_init(void)
{
	// init the send object
	memset(&gSendIRQ, 0, sizeof(gSendIRQ));
	timer_init(IBUS_STEP_FREQ, (IBUS_BIT_FREQ*10)/IBUS_RX_TIMEOUT); // setup frequency and timeout

	// init receiver
	memset(&gRcvIRQ, 0, sizeof(gRcvIRQ));
	uart_init(IBUS_BIT_FREQ);

	gRcvBuffer.buf_start = gRcvBuffer.buf_end = 0;
}


bool ibus_send(unsigned char source, unsigned char dest, unsigned char len, unsigned char *message)
{
	// wait for previous transmit/receive to end
	while(gTimer.mode != TM_OFF) //wait for "free line"
		sleep(1);

	// fill in our part
	gSendIRQ.send_buf[0] = source;
	gSendIRQ.send_buf[1] = len+2;
	gSendIRQ.send_buf[2] = dest;
	memcpy(gSendIRQ.send_buf+3, message, len);
	// Append checksum
	gSendIRQ.send_buf[len+3] = compute_checksum(gSendIRQ.send_buf, len+3);

	gSendIRQ.send_size = len + 4;

	// prepare everything so the ISR can start right away
	gSendIRQ.index = 0;
	gSendIRQ.byte = gSendIRQ.send_buf[gSendIRQ.index];
	gSendIRQ.bitmask = 0x01;
	gSendIRQ.step = -3;	//-3 for polling before sending
	gSendIRQ.parity = true;
	gSendIRQ.collision = false;
	gSendIRQ.busy = true;

	// last chance to wait for a new detected receive to end
	while(gTimer.mode != TM_OFF) // wait for "free line"
		sleep(1);

	// We should poll the line for 1.5 bits to see if it is always low

	and_b(~0x30, PBCR1_ADDR+1);	// GPIO for PB10
	timer_set_mode(TM_TRANSMIT); // run

	// make the call blocking until send out
	sleep((len+4)*(10 + gSendIRQ.parity)*HZ/IBUS_BIT_FREQ); // should take this long

	while(gSendIRQ.busy) // poll in case it lasts longer
		sleep(1); // (sould not happen)

	return gSendIRQ.collision;
}

int ibus_send_retry(unsigned char source, unsigned char dest, unsigned char len, unsigned char *message)
{
	int delay = rand() % 160 + 40;
	int retry = 0;
	while( ibus_send(source, dest, len, message) ) {
		sleep(HZ/delay);
		retry++;
	}

	return retry;
}


// returns the size of a message copy, 0 if timed out, negative on error
int ibus_recv(unsigned char *msg, unsigned char bufsize, int timeout)
{
	int retval = 0;

	do {
		if(gRcvIRQ.buf_read != gRcvIRQ.buf_write)
		{ // something in the queue
			t_rcv_queue_entry *p_entry = &gRcvIRQ.queue[gRcvIRQ.buf_read]; // short cut

			if(p_entry->error == RX_RECEIVED)
			{
				memcpy(msg, p_entry->buf, MIN(p_entry->size, bufsize));
				retval = p_entry->size; // return the message size
				idle_time = 0;
			}
/*			else if(p_entry->size > 0) { // ADDED_FPB
				memcpy(msg, p_entry->buf, MIN(p_entry->size, bufsize));
				retval = p_entry->size;
				p_entry->error = RX_RECEIVED;
			}
*/			else 
			{
				// an error occured
				retval = - p_entry->error; // return negative number
			}

			// next queue readout position
			gRcvIRQ.buf_read++;
			if(gRcvIRQ.buf_read >= IBUS_RCV_QUEUESIZE)
				gRcvIRQ.buf_read = 0;

			return retval; // exit
		}

		if(timeout != 0 || gTimer.mode != TM_OFF) // also carry on if reception in progress
		{
			if(timeout != -1 && timeout != 0) // if not infinite or expired
				timeout--;

			sleep(1); // wait for a while
		}
	} while (timeout != 0 || gTimer.mode != TM_OFF);

	return 0; // timeout

}









bool ibus_check_recv_message(unsigned char *message, unsigned char len)
{
	int l = MIN(len-1, message[1]+1);
	unsigned char c_chk = compute_checksum(message, l);
	return c_chk == message[message[1]+1];
}

unsigned char ibus_recipient(unsigned char *message, unsigned char len)
{
	if(len <2) return 0;
	return message[2];
}




