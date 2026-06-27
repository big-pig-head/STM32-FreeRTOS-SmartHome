#include "stm32f10x.h"                  // Device header
#include "ESP8266.h"
#include "Delay.h"
#include "usart2.h" 
#include "string.h"
#include "stdio.h"

/* 引用串口1的接收缓冲区，TCP数据提取后存入此处供命令解析 */
extern char Serial_RxPacket[];
extern uint8_t Serial_RxFlag;

/**
          引脚连接
ESP8266				STM32
3V3-----------------3.3V
GND-----------------GND
RX------------------PA2
TX------------------PA3
EN------------------3V3
**/

/*第01步：WIFI模块初始化*/
void WIFI_GPIO_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);         //开启GPIOA的时钟
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);						
	GPIO_SetBits(GPIOA,GPIO_Pin_12);
}

void Rst_WIFI(void)
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_12);
	Delay_ms(1000);
	GPIO_SetBits(GPIOA,GPIO_Pin_12);
	Delay_ms(1000);
}

/*第02步：进行AT指令配置*/
/*判断串口2收到的数据是不是和ack给的一样*/
u8* WIFI_Check_Cmd(u8 *str)
{
	char *strx = 0;
	if(USART2_RX_STA&0x8000)
	{
		USART2_TX_BUF[USART2_RX_STA&0x7FFF] = 0;
		strx = strstr((const char*) USART2_RX_BUF,(const char*)str);
	}
	return (u8*)strx;
}


/*
命令函数：
cmd:发送的AT命令
ack:期望收到的回答
time：等待时间
返回值:0、成功，1、发送失败
*/
u8 WIFI_Send_Cmd(u8 *cmd,u8 *ack,u16 time)
{
	u8 res = 0;
	USART2_RX_STA = 0;
	u2_printf("%s\r\n",cmd);
	if(time)
	{
		while(--time)
		{
			Delay_ms(10);
			if(USART2_RX_STA&0x8000)
			{
				//判断
				if(WIFI_Check_Cmd(ack))
				{
					break;
				}
				USART2_RX_STA = 0;
			}
		}
		if(time == 0) res = 1;
	}
	return res;
}

/**
  * 函    数：ESP8266连接服务器函数
  * 参    数：无
  * 返 回 值：无
  * 说    明：【TCP Server模式】ESP8266自建WiFi热点，手机连入后通过TCP APP控制
  *          不使用MQTT，无需外部服务器！
  *          AT指令流程：AT→ATE0→CWMODE=2→RST→AT→ATE0→CWMODE=2(再确认)
  *                      →CWSAP_DEF→CWSAP_CUR→CIPMUX=1→CIPSERVER=1,8080
  *          ★ 每步最多重试5次，避免ESP8266无响应时系统卡死在BSP_Init中
  */
void WIFI_Init_TCP(void)
{
	int retry;
	
	printf("====== ESP8266 TCP Server 初始化 ======\r\n");
	
	/* STM32先初始化USART2并复位ESP8266 */
	Rst_WIFI();
	
	//1、AT 测试（最多重试5次）
	retry = 5;
	while(retry-- && WIFI_Send_Cmd((u8*)"AT",(u8*)"OK",50))
	{
		printf("ESP8266-AT响应失败，剩余重试:%d\r\n", retry);
	}
	if(retry < 0) { printf("【失败】ESP8266无响应，跳过WiFi初始化\r\n"); return; }
	printf("【1/8】AT响应成功\r\n");
	
	//2、关闭AT回显（避免回显干扰命令响应解析）
	retry = 5;
	while(retry-- && WIFI_Send_Cmd((u8*)"ATE0",(u8*)"OK",50))
	{
		printf("关闭回显失败，剩余重试:%d\r\n", retry);
	}
	/* ATE0失败不退出，继续尝试 */
	printf("【2/8】ATE0关闭回显\r\n");
	
	//3、设置为AP模式（ESP8266自己当热点）
	retry = 5;
	while(retry-- && WIFI_Send_Cmd((u8*)"AT+CWMODE=2",(u8*)"OK",50))
	{
		printf("AP模式设置失败，剩余重试:%d\r\n", retry);
	}
	if(retry < 0) { printf("【失败】AP模式设置失败，跳过后续\r\n"); return; }
	printf("【3/8】CWMODE=2(AP模式)设置成功\r\n");
	
	//4、复位使AP模式生效
	retry = 5;
	while(retry-- && WIFI_Send_Cmd((u8*)"AT+RST",(u8*)"OK",200))
	{
		printf("复位失败，剩余重试:%d\r\n", retry);
	}
	if(retry < 0) { printf("【失败】复位失败，跳过后续\r\n"); return; }
	printf("【4/8】ESP8266复位成功，等待启动...\r\n");
	Delay_ms(3000);  /* 等待ESP8266复位完成 */
	
	/* 复位后需要重新测试AT */
	retry = 5;
	while(retry-- && WIFI_Send_Cmd((u8*)"AT",(u8*)"OK",50))
	{
		printf("复位后AT响应失败，剩余重试:%d\r\n", retry);
	}
	if(retry < 0) { printf("【失败】复位后ESP8266无响应，跳过后续\r\n"); return; }
	printf("【5/8】复位后AT通信正常\r\n");
	
	//★关键修复：复位后必须重新关闭回显并确认AP模式！
	//因为RST后CWMODE可能恢复默认值(1=STA)，导致WiFi热点无法创建
	retry = 5;
	while(retry-- && WIFI_Send_Cmd((u8*)"ATE0",(u8*)"OK",50)) {}
	
	retry = 5;
	while(retry-- && WIFI_Send_Cmd((u8*)"AT+CWMODE=2",(u8*)"OK",50))
	{
		printf("复位后AP模式设置失败，剩余重试:%d\r\n", retry);
	}
	if(retry < 0) { printf("【失败】复位后AP模式设置失败\r\n"); return; }
	printf("【6/8】复位后重新确认AP模式成功\r\n");
	
	//6、配置WiFi热点（使用CWSAP_DEF保存到Flash + CWSAP_CUR立即生效）
	//    SSID=SmartHome, 密码=12345678, 通道1(兼容性好), WPA2_PSK加密
	retry = 5;
	while(retry-- && WIFI_Send_Cmd((u8*)"AT+CWSAP_DEF=\"SmartHome\",\"12345678\",1,3",(u8*)"OK",200))
	{
		printf("WiFi热点配置失败，剩余重试:%d, 尝试备用格式...\r\n", retry);
		/* 兼容旧版AT固件：尝试不带_DEF的格式 */
		if(WIFI_Send_Cmd((u8*)"AT+CWSAP=\"SmartHome\",\"12345678\",1,3",(u8*)"OK",200) == 0)
			break;
	}
	if(retry < 0) { printf("【失败】WiFi热点配置失败，跳过后续\r\n"); return; }
	printf("【7/8】WiFi热点 SmartHome 创建成功 (密码:12345678, 通道:1)\r\n");
	
	//7、开启多连接模式
	retry = 5;
	while(retry-- && WIFI_Send_Cmd((u8*)"AT+CIPMUX=1",(u8*)"OK",50))
	{
		printf("多连接模式设置失败，剩余重试:%d\r\n", retry);
	}
	if(retry < 0) { printf("【失败】多连接模式设置失败，跳过后续\r\n"); return; }
	
	//8、启动TCP Server，监听8080端口
	retry = 5;
	while(retry-- && WIFI_Send_Cmd((u8*)"AT+CIPSERVER=1,8080",(u8*)"OK",50))
	{
		printf("TCP Server启动失败，剩余重试:%d\r\n", retry);
	}
	if(retry < 0) { printf("【失败】TCP Server启动失败\r\n"); return; }
	printf("【8/8】TCP Server启动成功\r\n");
	
	printf("========================================\r\n");
	printf("TCP Server 启动成功！\r\n");
	printf("手机操作步骤：\r\n");
	printf("  1.连接WiFi: SmartHome (密码:12345678)\r\n");
	printf("  2.打开TCP APP\r\n");
	printf("  3.连接: 192.168.4.1:8080\r\n");
	printf("  4.输入命令如: LEDON, LEDOFF, FengShanON 等\r\n");
	printf("========================================\r\n");
}


/**
  * 函    数：检查WiFi TCP客户端是否发来了数据
  * 参    数：无
  * 返 回 值：1-有新数据已写入Serial_RxPacket, 0-无新数据
  * 说    明：ESP8266作为TCP Server收到数据时，格式为 +IPD,<id>,<len>:<data>
  *          本函数解析该格式，将<data>部分格式化为串口命令格式，复用现有命令解析
  */
u8 WIFI_CheckTCPCommand(void)
{
	char *p_start, *p_colon, *p_end;
	u8 data_len;
	static char wifi_cmd_buf[50];
	
	/* 检查USART2是否收到了新数据 */
	if(!(USART2_RX_STA & 0x8000)) return 0;
	
	/* 在接收缓冲区中查找 +IPD, 标志 */
	p_start = strstr((const char*)USART2_RX_BUF, "+IPD,");
	if(p_start == NULL)
	{
		/* 不是TCP数据帧，清空缓冲区继续等待 */
		USART2_RX_STA = 0;
		return 0;
	}
	
	/* 找到冒号(:)分隔符，后面是实际数据 */
	p_colon = strchr(p_start, ':');
	if(p_colon == NULL)
	{
		USART2_RX_STA = 0;
		return 0;
	}
	p_colon++;  /* 跳过冒号，指向数据起始 */
	
	/* 提取数据到临时缓冲区（数据后可能跟\r\n或其它字符） */
	data_len = 0;
	p_end = p_colon;
	while(*p_end != '\r' && *p_end != '\n' && *p_end != '\0' && data_len < 49)
	{
		wifi_cmd_buf[data_len++] = *p_end;
		p_end++;
	}
	wifi_cmd_buf[data_len] = '\0';
	
	/* 格式化：仿造串口命令格式 @命令 存入Serial_RxPacket */
	sprintf(Serial_RxPacket, "@%s", wifi_cmd_buf);
	Serial_RxFlag = 1;
	
	printf("WiFi收到: %s\r\n", wifi_cmd_buf);
	
	/* 清空USART2接收状态，准备接收下一条 */
	USART2_RX_STA = 0;
	return 1;
}

/**
  * 函    数：ESP8266发布函数
  * 参    数：无
  * 返 回 值：无
  */
void ESP8266_Publish(const char *topic,const char *msg)
{
	char cmd[256] = {0};
	sprintf(cmd, "AT+MQTTPUB=0,\"%s\",\"%s\",0,0", topic, msg);
	while(WIFI_Send_Cmd((u8*)cmd,(u8*)"OK",500))
	{
		printf("发送数据成功\r\n");
	}
}

/**
  * 函    数：ESP8266订阅函数
  * 参    数：无
  * 返 回 值：无
  */
void ESP8266_Subscribe(const char *topic)
{
	char cmd[128] = {0};
	sprintf(cmd, "AT+MQTTSUB=0,\"%s\",0", topic);            //订阅的主题
	while(WIFI_Send_Cmd((u8*)cmd,(u8*)"OK",500))
	{
		printf("订阅主题成功\r\n");
	}
}






