#ifndef __LED_H
#define __LED_H

#define LEDState_ON 1;
#define LEDState_OFF 0;

void LED_Init(void);
void LED_ON(void);
void LED_OFF(void);	
void LED_Turn(void);
void LED_ON_task(void *pvParameters);
void LED_OFF_task(void *pvParameters);
void LED_Turn_task(void *pvParameters);

#endif
