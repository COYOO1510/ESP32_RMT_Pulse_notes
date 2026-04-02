# PIO_ESP32_RMT_Pulse
Arduino开发ESP32输出脉冲测试，可指定引脚、脉冲频率、脉冲宽度、脉冲个数。

### 介绍

> [!note] 信息
> ESP32 的 RMT（Remote Control Transceiver） 是一种高精度脉冲收发外设，最初用于红外遥控，但也可作为通用时序信号发生器。它能按设定的高低电平持续时间自动输出或接收数字脉冲，适合实现红外编码、WS2812驱动、固定数量脉冲输出、同步触发等功能。相比普通 GPIO 翻转，RMT 定时更准、占用 CPU 更少，更适合对脉宽和时序要求较高的场景。

### 测试

> [!note] 2kHz 测试
> 频率：2kHz
> 占空比：24%
> 脉冲个数：10

<p align="center">
  <img src="/README/TEK00003.PNG" alt="[TEK00003.png![TEK00003.PNG">
</p>

> [!note] 500kHz 测试
> 频率：500kHz
> 占空比：20%
> 脉冲个数：5

<p align="center">
  <img src="/README/TEK00005.PNG" alt="[TEK00005.png![TEK00005.PNG">
</p>

> 实用结论：
- 要有 **1% 粒度**：建议 frequency <= 10 kHz
- 要控制在 **±1% 误差内**：建议 frequency <= 20 kHz
- 50 kHz 时步进已是 5%，高频下占空比会明显变粗
- 500 kHz 虽可输出，但占空比基本不“准确”（步进约 50%）
