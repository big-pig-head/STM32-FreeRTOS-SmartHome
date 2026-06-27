/**************************************************************************
 * 文件名  ：bh1750.c
 * 描述    ：光强传感模块     
****************************************************************************/
#include "stm32f10x.h"                  // Device header
#include "bh1750.h"
#include "Delay.h"
#include "MyI2C.h"


#define LOG_ENABLE

#ifdef LOG_ENABLE
    #include "stdio.h"
    #define LOG   printf
#else
    #define LOG(format, ...)    
#endif

uint8_t current_mode;    // BH1750的测量模式
int current_light;     // BH1750的测量光照值

// BH1750延时函数                                     
void bh1750_Delay(uint16_t ms)
{// ms级延时，BH1750每次测量都需要时间，该函数用于等待测量结果
    Delay_ms(ms);
}

// 写命令
uint8_t bh1750_write_cmd(uint8_t cmd)
{
    return MyI2C_SendBytes(BH1750_ADDRESS_LOW, &cmd, 1);
}

// 写寄存器
uint8_t bh1750_read_regs(uint8_t *buf, uint8_t buf_size)
{
    return MyI2C_ReceiveBytes(BH1750_ADDRESS_LOW, buf, buf_size);
}

// 复位
uint8_t bh1750_reset(void)
{
    return bh1750_write_cmd(BH1750_RESET);
}
// 打开电源
uint8_t bh1750_power_on(void)
{
    return bh1750_write_cmd(BH1750_POWER_ON);
}

// 关闭电源
uint8_t bh1750_power_down(void)
{
    return bh1750_write_cmd(BH1750_POWER_DOWN);
}

// 设置测量模式
uint8_t bh1750_set_measure_mode(uint8_t mode)
{
    uint8_t result = 0;

    if(bh1750_write_cmd(mode))
    {
        result =  1;
    }
    return result;
}

// 单次读取光照值
uint8_t bh1750_single_read_light(uint8_t mode, int *light)
{// 单次测量模式在测量后会自动设置为断电模式   
    uint8_t temp[2];
    uint8_t result = 0;
    
    if(mode != BH1750_ONE_H_RES_MODE && mode != BH1750_ONE_H_RES_MODE2 && mode != BH1750_ONE_L_RES_MODE)
    {
        LOG("bh1750 single read measure mode error! mode:0x%02x\r\n", mode);
        return result;
    }
    if(bh1750_set_measure_mode(mode)) // 每次采集前先设置模式
    {
//        LOG("bh1750 set measure mode success! mode:0x%02x\r\n", mode);
        current_mode = mode;
        switch (mode)
        {
        case BH1750_ONE_H_RES_MODE:  // 单次H分辨率模式（精度1lx，测量时间120ms）
            bh1750_Delay(120);  // 等待采集完成
            break;
        case BH1750_ONE_H_RES_MODE2: // 单次H分辨率模式（精度0.5lx，测量时间120ms）
            bh1750_Delay(120);  // 等待采集完成
            break;
        case BH1750_ONE_L_RES_MODE:  // 单次L分辨率模式（精度4lx，测量时间16ms）
            bh1750_Delay(16);  // 等待采集完成
            break;
        default:
            break;
        }
        if(bh1750_read_regs(temp, 2))  // 读取测量结果
        {
            *light = ((float)((temp[0] << 8) + temp[1]) / 1.2); // 换算成光照值
            result = 1;
        }
        else
        {
            LOG("bh1750 read light failed!\r\n");
        }
    }
    else
    {
        LOG("bh1750 set measure mode failed! mode:0x%02x\r\n", mode);
        return result;
    }
    
    return result;
}

// 连续读取光照值
uint8_t bh1750_continuous_read_light(uint8_t mode, float *light)
{   
    uint8_t temp[2];
    uint8_t result = 0;
    
    if(mode != BH1750_CON_H_RES_MODE && mode != BH1750_CON_H_RES_MODE2 && mode != BH1750_CON_L_RES_MODE)
    {
        LOG("bh1750 continuous read measure mode error! mode:0x%02x\r\n", mode);
        return result;
    }

    if(mode != current_mode)
    {// 要使用的测量模式和BH1750当前的模式不同，则配置成相同模式
        if(bh1750_set_measure_mode(mode))
        {
            LOG("bh1750 set measure mode success! mode:0x%02x\r\n", mode);
            current_mode = mode;
        }
        else
        {// 模式设置失败
            LOG("bh1750 set measure mode failed! mode:0x%02x\r\n", mode);
            return result;
        }
        switch (mode)
        {
        case BH1750_CON_H_RES_MODE:  // 连续H分辨率模式（精度1lx，测量时间120ms）
            bh1750_Delay(120);  // 等待采集完成
            break;
        case BH1750_CON_H_RES_MODE2: // 连续H分辨率模式（精度0.5lx，测量时间120ms）
            bh1750_Delay(120);  // 等待采集完成
            break;
        case BH1750_CON_L_RES_MODE:  // 连续L分辨率模式（精度4lx，测量时间16ms）
            bh1750_Delay(16);  // 等待采集完成
            break;
        default:
            break;
        }
    }
    if(bh1750_read_regs(temp, 2))  // 读取测量结果
    {
        *light = ((float)((temp[0] << 8) + temp[1]) / 1.2); // 换算成光照值
        result = 1;
    }
    else
    {
        LOG("bh1750 read light failed!\r\n");
    }
    
    return result;
}

// BH1750初始化
uint8_t bh1750_init(void)
{
    uint8_t result = 0;
    MyI2C_Init();        // IIC初始化
    result = bh1750_power_on(); // 打开BH1750电源
    current_mode = 0;
    return result;
}

// 单次读取BH1750例程
void bh1750_read_example(void)
{
	bh1750_single_read_light(BH1750_ONE_H_RES_MODE2, &current_light);
}

