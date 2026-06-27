/**
*Encoding:GB2312
*文件功能:温湿度传感器功能实现
**/

#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "stdio.h"
#include "DHT11.h"

uint8_t DHT11_Result[5] = {0};


/**
*函数：DHT11初始化函数
*参数：无
*返回值：无
**/
void DHT11_Init(void)
{
	/*打开时钟外设*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);         //开启GPIOA的时钟
	/*配置GPIO*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;             //开漏输出
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);						
	GPIO_SetBits(GPIOA,GPIO_Pin_8);                             //DHT11采用单总线协议进行数据传输，DHT11空闲状态置为高电平               
	
}

/**
*函数：DHT11初始化函数
*参数：无
*返回值：无
*注释:总线空闲状态应为高电平，主机把总线拉低等待DHT11响应，且拉低时间必须大于18ms
**/
void DHT11_Start(void)
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_8);        
	Delay_ms(20);                           //主机拉低电平至少18ms发出开始信号
	GPIO_SetBits(GPIOA,GPIO_Pin_8);   
	Delay_us(20);                           //主机拉高电平延时20~40us等待DHT11发出响应信号
}

/**
*函数：DHT11响应函数
*参数：无
*返回值：无
*注释:DHT11接收到主机开始信号后进行响应
**/
uint8_t DHT11_Response(void)
{
	uint16_t time = 0;
	while(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8)&& time < 100)	           // 开始信号发出后处于高电平状态，接收信号后主机应该收到低电平，但是可能不会立即切换，因此用循环消耗一下这个切换时间
	{
		Delay_us(1);
		time++;
	}	
	if (time >= 100)
		return 1;                                                         // 返回1说明响应超时，可能电路出现问题
	time = 0;
	while(!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8)&& time < 100)	      // 真正的DHT11响应信号，80us低电平信号
	{
		Delay_us(1);
		time++;
	}	
	if (time >= 100)
		return 1;                     // 返回1说明响应超时，可能电路出现问题
	return 0;
}

/**
*函数：接收DHT11数据函数
*参数：无
*返回值：无
**/
uint8_t DHT11_Read_Bit(void)
{
	uint16_t time = 0;
	
	while (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8)&& time < 100)       //DHT11响应信号发出结束后，先输出80us高电平，准备发送数据
	{
		Delay_us(1);
		time++;
	}
	
	if (time >= 100)
		return 2;                    //返回2说明响应超时，可能电路出现问题	
	time = 0;
	while (!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8) && time < 100)      //DHT11发出50us低电平信号后发送传感器测量数据
	{
		Delay_us(1);
		time++;
	}
	if (time >= 30)                 //返回2说明响应超时，可能电路出现问题	
		return 2;
	
	Delay_us(30);                   //延时30us
	
	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8) == 0)     
		return 0;
	else                                                     // 如果30us后还是高电平，说明DHT11发送的是1
		return 1;
}


/**
*函数：接收DHT11字节数据函数
*参数：无
*返回值：无
**/
uint8_t DHT11_Read_Byte(void)
{
	uint8_t data = 0;
	uint8_t i = 0;
	
	for (i = 0; i < 8; i++)
	{
		data <<= 1;
		data = data | DHT11_Read_Bit();         //  | 是 按位或（Bitwise OR） 运算符。它的规则极其简单：只要两个对应位中有一个是 1，结果就是 1。只有两个都是 0，结果才是 0。
	}
	
	return data;
}

/**
*函数：将接收到的DHT11数据转换为温湿度
*参数：无
*返回值：无
**/
void DHT11_Read_Data(uint8_t *pData)
{
	DHT11_Start();
	
	if (DHT11_Response())
		return;
	
	uint8_t i;
	for (i = 0; i < 5; i++)
	{
		pData[i] = DHT11_Read_Byte();
	}
	
	if (pData[4] != pData[0] + pData[1] + pData[2] + pData[3])
	{
		for (i = 0; i < 5; i++)
		{
			pData[i] = 0;
		}
	}
}

/**
*函数：变换温湿度数据为常规数据
*参数：接收温湿度数据的结构体
*返回值：温湿度数据结构体
**/
dht11_result DHT11_GetResult(dht11_result *result)
{
	DHT11_Read_Data(DHT11_Result);
	result->humi = DHT11_Result[0]+DHT11_Result[1]/10.0;
	result->temp = DHT11_Result[2]+DHT11_Result[3]/10.0;
	return *result;
}


