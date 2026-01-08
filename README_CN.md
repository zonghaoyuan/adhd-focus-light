# ADHD 专注灯

[English](README.md)

一个基于 M5StickC Plus2 的红色 LED 心跳闪烁器，旨在帮助 ADHD 患者提高专注力。

## 背景

本项目的灵感来源于 [Hacker News 上的一条评论](https://news.ycombinator.com/item?id=38274782)，一位用户分享了他管理 ADHD 的个人技巧：

> 在显示器旁边放一个小 LED。让它像快速心跳一样闪烁（120-150 bpm），然后逐渐减慢到 60 bpm 左右。在不知不觉中，你的大脑会尝试与这个几乎看不见的光同步，让你平静下来并进入专注模式。效果就像催眠一样！

本项目同时基于 Qiaogun 的 [ADHD_Blink](https://github.com/Qiaogun/ADHD_Blink) 项目，该项目为 M5StickC Plus 实现了这一概念。本版本针对更新的 M5StickC Plus2 硬件进行了适配。

## 实物照片

| 简洁模式 | 信息模式 |
|:-------:|:-------:|
| ![简洁模式](images/minimal-mode.jpeg) | ![信息模式](images/info-mode.jpeg) |

## 功能特性

- **50% 占空比闪烁**：自然的闪烁模式（每拍亮灭各一半时间）
- **多种 BPM 模式**：120 → 100 → 80 → 60 → 暂停
- **可配置降速间隔**：每次降 5 BPM，间隔可选 30s/60s/90s/2m/关闭
- **自动休眠流程**：60 BPM 持续 5 分钟 → 暂停 2 分钟 → 自动关机
- **可调 LED 亮度**：4 档（30/80/150/255），默认 3 档
- **可调屏幕亮度**：4 档（20/60/120/200），默认 3 档
- **双显示模式**：简洁模式（仅 BPM）和信息模式
- **电池供电**：使用 M5StickC Plus2 内置电池的便携设计

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

#### 简洁模式（页面 0）- 默认
| 按键 | 短按 | 长按 |
|------|------|------|
| **BtnA**（正面） | 切换模式：120 → 100 → 80 → 60 → 暂停 → 120... | - |
| **BtnB**（侧面） | 切换到信息模式 | 调节屏幕亮度 |

#### 信息模式（页面 1）
| 按键 | 短按 | 长按 |
|------|------|------|
| **BtnA**（正面） | 调节 LED 亮度（1→2→3→4） | 调节降速间隔 |
| **BtnB**（侧面） | 切换到简洁模式 | 调节屏幕亮度 |

### 显示模式

- **简洁模式**：居中显示大号 BPM 数字或 "PAUSE" - 零干扰
- **信息模式**：显示电量、LED 亮度、BPM/暂停、运行时间、降速配置
  - 顶栏：`BAT:XX%[+]`（左）和 `LED:X`（右）
  - 中间：大号 BPM 数值或 "PAUSE"
  - 底栏：`Run:MM:SS`（左）和 `Ramp:XXs`（右）
  - LED 常亮以便调节亮度
  - 显示内容静态不更新（进入时的时间快照），节省电量

### 降速间隔

在信息模式下长按 BtnA 可切换：
- **30s**：快速降速
- **60s**：默认
- **90s**：慢速降速
- **2m**：非常慢的降速
- **OFF**：关闭自动降速

### 自动休眠流程

```
120 BPM → 100 → 80 → 60 → [5 分钟] → 暂停 → [2 分钟] → 自动关机
```

1. 每隔设定间隔，BPM 降低 5
2. 到达 60 BPM 后，继续运行 5 分钟
3. 自动进入暂停模式
4. 暂停 2 分钟后，设备自动关机

手动切换模式会重置所有计时器。

## 工作原理

1. **启动**：红色 LED 亮 2 秒作为自检
2. **运行**：LED 按当前 BPM 以 50% 占空比闪烁
3. **自动降速**：按配置间隔每次降 5 BPM（最低 60 BPM）
4. **自动休眠**：到达 60 BPM 后 5 分钟进入暂停，再 2 分钟后自动关机

## 默认设置

| 设置项 | 默认值 |
|--------|--------|
| 起始 BPM | 120 |
| LED 亮度 | 3 档（150/255） |
| 屏幕亮度 | 3 档（120/200） |
| 降速间隔 | 60 秒 |
| 显示模式 | 简洁模式 |

## 许可证

MIT 许可证 - 可自由使用、修改和分发。

## 贡献

欢迎贡献！请随时提交 Issue 或 Pull Request。

## 致谢

- 原始创意来自 [这条 Hacker News 评论](https://news.ycombinator.com/item?id=38274782)
- 基于 Qiaogun 的 [ADHD_Blink](https://github.com/Qiaogun/ADHD_Blink) 项目
- 使用 [M5StickCPlus2 库](https://github.com/m5stack/M5StickCPlus2) 构建
