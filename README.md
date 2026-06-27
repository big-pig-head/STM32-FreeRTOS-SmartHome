# STM32-FreeRTOS-SmartHome

基于 STM32F103C8T6 与 FreeRTOS 抢占式实时内核的智能家居控制系统，实现温湿度 / 光照采集、串口上位机交互、PWM 电机调速、外设灯光与音频播放，自主设计三层任务优先级调度架构。

## 实物展示

![实物图](./f30278b219aea9d5cfd53503fa507cd5.jpg)

> 基于 STM32F103C8T6 核心板 + 面包板搭建的智能家居硬件系统

## 系统架构

```
┌─────────────────────────────────────────────────────────┐
│                    三层优先级调度架构                     │
├─────────────────────────────────────────────────────────┤
│  采集层 (Prio 18)  │  DHT11 温湿度采集 / BH1750 光照采集  │
│  通信层 (Prio 17)  │  串口接收解析任务 (@CMD\r\n 协议)    │
│  执行层 (Prio 15)  │  LED控制 / 风扇PWM调速 / 音乐播放    │
└─────────────────────────────────────────────────────────┘
```

## 核心功能

- **环境感知**：DHT11 温湿度传感器、BH1750 I2C 光照传感器，500ms 周期定时采集
- **上位机通信**：自定义 `@CMD\r\n` 串口协议，支持远程控制 LED、风扇、音乐
- **外设控制**：GPIO LED 灯光、TIM2 PWM 直流风扇调速、蜂鸣器音乐播放
- **任务调度**：FreeRTOS 抢占式调度，同优先级时间片轮转，互斥锁保护 OLED 共享资源

## 仓库结构

```
STM32-FreeRTOS-SmartHome/
├── hardware/           # 电路连接原理图 (PDF)
├── stm32_firmware/     # STM32 固件源码 (Keil MDK 工程)
│   └── User/           #   应用层代码 (freertos_task.c / main.c)
│   └── HardWare/       #   驱动层代码 (DHT11 / BH1750 / PWM / OLED)
│   └── FreeRTOS/       #   RTOS 内核与配置
├── qt_source/          # Qt 上位机源码
├── qt_app/             # Qt 上位机可执行程序 (Windows)
├── demo.mp4            # 功能演示视频
└── README.md           # 项目说明
```

## 技术栈

| 层级 | 技术 |
|------|------|
| 主控 | STM32F103C8T6 (ARM Cortex-M3, 72MHz) |
| RTOS | FreeRTOS V9.0 (抢占式调度) |
| 通信 | USART1 串口中断接收、I2C、单总线 |
| 驱动 | GPIO、TIM2 PWM、EXTI 外部中断 |
| 上位机 | Qt 5 + QSerialPort |
| 编译 | Keil MDK-ARM |

## 演示视频

[观看演示视频](./demo.mp4)

## 使用说明

1. **硬件连接**：按 `hardware/基于STM32F103C8T6的智能家居系统电路连接.pdf` 接线
2. **固件编译**：使用 Keil MDK 打开 `stm32_firmware/智能家居系统/Project.uvprojx` 编译烧录
3. **上位机运行**：直接运行 `qt_app/SmartHome-ByUart-V1.0/SmartHomeByUart.exe`，选择串口连接
4. **指令格式**：上位机发送 `@LEDON\r\n`、`@FengShanON\r\n` 等控制硬件

## 开源协议

本项目基于开源项目二次开发，仅用于个人学习及求职展示。
