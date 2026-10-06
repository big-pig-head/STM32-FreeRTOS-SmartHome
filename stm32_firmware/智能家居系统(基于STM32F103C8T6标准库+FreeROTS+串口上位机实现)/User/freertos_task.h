#ifndef __FREERTOS_TASK_H
#define __FREERTOS_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "list.h"
#include "queue.h"
#include "event_groups.h"
#include "semphr.h"
#include "stdint.h"

void Start_Task(void * pvParameters);

/* OLED互斥锁，供所有需要写OLED的模块使用 */
extern SemaphoreHandle_t OLEDMutex;

/* ===== OLED 抽离：显示消息 =====
 * 业务任务(采集/串口/按键)只把"要显示什么"发进 OLED_Queue，
 * 由专门的 Display_task 统一刷 OLED，业务任务不再直接操作慢速 OLED。 */
typedef struct {
    uint8_t  cmd;    /* 0=保留 1=温度 2=湿度 3=光照 4=音乐播放 5=音乐停止 */
    float    fval;   /* 显示数值(温度/湿度/光照) */
} OLED_Msg;
extern QueueHandle_t OLED_Queue;

/* ===== 按键事件化：按键事件 =====
 * 按键中断只发事件进 Key_Queue，消抖/确认/外设操作统一放到 KeyHandle_task，
 * 避免在中断里做耗时操作。 */
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_LED,    /* 按键1：LED */
    KEY_EVENT_FAN,    /* 按键2：风扇 */
    KEY_EVENT_MUSIC   /* 按键3：音乐 */
} Key_Event_t;
extern QueueHandle_t Key_Queue;

#endif
