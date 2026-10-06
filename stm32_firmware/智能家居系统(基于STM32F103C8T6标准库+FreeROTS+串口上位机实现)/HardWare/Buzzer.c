/**
*Encoding:GB2312
*文件功能:蜂鸣器硬件功能实现
*说明:用TIM2的PWM输出驱动无源蜂鸣器，通过改变PWM频率来产生不同音调（唱歌）
**/

#include "stm32f10x.h"                  // Device header（STM32F10x标准库头文件）
#include "Delay.h"                      // 延时函数头文件（Delay_ms / Delay_us）

uint8_t MusicState = 0;             // 全局变量：记录音乐播放状态（0=空闲/未播放，1=播放中，2=已手动停止），供任务和中断读写

/**
*函数：蜂鸣器所使用外设初始化函数（注册PWM发声通道）
*参数：无
*返回值：无
*作用：把TIM2配置成PWM模式，让PA0输出方波驱动蜂鸣器，并设置初始占空比50%
**/
void Music_init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);          //开启TIM2的时钟（用TIM2做PWM发生器，必须先开它的时钟才能用）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);         //开启GPIOA的时钟（PA0要用，先开时钟才能配置）
	
	GPIO_InitTypeDef GPIO_InitStructure;                          //定义GPIO配置结构体
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;               //复用推挽输出：把PA0交给TIM2外设控制（不是普通IO手动写电平）
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;                     //选中PA0引脚（TIM2_CH1输出通道接在PA0上）
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;             //输出速度50MHz（翻转够快，满足PWM方波）
	GPIO_Init(GPIOA, &GPIO_InitStructure);                        //把上面的配置真正写入寄存器，PA0初始化完成
	TIM_InternalClockConfig(TIM2);                                //选择TIM2使用内部时钟（72MHz），不使用外部时钟源
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;            //定义时基单元配置结构体
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;   //时钟分频1（用于滤波采样，不影响计数频率）
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up; //向上计数模式（CNT从0数到ARR后溢出回0）
	TIM_TimeBaseInitStructure.TIM_Period = 99;	                  //ARR=99：计数到99溢出，与PSC一起决定PWM频率
	TIM_TimeBaseInitStructure.TIM_Prescaler = 1439;	              //PSC=1439：72MHz/(1439+1)=50kHz计数时钟，再/(99+1)=500Hz初始频率
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 50;	      //重复计数器：仅高级定时器有效，TIM2通用定时器填了也不起作用（可忽略）
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);           //把配置写入TIM2，时基单元初始化完成
	

	TIM_OCInitTypeDef TIM_OCInitStructure;                        //定义输出比较（PWM）配置结构体
	TIM_OCStructInit(&TIM_OCInitStructure);                       //给结构体所有成员赋默认值，避免初值不确定
	
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;             //输出比较模式设为PWM1：CNT<CCR输出有效电平，CNT>=CCR输出无效电平
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;     //输出极性高：有效电平为高，构成方波
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; //使能该通道的PWM输出
	TIM_OCInitStructure.TIM_Pulse = 50;		                      //CCR=50：占空比=CCR/(ARR+1)=50/100=50%
	TIM_OC1Init(TIM2, &TIM_OCInitStructure);                      //把配置写入TIM2的通道1（CH1输出到PA0）
	TIM_Cmd(TIM2, ENABLE);                                        //使能TIM2，定时器开始运行，PA0开始输出方波驱动蜂鸣器
}

/**
*函数：蜂鸣器频率设置（改变音调）
*参数：a=PSC预分频值（决定PWM频率，即音调）
*返回值：无
*作用：改TIM2的预分频器，从而改变PWM方波频率=蜂鸣器振动频率=音调高低
**/
void Sound_SetHZ(uint16_t a)
{
	TIM_PrescalerConfig(TIM2,a,TIM_PSCReloadMode_Immediate);  //立即更新TIM2的PSC为a，频率=72MHz/(a+1)/(ARR+1)，a值越小频率越高音调越高
}

/**
*函数：播放一个音符（阻塞版）
*参数：a=频率值，b=发声时长(ms)，c=停顿时长(ms)
*返回值：无
*作用：初始化后，让蜂鸣器以a频率响b毫秒，再静音c毫秒
*注意：Delay_ms是阻塞延时，调用期间CPU卡死；优化后已不再使用，改用Music.c的非阻塞状态机
**/
void Play_Music(int a,int b,int c)
{
	Music_init();                          //初始化TIM2 PWM发声通道
	Sound_SetHZ(a);Delay_ms(b);            //设为a频率发声，阻塞等b毫秒（CPU卡死等待）
	Sound_SetHZ(20);Delay_ms(c);           //改到极高频率(约34kHz，超出人耳可听范围=听感静音)停顿c毫秒，再进下一音符
}

/**
*函数：暂停音乐（停止发声）
*参数：无
*返回值：无
*作用：关闭TIM2，停止PWM输出，蜂鸣器静音
**/
void Stop_Music(void)
{
	TIM_Cmd(TIM2, DISABLE);  //关闭TIM2使能位，定时器停止，PA0不再输出方波，蜂鸣器停止发声
}
