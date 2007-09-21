// Prototypes for ibus functions

/*void timer_init(unsigned hz, unsigned to); // setup static timer registers and values
void timer_set_mode(int mode); // define for what the timer should be used right now

void uart_init(unsigned baudrate); // UART setup
*/

// receive status
#define RX_BUSY      0 // reception in progress
#define RX_RECEIVED  1 // valid data available
#define RX_FRAMING   2 // frame error
#define RX_OVERRUN   3 // receiver overrun
#define RX_PARITY    4 // parity error
#define RX_SYMBOL    5 // invalid bit timing
#define RX_OVERFLOW  6 // buffer full
#define RX_OVERLAP   7 // receive interrupt while transmitting


#define IBUS_MAX_SIZE      255 // maximum length of an I-Bus packet, incl. checksum

typedef struct {
	unsigned char buffer[4*IBUS_MAX_SIZE]; // incoming line buffer
	unsigned buf_start;
	unsigned buf_end;
} t_gRcvBuffer;

extern t_gRcvBuffer gRcvBuffer;

bool ibus_urgent_message(void);
void ibus_init(void); // prepare the I-Bus layer
bool ibus_send(unsigned char source, unsigned char dest, unsigned char len, unsigned char *message);
int ibus_send_retry(unsigned char source, unsigned char dest, unsigned char len, unsigned char *message);
int ibus_recv(unsigned char *msg, unsigned char bufsize, int timeout);
bool ibus_send_byte(unsigned char b);
bool ibus_check_recv_message(unsigned char *message, unsigned char len);
unsigned char ibus_recipient(unsigned char *message, unsigned char len);

unsigned char compute_checksum(unsigned char *msg, unsigned char len);

void dump_packet(char* dest, int dst_size, char* src, int n);
