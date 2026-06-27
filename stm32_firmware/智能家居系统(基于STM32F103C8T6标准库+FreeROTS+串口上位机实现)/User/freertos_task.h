#ifndef __FREERTOS_TASK_H
#define __FREERTOS_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "list.h"
#include "queue.h"
#include "event_groups.h"
#include "semphr.h"

void Start_Task(void * pvParameters);
void Query_task(void *pvParameters);

/* OLED互斥锁，供所有需要写OLED的模块使用 */
extern SemaphoreHandle_t OLEDMutex;

#endif
