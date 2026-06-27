#ifndef __DHT11_H
#define __DHT11_H

typedef struct{
	float humi;
	float temp;
}dht11_result;

void DHT11_Init(void);
void DHT11_Read_Data(uint8_t *pData);
dht11_result DHT11_GetResult(dht11_result *result);

#endif

