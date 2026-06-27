/**
*Encoding:GB2312
*文件功能:项目功能实现
**/

#include "stm32f10x.h"                  // Device header
#include "stdio.h"
#include "String.h"
#include "Delay.h"
#include "usart1.h"                //这个串口是给电脑使用的，用来在串口助手显示信息
#include "usart2.h"                //这个串口是给ESP8266使用的
#include "timer.h"
#include "Key.h"
#include "LED.h"
#include "OLED.h"
#include "Buzzer.h"
#include "Music.h"
#include "Motor.h"
#include "DHT11.h"
#include "BH1750.h"
#include "ESP8266.h"
#include "FreeRTOS.h"
#include "task.h"
#include "list.h"
#include "queue.h"
#include "event_groups.h"
#include "freertos_task.h"

static void BSP_Init(void);

//开始任务优先级
#define START_TASK_PRIO          1
//堆栈大小
#define START_TASK_STACK_SIZE  128
//任务函数
void Start_Task(void * pvParameters);
//任务句柄
TaskHandle_t  Start_task_handler;
StackType_t   start_task_stack[START_TASK_STACK_SIZE];
StaticTask_t  start_task_tcb;

extern float current_light;
extern uint8_t MusicState;
char buf1[100];

int main(void)
{	                     
	BSP_Init();
	Start_task_handler = xTaskCreateStatic((TaskFunction_t   )   Start_Task,
										   (char *           )	 "Start_Task",
										   (uint32_t         )	 START_TASK_STACK_SIZE,
									       (void *           )	 NULL,
										   (UBaseType_t      )	 START_TASK_PRIO,
										   (StackType_t *    )	 start_task_stack,	
										   (StaticTask_t *   )	 &start_task_tcb);
	vTaskStartScheduler();//开启任务调度	

	while (1)
	{	
		
	}
}

/*********************************************************************** 
* @ 函数名 ： BSP_Init 
* @ 功能说明： 板级外设初始化，所有板子上的初始化均可放在这个函数里面 
* @ 参数 ： 
* @ 返回值 ： 无 
*********************************************************************/

static void BSP_Init(void)
{	
/*
*STM32中断优先级分组为4，即4bit都用来表示抢占优先级，范围为：0~15 11 * 优先级分组只需要分组一次即可，以后如果有其他的任务需要用到中断， 12 * 都统一用这个优先级分组，千万不要再分组，切忌。 
*/
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4); 
	Delay_init();                        //FreeROTS系统定时器初始化
	Key1_Init();                         //按键1初始化，用于控制LED灯
	Key2_Init();                         //按键2初始化，用来控制直流电机，模拟风扇
	Key3_Init();                         //按键3初始化，用来控制蜂鸣器播放音乐
	Serial_Init();						 //串口1初始化，用于在串口显示数据
	LED_Init();	                         //LED_Init初始化
	Motor_Init();                        //直流电机初始化    
	DHT11_Init();                        //DHT11温湿度传感器初始化	
	OLED_Init();                         //OLED初始化
	bh1750_init();                       //BH1750光照传感器初始化
	OLED_ShowChinese(0,0,"温度：");
	OLED_ShowChinese(0,17,"湿度：");
	OLED_ShowChinese(0,33,"亮度：");
	OLED_ShowChinese(0,49,"音乐：");
	OLED_Update();                       //刷新屏幕显示
}






