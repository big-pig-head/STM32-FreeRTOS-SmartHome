# STM32-FreeRTOS-智能家居

基于 STM32F103C8T6 + FreeRTOS 抢占式实时内核的智能家居控制系统：通过按键和 PC 上位机双路控制 LED、风扇、蜂鸣器，实时采集室内温湿度与光照强度，并在 OLED 上可视化显示、回传上位机。

## 系统架构

基于 FreeRTOS 抢占式调度，按"实时性 / 频次"将 6 个任务分为三层优先级，**高优先级可抢占低优先级**，同优先级时间片轮转：

```
采集层  Prio 18   DHT11 温湿度采集任务 / BH1750 光照采集任务   （周期 500ms）
通信层  Prio 17   串口指令接收与解析任务                       （事件驱动）
执行层  Prio 15   音乐播放任务 / 按键处理任务                   （事件驱动）
显示层  Prio 13   OLED 统一显示任务                            （事件驱动）
```

任务按两种方式被调度：
- **周期型任务**（DHT11 / BH1750）：执行完一次后 `vTaskDelay(500)` 阻塞，到时自动回到就绪态再次执行；
- **事件型任务**（串口 / 音乐 / 按键 / OLED）：平时阻塞在消息队列上，收到消息或指令才被唤醒执行。

## 核心功能

- **环境采集**：DHT11 单总线温湿度、BH1750 I2C 光照，500ms 周期采集，均值校验后上 OLED / 上位机
- **本地按键控制**：按键外部中断（EXTI）只把事件写入消息队列，由按键任务统一消抖、确认、操作外设
- **上位机远程控制**：UART 中断接收 + 自定义串口指令帧解析，远程控制 LED、风扇调速、蜂鸣器播放/停止
- **外设驱动**：GPIO LED、TIM PWM 风扇调速、TIM2 PWM 蜂鸣器音乐播放、I2C OLED 显示

## 设计要点（本轮优化重点）

1. **OLED 抽离为独立显示任务**
   采集 / 串口 / 按键等业务任务不再直接写慢速 OLED，只把"要显示什么"发进 `OLED_Queue` 消息队列，由专门的 `Display_task` 统一刷屏。消除多任务并发抢占慢速 OLED 导致的显示错乱与互相阻塞。

2. **按键中断事件化**
   按键 EXTI 中断里不再做消抖和外设操作，只把按键事件（LED / 风扇 / 音乐）写进 `Key_Queue` 消息队列后立即返回；消抖、确认、外设操作统一放到 `KeyHandle_task`。中断处理极短，不阻塞其他中断。

3. **蜂鸣器非阻塞播放**
   发声交给 TIM2 硬件（PWM 输出到 PA0），软件只做时间调度：`Music_Play_Start()` 设首音符频率并记录结束时刻后立即返回，`Music_Play_Process()` 每 10ms 由播放任务驱动一次，用系统 tick（`xTaskGetTickCount`）判断"到点没"，到点才切下一个音符。相比原先 `Delay` 整曲阻塞几十秒，播放几乎不占 CPU。

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
| RTOS | FreeRTOS（抢占式调度，静态任务创建） |
| 通信 | UART 串口中断接收 + 自定义指令帧、I2C、单总线 |
| 同步 | 消息队列（OLED 显示 / 按键事件） |
| 驱动 | GPIO、TIM PWM、EXTI 外部中断 |
| 上位机 | Qt 5 + QSerialPort |
| 编译 | Keil MDK-ARM |

## 演示视频

[观看演示视频](./demo.mp4)

## 使用说明

1. **硬件连接**：按 `hardware/基于STM32F103C8T6的智能家居系统电路连接.pdf` 接线
2. **固件编译**：使用 Keil MDK 打开 `stm32_firmware/智能家居系统(基于STM32F103C8T6标准库+FreeROTS+串口上位机实现)/Project.uvprojx` 编译烧录
3. **上位机运行**：直接运行 `qt_app/SmartHome-ByUart-V1.0/SmartHomeByUart.exe`，选择串口连接
4. **控制方式**：本机按键控制，或上位机发送串口指令远程控制

## 开源协议

本项目仅用于个人学习及求职展示。
