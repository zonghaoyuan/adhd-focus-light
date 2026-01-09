# ADHD Focus Light

[中文版](README_CN.md)

A red LED heartbeat blinker for M5StickC Plus2 designed to help people with ADHD improve focus and concentration.

## Background

This project is inspired by a [Hacker News comment](https://news.ycombinator.com/item?id=38274782) where a user shared their personal hack for managing ADHD:

> Place a tiny LED by the side of your monitor. Make it blink like a fast heartbeat (120-150 bpm) and gradually slow down to around 60 bpm. Without realizing, your brain will try to sync with the light that you can barely see, calming you down and allowing you to go focus-mode. Works like hypnosis!

This project is also based on [ADHD_Blink](https://github.com/Qiaogun/ADHD_Blink) by Qiaogun, which implemented this concept for M5StickC Plus. This version is updated for the newer M5StickC Plus2 hardware.

## Photos

| Minimal Mode | Info Mode |
|:------------:|:---------:|
| ![Minimal Mode](images/minimal-mode.jpeg) | ![Info Mode](images/info-mode.jpeg) |

## Features

- **50% Duty Cycle Flash**: Natural blinking pattern (half on, half off per beat)
- **Multiple BPM Modes**: 120 → 100 → 80 → 60 → PAUSE
- **Configurable Ramp-Down**: BPM decreases by 5 at configurable intervals (30s/60s/90s/2m/OFF)
- **Auto Sleep Flow**: 60 BPM for 5 min → PAUSE for 2 min → Auto power off
- **Adjustable LED Brightness**: 4 levels (30/80/150/255), default level 3
- **Adjustable Screen Brightness**: 4 levels (20/60/120/200), default level 3
- **Dual Display Modes**: Minimal mode (BPM only) and Info mode
- **Battery Powered**: Portable design using M5StickC Plus2's built-in battery

## Hardware Requirements

- [M5StickC Plus2](https://shop.m5stack.com/products/m5stickc-plus2-esp32-mini-iot-development-kit) (ESP32-based mini development kit)

## Installation

### Prerequisites

1. Install [Arduino IDE](https://www.arduino.cc/en/software) or [arduino-cli](https://arduino.github.io/arduino-cli/)
2. Add M5Stack board manager URL:
   ```
   https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
   ```
3. Install M5Stack ESP32 board package
4. Install `M5StickCPlus2` library

### Using Arduino CLI

```bash
# Add M5Stack board URL
arduino-cli config add board_manager.additional_urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json

# Update index and install board
arduino-cli core update-index
arduino-cli core install m5stack:esp32

# Compile
arduino-cli compile --fqbn m5stack:esp32:m5stack_stickc_plus2 adhd.ino

# Upload (replace PORT with your serial port)
arduino-cli upload -p PORT --fqbn m5stack:esp32:m5stack_stickc_plus2 adhd.ino
```

### Using Arduino IDE

1. Open `adhd.ino` in Arduino IDE
2. Select Board: `M5StickC Plus2`
3. Select the correct Port
4. Click Upload

## Usage

### Button Controls

#### Minimal Mode (Page 0) - Default
| Button | Short Press | Long Press |
|--------|-------------|------------|
| **BtnA** (front) | Cycle modes: 120 → 100 → 80 → 60 → PAUSE → 120... | - |
| **BtnB** (side) | Switch to Info Mode | Cycle screen brightness |

#### Info Mode (Page 1)
| Button | Short Press | Long Press |
|--------|-------------|------------|
| **BtnA** (front) | Cycle LED brightness (1→2→3→4) | Cycle ramp interval |
| **BtnB** (side) | Switch to Minimal Mode | Cycle screen brightness |

### Display Modes

- **Minimal Mode**: Large centered BPM number or "PAUSE" - zero distractions
- **Info Mode**: Shows battery %, LED brightness, BPM/PAUSE, run time, and ramp config
  - Top bar: `BAT:XX%[+]` (left) and `LED:X` (right)
  - Center: Large BPM value or "PAUSE"
  - Bottom bar: `Run:MM:SS` (left) and `Ramp:XXs` (right)
  - LED stays on solid to help adjust brightness
  - Display is static (time snapshot when entering) to save power

### Ramp Intervals

Configurable via long press on BtnA in Info Mode:
- **30s**: Fast ramp-down
- **60s**: Default
- **90s**: Slow ramp-down
- **2m**: Very slow ramp-down
- **OFF**: No auto ramp-down

### Auto Sleep Flow

```
120 BPM → 100 → 80 → 60 → [5 min] → PAUSE → [2 min] → Power Off
```

1. BPM decreases by 5 at each ramp interval
2. After reaching 60 BPM, continues for 5 minutes
3. Automatically enters PAUSE mode
4. After 2 minutes in PAUSE, device powers off

Manual mode switching resets all timers.

## How It Works

1. **Startup**: Red LED lights up for 2 seconds as a self-test
2. **Running**: LED flashes at 50% duty cycle at current BPM
3. **Auto Ramp-Down**: BPM decreases by 5 at configured interval (min 60 BPM)
4. **Auto Sleep**: After reaching 60 BPM for 5 min, pauses then powers off

## Default Settings

| Setting | Default Value |
|---------|---------------|
| Starting BPM | 120 |
| LED Brightness | Level 3 (150/255) |
| Screen Brightness | Level 3 (120/200) |
| Ramp Interval | 60 seconds |
| Display Mode | Minimal |

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=zonghaoyuan/adhd-focus-light&type=Date)](https://star-history.com/#zonghaoyuan/adhd-focus-light&Date)

## License

MIT License - Feel free to use, modify, and distribute.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## Acknowledgments

- Original idea from [this Hacker News comment](https://news.ycombinator.com/item?id=38274782)
- Based on [ADHD_Blink](https://github.com/Qiaogun/ADHD_Blink) by Qiaogun
- Built with [M5StickCPlus2 Library](https://github.com/m5stack/M5StickCPlus2)
