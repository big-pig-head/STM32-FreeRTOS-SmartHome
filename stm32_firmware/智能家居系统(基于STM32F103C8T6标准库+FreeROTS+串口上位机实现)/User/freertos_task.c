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


/*
 * ============================================================
 * 任务管理与任务创建文件（FreeRTOS 部分）
 * ------------------------------------------------------------
 * 【本文件做什么】
 *  1. 用静态方式（xTaskCreateStatic）创建系统里所有的业务任务；
 *  2. 定义每个任务的三个要素：任务句柄 handler、任务栈 stack、
 *     任务控制块 tcb；
 *  3. 提供两个内存钩子，给 FreeRTOS 内核自带的"空闲任务"和
 *     "定时器任务"分配静态内存。
 * ------------------------------------------------------------
 * 【TCB 是什么】（Task Control Block，任务控制块）
 *  FreeRTOS 用 TCB 记录一个任务的全部运行信息，相当于"任务身份证"：
 *    - 任务栈指针（CPU 恢复到哪一行继续执行）
 *    - 任务优先级
 *    - 任务当前状态（就绪/阻塞/挂起/运行）
 *    - 事件列表项（用于排队、延时、等待信号量等）
 *  每个任务都必须有一个 tcb。本工程采用静态创建，所以 tcb 由我们
 *  在全局定义（例如 PlayMusic_tcb），创建任务时把它的地址传给内核。
 * ============================================================
 */


extern TaskHandle_t  Start_task_handler;      // 开始任务句柄（创建完各业务任务后删除自身）
extern bool MotorState;                       // 电机开关状态（定义在 Motor.c）
extern int current_light;                     // 当前光照值（BH1750 读取结果）
extern char Serial_RxPacket[100];             // 串口接收到的整帧指令字符串
extern uint8_t MusicState;                    // 音乐播放状态（0=未播 1=播放中 2=已停）

/* OLED 互斥锁：业务任务写 OLED 前先拿锁，防止多个任务同时写屏 */
SemaphoreHandle_t OLEDMutex;

/* OLED 显示队列：业务任务把"要显示的内容"发给 Display_task，由它统一刷屏 */
QueueHandle_t OLED_Queue;
/* 按键事件队列：按键中断只发事件给 KeyHandle_task，由它执行外设操作 */
QueueHandle_t Key_Queue;




/*
 * ============================================================
 * FreeRTOS 内核自己的两个内部任务（空闲任务 / 定时器任务）也需要内存，
 * 下面两组是给它们分配"静态内存"的钩子函数，系统启动时会自动调用。
 * ------------------------------------------------------------
 * idle_task_tcb / idle_task_stack：
 *   空闲任务（Idle Task）在"没有任何业务任务要跑"时才运行，
 *   负责回收被删除任务的资源。它的 tcb 和栈也由我们静态提供。
 * timer_task_tcb / timer_task_stack：
 *   定时器任务（Timer Task），负责处理用 xTimer 创建的软件定时器。
 * ============================================================
 */
StaticTask_t idle_task_tcb;                                 // 空闲任务的控制块（TCB）
StackType_t  idle_task_stack[configMINIMAL_STACK_SIZE];     // 空闲任务的栈

StaticTask_t timer_task_tcb;                                // 定时器任务的控制块（TCB）
StackType_t  timer_task_stack[configTIMER_TASK_STACK_DEPTH];// 定时器任务的栈

// 内存钩子：把上面定义的 tcb/栈地址交给 FreeRTOS 内核
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
								    StackType_t ** ppxIdleTaskStackBuffer,
								    uint32_t * pulIdleTaskStackSize )
{
	* ppxIdleTaskTCBBuffer = &idle_task_tcb;         // 空闲任务 tcb 地址
	* ppxIdleTaskStackBuffer = idle_task_stack;      // 空闲任务栈首地址
	* pulIdleTaskStackSize = configMINIMAL_STACK_SIZE; // 空闲任务栈大小
}

// 内存钩子：把定时器任务的 tcb/栈地址交给 FreeRTOS 内核
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
								     StackType_t ** ppxTimerTaskStackBuffer,
								     uint32_t * pulTimerTaskStackSize )
{
	* ppxTimerTaskTCBBuffer = &timer_task_tcb;        // 定时器任务 tcb 地址
	* ppxTimerTaskStackBuffer = timer_task_stack;     // 定时器任务栈首地址
	* pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH; // 定时器任务栈大小
}


/* ============================================================
 * 任务1：PlayMusic_task —— 音乐播放任务（非阻塞版）
 * 作用：驱动蜂鸣器非阻塞播放音乐。播放中每 10ms 调用一次
 *       Music_Play_Process() 推进音符，不再用 B_Music() 整曲阻塞。
 * 优先级：15（数字越小优先级越高）
 * 触发方式：轮询全局标志 MusicState（0 未播 / 1 播放中 / 2 已停）
 * ------------------------------------------------------------
 * PlayMusic_Task_handler ：任务句柄（创建后用于删除/挂起/恢复任务）
 * PlayMusic_stack[128]   ：任务栈，任务内局部变量和函数调用压栈用
 * PlayMusic_tcb          ：任务控制块（该任务的"身份证"，存运行状态）
 * ============================================================ */
#define PlayMusic_TASK_PRIO         15
#define PlayMusic_STACK_SIZE       128
TaskHandle_t PlayMusic_Task_handler;
StackType_t   PlayMusic_stack[PlayMusic_STACK_SIZE];
StaticTask_t PlayMusic_tcb;
void PlayMusic_task(void *pvParameters);

/* ============================================================
 * 任务2：DHT11_task —— 温湿度采集任务
 * 作用：周期性读取 DHT11 温湿度传感器，通过串口打印，
 *       并把温度/湿度发到 OLED 显示队列，由显示任务刷新屏幕。
 * 优先级：18（比音乐/串口任务都低，属于后台采集类）
 * 周期：每 500ms 采一次（vTaskDelay(500)）
 * ------------------------------------------------------------
 * DHT11_Task_handler ：任务句柄
 * DHT11_stack[128]   ：任务栈
 * DHT11_tcb          ：任务控制块（存该任务运行状态）
 * ============================================================ */
#define DHT11_TASK_PRIO        18
#define DHT11_STACK_SIZE       128
TaskHandle_t DHT11_Task_handler;
StackType_t   DHT11_stack[PlayMusic_STACK_SIZE];
StaticTask_t DHT11_tcb;
void DHT11_task(void *pvParameters);

/* ============================================================
 * 任务3：BH1750_task —— 光照采集任务
 * 作用：周期性读取 BH1750 光照传感器，通过串口打印，
 *       并把光照值发到 OLED 显示队列，由显示任务刷新屏幕。
 * 优先级：18（与 DHT11 同级，后台采集类）
 * 周期：每 500ms 采一次（vTaskDelay(500)）
 * ------------------------------------------------------------
 * BH1750_Task_handler ：任务句柄
 * BH1750_stack[128]   ：任务栈
 * BH1750_tcb          ：任务控制块（存该任务运行状态）
 * ============================================================ */
#define BH1750_TASK_PRIO        18
#define BH1750_STACK_SIZE       128
TaskHandle_t BH1750_Task_handler;
StackType_t   BH1750_stack[PlayMusic_STACK_SIZE];
StaticTask_t BH1750_tcb;
void BH1750_task(void *pvParameters);


/* ============================================================
 * 任务4：SertialReceive_task —— 串口指令接收任务
 * 作用：解析上位机（Qt 串口助手）下发的指令字符串，
 *       根据指令控制 LED 开关、音乐播放/停止、风扇开关和转速。
 * 优先级：17（比传感器采集高，指令要及时响应）
 * 触发方式：轮询 Serial_RxFlag（串口收完一帧后置 1）
 * ------------------------------------------------------------
 * SertialReceive_Task_handler ：任务句柄
 * SertialReceive_stack[256]   ：任务栈（256 个字，比普通任务大）
 * SertialReceive_tcb          ：任务控制块（存该任务运行状态）
 * ============================================================ */
#define SertialReceive_PRIO     17
#define SertialReceive_STACK_SIZE       256
TaskHandle_t SertialReceive_Task_handler;
StackType_t   SertialReceive_stack[SertialReceive_STACK_SIZE];
StaticTask_t SertialReceive_tcb;
/*
 * ============================================================
 * 任务4 实现：SertialReceive_task —— 串口指令接收与执行任务
 * 作用：接收上位机(Qt)通过串口下发的指令帧，解析出指令码后，
 *       直接驱动对应外设(LED / 风扇 / 音乐)，并通知 OLED 显示当前状态。
 * 触发方式：轮询 Serial_RxFlag（每500ms查一次），串口收满一帧会置1，
 *           本任务收到后调用 CommandProcess 把帧解析成指令码，再按码分支执行。
 * 说明：串口硬件的字节接收由串口中断完成，本任务只负责 帧→指令码→动作。
 * ============================================================ */
void SertialReceive_task(void *pvParameters);

/* ============================================================
 * 任务5：Display_task —— OLED 显示任务（OLED 抽离）
 * 作用：统一从 OLED_Queue 接收各业务任务发来的显示消息，
 *       负责刷新 OLED 屏幕，避免多个任务同时操作慢速 OLED。
 * 优先级：13（所有任务中最高，保证显示及时）
 * 触发方式：阻塞等待 OLED_Queue 消息（最多等 200ms）
 * ------------------------------------------------------------
 * Display_Task_handler ：任务句柄
 * Display_stack[128]   ：任务栈
 * Display_tcb          ：任务控制块（存该任务运行状态）
 * ============================================================ */
#define Display_TASK_PRIO        13
#define Display_STACK_SIZE       128
TaskHandle_t Display_Task_handler;
StackType_t   Display_stack[Display_STACK_SIZE];
StaticTask_t  Display_tcb;
void Display_task(void *pvParameters);

/* ============================================================
 * 任务6：KeyHandle_task —— 按键处理任务（按键事件化）
 * 作用：统一从 Key_Queue 接收按键中断发来的事件，
 *       执行 LED 翻转、风扇开关、音乐播放/停止等操作。
 *       这样中断里只发一条消息，外设操作都放到本任务，不会卡死中断。
 * 优先级：15（与音乐任务同级）
 * 触发方式：阻塞等待 Key_Queue 消息（最多等 200ms）
 * ------------------------------------------------------------
 * KeyHandle_Task_handler ：任务句柄
 * KeyHandle_stack[128]   ：任务栈
 * KeyHandle_tcb          ：任务控制块（存该任务运行状态）
 * ============================================================ */
#define KeyHandle_TASK_PRIO      15
#define KeyHandle_STACK_SIZE     128
TaskHandle_t KeyHandle_Task_handler;
StackType_t   KeyHandle_stack[KeyHandle_STACK_SIZE];
StaticTask_t  KeyHandle_tcb;
void KeyHandle_task(void *pvParameters);




/*
 * ============================================================
 * 上位机指令宏定义
 * 上位机（Qt）通过串口下发这些字符串指令（如 "LEDON"），
 * 本文件用宏给每个指令一个编号，方便 switch 分支处理。
 * ============================================================ */
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

// 解析上位机指令字符串，返回对应的指令码（宏编号）
// 参数：str 指令字符串（如 "LEDON"）
// 返回：无法识别时返回 COMMANDERR(0xFF)
u8 CommandProcess(u8 *str)
{
	u8 CommandValue=COMMANDERR;
	if(strcmp((char*)str,"LEDON")==0) CommandValue=LEDON;
	else if(strcmp((char*)str,"LEDOFF")==0) CommandValue=LEDOFF;
	else if(strcmp((char*)str,"MusicPlay")==0) CommandValue=MusicPlay;
	else if(strcmp((char*)str,"MusicStop")==0) CommandValue=MusicStop;
	else if(strcmp((char*)str,"FengShanON")==0) CommandValue=FengShanON;
	else if(strcmp((char*)str,"FengShanOFF")==0) CommandValue=FengShanOFF;
	else if(strcmp((char*)str,"FengSu25")==0) CommandValue=FengSu25;
	else if(strcmp((char*)str,"FengSu50")==0) CommandValue=FengSu50;
	else if(strcmp((char*)str,"FengSu75")==0) CommandValue=FengSu75;
	else if(strcmp((char*)str,"FengSu100")==0) CommandValue=FengSu100;
	return CommandValue;
}



/*
 * ============================================================
 * Start_Task —— 开始任务
 * 作用：系统第一个任务，负责创建上面所有业务任务和队列、互斥锁，
 *       创建完成后把自己删除（vTaskDelete 自身）。
 * 说明：创建任务用静态方式 xTaskCreateStatic，需要预先提供
 *       每个任务的 handler / stack / tcb。
 * ============================================================ */
void Start_Task(void * pvParameters)
{
	taskENTER_CRITICAL();// 进入临界区（关闭调度），确保以下创建过程不被中断

	// 创建 OLED 互斥锁：防止多个任务同时写 OLED 屏幕
	OLEDMutex = xSemaphoreCreateMutex();

	/*
	 * 静态任务创建 xTaskCreateStatic 七个参数含义（对下面6个任务通用）：
	 *  ①(TaskFunction_t)任务函数 —— 任务的入口函数，任务从这里开始运行
	 *  ②(char*)"任务名"          —— 任务名，仅用于内核/调试查看，不参与调度
	 *  ③(uint32_t)栈大小(字)     —— 任务私有栈的字数(1字=4字节)，决定可用局部变量深度
	 *  ④(void*)任务参数          —— 传给入口函数的参数，本工程无参传NULL
	 *  ⑤(UBaseType_t)优先级      —— 数字越大优先级越高，抢占式调度先执行
	 *  ⑥(StackType_t*)静态栈数组 —— 静态分配的栈空间首地址，避免动态malloc
	 *  ⑦(StaticTask_t*)&TCB      —— 任务控制块地址，内核用TCB记录栈指针/状态/优先级
	 * 返回值 TaskHandle_t 任务句柄 -> 存入各 Task_handler，用于删除/挂起等
	 */
	// 创建任务1：音乐播放任务
	//   ①入口=PlayMusic_task  ②名=PlayMusic_task  ③栈=PlayMusic_STACK_SIZE  ④参数=NULL  ⑤优先级=PlayMusic_TASK_PRIO  ⑥栈数组=PlayMusic_stack  ⑦TCB=&PlayMusic_tcb
	PlayMusic_Task_handler = xTaskCreateStatic((TaskFunction_t       )PlayMusic_task,
									         (char *                 ) "PlayMusic_task",
						                     (uint32_t               )PlayMusic_STACK_SIZE,
						                     (void *                 )NULL,
						                     (UBaseType_t            )PlayMusic_TASK_PRIO,
									         (StackType_t *          )PlayMusic_stack,
				                             (StaticTask_t *         )&PlayMusic_tcb);
	// 创建任务2：温湿度采集任务
	//   ①入口=DHT11_task  ②名=DHT11_task  ③栈=DHT11_STACK_SIZE  ④参数=NULL  ⑤优先级=DHT11_TASK_PRIO  ⑥栈数组=DHT11_stack  ⑦TCB=&DHT11_tcb

	DHT11_Task_handler = xTaskCreateStatic((TaskFunction_t         )DHT11_task,
									         (char *                 )"DHT11_task",
						                     (uint32_t               )DHT11_STACK_SIZE,
						                     (void *                 )NULL,
						                     (UBaseType_t            )DHT11_TASK_PRIO,
									         (StackType_t *          )DHT11_stack,
				                             (StaticTask_t *         )&DHT11_tcb);
	// 创建任务3：光照采集任务
	//   ①入口=BH1750_task  ②名=BH1750_task  ③栈=BH1750_STACK_SIZE  ④参数=NULL  ⑤优先级=BH1750_TASK_PRIO  ⑥栈数组=BH1750_stack  ⑦TCB=&BH1750_tcb

	BH1750_Task_handler = xTaskCreateStatic((TaskFunction_t         )BH1750_task,
									         (char *                 )"BH1750_task",
						                     (uint32_t               )BH1750_STACK_SIZE,
						                     (void *                 )NULL,
						                     (UBaseType_t            )BH1750_TASK_PRIO,
									         (StackType_t *          )BH1750_stack,
				                             (StaticTask_t *         )&BH1750_tcb);

	// 创建任务4：串口指令接收任务
	//   ①入口=SertialReceive_task  ②名=SertialReceive_task  ③栈=SertialReceive_STACK_SIZE  ④参数=NULL  ⑤优先级=SertialReceive_PRIO  ⑥栈数组=SertialReceive_stack  ⑦TCB=&SertialReceive_tcb

	SertialReceive_Task_handler = xTaskCreateStatic((TaskFunction_t      )SertialReceive_task,
												 (char *                 )"SertialReceive_task",
												 (uint32_t               )SertialReceive_STACK_SIZE,
												 (void *                 )NULL,
												 (UBaseType_t            )SertialReceive_PRIO,
												 (StackType_t *          )SertialReceive_stack,
												 (StaticTask_t *         )&SertialReceive_tcb);

	// 创建 OLED 显示队列 + 按键事件队列（容量各 4 条消息）
	OLED_Queue = xQueueCreate(4, sizeof(OLED_Msg));
	Key_Queue  = xQueueCreate(4, sizeof(Key_Event_t));
												 
/* OLED互斥锁，供所有需要写OLED的模块使用 */
//extern SemaphoreHandle_t OLEDMutex;

/* ===== OLED 抽离：显示消息 =====
 * 业务任务(采集/串口/按键)只把"要显示什么"发进 OLED_Queue，
 * 由专门的 Display_task 统一刷 OLED，业务任务不再直接操作慢速 OLED。 */
//typedef struct {
//    uint8_t  cmd;    /* 0=保留 1=温度 2=湿度 3=光照 4=音乐播放 5=音乐停止 */
//    float    fval;   /* 显示数值(温度/湿度/光照) */
//} OLED_Msg;
//extern QueueHandle_t OLED_Queue;

/* ===== 按键事件化：按键事件 =====
 * 按键中断只发事件进 Key_Queue，消抖/确认/外设操作统一放到 KeyHandle_task，
 * 避免在中断里做耗时操作。 */
//typedef enum {
//    KEY_EVENT_NONE = 0,
//    KEY_EVENT_LED,    /* 按键1：LED */
//    KEY_EVENT_FAN,    /* 按键2：风扇 */
//    KEY_EVENT_MUSIC   /* 按键3：音乐 */
//} Key_Event_t;
//extern QueueHandle_t Key_Queue;



	// 创建任务5：OLED 显示任务（统一刷屏）
	//   ①入口=Display_task  ②名=Display_task  ③栈=Display_STACK_SIZE  ④参数=NULL  ⑤优先级=Display_TASK_PRIO  ⑥栈数组=Display_stack  ⑦TCB=&Display_tcb

	Display_Task_handler = xTaskCreateStatic((TaskFunction_t      )Display_task,
												 (char *                 )"Display_task",
												 (uint32_t               )Display_STACK_SIZE,
												 (void *                 )NULL,
												 (UBaseType_t            )Display_TASK_PRIO,
												 (StackType_t *          )Display_stack,
												 (StaticTask_t *         )&Display_tcb);
	// 创建任务6：按键处理任务
	//   ①入口=KeyHandle_task  ②名=KeyHandle_task  ③栈=KeyHandle_STACK_SIZE  ④参数=NULL  ⑤优先级=KeyHandle_TASK_PRIO  ⑥栈数组=KeyHandle_stack  ⑦TCB=&KeyHandle_tcb

	KeyHandle_Task_handler = xTaskCreateStatic((TaskFunction_t      )KeyHandle_task,
												 (char *                 )"KeyHandle_task",
												 (uint32_t               )KeyHandle_STACK_SIZE,
												 (void *                 )NULL,
												 (UBaseType_t            )KeyHandle_TASK_PRIO,
												 (StackType_t *          )KeyHandle_stack,
												 (StaticTask_t *         )&KeyHandle_tcb);
	vTaskDelete(Start_task_handler);// 创建完成，删除开始任务自身
	taskEXIT_CRITICAL();// 退出临界区，恢复任务调度
}




/*
 * ============================================================
 * 任务2 实现：DHT11_task —— 温湿度采集任务
 * 逻辑：死循环里读一次 DHT11，若读到的温湿度都有效就打印并
 *       发到 OLED 显示队列（cmd=1 温度 / cmd=2 湿度），
 *       最后延时 500ms 再采下一次。
 * 作用：周期采集温湿度，供上位机显示和 OLED 展示。
 * ============================================================ */
/**
 * DHT11 温湿度采集任务
 **/
void DHT11_task(void *pvParameters)
{
	while(1)
	{
		dht11_result MyDHT11Result;                 // 存放一次读取的温湿度结果
		MyDHT11Result= DHT11_GetResult(&MyDHT11Result); // 调用 DHT11 读取函数
		if(MyDHT11Result.temp&&MyDHT11Result.humi)  // 温度和湿度都读到了（非0）
		{
			printf("Temp=%.1f",MyDHT11Result.temp); // 串口打印温度
			printf("\r\n");
			Delay_xms(1000);                        // 等待1000ms（方便上位机接收）
			printf("Humi=%.1f",MyDHT11Result.humi); // 串口打印湿度
			printf("\r\n");
			Delay_xms(1000);                        // 等待1000ms
			/* 发到 OLED 显示队列 */
			if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(100)) == pdTRUE) // 拿互斥锁，最多等100ms
			{
			OLED_Msg m;                             // 定义一个显示消息
			m.cmd = 1; m.fval = MyDHT11Result.temp; xQueueSend(OLED_Queue,&m,0); // 发温度(cmd=1)
			m.cmd = 2; m.fval = MyDHT11Result.humi; xQueueSend(OLED_Queue,&m,0); // 发湿度(cmd=2)

				xSemaphoreGive(OLEDMutex);          // 释放互斥锁
			}
		}
		vTaskDelay(500);                            // 延时500ms，进入阻塞，让出CPU
	}
}

/*
 * ============================================================
 * 任务3 实现：BH1750_task —— 光照采集任务
 * 逻辑：死循环里读一次光照，若光照值有效就打印并发到
 *       OLED 显示队列（cmd=3 光照），最后延时 500ms。
 * 作用：周期采集光照，供上位机显示和 OLED 展示。
 * ============================================================ */
/**
 * BH1750 光照采集任务
 **/
void BH1750_task(void *pvParameters)
{
	while(1)
	{
		bh1750_read_example();                      // 读取 BH1750 光照传感器
		if(current_light)                           // 光照值非0（读取有效）
		{
			printf("LiangDu:%d",current_light);     // 串口打印光照值
			Delay_xms(100);                         // 等待100ms
			printf("\r\n");
			/* 发到 OLED 显示队列 */
			if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(100)) == pdTRUE) // 拿互斥锁
			{
			OLED_Msg m;                             // 定义一个显示消息
			m.cmd = 3; m.fval = current_light; xQueueSend(OLED_Queue,&m,0); // 发光照(cmd=3)

				xSemaphoreGive(OLEDMutex);          // 释放互斥锁
			}
		}
		vTaskDelay(500);                            // 延时500ms，进入阻塞，让出CPU
	}
}



/*
 * ============================================================
 * 任务1 实现：PlayMusic_task —— 音乐播放任务（非阻塞版）
 * 逻辑：轮询 MusicState 标志。播放中(==1)就每 10ms 驱动一次
 *       非阻塞播放状态机 Music_Play_Process() 推进音符；
 *       停止后复位 music_started，下次播放从头开始。
 * 作用：让蜂鸣器用 TIM2 硬件持续发声，任务本身几乎不占 CPU，
 *       取代原先阻塞整曲的 B_Music()。
 * ============================================================ */
/* ============================================================
 * 播放音乐任务（非阻塞版）
 * 蜂鸣器发声靠 TIM2 硬件持续输出，任务不再用 B_Music() 整曲阻塞，
 * 改为每10ms驱动一次非阻塞播放状态机(Music_Play_Process)，几乎不占CPU。
 * ============================================================ */
void PlayMusic_task(void *pvParameters)
{
	extern uint8_t music_started;               // Music.c 里非阻塞播放的初始化标志
	while(1)
	{
		if(MusicState == 1)             // 播放中：驱动非阻塞播放状态机
		{
			if(!music_started)          // 还没初始化
			{
				Music_Play_Start();     // 首次/恢复：初始化TIM2并播第一个音符
			}
			Music_Play_Process();       // 查音符是否到点，切下一个音符（不阻塞）
		}
		else                            // 未播放或已停止
		{
			music_started = 0;          // 停止后复位，下次播放重新从第一个音符开始
		}
		vTaskDelay(10);                 // 每10ms驱动一次，任务几乎不占CPU
	}
}
/*
 * ============================================================
 * 任务4 实现：SertialReceive_task —— 串口指令接收与执行任务
 * 作用：接收上位机(Qt)通过串口下发的指令帧，解析出指令码后，
 *       直接驱动对应外设(LED / 风扇 / 音乐)，并通知 OLED 显示当前状态。
 * 触发方式：轮询 Serial_RxFlag（每500ms查一次），串口收满一帧会置1，
 *           本任务收到后调用 CommandProcess 把帧解析成指令码，再按码分支执行。
 * 说明：串口硬件的字节接收由串口中断完成，本任务只负责 帧→指令码→动作。
 * ============================================================ */
void SertialReceive_task(void *pvParameters)
{
	u8 CommandValue=COMMANDERR;         // 存放解析出的指令码，默认无效
	while(1)
	{
		if (Serial_RxFlag == 1)         // 串口收完一帧（标志置1）
		{
			CommandValue=CommandProcess((u8*)Serial_RxPacket); // 解析上位机指令
			if(CommandValue!=COMMANDERR) // 指令有效
			{
				switch(CommandValue)     // 按指令码分支执行
				{
					case LEDON:          // 打开 LED
						LED_ON();
						break;
					case LEDOFF:         // 关闭 LED
						LED_OFF();
						break;
			case MusicPlay:              // 开始/恢复播放音乐
				// 音乐开始播放：0 未播放 -> 1 播放
				if(MusicState == 0)  // MusicState=0 表示未播放
				{
					MusicState = 1;  // 置播放标志，PlayMusic_task 检测到后开始播放
					if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(50)) == pdTRUE) // 拿互斥锁
					{
						OLED_Msg m;  // 定义显示消息
						m.cmd = 4; xQueueSend(OLED_Queue,&m,0); // 发"正在播放"(cmd=4)

						xSemaphoreGive(OLEDMutex); // 释放互斥锁
					}
					printf("上位机: 开始播放音乐, MusicState=%d\r\n", MusicState);
				}
				else if(MusicState == 2)  // MusicState=2 表示已停止，恢复播放
				{
					MusicState = 1;  // 置播放标志，PlayMusic_task 检测到后恢复播放
					if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(50)) == pdTRUE) // 拿互斥锁
					{
						OLED_Msg m;  // 定义显示消息
						m.cmd = 4; xQueueSend(OLED_Queue,&m,0); // 发"正在播放"(cmd=4)

						xSemaphoreGive(OLEDMutex); // 释放互斥锁
					}
					printf("上位机: 恢复播放音乐, MusicState=%d\r\n", MusicState);
				}
				break;
			case MusicStop:              // 停止播放音乐
				if(MusicState == 1)  // MusicState=1 播放中，执行停止
				{
					TIM_Cmd(TIM2, DISABLE);  // 关闭 TIM2，停止发声
					MusicState = 2;  // 置 2 标记手动停止
					if(xSemaphoreTake(OLEDMutex, pdMS_TO_TICKS(50)) == pdTRUE) // 拿互斥锁
					{
						OLED_Msg m;  // 定义显示消息
						m.cmd = 5; xQueueSend(OLED_Queue,&m,0); // 发"停止播放"(cmd=5)

						xSemaphoreGive(OLEDMutex); // 释放互斥锁
					}
					printf("上位机: 停止播放音乐, MusicState=%d\r\n", MusicState);
				}
				break;
					case FengShanON:     // 开启风扇（默认60%转速）
						GPIO_SetBits(GPIOA, GPIO_Pin_4);   // PA4 置高（使能电机方向A）
						GPIO_ResetBits(GPIOA, GPIO_Pin_5); // PA5 置低（使能电机方向B）
						Delay_xms(20); // 等待20ms
						Motor_SetSpeed(60); // 设置转速60%
						break;
					case FengShanOFF:    // 关闭风扇
						GPIO_SetBits(GPIOA, GPIO_Pin_4);   // PA4 置高
						GPIO_SetBits(GPIOA, GPIO_Pin_5);   // PA5 置高（都高=停止）
						Delay_xms(20);
						break;
					case FengSu25:       // 风扇25%转速
						GPIO_SetBits(GPIOA, GPIO_Pin_4);
						GPIO_ResetBits(GPIOA, GPIO_Pin_5);
						Delay_xms(20);
						Motor_SetSpeed(25);
						break;
					case FengSu50:       // 风扇50%转速
						GPIO_SetBits(GPIOA, GPIO_Pin_4);
						GPIO_ResetBits(GPIOA, GPIO_Pin_5);
						Delay_xms(20);
						Motor_SetSpeed(50);
						break;
					case FengSu75:       // 风扇75%转速
						GPIO_SetBits(GPIOA, GPIO_Pin_4);
						GPIO_ResetBits(GPIOA, GPIO_Pin_5);
						Delay_xms(20);
						Motor_SetSpeed(75);
						break;
					case FengSu100:      // 风扇100%转速
						GPIO_SetBits(GPIOA, GPIO_Pin_4);
						GPIO_ResetBits(GPIOA, GPIO_Pin_5);
						Delay_xms(20);
						Motor_SetSpeed(100);
						break;
				}
				Serial_RxFlag = 0; // 清除串口接收标志，准备收下一帧
			}
		}
		vTaskDelay(500); // 每500ms检查一次是否有新指令
	}
}


/*
 * ============================================================
 * 任务5 实现：Display_task —— OLED 显示任务
 * 逻辑：阻塞等待 OLED_Queue 消息（最多 200ms），收到后按 cmd
 *       分派刷新对应区域。所有 OLED 写屏操作集中在这里，
 *       避免多任务同时操作慢速 OLED 造成显示错乱。
 * 作用：统一管理屏幕显示。
 * 消息类型：cmd=1温度 / 2湿度 / 3光照 / 4正在播放 / 5停止播放
 * ============================================================ */
void Display_task(void *pvParameters)
{
	OLED_Msg msg; // 定义一个接收用的显示消息
	while(1)
	{
		if(xQueueReceive(OLED_Queue, &msg, pdMS_TO_TICKS(200)) == pdTRUE) // 收消息，最多等200ms
		{
			switch(msg.cmd) // 按消息类型刷新
			{
				case 1:   /* 温度 */ // 刷新温度显示区域
					OLED_ClearArea(49,0,79,16);      // 清温度区域
					OLED_ShowFloatNum(49,0,msg.fval,2,1,OLED_8X16); // 显示温度值
					OLED_ShowChinese(92,0,"度");     // 显示"度"
					OLED_Update();                   // 刷新屏幕
					break;
				case 2:   /* 湿度 */ // 刷新湿度显示区域
					OLED_ClearArea(49,17,79,16);     // 清湿度区域
					OLED_ShowFloatNum(49,17,msg.fval,2,1,OLED_8X16); // 显示湿度值
					OLED_ShowString(92,17,"%RH",OLED_8X16); // 显示单位
					OLED_Update();
					break;
				case 3:   /* 光照 */ // 刷新光照显示区域
					OLED_ClearArea(49,33,79,16);     // 清光照区域
					OLED_ShowFloatNum(49,33,msg.fval,2,1,OLED_8X16); // 显示光照值
					OLED_Update();
					break;
				case 4:   /* 音乐播放 */ // 显示"正在播放"
					OLED_ClearArea(49,49,79,16);
					OLED_ShowChinese(49,49,"正在播放");
					OLED_Update();
					break;
				case 5:   /* 音乐停止 */ // 显示"停止播放"
					OLED_ClearArea(49,49,79,16);
					OLED_ShowChinese(49,49,"停止播放");
					OLED_Update();
					break;
				default:
					break;
			}
		}
	}
}

/* ============================================================
 * 任务6 实现：KeyHandle_task —— 按键处理任务
 * 作用：统一从 Key_Queue 接收按键事件并执行外设操作。
 *       按键中断(EXTI)只发一条事件消息到队列，不做外设操作，
 *       这样中断处理极短、不会阻塞其他中断。
 * 事件类型：KEY_EVENT_LED / KEY_EVENT_FAN / KEY_EVENT_MUSIC
 * ============================================================ */
void KeyHandle_task(void *pvParameters)
{
	Key_Event_t ev;                     // 定义一个接收用的按键事件
	extern bool MotorState;             // 电机开关状态（定义在 Motor.c）
	while(1)
	{
		if(xQueueReceive(Key_Queue, &ev, pdMS_TO_TICKS(200)) == pdTRUE) // 收按键事件，最多等200ms
		{
			switch(ev)                   // 按事件类型执行
			{
				case KEY_EVENT_LED:              /* 按键1：翻转 LED */
					LED_Turn();                  // 翻转 LED 亮灭
					printf("KEY1: LED toggled\r\n");
					break;
				case KEY_EVENT_FAN:              /* 按键2：风扇开关 */
					if(MotorState)               // 当前开 -> 关
					{
						GPIO_SetBits(GPIOA, GPIO_Pin_4); // PA4 置高
						GPIO_SetBits(GPIOA, GPIO_Pin_5); // PA5 置高（都高=停止）
						MotorState = false;      // 更新状态为关
						printf("KEY2: 风扇关闭\r\n");
					}
					else                         // 当前关 -> 开
					{
						GPIO_SetBits(GPIOA, GPIO_Pin_4);   // PA4 置高
						GPIO_ResetBits(GPIOA, GPIO_Pin_5); // PA5 置低（使能转动）
						Motor_SetSpeed(60);      // 设置转速60%
						printf("KEY2: 风扇开启 Speed=60\r\n");
					}
					break;
				case KEY_EVENT_MUSIC:            /* 按键3：音乐 播/停/续 */
					if(MusicState == 0)          /* 未播放 -> 开始 */
					{
						MusicState = 1;          // 置播放标志
						printf("PA7: 开始播放音乐, MusicState=%d\r\n", MusicState);
					}
					else if(MusicState == 1)     /* 播放中 -> 停止 */
					{
						TIM_Cmd(TIM2, DISABLE);  // 关闭 TIM2 停止发声
						MusicState = 2;          // 置停止标志
						printf("PA7: 停止音乐, MusicState=%d\r\n", MusicState);
					}
					else if(MusicState == 2)     /* 已停止 -> 恢复 */
					{
						MusicState = 1;          // 置播放标志
						printf("PA7: 恢复播放, MusicState=%d\r\n", MusicState);
					}
					break;
				default:
					break;
			}
		}
	}
}
