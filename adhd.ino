#include "M5StickCPlus2.h"

// ----------------- LED control -----------------
// M5StickC Plus2 的红色 LED 通过电源管理芯片控制，不是 GPIO
// setLed() 接受 0-255 的亮度值，255 为最亮
static inline void setRedLed(bool on) {
  StickCP2.Power.setLed(on ? 255 : 0);
}

// ----------------- Modes -----------------
enum Mode { BPM120, BPM85, BPM70, PAUSE, NUM_MODES };
Mode currentMode = BPM120;
Mode savedMode = BPM120;     // 用于 PWR 暂停/恢复
int  currentBPM = 120;
int  savedBPM   = 120;

// 每分钟降速
uint32_t lastBPMUpdateMs = 0;

// ----------------- Heartbeat pattern (lub-dub) -----------------
constexpr uint16_t P1_ON_MS = 60;
constexpr uint16_t GAP_MS   = 80;
constexpr uint16_t P2_ON_MS = 60;

bool     inBeat = false;
uint32_t beatStartMs = 0;
uint32_t nextBeatDueMs = 0;

// ----------------- UI -----------------
uint32_t sessionStartMs = 0;
bool screenOn = true;

uint8_t uiPage = 0; // 0=简洁 1=信息
uint8_t brightIdx = 2;
const uint8_t BRIGHTS[] = {20, 60, 120, 200};

struct UiCache {
  int bpm = -1;
  Mode mode = NUM_MODES;
  uint32_t sec = 0;
  uint32_t nextDrop = 9999;
  int batPct = -1;
  int batMv  = -1;
  bool charging = false;
  uint8_t page = 255;
  bool screenOn = true;
  uint8_t bright = 255;
} ui;

// 使用数组代替函数，避免 Arduino 预处理器自动生成函数原型导致的编译错误
static const char* MODE_NAMES[] = {"120", "85", "70", "PAUSE", "?"};
#define modeName(m) (MODE_NAMES[(m) < NUM_MODES ? (m) : NUM_MODES])

void uiDrawFrame() {
  StickCP2.Display.fillScreen(BLACK);
  StickCP2.Display.setTextColor(WHITE);
  StickCP2.Display.setTextSize(2);
}

void uiDrawTopBar(int batPct, bool charging) {
  StickCP2.Display.fillRect(0, 0, StickCP2.Display.width(), 22, BLACK);
  StickCP2.Display.setCursor(2, 2);
  StickCP2.Display.printf("M:%s  Pg:%d  B:%d%% %s",
                          modeName(currentMode), uiPage, batPct, charging ? "CHG" : "");
}

void uiDrawBigBpm(int bpm) {
  StickCP2.Display.fillRect(0, 28, StickCP2.Display.width(), 78, BLACK);
  StickCP2.Display.setTextSize(4);
  StickCP2.Display.setCursor(10, 45);
  StickCP2.Display.printf("%d", bpm);
  StickCP2.Display.setTextSize(2);
  StickCP2.Display.setCursor(120, 65);
  StickCP2.Display.print("BPM");
}

void uiDrawBottom(uint32_t elapsedSec, uint32_t nextDropSec) {
  StickCP2.Display.fillRect(0, 110, StickCP2.Display.width(), 25, BLACK);
  StickCP2.Display.setCursor(2, 112);
  StickCP2.Display.printf("t %02lu:%02lu  next -5:%lus",
                          elapsedSec / 60, elapsedSec % 60, nextDropSec);
}

void uiDrawInfoLine(int batMv, uint8_t bright) {
  StickCP2.Display.fillRect(0, 90, StickCP2.Display.width(), 18, BLACK);
  StickCP2.Display.setCursor(2, 92);
  StickCP2.Display.printf("BAT:%dmV  BR:%d", batMv, bright);
}

void uiApplyBrightness() {
  StickCP2.Display.setBrightness(BRIGHTS[brightIdx]);
}

void uiSetScreen(bool on) {
  screenOn = on;
  if (!screenOn) {
    StickCP2.Display.fillScreen(BLACK);
    StickCP2.Display.setBrightness(0);
  } else {
    uiApplyBrightness();
    uiDrawFrame();
    // 强制刷新
    ui = UiCache{};
    ui.page = 255;
  }
}

void tickUI() {
  if (!screenOn) return;

  uint32_t now = millis();
  uint32_t elapsedSec = (now - sessionStartMs) / 1000;

  // 计算下一次降速倒计时
  uint32_t nextDropSec = 0;
  if (currentMode != PAUSE) {
    uint32_t passed = (now - lastBPMUpdateMs) / 1000;
    nextDropSec = (passed >= 60) ? 0 : (60 - passed);
  }

  int batPct = StickCP2.Power.getBatteryLevel();     // 0~100（若库不支持可能返回异常值）
  int batMv  = StickCP2.Power.getBatteryVoltage();   // mV
  bool charging = (StickCP2.Power.isCharging() != 0);

  // 页面变化/首次：画框架
  if (ui.page != uiPage) {
    uiDrawFrame();
    ui.page = uiPage;
    // 强制全部重画
    ui.bpm = -1; ui.mode = NUM_MODES; ui.sec = 0; ui.nextDrop = 9999;
    ui.batPct = -1; ui.batMv = -1; ui.charging = !charging;
  }

  // 顶栏：模式/电量/充电变化才重画
  if (ui.mode != currentMode || ui.batPct != batPct || ui.charging != charging) {
    uiDrawTopBar(batPct, charging);
    ui.mode = currentMode;
    ui.batPct = batPct;
    ui.charging = charging;
  }

  // BPM：变化才重画
  if (ui.bpm != currentBPM) {
    uiDrawBigBpm(currentBPM);
    ui.bpm = currentBPM;
  }

  // 底栏：每秒更新一次（时间/倒计时）
  if (ui.sec != elapsedSec || ui.nextDrop != nextDropSec) {
    uiDrawBottom(elapsedSec, nextDropSec);
    ui.sec = elapsedSec;
    ui.nextDrop = nextDropSec;
  }

  // 信息页额外内容：电压/亮度（变化才画）
  if (uiPage == 1 && (ui.batMv != batMv || ui.bright != BRIGHTS[brightIdx])) {
    uiDrawInfoLine(batMv, BRIGHTS[brightIdx]);
    ui.batMv = batMv;
    ui.bright = BRIGHTS[brightIdx];
  }
}

// ----------------- Heartbeat tick -----------------
void tickHeartbeat() {
  uint32_t now = millis();

  if (currentMode == PAUSE) {
    setRedLed(false);
    return;
  }

  // 开始下一拍
  if (!inBeat && (int32_t)(now - nextBeatDueMs) >= 0) {
    inBeat = true;
    beatStartMs = now;

    uint32_t beatInterval = 60000UL / (uint32_t)currentBPM;
    nextBeatDueMs = now + beatInterval;
  }

  // 在一拍内部跑 lub-dub
  if (inBeat) {
    uint32_t t = now - beatStartMs;
    if (t < P1_ON_MS) {
      setRedLed(true);
    } else if (t < (uint32_t)P1_ON_MS + GAP_MS) {
      setRedLed(false);
    } else if (t < (uint32_t)P1_ON_MS + GAP_MS + P2_ON_MS) {
      setRedLed(true);
    } else {
      setRedLed(false);
      inBeat = false;
    }
  }
}

// ----------------- Ramp tick -----------------
void tickRamp() {
  if (currentMode == PAUSE) return;

  uint32_t now = millis();
  if (now - lastBPMUpdateMs >= 60000UL) {
    lastBPMUpdateMs += 60000UL;  // 防漂移
    if (currentBPM > 65) currentBPM -= 5;
  }
}

// ----------------- Buttons -----------------
uint32_t lastHoldMsA = 0;
uint32_t lastHoldMsB = 0;

void handleButtons() {
  StickCP2.update();

  // A：短按切模式
  if (StickCP2.BtnA.wasPressed()) {
    currentMode = static_cast<Mode>((currentMode + 1) % NUM_MODES);
    switch (currentMode) {
      case BPM120: currentBPM = 120; break;
      case BPM85:  currentBPM = 85;  break;
      case BPM70:  currentBPM = 70;  break;
      case PAUSE:  break;
      default: break;
    }
    // 重新对齐节拍/降速计时
    uint32_t now = millis();
    lastBPMUpdateMs = now;
    nextBeatDueMs = now;
    inBeat = false;
    setRedLed(false);
  }

  // A：长按暂停/继续（恢复到暂停前的模式与 BPM）
  if (StickCP2.BtnA.wasHold() && millis() - lastHoldMsA > 800) {
    lastHoldMsA = millis();
    if (currentMode == PAUSE) {
      currentMode = savedMode;
      currentBPM  = savedBPM;
    } else {
      savedMode = currentMode;
      savedBPM  = currentBPM;
      currentMode = PAUSE;
    }
    uint32_t now = millis();
    lastBPMUpdateMs = now;
    nextBeatDueMs = now;
    inBeat = false;
    setRedLed(false);
  }

  // B：短按切页面
  if (StickCP2.BtnB.wasPressed()) {
    uiPage ^= 1;
  }

  // B：长按调亮度（加个节流，避免一次长按跳太多）
  if (StickCP2.BtnB.wasHold() && millis() - lastHoldMsB > 500) {
    lastHoldMsB = millis();
    brightIdx = (brightIdx + 1) % (sizeof(BRIGHTS) / sizeof(BRIGHTS[0]));
    if (screenOn) uiApplyBrightness();
  }
}

void setup() {
  auto cfg = M5.config();
  StickCP2.begin(cfg);

  // 电池供电/唤醒场景：保持供电（对 USB 供电也不会有副作用）
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  StickCP2.Display.setRotation(3);
  StickCP2.Display.setTextSize(2);
  uiApplyBrightness();
  uiDrawFrame();

  sessionStartMs = millis();
  lastBPMUpdateMs = sessionStartMs;
  nextBeatDueMs   = sessionStartMs;

  // 启动自检：红灯强制亮 2 秒，帮助确认控制没问题
  setRedLed(true);
  delay(2000);
  setRedLed(false);

  // 强制 UI 首次全画
  ui = UiCache{};
  ui.page = 255;
}

void loop() {
  handleButtons();
  tickRamp();
  tickHeartbeat();
  tickUI();
}