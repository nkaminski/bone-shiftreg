#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h> 
#include <unistd.h> 
#include <assert.h> 
#include "shiftreg.h"

// Exposes the physical address defined in the passed structure using mmap on /dev/mem
int map_gpio(sitara_gpio_t *p)
{
	volatile unsigned int* oeaddr;
	unsigned int tmp;
	// Open /dev/mem
	if ((p->mem_fd = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) {
		fprintf(stderr, "Failed to open /dev/mem, try checking permissions.");
		return -1;
	}
	printf("%x\n",p->physaddr);
	p->virtaddr = mmap(
			NULL,
			sysconf(_SC_PAGESIZE),
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			p->mem_fd,
			p->physaddr       // Physical address
			);

	if (p->virtaddr == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	return 0;
}

void unmap_gpio(sitara_gpio_t *p)
{
	munmap((void *)p->virtaddr, sysconf(_SC_PAGESIZE));
	close(p->mem_fd);
}

inline void set_gpio(sitara_gpio_t *p, char num, char val)
{

	if(val){
		#ifdef SERDEBUG
		printf("{high, %hhu}\n", num);
		#endif
		*(uint32_t*)(p->virtaddr + GPIO_SETDATAOUT) = (1 << num);
	}else{
		#ifdef SERDEBUG
		printf("{low, %hhu}\n",num);
		#endif
		*(uint32_t*)(p->virtaddr + GPIO_CLEARDATAOUT) = (1 << num);
	}
}

void shiftout(shiftreg_t *sr, sitara_gpio_t *gpio){
	int i=0, bit, byte;
	char val;
	//Sanity checks
	assert(gpio != NULL);
	assert(sr != NULL);
	assert(gpio->virtaddr != NULL);

	//set clear and make sure clock is zero
	set_gpio(gpio,sr->clear,0);
	set_gpio(gpio,sr->serclk,0);

	//latch it
	set_gpio(gpio,sr->latch,1);
	usleep(BIT_TIME);
	set_gpio(gpio,sr->latch,0);

	//unset clear
	set_gpio(gpio,sr->clear,1);
	usleep(BIT_TIME);
	set_gpio(gpio,sr->serclk,1);
	usleep(BIT_TIME);
	set_gpio(gpio,sr->serclk,0);
	usleep(BIT_TIME);	
	for(i=0; i<sr->nbits; i++){
		byte = i/8;
		bit = i%8;
		val = (sr->dataptr[byte]) & (1 << bit);
		set_gpio(gpio,sr->ser, val);
		usleep(BIT_TIME);
		set_gpio(gpio,sr->serclk,1);
		usleep(BIT_TIME);
		set_gpio(gpio,sr->serclk,0);
		usleep(BIT_TIME);	
	}
	set_gpio(gpio,sr->latch,1);
	usleep(BIT_TIME);
	set_gpio(gpio,sr->latch,0);
	usleep(BIT_TIME);	
}

int main(int argc, char **argv)
{
	sitara_gpio_t gpio;
	shiftreg_t reg;
	assert(argc == 2);
	char data = argv[1][0];
	//Initialization
	gpio.physaddr = GPIOCHIP_BASE; 
	gpio.virtaddr = NULL;
	reg.ser = 14;
	reg.serclk = 15;
	reg.latch = 16;
	reg.clear = 17;
	reg.nbits = 8;
	reg.dataptr = &data;

	if(map_gpio(&gpio) == -1) {
		fprintf(stderr, "Failed to map the physical GPIO output register into the virtual memory space.\n");
		return -1;
	}
	shiftout(&reg, &gpio);	
	unmap_gpio(&gpio);
	return 0;
}
