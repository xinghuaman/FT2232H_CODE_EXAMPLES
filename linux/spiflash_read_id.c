// credits 
// https://gist.github.com/bjornvaktaren/d2461738ec44e3ad8b3bae4ce69445b4
// https://github.com/flashrom/flashrom/blob/master/ft2232_spi.c

// gcc spiflash_read_id.c -o spiflash_read_id  -lftdi

#include <ftdi.h>
#include <usb.h>
#include <stdio.h>
#include <string.h>

// FT2232H 
#define VENDOR 		0x0403
#define PRODUCT 	0x6010


#define CK  0x01 // AD0 SPI clock
#define DO 	0x02 // AD1 SPI data out
#define DI  0x04 // AD2 SPI data in
#define CS  0x08 // AD3 SPI chip select
#define L0  0x10 // AD4 GPIOL0
#define L1  0x20 // AD5 GPIOL1
#define L2  0x40 // AD6 GPIOL2
#define L3  0x80 // AD7 GPIOL3



// idle : 0x08  CS=high, DI=low, DO=low, SK=low
// outpins: 0x0B  CS=output, DI=input, DO=output, SK=output

static uint8_t outpins = CS | DO | CK;

struct ftdi_context* ftdi;

int spi_init(void);
int spi_rw_buffer(uint8_t* pBuffer, int numBytes);



int main(void){
   	int ftdi_status = 0;
    ftdi = ftdi_new();
   	if ( ftdi == NULL ) {
		fprintf(stderr, "Failed to initialize device\r\n");
		return 1;
		}
    
   	ftdi_status = ftdi_usb_open(ftdi, VENDOR, PRODUCT);
   	if ( ftdi_status != 0 ) {
      	fprintf(stderr, "Can't open device. Got error %s\r\n", ftdi_get_error_string(ftdi));
      	return 1;
   		}
    //unsigned int chipid;
    //printf("ftdi_read_chipid: %d\n", ftdi_read_chipid(ftdi, &chipid));
    //printf("FTDI chipid: %X\n", chipid);
    
   	ftdi_usb_reset(ftdi);
   	ftdi_set_interface(ftdi, INTERFACE_A);
  	ftdi_set_bitmode(ftdi, 0, 0); // reset
   	ftdi_set_bitmode(ftdi, 0, BITMODE_MPSSE); // enable mpsse on all bits
   	ftdi_usb_purge_buffers(ftdi);
   	usleep(50000); // sleep 50 ms for setup to complete
   
    ///////////////////// read JEDEC ID register in W25Q16 spi flash ////////////////

	spi_init();
	uint8_t rwbuf[4] = {0x9F, 0, 0, 0}; 
	spi_rw_buffer(rwbuf, 4);
    printf("JEDEC ID : %02X%02X%02X\r\n", rwbuf[1],rwbuf[2],rwbuf[3]);

	/////////////////////////////////////////////////////////////////////////////////
	
	    
   	ftdi_usb_reset(ftdi);
   	ftdi_usb_close(ftdi);
   	return 0;
	}
	
	
	
int spi_init(void) {
	int inx = 0;
	uint8_t buf[256] = {0};
	// Setup MPSSE; Operation code followed by 0 or more arguments.
	buf[inx++] = TCK_DIVISOR;     // opcode: set clk divisor
	// spi clock = 12/divisor = 1 MHz
	buf[inx++] = 0x05;            // (divisor/2 - 1) low byte
	buf[inx++] = 0x00;            // (divisor/2 - 1) high byte
	buf[inx++] = DIS_ADAPTIVE;    // opcode: disable adaptive clocking
	buf[inx++] = DIS_3_PHASE;     // opcode: disable 3-phase clocking
	buf[inx++] = SET_BITS_LOW;    // opcode: set low bits (AD[0-7])
	buf[inx++] = CS; // argument: inital pin states
	buf[inx++] = outpins;    // argument: pin direction
	
	// Write the setup to the chip.
	if ( ftdi_write_data(ftdi, buf, inx) != inx ) {
		fprintf(stderr,"Write failed\r\n");
		return -1;
		}
	return 0;
	}

int spi_rw_buffer(uint8_t* pBuffer, int numBytes) {
	int inx = 0;
	uint8_t buf[256] = {0};

	// assert CS (active low)
	buf[inx++] = SET_BITS_LOW;
	buf[inx++] = 0;
	buf[inx++] = outpins;

	// commands to write and read n bytes in SPI0 (polarity = phase = 0) mode
	buf[inx++] = MPSSE_DO_WRITE | MPSSE_WRITE_NEG | MPSSE_DO_READ;
	buf[inx++] = (numBytes - 1) & 0xff; // length-1 low byte
	buf[inx++] = ((numBytes - 1)>>8) & 0xff; // length-1 high byte
	memcpy(buf+inx, pBuffer, numBytes);
	inx += numBytes;  

	// de-assert CS
	buf[inx++] = SET_BITS_LOW;
	buf[inx++] = CS;
	buf[inx++] = outpins;
	//printf("Writing: 0x");
	//for ( int i = 0; i < inx; ++i ) {
	//	printf("%02X",buf[i]);
	//	}
	//printf("\r\n");
	
	ftdi_usb_purge_tx_buffer(ftdi);
	if ( ftdi_write_data(ftdi, buf, inx) != inx ) {
		fprintf(stderr, "spi write failed\r\n");
		return -1;
		}	

	// now get the data we  just read from the chip
	uint8_t readBuf[256] = {0};
	if ( ftdi_read_data(ftdi, readBuf, numBytes) != numBytes ) {
		fprintf(stderr, "spi read failed\r\n");
		return -1;
		}
	else{
		memcpy(pBuffer, readBuf, numBytes); 
		}
	return 0;
	}

	
