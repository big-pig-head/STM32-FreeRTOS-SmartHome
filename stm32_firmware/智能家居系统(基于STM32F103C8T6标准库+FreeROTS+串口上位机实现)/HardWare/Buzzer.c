/**
*Encoding:GB2312
*文件功能:蜂鸣器硬件功能实现
**/

#include "stm32f10x.h"                  // Device header
#include "Delay.h"

uint8_t MusicState = 0;             // 全局变量来记录音乐播放的状态 

/**
*函数：蜂鸣器所使用外设初始化函数
*参数：无
*返回值：无
**/
void Music_init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);          //开启TIM2的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);         //开启GPIOA的时钟
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;                     
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;             //将PA0引脚初始化为复用推挽输出	
	GPIO_Init(GPIOA, &GPIO_InitStructure);                        //受外设控制的引脚，均需要配置为复用模式	
	TIM_InternalClockConfig(TIM2);                                //选择TIM2为内部时钟，若不调用此函数，TIM默认也为内部时钟
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 99;	
	TIM_TimeBaseInitStructure.TIM_Prescaler = 1439;
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 50;	
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
	

	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);
	
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 50;		//CCR
	TIM_OC1Init(TIM2, &TIM_OCInitStructure);
	TIM_Cmd(TIM2, ENABLE);
}

/**
*函数：蜂鸣器频率设置
*参数：无
*返回值：无
**/
void Sound_SetHZ(uint16_t a)
{
	TIM_PrescalerConfig(TIM2,a,TIM_PSCReloadMode_Immediate);
}

/**
*函数：播放音乐
*参数：无
*返回值：无
**/
void Play_Music(int a,int b,int c)
{
	Music_init();
	Sound_SetHZ(a);Delay_ms(b);Sound_SetHZ(20);Delay_ms(c);
}

/**
*函数：暂停音乐
*参数：无
*返回值：无
**/
void Stop_Music(void)
{
	TIM_Cmd(TIM2, DISABLE);;  //// 关闭定时器，停止PWM输出
}

