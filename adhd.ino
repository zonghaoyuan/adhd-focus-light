#include "M5StickCPlus2.h"

// ----------------- LED control -----------------
const uint8_t LED_BRIGHTS[] = {30, 80, 150, 255};
const char LED_BRIGHT_NAMES[] = {'1', '2', '3', '4'};
uint8_t ledBrightIdx = 2;  // 默认中高亮度

static inline void setRedLed(bool on) {
  StickCP2.Power.setLed(on ? LED_BRIGHTS[ledBrightIdx] : 0);
}

// ----------------- Modes -----------------
enum Mode { BPM120, BPM100, BPM80, BPM60, PAUSE, NUM_MODES };
Mode currentMode = BPM120;
int  currentBPM = 120;

// ----------------- Ramp (降速配置) -----------------
const uint16_t RAMP_INTERVALS[] = {30, 60, 90, 120, 0};
const char* RAMP_NAMES[] = {"30s", "60s", "90s", "2m", "OFF"};
uint8_t rampIdx = 1;  // 默认 60s
uint32_t lastBPMUpdateMs = 0;

// 到达 60 BPM 后的自动暂停计时
const uint32_t AUTO_PAUSE_DELAY_MS = 5UL * 60UL * 1000UL;  // 5 分钟
uint32_t reachedMinBpmMs = 0;  // 到达 60 BPM 的时间点（0 表示未到达）

// ----------------- Screen Brightness -----------------
const uint8_t SCREEN_BRIGHTS[] = {20, 60, 120, 200};
uint8_t screenBrightIdx = 2;

// ----------------- Heartbeat -----------------
uint32_t beatSyncMs = 0;

// ----------------- UI -----------------
uint32_t sessionStartMs = 0;
bool screenOn = true;
uint8_t uiPage = 0; // 0=简洁 1=信息
bool uiNeedsRefresh = true;  // 是否需要刷新屏幕

struct UiCache {
  int bpm = -1;
  Mode mode = NUM_MODES;
  uint32_t sec = 0;
  uint32_t infoPageEnterSec = 0;  // 进入信息模式时的时间快照
  int batPct = -1;
  bool charging = false;
  uint8_t page = 255;
  uint8_t rampIdx = 255;
  uint8_t ledBrightIdx = 255;
} ui;

// ----------------- UI Draw Functions -----------------

void uiDrawFrame() {
  StickCP2.Display.fillScreen(BLACK);
  StickCP2.Display.setTextColor(WHITE);
  StickCP2.Display.setTextSize(2);
}

// 简洁模式：居中显示 BPM 或 PAUSE
void uiDrawMinimal() {
  StickCP2.Display.fillScreen(BLACK);
  StickCP2.Display.setTextColor(WHITE);

  if (currentMode == PAUSE) {
    StickCP2.Display.setTextSize(5);
    int16_t x = (StickCP2.Display.width() - 5 * 30) / 2;
    int16_t y = (StickCP2.Display.height() - 40) / 2;
    StickCP2.Display.setCursor(x, y);
    StickCP2.Display.print("PAUSE");
  } else {
    StickCP2.Display.setTextSize(7);
    int digits = (currentBPM >= 100) ? 3 : 2;
    int16_t x = (StickCP2.Display.width() - digits * 42) / 2;
    int16_t y = (StickCP2.Display.height() - 56) / 2;
    StickCP2.Display.setCursor(x, y);
    StickCP2.Display.printf("%d", currentBPM);
  }
}

// 信息模式：顶栏显示电量和 LED 亮度（左右对齐）
void uiDrawTopBar(int batPct, bool charging) {
  StickCP2.Display.fillRect(0, 0, StickCP2.Display.width(), 22, BLACK);
  StickCP2.Display.setTextSize(2);
  // 左侧：电量
  StickCP2.Display.setCursor(2, 2);
  StickCP2.Display.printf("BAT:%d%%%s", batPct, charging ? "+" : "");
  // 右侧：LED 亮度（右对齐，"LED:3" 约 5*12=60 像素宽）
  StickCP2.Display.setCursor(StickCP2.Display.width() - 62, 2);
  StickCP2.Display.printf("LED:%c", LED_BRIGHT_NAMES[ledBrightIdx]);
}

// 信息模式：中间显示 BPM 或 PAUSE
void uiDrawBigBpm() {
  StickCP2.Display.fillRect(0, 28, StickCP2.Display.width(), 78, BLACK);
  StickCP2.Display.setTextSize(4);
  StickCP2.Display.setCursor(10, 50);

  if (currentMode == PAUSE) {
    StickCP2.Display.print("PAUSE");
  } else {
    StickCP2.Display.printf("%d", currentBPM);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setCursor(130, 65);
    StickCP2.Display.print("BPM");
  }
}

// 信息模式：底栏显示运行时间和降速配置（左右对齐，静态显示）
void uiDrawBottom(uint32_t elapsedSec) {
  StickCP2.Display.fillRect(0, 110, StickCP2.Display.width(), 25, BLACK);
  StickCP2.Display.setTextSize(2);
  // 左侧：运行时间
  StickCP2.Display.setCursor(2, 112);
  StickCP2.Display.printf("Run:%02lu:%02lu", elapsedSec / 60, elapsedSec % 60);
  // 右侧：降速配置（右对齐）
  // "Ramp:60s" 约 8*12=96 像素宽，"Ramp:OFF" 也差不多
  StickCP2.Display.setCursor(StickCP2.Display.width() - 108, 112);
  StickCP2.Display.printf("Ramp:%s", RAMP_NAMES[rampIdx]);
}

void uiApplyScreenBrightness() {
  StickCP2.Display.setBrightness(SCREEN_BRIGHTS[screenBrightIdx]);
}

void uiSetScreen(bool on) {
  screenOn = on;
  if (!screenOn) {
    StickCP2.Display.fillScreen(BLACK);
    StickCP2.Display.setBrightness(0);
  } else {
    uiApplyScreenBrightness();
    uiDrawFrame();
    ui = UiCache{};
    ui.page = 255;
  }
}

void tickUI() {
  if (!screenOn) return;

  uint32_t now = millis();
  uint32_t elapsedSec = (now - sessionStartMs) / 1000;

  // 页面变化时强制刷新
  if (ui.page != uiPage) {
    ui.page = uiPage;
    ui.bpm = -1; ui.mode = NUM_MODES; ui.sec = 0;
    ui.batPct = -1; ui.rampIdx = 255; ui.ledBrightIdx = 255;
    uiNeedsRefresh = true;
    if (uiPage == 1) {
      ui.infoPageEnterSec = elapsedSec;
    }
  }

  // ===== 简洁模式（page 0）：只在 BPM/mode 变化时刷新 =====
  if (uiPage == 0) {
    if (ui.bpm != currentBPM || ui.mode != currentMode) {
      uiDrawMinimal();
      ui.bpm = currentBPM;
      ui.mode = currentMode;
    }
    return;
  }

  // ===== 信息模式（page 1）：只在按钮触发时刷新 =====
  if (!uiNeedsRefresh) return;
  uiNeedsRefresh = false;

  int batPct = StickCP2.Power.getBatteryLevel();
  bool charging = (StickCP2.Power.isCharging() != 0);

  // 首次进入时画框架
  if (ui.mode == NUM_MODES) {
    uiDrawFrame();
  }

  // 顶栏
  uiDrawTopBar(batPct, charging);
  ui.batPct = batPct;
  ui.ledBrightIdx = ledBrightIdx;

  // 中间 BPM/PAUSE
  uiDrawBigBpm();
  ui.bpm = currentBPM;
  ui.mode = currentMode;

  // 底栏
  uiDrawBottom(ui.infoPageEnterSec);
  ui.rampIdx = rampIdx;
}

// ----------------- Heartbeat (50% 占空比) -----------------
void tickHeartbeat() {
  // 暂停模式下关闭
  if (currentMode == PAUSE) {
    setRedLed(false);
    return;
  }

  // 信息模式下常亮，方便调节亮度
  if (uiPage == 1) {
    setRedLed(true);
    return;
  }

  uint32_t now = millis();
  uint32_t beatInterval = 60000UL / (uint32_t)currentBPM;
  uint32_t halfInterval = beatInterval / 2;
  uint32_t posInBeat = (now - beatSyncMs) % beatInterval;
  setRedLed(posInBeat < halfInterval);
}

// ----------------- Ramp tick & Auto shutdown -----------------
void tickRamp() {
  uint32_t now = millis();

  // PAUSE 模式下：2 分钟后自动关机
  if (currentMode == PAUSE) {
    if (reachedMinBpmMs > 0 && now - reachedMinBpmMs >= 2UL * 60UL * 1000UL) {
      StickCP2.Power.powerOff();  // 关机
    }
    return;
  }

  // 检查是否到达 60 BPM 并持续 5 分钟后自动暂停
  if (currentBPM <= 60 && reachedMinBpmMs > 0) {
    if (now - reachedMinBpmMs >= AUTO_PAUSE_DELAY_MS) {
      currentMode = PAUSE;
      reachedMinBpmMs = now;  // 重置计时，用于 2 分钟后关机
      setRedLed(false);
      return;
    }
  }

  uint16_t interval = RAMP_INTERVALS[rampIdx];
  if (interval == 0) return;

  uint32_t intervalMs = (uint32_t)interval * 1000UL;

  if (now - lastBPMUpdateMs >= intervalMs) {
    lastBPMUpdateMs += intervalMs;
    if (currentBPM > 60) {
      currentBPM -= 5;
    } else if (reachedMinBpmMs == 0) {
      // 刚到达 60 BPM，开始 5 分钟计时
      reachedMinBpmMs = now;
    }
  }
}

// ----------------- Buttons -----------------
// 用于正确区分长按和短按
bool btnAHoldTriggered = false;
bool btnBHoldTriggered = false;
uint32_t lastHoldMsA = 0;
uint32_t lastHoldMsB = 0;

void handleButtons() {
  StickCP2.update();

  // ========== Button A ==========
  // 检测长按（在信息模式下修改降速间隔）
  if (StickCP2.BtnA.wasHold() && millis() - lastHoldMsA > 500) {
    lastHoldMsA = millis();
    btnAHoldTriggered = true;

    if (uiPage == 1) {
      // 信息模式：长按修改降速间隔
      rampIdx = (rampIdx + 1) % (sizeof(RAMP_INTERVALS) / sizeof(RAMP_INTERVALS[0]));
      lastBPMUpdateMs = millis();
      uiNeedsRefresh = true;
    }
  }

  // 检测松开（短按）
  if (StickCP2.BtnA.wasReleased()) {
    if (!btnAHoldTriggered) {
      // 短按
      if (uiPage == 0) {
        // 简洁模式：切换 BPM 模式
        currentMode = static_cast<Mode>((currentMode + 1) % NUM_MODES);
        switch (currentMode) {
          case BPM120: currentBPM = 120; break;
          case BPM100: currentBPM = 100; break;
          case BPM80:  currentBPM = 80;  break;
          case BPM60:  currentBPM = 60;  break;
          case PAUSE:  break;
          default: break;
        }
        uint32_t now = millis();
        lastBPMUpdateMs = now;
        beatSyncMs = now;
        reachedMinBpmMs = 0;  // 重置自动暂停/关机计时
        setRedLed(false);
      } else {
        // 信息模式：短按修改 LED 亮度
        ledBrightIdx = (ledBrightIdx + 1) % (sizeof(LED_BRIGHTS) / sizeof(LED_BRIGHTS[0]));
        uiNeedsRefresh = true;
      }
    }
    btnAHoldTriggered = false;
  }

  // ========== Button B ==========
  // 检测长按（调节屏幕亮度）
  if (StickCP2.BtnB.wasHold() && millis() - lastHoldMsB > 500) {
    lastHoldMsB = millis();
    btnBHoldTriggered = true;

    // 长按 B 调节屏幕亮度（任意模式下都可以）
    screenBrightIdx = (screenBrightIdx + 1) % (sizeof(SCREEN_BRIGHTS) / sizeof(SCREEN_BRIGHTS[0]));
    uiApplyScreenBrightness();
  }

  // 检测松开（短按切页面）
  if (StickCP2.BtnB.wasReleased()) {
    if (!btnBHoldTriggered) {
      uiPage ^= 1;
    }
    btnBHoldTriggered = false;
  }
}

void setup() {
  auto cfg = M5.config();
  StickCP2.begin(cfg);

  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  StickCP2.Display.setRotation(3);
  StickCP2.Display.setTextSize(2);
  uiApplyScreenBrightness();
  uiDrawFrame();

  sessionStartMs = millis();
  lastBPMUpdateMs = sessionStartMs;
  beatSyncMs = sessionStartMs;

  // 启动自检：红灯亮 2 秒
  setRedLed(true);
  delay(2000);
  setRedLed(false);

  ui = UiCache{};
  ui.page = 255;
}

void loop() {
  handleButtons();
  tickRamp();
  tickHeartbeat();
  tickUI();
}
