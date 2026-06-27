#ifndef __ESP8266_H
#define __ESP8266_H

void WIFI_GPIO_Init(void);
void Rst_WIFI(void);
u8* WIFI_Check_Cmd(u8 *str);
u8 WIFI_Send_Cmd(u8 *cmd,u8 *ack,u16 time);

/* TCP Server模式：ESP8266自建WiFi热点，手机TCP APP直连控制，无需外部服务器 */
void WIFI_Init_TCP(void);
u8 WIFI_CheckTCPCommand(void);

/* 以下为原始MQTT模式函数（已废弃，保留供参考） */
//void WIFI_Init(void);
//void ESP8266_Publish(const char *topic,const char *msg);
//void ESP8266_Subscribe(const char *topic);

#endif