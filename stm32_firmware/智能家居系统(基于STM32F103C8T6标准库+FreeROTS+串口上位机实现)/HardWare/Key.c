/**
*Encoding:GB2312
*文件功能:按键功能实现
**/
#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "Key.h"
#include "LED.h"
#include "Motor.h"
#include "stdbool.h"
#include "Buzzer.h"
#include "Music.h"
#include "Motor.h"
#include "stdio.h"
#include "usart1.h"
#include "FreeRTOS.h"
#include "task.h"
#include "list.h"
#include "queue.h"
#include "event_groups.h"
#include "OLED.h"

extern TaskHandle_t LEDON_Task_handler;
extern TaskHandle_t LEDOFF_Task_handler;
extern TaskHandle_t MotorSpeed_Task_handler;
extern TaskHandle_t PlayMusic_Task_handler;
extern uint8_t MusicState;                          // 定义一个全局变量来记录音乐播放的状态 

/**
*函数：LED按键1初始化函数
*参数：无
*返回值：无
**/
void Key1_Init(void)
{
	/*第1步：打开时钟外设*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);         //开启GPIOC的时钟
//	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);          // 开启AFIO的时钟
	/*EXTI和NVIC的时钟一直是打开的，不需要开启：
	EXTI作为一个独立外设，按理说应该是需要开启时钟的但是寄存器里没有EXTI时钟的控制位
	NVIC是内核的外设，内核的外设是不需要开启时钟的
	RCC管的都是内核外的外设*/
	/*第2步：配置GPIO*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);						//将PC15引脚初始化为上拉输入	
	/*第3步：配置AFIO:
	它的库函数是和GPIO在一个文件里的*/
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource15);
	/*第4步：配置EXTI*/
	EXTI_InitTypeDef EXTI_InitStruct;
	EXTI_InitStruct.EXTI_Line = EXTI_Line15;              //指定中断线为第15个线路
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;      //指定中断线的模式为中断触发
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;  //指定中断触发方式为下降沿均触发
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;                //指定选择的中断线的新状态
	EXTI_Init(&EXTI_InitStruct);
	/*第5步：配置NVIC
	因为NVIC是内核外设，所以它的库函数是被ST发配到杂项这里来了，在misc.h文件里*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);       
	/*设置优先级分组方式，需要注意的是，这个分组方式整个芯片只能用一种，所以这个分组代码整个工程只需要
	执行依次就行了，如果把它放在模块里面进行分组，要确保每个模块分组都是选的同一个，也可以把这个分组代码
	放在主函数的最开始，这样模块里就不用在再进行分组了*/	
	NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel = EXTI15_10_IRQn;   //指定中断通道  
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 15;   //将抢占优先级设置为1，优先级是在多个中断源同时申请，产生拥挤的时候才有作用
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;    //将响应优先级设置为2
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;       //使能中断通道
	NVIC_Init(&NVIC_InitStruct);
}

/**
*函数：风扇按键2初始化函数
*参数：无
*返回值：无
**/
void Key2_Init(void)
{
	/*第1步：打开时钟外设*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);         //开启GPIOB的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);          // 开启AFIO的时钟
	/*EXTI和NVIC的时钟一直是打开的，不需要开启：
	EXTI作为一个独立外设，按理说应该是需要开启时钟的但是寄存器里没有EXTI时钟的控制位
	NVIC是内核的外设，内核的外设是不需要开启时钟的
	RCC管的都是内核外的外设*/
	/*第2步：配置GPIO*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);						//将PB1引脚初始化为上拉输入	
	/*第3步：配置AFIO:
	它的库函数是和GPIO在一个文件里的*/
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB,GPIO_PinSource1);
	
	/*第4步：配置EXTI*/    //EXIT是外部中断，比如按键这些
	EXTI_InitTypeDef EXTI_InitStruct;
	EXTI_InitStruct.EXTI_Line = EXTI_Line1;              //指定中断线为第1个线路
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;      //指定中断线的模式为中断触发
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;  //指定中断触发方式为下降沿均触发
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;                //指定选择的中断线的新状态
	EXTI_Init(&EXTI_InitStruct);
	
	/*第5步：配置NVIC                     //中断的优先级，和相关的配置
	因为NVIC是内核外设，所以它的库函数是被ST发配到杂项这里来了，在misc.h文件里*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);       
	/*设置优先级分组方式，需要注意的是，这个分组方式整个芯片只能用一种，所以这个分组代码整个工程只需要
	执行依次就行了，如果把它放在模块里面进行分组，要确保每个模块分组都是选的同一个，也可以把这个分组代码
	放在主函数的最开始，这样模块里就不用在再进行分组了*/	
	NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel = EXTI1_IRQn;             //指定中断通道  
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 15;   //将抢占优先级设置为2，优先级是在多个中断源同时申请，产生拥挤的时候才有作用
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;    
	//将响应优先级设置为1
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;       //使能中断通道
	NVIC_Init(&NVIC_InitStruct);
}

/**
*函数：蜂鸣器按键3初始化函数
*参数：无
*返回值：无
**/
void Key3_Init(void)
{
	/*第1步：打开时钟外设*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);         //开启GPIOA的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);          // 开启AFIO的时钟
	/*EXTI和NVIC的时钟一直是打开的，不需要开启：
	EXTI作为一个独立外设，按理说应该是需要开启时钟的但是寄存器里没有EXTI时钟的控制位
	NVIC是内核的外设，内核的外设是不需要开启时钟的
	RCC管的都是内核外的外设*/
	/*第2步：配置GPIO*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);						//将PA0引脚初始化为上拉输入	
	/*第3步：配置AFIO:
	它的库函数是和GPIO在一个文件里的*/
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA,GPIO_PinSource7);
	/*第4步：配置EXTI*/
	EXTI_InitTypeDef EXTI_InitStruct;
	EXTI_InitStruct.EXTI_Line = EXTI_Line7;              //指定中断线为第0个线路
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;      //指定中断线的模式为中断触发
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;  //指定中断触发方式为上升沿和下降沿均触发
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;                //指定选择的中断线的新状态
	EXTI_Init(&EXTI_InitStruct);
	/*第5步：配置NVIC
	因为NVIC是内核外设，所以它的库函数是被ST发配到杂项这里来了，在misc.h文件里*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);       
	/*设置优先级分组方式，需要注意的是，这个分组方式整个芯片只能用一种，所以这个分组代码整个工程只需要
	执行依次就行了，如果把它放在模块里面进行分组，要确保每个模块分组都是选的同一个，也可以把这个分组代码
	放在主函数的最开始，这样模块里就不用在再进行分组了*/	
	NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel = EXTI9_5_IRQn;   //指定中断通道  
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 15;   //将抢占优先级设置为1，优先级是在多个中断源同时申请，产生拥挤的时候才有作用
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;    //将响应优先级设置为1
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;       //使能中断通道
	NVIC_Init(&NVIC_InitStruct);
}


/*中断函数：中断函数的名字是固定的，在"startup_stm32f10x_md.s"文件里查看*/
/**
*函数：按键翻转LED状态，PC15按键
*参数：无
*返回值：无
**/
void EXTI15_10_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line15)==SET)
	{
		Delay_xms(20);                                              //按键消抖
		if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15) == 0)         //读PC15输入寄存器的状态，如果为0，则代表按键1按下
		{
			LED_Turn();
			printf("KEY1: LED toggled\r\n");
		}
		EXTI_ClearITPendingBit(EXTI_Line15);     //将通道15中断标志位清除
	}
}	


/*中断函数：中断函数的名字是固定的，在"startup_stm32f10x_md.s"文件里查看*/
/**
*函数：按键翻转风扇状态，PB1按键
*参数：无
*返回值：无
**/
void EXTI1_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line1)==SET)
	{
		Delay_xms(20);                                              //按键消抖
		if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)          //读PB1输入寄存器的状态，如果为0，则代表按键2按下
		{
			extern bool MotorState;
			if(MotorState)
			{
				GPIO_SetBits(GPIOA, GPIO_Pin_4);
				GPIO_SetBits(GPIOA, GPIO_Pin_5);
				MotorState = false;
				printf("KEY2: 风扇关闭\r\n");
			}
			else
			{
				GPIO_SetBits(GPIOA, GPIO_Pin_4);
				GPIO_ResetBits(GPIOA, GPIO_Pin_5);
				Motor_SetSpeed(60);
				printf("KEY2: 风扇开启 Speed=60\r\n");
			}
		}
		EXTI_ClearITPendingBit(EXTI_Line1);     //将通道1中断标志位清除
	}
}

/*中断函数：中断函数的名字是固定的，在"startup_stm32f10x_md.s"文件里查看*/
/**
*函数：按键开始或停止音乐（纯FLAG驱动，不在ISR里调vTaskSuspend——它不是ISR-safe的！）
*参数：无
*返回值：无
**/
void EXTI9_5_IRQHandler(void)
{	
	if(EXTI_GetITStatus(EXTI_Line7)==SET)
	{	
		Delay_xms(20);                                              //按键消抖
		if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7) == 0)
		{	
			if(MusicState == 0)                                    // 音乐未播放 → 开始播放
			{
				MusicState = 1;
				printf("PA7: 开始播放音乐, MusicState=%d\r\n", MusicState);
			}
			else if(MusicState == 1)                               // 音乐播放中 → 停止
			{
				TIM_Cmd(TIM2, DISABLE);                          // 立即关闭定时器，停止发声
				MusicState = 2;                                  // 标记：手动停止（非自然播完）
				printf("PA7: 停止音乐, MusicState=%d\r\n", MusicState);
			}
			else if(MusicState == 2)                               // 音乐已停止 → 继续播放
			{
				MusicState = 1;                                  // 恢复播放标志
				printf("PA7: 恢复播放, MusicState=%d\r\n", MusicState);
			}
		}
		EXTI_ClearITPendingBit(EXTI_Line7);     //将通道7中断标志位清除
	}
}
