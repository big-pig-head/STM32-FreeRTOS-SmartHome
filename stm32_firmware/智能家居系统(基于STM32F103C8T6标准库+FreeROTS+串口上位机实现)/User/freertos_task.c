#include "FreeRTOS.h"
#include "task.h"
#include "list.h"
#include "queue.h"
#include "event_groups.h"
#include "freertos_task.h"
#include "LED.h"
#include "Key.h"
#include "Delay.h"
#include "usart1.h"
#include "stdio.h"
#include "usart1.h"
#include "Motor.h"
#include "Music.h"
#include "DHT11.h"
#include "OLED.h"
#include "BH1750.h"
#include "ESP8266.h"
#include "String.h"
#include "Buzzer.h"
#include "semphr.h"
#include  "portmacro.h"
#include <ctype.h>


extern TaskHandle_t  Start_task_handler;
extern bool MotorState; 
extern int current_light;
extern char Serial_RxPacket[100];
extern uint8_t MusicState;                          // 定义一个全局变量来记录音乐播放的状态

/* OLED互斥锁：防止DHT11/BH1750任务与串口任务同时写OLED导致显示错乱 */
SemaphoreHandle_t OLEDMutex;



//空闲任务配置
StaticTask_t idle_task_tcb;
StackType_t  idle_task_stack[configMINIMAL_STACK_SIZE];

//软件定时器任务配置
StaticTask_t timer_task_tcb;
StackType_t  timer_task_stack[configTIMER_TASK_STACK_DEPTH];

//空闲任务内存分配
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
								    StackType_t ** ppxIdleTaskStackBuffer,
								    uint32_t * pulIdleTaskStackSize )
{
	* ppxIdleTaskTCBBuffer = &idle_task_tcb;
	* ppxIdleTaskStackBuffer = idle_task_stack;
	* pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

//软件定时器内存分配
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
								     StackType_t ** ppxTimerTaskStackBuffer,
								     uint32_t * pulTimerTaskStackSize )
{
	* ppxTimerTaskTCBBuffer = &timer_task_tcb;
	* ppxTimerTaskStackBuffer = timer_task_stack;
	* pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}



/* LED_ON任务配置
 * 包括: 任务优先级 堆栈大小 任务句柄 创建任务
 * 优先级15：LED控制属于执行层，实时性要求低，500ms响应即可
 */
#define LEDON_TASK_PRIO         15
#define LEDON_STACK_SIZE       128
TaskHandle_t LEDON_Task_handler;
StackType_t   LEDON_stack[LEDON_STACK_SIZE];
StaticTask_t  LEDON_tcb;
void LEDON_task(void *pvParameters);

/* LED_OFF任务配置
 * 包括: 任务优先级 堆栈大小 任务句柄 创建任务
 * 优先级15：LED控制属于执行层，实时性要求低，500ms响应即可
 */
#define LEDOFF_TASK_PRIO         15
#define LEDOFF_STACK_SIZE       128
TaskHandle_t LEDOFF_Task_handler;
StackType_t   LEDOFF_stack[LEDOFF_STACK_SIZE];
StaticTask_t  LEDOFF_tcb;
void LEDOFF_task(void *pvParameters);

/* LED_Turn任务配置
 * 包括: 任务优先级 堆栈大小 任务句柄 创建任务
 * 优先级15：按键轮询属于执行层，实时性要求低，500ms响应即可
 */
#define LED_TURNTASK_PRIO         15
#define LED_TURN_STACK_SIZE       128
TaskHandle_t LED_TurnTask_handler;
StackType_t   LEDTurn_stack[LED_TURN_STACK_SIZE];
StaticTask_t  LEDTurn_tcb;
void LED_Turn_task(void *pvParameters);

/* 查询任务状态配置
 * 包括: 任务优先级 堆栈大小 任务句柄 创建任务
 * 优先级15：查询任务属于辅助层，实时性要求低
 */
#define Query_TASK_PRIO         15
#define Query_STACK_SIZE       128
TaskHandle_t QueryTask_handler;
StackType_t  Query_stack[Query_STACK_SIZE];
StaticTask_t Query_tcb;
void Query_task(void *pvParameters);

/* Motor_Speed任务配置
 * 包括: 任务优先级 堆栈大小 任务句柄 创建任务
 * 优先级15：电机控制属于执行层，轮询PB1按键，实时性要求低
 */
#define MotorSpeed_TASK_PRIO         15
#define MotorSpeed_STACK_SIZE       128
TaskHandle_t MotorSpeed_Task_handler;
StackType_t   MotorSpeed_stack[MotorSpeed_STACK_SIZE];
StaticTask_t MotorSpeed_tcb;
void MotorSpeed_task(void *pvParameters);

/* Play_music任务配置
 * 包括: 任务优先级 堆栈大小 任务句柄 创建任务
 * 优先级15：音乐播放属于执行层，实时性要求低
 * ★注意：PlayMusic_task调用B_Music()时会长时间运行（整首歌几十秒），
 *         优先级低于传感器和串口任务，确保采集和上位机通信不会被阻塞
 */
#define PlayMusic_TASK_PRIO         15
#define PlayMusic_STACK_SIZE       128
TaskHandle_t PlayMusic_Task_handler;
StackType_t   PlayMusic_stack[PlayMusic_STACK_SIZE];
StaticTask_t PlayMusic_tcb;
void PlayMusic_task(void *pvParameters);
extern uint8_t MusicState;                          // 定义一个全局变量来记录音乐播放的状态 

/* DHT11_task温度传感器任务配置
 * 包括: 任务优先级 堆栈大小 任务句柄 创建任务
 * 优先级18：传感器采集层，实时性要求最高，需要抢占执行层任务保证采集周期准确
 */
#define DHT11_TASK_PRIO        18
#define DHT11_STACK_SIZE       128
TaskHandle_t DHT11_Task_handler;
StackType_t   DHT11_stack[PlayMusic_STACK_SIZE];
StaticTask_t DHT11_tcb;
void DHT11_task(void *pvParameters);

/* BH1750_task光照传感器任务配置
 * 包括: 任务优先级 堆栈大小 任务句柄 创建任务
 * 优先级18：传感器采集层，与DHT11同级，实时性要求最高
 */
#define BH1750_TASK_PRIO        18
#define BH1750_STACK_SIZE       128
TaskHandle_t BH1750_Task_handler;
StackType_t   BH1750_stack[PlayMusic_STACK_SIZE];
StaticTask_t BH1750_tcb;
void BH1750_task(void *pvParameters);


/* SertialReceive_task串口接收任务配置
 * 包括: 任务优先级 堆栈大小 任务句柄 创建任务
 * 优先级17：通信解析层，上位机交互入口，需要及时响应指令
 *           高于执行层(15)，低于采集层(18)
 *           栈256字：switch-case分支多，sprintf局部变量占用大
 */
#define SertialReceive_PRIO     17
#define SertialReceive_STACK_SIZE       256
TaskHandle_t SertialReceive_Task_handler;
StackType_t   SertialReceive_stack[SertialReceive_STACK_SIZE];
StaticTask_t SertialReceive_tcb;
void SertialReceive_task(void *pvParameters);


//二值信号量句柄
SemaphoreHandle_t BinarySemaphore; //二值信号量句柄,用来接收串口下发的指令，控制外设工作

//用于命令解析用的命令值
#define LEDON 1
#define LEDOFF 2
#define MusicPlay 3
#define MusicStop 4
#define FengShanON 5
#define FengShanOFF 6
#define FengSu25 7
#define FengSu50 8
#define FengSu75 9
#define FengSu100 10
#define COMMANDERR	0XFF

//命令处理函数，将字符串命令转换成命令值
//str：命令
//返回值: 0XFF，命令错误；其他值，命令值
u8 CommandProcess(u8 *str)
{
	u8 CommandValue=COMMANDERR;
	if(strcmp((char*)str,"LEDON")==0) CommandValue=LEDON;
	else if(strcmp((char*)str,"LEDOFF")==0) CommandValue=LEDOFF;
	else if(strcmp((char*)str,"MusicPlay")==0) CommandValue=MusicPlay;
	else if(strcmp((char*)str,"MusicStop")==0) CommandValue=MusicStop;
	else if(strcmp((char*)str,"FengShanON")==0) CommandValue=FengShanON;
	else if(strcmp((char*)str,"FengShanOFF")==0) CommandValue=FengShanOFF;
	else if(strcmp((char*)str,"FengShanOFF")==0) CommandValue=FengShanOFF;
	else if(strcmp((char*)str,"FengSu25")==0) CommandValue=FengSu25;
	else if(strcmp((char*)str,"FengSu50")==0) CommandValue=FengSu50;
	else if(strcmp((char*)str,"FengSu75")==0) CommandValue=FengSu75;
	else if(strcmp((char*)str,"FengSu100")==0) CommandValue=FengSu100;
	return CommandValue;
}



void Start_Task(void * pvParameters)
{
	taskENTER_CRITICAL();//进入临界区，创建任务完成后，才开始执行优先级任务
	//创建二值信号量
	BinarySemaphore=xSemaphoreCreateBinary();
	//创建OLED互斥锁
	OLEDMutex = xSemaphoreCreateMutex();
	
//	LEDON_Task_handler = xTaskCreateStatic((TaskFunction_t           )LED_ON_task,
//									         (char *                 ) "LED_ON_task",
//						                     (uint32_t               )LEDON_STACK_SIZE,
//						                     (void *                 )NULL,
//						                     (UBaseType_t            )LEDON_TASK_PRIO,
//									         (StackType_t *          )LEDON_stack,
//				                             (StaticTask_t *         )&LEDON_tcb);
//										 
//	LEDOFF_Task_handler = xTaskCreateStatic((TaskFunction_t          )LED_OFF_task,
//									         (char *                 ) "LEDOFF_task",
//						                     (uint32_t               )LEDOFF_STACK_SIZE,
//						                     (void *                 )NULL,
//						                     (UBaseType_t            )LEDOFF_TASK_PRIO,
//									         (StackType_t *          )LEDOFF_stack,
//				                             (StaticTask_t *         )&LEDOFF_tcb);
	
	/* LED翻转任务（轮询PC15按键） */
	LED_TurnTask_handler = xTaskCreateStatic((TaskFunction_t         )LED_Turn_task,
									         (char *                 ) "LED_Turn_task",
						                     (uint32_t               )LED_TURN_STACK_SIZE,
						                     (void *                 )NULL,
						                     (UBaseType_t            )LED_TURNTASK_PRIO,
									         (StackType_t *          )LEDTurn_stack,
				                             (StaticTask_t *         )&LEDTurn_tcb);	
	

	MotorSpeed_Task_handler = xTaskCreateStatic((TaskFunction_t      )MotorSpeed_task,
									         (char *                 ) "MotorSpeed_task",
						                     (uint32_t               )MotorSpeed_STACK_SIZE,
						                     (void *                 )NULL,
						                     (UBaseType_t            )MotorSpeed_TASK_PRIO,
									         (StackType_t *          )MotorSpeed_stack,
				                             (StaticTask_t *         )&MotorSpeed_tcb);	
											 
	
	PlayMusic_Task_handler = xTaskCreateStatic((TaskFunction_t       )PlayMusic_task,
									         (char *                 ) "PlayMusic_task",
						                     (uint32_t               )PlayMusic_STACK_SIZE,
						                     (void *                 )NULL,
						                     (UBaseType_t            )PlayMusic_TASK_PRIO,
									         (StackType_t *          )PlayMusic_stack,
				                             (StaticTask_t *         )&PlayMusic_tcb);			
	
	DHT11_Task_handler = xTaskCreateStatic((TaskFunction_t         )DHT11_task,
									         (char *                 )"DHT11_task",
						                     (uint32_t               )DHT11_STACK_SIZE,
						                     (void *                 )NULL,
						                     (UBaseType_t            )DHT11_TASK_PRIO,
									         (StackType_t *          )DHT11_stack,
				                             (StaticTask_t *         )&DHT11_tcb);
									
	BH1750_Task_handler = xTaskCreateStatic((TaskFunction_t         )BH1750_task,
									         (char *                 )"BH1750_task",
						                     (uint32_t               )BH1750_STACK_SIZE,
						                     (void *                 )NULL,
						                     (UBaseType_t            )BH1750_TASK_PRIO,
									         (StackType_t *          )BH1750_stack,
				                             (StaticTask_t *         )&BH1750_tcb);									
	

	SertialReceive_Task_handler = xTaskCreateStatic((TaskFunction_t      )SertialReceive_task,
												 (char *                 )"SertialReceive_task",
												 (uint32_t               )SertialReceive_STACK_SIZE,
												 (void *                 )NULL,
												 (UBaseType_t            )SertialReceive_PRIO,
												 (StackType_t *          )SertialReceive_stack,
												 (StaticTask_t *         )&SertialReceive_tcb);	
											 
											 
//	QueryTask_handler = xTaskCreateStatic((TaskFunction_t         )Query_task,
//									         (char *                 ) "Query_task",
//						                     (uint32_t               )Query_STACK_SIZE,
//						                     (void *                 )NULL,
//						                     (UBaseType_t            )Query_TASK_PRIO,
//									         (StackType_t *          )Query_stack,
//				                             (StaticTask_t *         )&Query_tcb);								 								 
	vTaskDelete(Start_task_handler);//删除开始任务，删除自身用NULL
	taskEXIT_CRITICAL();//退出临界区		
}


/**
  * 函    数：开灯任务函数，这个函数给上位机使用
  * 参    数：无
  * 返 回 值：无
  */
void LED_ON_task(void *pvParameters)
{
	while(1)
	{
		LED_ON();
		vTaskDelay(500);
	}
}

/**
  * 函    数：关灯任务函数，这个函数给上位机使用
  * 参    数：无
  * 返 回 值：无
  */
void LED_OFF_task(void *pvParameters)
{
	while(1)
	{
		LED_OFF();
		vTaskDelay(500);	
	}
}

/* === LED_Turn_task ===
 * 轮询PC15按键，检测到按下则翻转LED
 */
void LED_Turn_task(void *pvParameters)
{
	while(1)
	{
		if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15) == 0)
		{
			Delay_xms(20);                              //按键消抖
			if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15) == 0)
			{
				LED_Turn();
				printf("LED_Turn_task: LED toggled\r\n");
			}
		}
		vTaskDelay(500);
	}
}

/**
  * 函    数：电机任务函数
  * 参    数：无
  * 返 回 值：无
  */
void MotorSpeed_task(void *pvParameters)
{
	while(1)
	{
		if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)			//读PB1输入寄存器的状态，如果为0，则代表按键2按下
		{
			Delay_xms(20);                             //按键消抖
			if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)
			{
				printf("检测到风扇按键按下\r\n");
				if(MotorState)
				{
					GPIO_SetBits(GPIOA, GPIO_Pin_4);
					GPIO_SetBits(GPIOA, GPIO_Pin_5);
					MotorState = false;
				}
				else
				{
					Motor_SetSpeed(60);                       //按下按键后，如果电机原来不转，设置转速默认为60，电机开始转动
				}
			}
		}
		vTaskDelay(500);	
	}
}

/**
  * 函    数：播放音乐任务函数
  * 参    数：无
  * 返 回 值：无
  */
void PlayMusic_task(void *pvParameters)
{
	while(1)
	{
		if(MusicState == 1)           // MusicState=1时播放音乐
		{
			B_Music();
			/* 判断：是自然播完（MusicState仍为1）还是被手动停止（MusicState变成2） */
			if(MusicState != 2)        /* 自然播完，置0表示播放完毕 */
			{
				MusicState = 0;
				if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(50)) == pdTRUE)
				{
					OLED_ClearArea(49,49,79,16);
					OLED_ShowChinese(49,49,"已结束");
					OLED_Update();
					xSemaphoreGive(OLEDMutex);
				}
			}
			/* 如果 MusicState==2，保持不动，等待用户按PA7恢复播放 */
		}
		vTaskDelay(50);   /* 缩短轮询间隔，避免MusicState=1被漏检 */
	}
}

/**
  * 函    数：温湿度传感器DHT11任务函数
  * 参    数：无
  * 返 回 值：无
  */
void DHT11_task(void *pvParameters)
{
	while(1)
	{	
		dht11_result MyDHT11Result;
		MyDHT11Result= DHT11_GetResult(&MyDHT11Result);
//		printf("温度：%.1f;湿度：%.1f\r\n",MyDHT11Result.temp,MyDHT11Result.humi);
		if(MyDHT11Result.temp&&MyDHT11Result.humi)
		{
			printf("Temp=%.1f",MyDHT11Result.temp);
			printf("\r\n");
			Delay_xms(1000);
			printf("Humi=%.1f",MyDHT11Result.humi);
			printf("\r\n");
			Delay_xms(1000);
			/* 加互斥锁保护OLED写操作 */
			if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(100)) == pdTRUE)
			{
				OLED_ClearArea(49,0,79,16);
				OLED_ClearArea(49,17,79,16);
				OLED_ShowFloatNum(49, 0, MyDHT11Result.temp, 2, 1, OLED_8X16);
				OLED_ShowChinese(92,0,"℃");
				OLED_ShowFloatNum(49, 17, MyDHT11Result.humi, 2, 1, OLED_8X16);
				OLED_ShowString(92,17, "%RH", OLED_8X16);
				OLED_Update();
				xSemaphoreGive(OLEDMutex);
			}
		}
		vTaskDelay(500);
	}
}

/**
  * 函    数：温湿度传感器DHT11任务函数
  * 参    数：无
  * 返 回 值：无
  */
void BH1750_task(void *pvParameters)
{
	while(1)
	{	
		bh1750_read_example();
		if(current_light)
		{
			printf("LiangDu:%d",current_light);
			Delay_xms(100);
			printf("\r\n");
			/* 加互斥锁保护OLED写操作 */
			if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(100)) == pdTRUE)
			{
				OLED_ClearArea(49,33,79,16);
				OLED_ShowFloatNum(49, 33, current_light, 2, 1, OLED_8X16);
				OLED_Update();
				xSemaphoreGive(OLEDMutex);
			}
		}
		vTaskDelay(500);
	}
}


/**
  * 函    数：串口接收任务函数，接收到串口数据后执行相应操作
  * 参    数：无
  * 返 回 值：无
  */
void SertialReceive_task(void *pvParameters)
{
	u8 CommandValue=COMMANDERR;
	//BaseType_t err=pdFALSE;
	while(1)
	{		
		if (Serial_RxFlag == 1)	
		{
			CommandValue=CommandProcess((u8*)Serial_RxPacket); //命令解析			
			if(CommandValue!=COMMANDERR)
			{
				switch(CommandValue)
				{
					
					case LEDON:
						LED_ON();

						break;
					case LEDOFF:
						LED_OFF();
						break;
			case MusicPlay:
				//要么第一次播放，要么是恢复播放任务
				if(MusicState == 0)                                    // MusicState = 0时说明没有播放音乐
				{
					MusicState = 1;                                    // 纯FLAG驱动，PlayMusic_task轮询检测到1后开始播放
					if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(50)) == pdTRUE)
					{
						OLED_ClearArea(49,49,79,16);
						OLED_ShowChinese(49,49,"正在播放");
						OLED_Update();
						xSemaphoreGive(OLEDMutex);
					}
					printf("串口: 开始播放音乐, MusicState=%d\r\n", MusicState);
				}
				else if(MusicState == 2)                               // 音乐已停止 → 恢复
				{
					MusicState = 1;                                    // 纯FLAG驱动，PlayMusic_task检测到1后继续播放
					if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(50)) == pdTRUE)
					{
						OLED_ClearArea(49,49,79,16);
						OLED_ShowChinese(49,49,"正在播放");
						OLED_Update();
						xSemaphoreGive(OLEDMutex);
					}
					printf("串口: 恢复播放, MusicState=%d\r\n", MusicState);
				}
				break;
			case MusicStop:
				if(MusicState == 1)                                    // 音乐播放中 → 停止
				{
					TIM_Cmd(TIM2, DISABLE);                            // 立即关闭定时器，停止发声
					MusicState = 2;                                    // 标记：手动停止
					if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(50)) == pdTRUE)
					{
						OLED_ClearArea(49,49,79,16);
						OLED_ShowChinese(49,49,"停止播放");
						OLED_Update();
						xSemaphoreGive(OLEDMutex);
					}
					printf("串口: 停止音乐, MusicState=%d\r\n", MusicState);
				}
				break;
					case FengShanON:
						GPIO_SetBits(GPIOA, GPIO_Pin_4);
						GPIO_ResetBits(GPIOA, GPIO_Pin_5);
						Delay_xms(20);                              //延时20ms，否则会出现电机堵转现象
						Motor_SetSpeed(60);
						break;
					case FengShanOFF:
						GPIO_SetBits(GPIOA, GPIO_Pin_4);
						GPIO_SetBits(GPIOA, GPIO_Pin_5);
						Delay_xms(20);
						break;
					case FengSu25:
						GPIO_SetBits(GPIOA, GPIO_Pin_4);
						GPIO_ResetBits(GPIOA, GPIO_Pin_5);
						Delay_xms(20);
						Motor_SetSpeed(25);
						break;
					case FengSu50:
						GPIO_SetBits(GPIOA, GPIO_Pin_4);
						GPIO_ResetBits(GPIOA, GPIO_Pin_5);
						Delay_xms(20);
						Motor_SetSpeed(50);
						break;
					case FengSu75:
						GPIO_SetBits(GPIOA, GPIO_Pin_4);
						GPIO_ResetBits(GPIOA, GPIO_Pin_5);
						Delay_xms(20);
						Motor_SetSpeed(75);
						break;
					case FengSu100:
						GPIO_SetBits(GPIOA, GPIO_Pin_4);
						GPIO_ResetBits(GPIOA, GPIO_Pin_5);
						Delay_xms(20);
						Motor_SetSpeed(100);
						break;	
				}
				Serial_RxFlag = 0;			//处理完成后，需要将接收数据包标志位清零，否则将无法接收后续数据包
			}
		}
		vTaskDelay(500);
	}
}


/**
  * 函    数：查询任务状态
  * 参    数：无
  * 返 回 值：无
  */
void Query_task(void *pvParameters)
{
	while(1)
	{
		u32 TotalRunTime;
		UBaseType_t ArraySize,x;
		TaskStatus_t *StatusArray;
		ArraySize = uxTaskGetNumberOfTasks();       //获取系统任务数量
		StatusArray = pvPortMalloc(ArraySize*sizeof(TaskStatus_t));     //申请内存
		if(StatusArray != NULL)                //内存申请成功
		{
			ArraySize = uxTaskGetSystemState(
			(TaskStatus_t*)StatusArray,        //任务信息存储数组
			(UBaseType_t)ArraySize,            //任务信息存储数组大小
			(uint32_t *)&TotalRunTime);         //保存系统总的运行时间
			printf("TaskName\t\tPriority\t\taskNumber\t\t\r\n");
			for(x=0;x<ArraySize;x++)
			{
				printf("%s\t\t%d\t\t%d\t\t\t\r\n",StatusArray[x].pcTaskName,
				(int)StatusArray[x].uxCurrentPriority,(int)StatusArray[x].xTaskNumber);
			}
			vPortFree(StatusArray);
		}	
	}
}


