# ADHD 专注灯

[English](README.md)

一个基于 M5StickC Plus2 的红色 LED 心跳闪烁器，旨在帮助 ADHD 患者提高专注力。

## 背景

本项目的灵感来源于 [Hacker News 上的一条评论](https://news.ycombinator.com/item?id=38274782)，一位用户分享了他管理 ADHD 的个人技巧：

> 在显示器旁边放一个小 LED。让它像快速心跳一样闪烁（120-150 bpm），然后逐渐减慢到 60 bpm 左右。在不知不觉中，你的大脑会尝试与这个几乎看不见的光同步，让你平静下来并进入专注模式。效果就像催眠一样！

本项目同时基于 Qiaogun 的 [ADHD_Blink](https://github.com/Qiaogun/ADHD_Blink) 项目，该项目为 M5StickC Plus 实现了这一概念。本版本针对更新的 M5StickC Plus2 硬件进行了适配。

## 功能特性

- **心跳模式**：模拟真实心跳的 "lub-dub" 双闪模式
- **多种 BPM 模式**：120 BPM（警觉）、85 BPM（正常）、70 BPM（放松）和暂停
- **自动降速**：每分钟 BPM 自动降低 5，逐渐让用户平静下来
- **电池供电**：使用 M5StickC Plus2 内置电池的便携设计
- **OLED 显示屏**：显示当前 BPM、模式、会话时间和电池状态
- **可调亮度**：屏幕亮度可根据不同环境调整

## 硬件要求

- [M5StickC Plus2](https://shop.m5stack.com/products/m5stickc-plus2-esp32-mini-iot-development-kit)（基于 ESP32 的迷你开发套件）

## 安装

### 前置条件

1. 安装 [Arduino IDE](https://www.arduino.cc/en/software) 或 [arduino-cli](https://arduino.github.io/arduino-cli/)
2. 添加 M5Stack 开发板管理器 URL：
   ```
   https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
   ```
3. 安装 M5Stack ESP32 开发板包
4. 安装 `M5StickCPlus2` 库

### 使用 Arduino CLI

```bash
# 添加 M5Stack 开发板 URL
arduino-cli config add board_manager.additional_urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json

# 更新索引并安装开发板
arduino-cli core update-index
arduino-cli core install m5stack:esp32

# 编译
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 adhd.ino

# 上传（将 PORT 替换为你的串口）
arduino-cli upload -p PORT --fqbn m5stack:esp32:m5stack_stickc_plus2 adhd.ino
```

### 使用 Arduino IDE

1. 在 Arduino IDE 中打开 `adhd.ino`
2. 选择开发板：`M5StickC Plus2`
3. 选择正确的端口
4. 点击上传

## 使用方法

### 按键控制

| 按键 | 短按 | 长按 |
|------|------|------|
| **BtnA**（正面 M5 按钮） | 切换模式：120 BPM → 85 BPM → 70 BPM → 暂停 → 120 BPM | 快速暂停/恢复（返回之前的模式和 BPM） |
| **BtnB**（侧面按钮） | 切换显示页面（简洁/详细） | 切换屏幕亮度 |

### 显示信息

- **页面 0（简洁）**：模式、页码、电池百分比、BPM、会话时间、下次 BPM 降低倒计时
- **页面 1（详细）**：额外显示电池电压和亮度级别

### 心跳模式

每次心跳包含：
- 第一次闪烁：亮 60ms
- 间隔：灭 80ms
- 第二次闪烁：亮 60ms
- 休息：直到下一次心跳

## 工作原理

1. **启动**：红色 LED 亮 2 秒作为自检
2. **运行**：LED 按选定的 BPM 以心跳模式闪烁
3. **自动降速**：每 60 秒，BPM 降低 5（最低 65 BPM）
4. **会话计时器**：在显示屏上跟踪总会话时长

## 许可证

MIT 许可证 - 可自由使用、修改和分发。

## 贡献

欢迎贡献！请随时提交 Issue 或 Pull Request。

## 致谢

- 原始创意来自 [这条 Hacker News 评论](https://news.ycombinator.com/item?id=38274782)
- 基于 Qiaogun 的 [ADHD_Blink](https://github.com/Qiaogun/ADHD_Blink) 项目
- 使用 [M5StickCPlus2 库](https://github.com/m5stack/M5StickCPlus2) 构建
