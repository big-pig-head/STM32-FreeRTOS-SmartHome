#ifndef __LIGHTSENSOR_H
#define __LIGHTSENSOR_H

#define BH1750_WRITE_ADDR     0x46      //从机地址+最后写方向位
#define BH1750_Read_ADDR      0x47      //从机地址+最后读方向位

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

#endif

