#ifndef __MYI2C_H
#define __MYI2C_H

/*这里将软件I2C使用引脚及外设定义在.H文件里，方便以后更换外设时修改,移植时只需要对这四个定义修改即可*/
#define      I2C_GPIO_CLK                	RCC_APB2Periph_GPIOB
#define      I2C_GPIO_PORT                 	GPIOB
#define      I2C_SCL_GPIO_PIN            	GPIO_Pin_13
#define      I2C_SDA_GPIO_PIN               GPIO_Pin_14

void MyI2C_W_SCL(uint8_t BitValue);
void MyI2C_W_SDA(uint8_t BitValue);
uint8_t MyI2C_R_SDA(void);
void MyI2C_Init(void);
void Set_I2C_SDAMode(uint8_t mode);
void MyI2C_Start(void);
void MyI2C_Stop(void);	
uint8_t MyI2C_SendAck(int ack);
uint8_t MyI2C_ReciveAck(void);
uint8_t MyI2C_SendByte(uint8_t Byte);
uint8_t MyI2C_ReceiveByte(void);
uint8_t MyI2C_SendBytes(uint8_t addr, uint8_t *buf, uint8_t buf_size);
uint8_t MyI2C_ReceiveBytes(uint8_t addr, uint8_t *buf, uint8_t buf_size);

#endif



