# ADHD Focus Light

A red LED heartbeat blinker for M5StickC Plus2 designed to help people with ADHD improve focus and concentration.

## Background

Research suggests that rhythmic visual stimuli can help individuals with ADHD regulate attention and improve focus. This project creates a simple, portable device that produces a calming heartbeat-like red light pattern.

The device mimics a natural heartbeat rhythm with a "lub-dub" pattern, gradually slowing down over time to help the user transition into a more relaxed, focused state.

## Features

- **Heartbeat Pattern**: Realistic "lub-dub" double-flash pattern mimicking a natural heartbeat
- **Multiple BPM Modes**: 120 BPM (alert), 85 BPM (normal), 70 BPM (relaxed), and Pause
- **Auto Ramp-Down**: BPM automatically decreases by 5 every minute to gradually calm the user
- **Battery Powered**: Portable design using M5StickC Plus2's built-in battery
- **OLED Display**: Shows current BPM, mode, session time, and battery status
- **Adjustable Brightness**: Screen brightness can be adjusted for different environments

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

| Button | Short Press | Long Press |
|--------|-------------|------------|
| **BtnA** (front M5 button) | Cycle modes: 120 BPM → 85 BPM → 70 BPM → PAUSE → 120 BPM | Quick pause/resume (returns to previous mode and BPM) |
| **BtnB** (side button) | Toggle display page (simple/detailed) | Cycle screen brightness |

### Display Information

- **Page 0 (Simple)**: Mode, page number, battery percentage, BPM, session time, next BPM decrease countdown
- **Page 1 (Detailed)**: Additional battery voltage and brightness level

### Heartbeat Pattern

Each heartbeat consists of:
- First flash: 60ms ON
- Gap: 80ms OFF
- Second flash: 60ms ON
- Rest: Until next beat

## How It Works

1. **Startup**: Red LED lights up for 2 seconds as a self-test
2. **Running**: LED flashes in heartbeat pattern at the selected BPM
3. **Auto Ramp-Down**: Every 60 seconds, BPM decreases by 5 (minimum 65 BPM)
4. **Session Timer**: Tracks total session duration on display

## License

MIT License - Feel free to use, modify, and distribute.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## Acknowledgments

- Built with [M5StickCPlus2 Library](https://github.com/m5stack/M5StickCPlus2)
- Inspired by research on rhythmic visual stimuli for ADHD focus improvement
