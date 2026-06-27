/**************************************************************************
 * 文件名  ：MyI2C.c
 * 描述    ：软件模拟IIC程序  
****************************************************************************/

#include "stm32f10x.h"                  // Device header
#include "MyI2C.h"
#include "Delay.h"

/*引脚配置层*/

/**
  * 函    数：I2C写SCL引脚电平
  * 参    数：BitValue 协议层传入的当前需要写入SCL的电平，范围0~1
  * 返 回 值：无
  * 注意事项：此函数需要用户实现内容，当BitValue为0时，需要置SCL为低电平，当BitValue为1时，需要置SCL为高电平
  */
void MyI2C_W_SCL(uint8_t BitValue)
{
	GPIO_WriteBit(I2C_GPIO_PORT, I2C_SCL_GPIO_PIN, (BitAction)BitValue);		//根据BitValue，设置SCL引脚的电平
	Delay_us(10);												//延时10us，防止时序频率超过要求
}

/**
  * 函    数：I2C写SDA引脚电平
  * 参    数：BitValue 协议层传入的当前需要写入SDA的电平，范围0~0xFF
  * 返 回 值：无
  * 注意事项：此函数需要用户实现内容，当BitValue为0时，需要置SDA为低电平，当BitValue非0时，需要置SDA为高电平
  */
void MyI2C_W_SDA(uint8_t BitValue)
{
	GPIO_WriteBit(I2C_GPIO_PORT, I2C_SDA_GPIO_PIN, (BitAction)BitValue);		//根据BitValue，设置SDA引脚的电平，BitValue要实现非0即1的特性
	Delay_us(10);												//延时10us，防止时序频率超过要求
}

/**
  * 函    数：I2C读SDA引脚电平
  * 参    数：无
  * 返 回 值：协议层需要得到的当前SDA的电平，范围0~1
  * 注意事项：此函数需要用户实现内容，当前SDA为低电平时，返回0，当前SDA为高电平时，返回1
  */
uint8_t MyI2C_R_SDA(void)
{
	uint8_t BitValue;
	BitValue = GPIO_ReadInputDataBit(I2C_GPIO_PORT, I2C_SDA_GPIO_PIN);		//读取SDA电平
	Delay_us(10);												//延时10us，防止时序频率超过要求
	return BitValue;											//返回SDA电平
}

/**
  * 函    数：I2C初始化
  * 参    数：无
  * 返 回 值：无
  * 注意事项：此函数需要用户实现内容，实现SCL和SDA引脚的初始化
  */
void MyI2C_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(I2C_GPIO_CLK, ENABLE);	//开启GPIOB的时钟
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = I2C_SCL_GPIO_PIN | I2C_SDA_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);					//将PB10和PB11引脚初始化为开漏输出
	
	/*设置默认电平*/
	GPIO_SetBits(I2C_GPIO_PORT, I2C_SCL_GPIO_PIN | I2C_SDA_GPIO_PIN);			//设置PB10和PB11引脚初始化后默认为高电平（释放总线状态）
}

/*协议层*/

/**
  * 函    数：SDA引脚输入输出模式配置
  * 参    数：mode,定义SDA引脚模式，大于0为输出模式，否则为输入模式
  * 返 回 值：无
  */
void Set_I2C_SDAMode(uint8_t mode)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    if(mode > 0)
    {// 设置为输出模式
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏或推挽，外部需要接上拉电阻
    }
    else
    {// 设置为输入模式
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU; // 浮空或上拉，外部需要接上拉电阻
    }
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_Pin = I2C_SDA_GPIO_PIN;
    GPIO_Init(I2C_GPIO_PORT, &GPIO_InitStruct);
}

/**
  * 函    数：I2C起始
  * 参    数：无
  * 返 回 值：无
  */
void MyI2C_Start(void)
{
	MyI2C_W_SDA(1);							//释放SDA，确保SDA为高电平
	MyI2C_W_SCL(1);							//释放SCL，确保SCL为高电平
	Delay_us(5);
	MyI2C_W_SDA(0);							//在SCL高电平期间，拉低SDA，产生起始信号
	MyI2C_W_SCL(0);							//起始后把SCL也拉低，即为了占用总线，也为了方便总线时序的拼接
	Delay_us(5);
}

/**
  * 函    数：I2C终止
  * 参    数：无
  * 返 回 值：无
  */
void MyI2C_Stop(void)
{
	MyI2C_W_SCL(0);	
	MyI2C_W_SDA(0);							//拉低SDA，确保SDA为低电平
	Delay_us(5);
	MyI2C_W_SCL(1);							//释放SCL，使SCL呈现高电平
	MyI2C_W_SDA(1);							//在SCL高电平期间，释放SDA，产生终止信号
	Delay_us(5);
}

/**
  * 函    数：I2C发送应答信号
  * 参    数：ack，主机应答信号，1或0，1表示下一个时序不需要再接收数据，0表示下一个时序继续接收数据
  * 返 回 值：返回1，表示程序运行正常
  */
uint8_t MyI2C_SendAck(int ack)
{
	Set_I2C_SDAMode(1);               
	if(ack == 1)
    {// 发送ACk
        MyI2C_W_SDA(1); 
    }
	else if(ack == 0)
	{
		// 发送ACk
        MyI2C_W_SDA(0); 
	}
	else
    {
        return 0;  // 入参有误，发送失败
    }  
	MyI2C_W_SCL(1);	
	Delay_us(5);
	MyI2C_W_SCL(0);	
	Delay_us(5);
	return 1;     //发送成功返回1为真	
}

/**
  * 函    数：I2C接收应答信号
  * 参    数：无
  * 返 回 值：返回1，表示程序运行正常
  */
uint8_t MyI2C_ReciveAck(void)
{
	uint8_t ack = 0;
    uint8_t timeout = 5;

    MyI2C_W_SDA(1); 
    Set_I2C_SDAMode(0);  // SDA输入模式，就是gpio口变为输入模式，来接收ACK信号，接收完之后再变回去gpio输出模式
    MyI2C_W_SCL(1);           
    Delay_us(5);  
    while(timeout--)       
    {// 等待从设备发送ACK
        if(MyI2C_R_SDA() == 0)
        {// 读到应答信号
            ack = 1;  
        }
        else
        {// 没有读到应答信号，继续等待
            Delay_us(1);
            if(timeout == 0)
            {// 等待超时
                ack = 0; 
            }
        }
    }           
    MyI2C_W_SCL(0);             
    Delay_us(5);        
    Set_I2C_SDAMode(1); // SDA输出模式 
    return ack;
}

/**
  * 函    数：I2C发送一个字节
  * 参    数：Byte 要发送的一个字节数据，范围：0x00~0xFF
  * 返 回 值：无
  */
uint8_t MyI2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i ++)				//循环8次，主机依次发送数据的每一位
	{
		if(0x80 & Byte)
        {	
			MyI2C_W_SDA(1);
		}	
		else
		{
			MyI2C_W_SDA(0);
		}
		Byte <<= 1;
		MyI2C_W_SCL(1);						//释放SCL，从机在SCL高电平期间读取SDA
		Delay_us(5);  
		MyI2C_W_SCL(0);						//拉低SCL，主机开始发送下一位数据
		Delay_us(5);  
	}
	return MyI2C_ReciveAck();
}

/**
  * 函    数：I2C接收一个字节
  * 参    数：无
  * 返 回 值：接收到的一个字节数据，范围：0x00~0xFF
  */
uint8_t MyI2C_ReceiveByte(void)
{
	uint8_t i, Byte = 0,bit;				//定义接收的数据，并赋初值0x00，此处必须赋初值0x00，后面会用到
	Set_I2C_SDAMode(0);                     //SDA输入模式 
	for (i = 0; i < 8; i ++)				//循环8次，主机依次接收数据的每一位
	{
		Byte <<= 1;
		MyI2C_W_SCL(1);						//释放SCL，主机机在SCL高电平期间读取SDA
		Delay_us(5);
		if (MyI2C_R_SDA() == 1)
		{
			bit = 0x80;
		}	
		else
		{
			bit = 0x00;
		}
		Byte |= bit; 
		MyI2C_W_SCL(0);						//拉低SCL，从机在SCL低电平期间写入SDA
		Delay_us(5);
	}
	Set_I2C_SDAMode(1);                     //SDA输出模式 
	return Byte;							//返回接收到的一个字节数据
}

/**
  * 函    数：I2C发送一个字节
  * 参    数：addr：发送设备地址，
  * 返 回 值：返回应答信号，返回1说明发送成功，返回0发送失败
  */
uint8_t MyI2C_SendBytes(uint8_t addr, uint8_t *buf, uint8_t buf_size)
{
    uint8_t i;
    uint8_t result = 0;
	MyI2C_Start();
    if(MyI2C_SendByte(addr << 1))    // 发送设备地址(7bit地址)
    {// 收到应答，发送成功
        for (i = 0; i < buf_size; i++)  // 发送数据
        {
            if(MyI2C_SendByte(buf[i]))
            {// 收到应答，发送成功
                result = 1;
            }
            else
            {// 没有收到应答，发送失败
                result = 0; 
            }
        }
    }
    MyI2C_Stop();                   // 发送停止信号
    return result;
}

/* IIC接收多个数据 */
uint8_t MyI2C_ReceiveBytes(uint8_t addr, uint8_t *buf, uint8_t buf_size)
{
    uint8_t i;    
    uint8_t result = 0;
	MyI2C_Start();
    if(MyI2C_SendByte((addr << 1) | 1))  // 发送设备地址(7bit地址)
    {
        for (i = 0; i < buf_size; i++)    // 连续读取数据
        {
            *buf++ = MyI2C_ReceiveByte(); 
            if (i == buf_size - 1)
            {
                MyI2C_SendAck(1);        // 最后一个数据需要回NACK
            }
            else
            {        
                MyI2C_SendAck(0);        // 发送ACK
            }
        }
        result = 1;
    }
    MyI2C_Stop();                   // 发送停止信号

	return result;
}

