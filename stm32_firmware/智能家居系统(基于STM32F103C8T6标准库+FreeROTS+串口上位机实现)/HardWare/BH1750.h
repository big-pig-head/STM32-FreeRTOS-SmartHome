#ifndef __BH1750_H__
#define __BH1750_H__

#include "stm32f10x.h"

#define BH1750_ADDRESS_LOW      0x23    // addr low  7bit:0x23 8bit:0x46
#define BH1750_ADDRESS_HIGH     0x5C    // addr high 7bit:0x5C 8bit:0xB8

/*bh1750 registers define */
#define BH1750_POWER_ON			0x01	// power on
#define BH1750_POWER_DOWN   	0x00	// power down
#define BH1750_RESET			0x07	// reset	
#define BH1750_CON_H_RES_MODE	0x10	// Continuously H-Resolution Mode
#define BH1750_CON_H_RES_MODE2	0x11	// Continuously H-Resolution Mode2 
#define BH1750_CON_L_RES_MODE	0x13	// Continuously L-Resolution Mode
#define BH1750_ONE_H_RES_MODE	0x20	// One Time H-Resolution Mode
#define BH1750_ONE_H_RES_MODE2	0x21	// One Time H-Resolution Mode2
#define BH1750_ONE_L_RES_MODE	0x23	// One Time L-Resolution Mode

uint8_t bh1750_init(void);
void bh1750_read_example(void);

#endif
