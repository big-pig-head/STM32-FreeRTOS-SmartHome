/**
*Encoding:GB2312
*文件功能:直流电机驱动功能实现
**/


#include "stm32f10x.h"                  // Device header
#include "PWM.h"
#include "stdbool.h"

bool MotorState;                //定义一个全局变量用来记录电机的状态

/**
*函数：直流电机驱动模块初始化函数
*参数：无
*返回值：无
**/
void Motor_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	PWM_Init();
}

/**
*函数：直流电机转速设置函数
*参数：Speed设置电机转速,返回state电机状态
*返回值：返回state电机状态：转/不转
**/
bool Motor_SetSpeed(int8_t Speed)
{
	if (!MotorState)
	{
		if (Speed >= 0)
		{
			GPIO_SetBits(GPIOA, GPIO_Pin_4);
			GPIO_ResetBits(GPIOA, GPIO_Pin_5);
			PWM_SetCompare2(Speed);
		}
		else
		{
			GPIO_ResetBits(GPIOA, GPIO_Pin_4);
			GPIO_SetBits(GPIOA, GPIO_Pin_5);
			PWM_SetCompare2(-Speed);
		}
		MotorState = true;
	}
	return MotorState;                         //如果风扇处于转动过程中返回真，用来处理中断状态翻转
}







