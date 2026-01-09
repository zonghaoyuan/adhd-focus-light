#include "M5StickCPlus2.h"

// ----------------- LED control -----------------
const uint8_t LED_BRIGHTS[] = {30, 80, 150, 255};
const char LED_BRIGHT_NAMES[] = {'1', '2', '3', '4'};
uint8_t ledBrightIdx = 2;  // Default medium-high brightness

static inline void setRedLed(bool on) {
  StickCP2.Power.setLed(on ? LED_BRIGHTS[ledBrightIdx] : 0);
}

// ----------------- Modes -----------------
enum Mode { BPM120, BPM100, BPM80, BPM60, PAUSE, NUM_MODES };
Mode currentMode = BPM120;
int  currentBPM = 120;

// ----------------- Ramp Configuration -----------------
const uint16_t RAMP_INTERVALS[] = {30, 60, 90, 120, 0};
const char* RAMP_NAMES[] = {"30s", "60s", "90s", "2m", "OFF"};
uint8_t rampIdx = 1;  // Default 60s
uint32_t lastBPMUpdateMs = 0;

// Auto-pause timer after reaching 60 BPM
const uint32_t AUTO_PAUSE_DELAY_MS = 5UL * 60UL * 1000UL;  // 5 minutes
uint32_t reachedMinBpmMs = 0;  // Timestamp when 60 BPM was reached (0 = not reached)

// ----------------- Screen Brightness -----------------
const uint8_t SCREEN_BRIGHTS[] = {20, 60, 120, 200};
uint8_t screenBrightIdx = 2;

// ----------------- Heartbeat -----------------
uint32_t beatSyncMs = 0;

// ----------------- UI -----------------
uint32_t sessionStartMs = 0;
bool screenOn = true;
uint8_t uiPage = 0; // 0=minimal 1=info
bool uiNeedsRefresh = true;  // Whether screen needs refresh

struct UiCache {
  int bpm = -1;
  Mode mode = NUM_MODES;
  uint32_t sec = 0;
  uint32_t infoPageEnterSec = 0;  // Time snapshot when entering info mode
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

// Minimal mode: centered BPM or PAUSE
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

// Info mode: top bar with battery and LED brightness (left-right aligned)
void uiDrawTopBar(int batPct, bool charging) {
  StickCP2.Display.fillRect(0, 0, StickCP2.Display.width(), 22, BLACK);
  StickCP2.Display.setTextSize(2);
  // Left: battery
  StickCP2.Display.setCursor(2, 2);
  StickCP2.Display.printf("BAT:%d%%%s", batPct, charging ? "+" : "");
  // Right: LED brightness (right-aligned, "LED:3" ~5*12=60 pixels wide)
  StickCP2.Display.setCursor(StickCP2.Display.width() - 62, 2);
  StickCP2.Display.printf("LED:%c", LED_BRIGHT_NAMES[ledBrightIdx]);
}

// Info mode: center BPM or PAUSE
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

// Info mode: bottom bar with run time and ramp config (left-right aligned, static)
void uiDrawBottom(uint32_t elapsedSec) {
  StickCP2.Display.fillRect(0, 110, StickCP2.Display.width(), 25, BLACK);
  StickCP2.Display.setTextSize(2);
  // Left: run time
  StickCP2.Display.setCursor(2, 112);
  StickCP2.Display.printf("Run:%02lu:%02lu", elapsedSec / 60, elapsedSec % 60);
  // Right: ramp config (right-aligned)
  // "Ramp:60s" ~8*12=96 pixels wide
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

  // Force refresh on page change
  if (ui.page != uiPage) {
    ui.page = uiPage;
    ui.bpm = -1; ui.mode = NUM_MODES; ui.sec = 0;
    ui.batPct = -1; ui.rampIdx = 255; ui.ledBrightIdx = 255;
    uiNeedsRefresh = true;
    if (uiPage == 1) {
      ui.infoPageEnterSec = elapsedSec;
    }
  }

  // ===== Minimal mode (page 0): refresh only on BPM/mode change =====
  if (uiPage == 0) {
    if (ui.bpm != currentBPM || ui.mode != currentMode) {
      uiDrawMinimal();
      ui.bpm = currentBPM;
      ui.mode = currentMode;
    }
    return;
  }

  // ===== Info mode (page 1): refresh only on button press =====
  if (!uiNeedsRefresh) return;
  uiNeedsRefresh = false;

  int batPct = StickCP2.Power.getBatteryLevel();
  // isCharging() returns: 0=not charging, 1=charging, 2=charge complete
  // Only show charging indicator when state is 1
  bool charging = (StickCP2.Power.isCharging() == 1);

  // Draw frame on first entry
  if (ui.mode == NUM_MODES) {
    uiDrawFrame();
  }

  // Top bar
  uiDrawTopBar(batPct, charging);
  ui.batPct = batPct;
  ui.ledBrightIdx = ledBrightIdx;

  // Center BPM/PAUSE
  uiDrawBigBpm();
  ui.bpm = currentBPM;
  ui.mode = currentMode;

  // Bottom bar
  uiDrawBottom(ui.infoPageEnterSec);
  ui.rampIdx = rampIdx;
}

// ----------------- Heartbeat (50% duty cycle) -----------------
void tickHeartbeat() {
  // Turn off in pause mode
  if (currentMode == PAUSE) {
    setRedLed(false);
    return;
  }

  // Stay on in info mode to help adjust brightness
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

  // In PAUSE mode: auto power off after 2 minutes
  if (currentMode == PAUSE) {
    if (reachedMinBpmMs > 0 && now - reachedMinBpmMs >= 2UL * 60UL * 1000UL) {
      StickCP2.Power.powerOff();
    }
    return;
  }

  // Check if at 60 BPM for 5 minutes, then auto pause
  if (currentBPM <= 60 && reachedMinBpmMs > 0) {
    if (now - reachedMinBpmMs >= AUTO_PAUSE_DELAY_MS) {
      currentMode = PAUSE;
      reachedMinBpmMs = now;  // Reset timer for 2-minute power off
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
      // Just reached 60 BPM, start 5-minute timer
      reachedMinBpmMs = now;
    }
  }
}

// ----------------- Buttons -----------------
// For proper short/long press distinction
bool btnAHoldTriggered = false;
bool btnBHoldTriggered = false;
uint32_t lastHoldMsA = 0;
uint32_t lastHoldMsB = 0;

void handleButtons() {
  StickCP2.update();

  // ========== Button A ==========
  // Detect long press (modify ramp interval in info mode)
  if (StickCP2.BtnA.wasHold() && millis() - lastHoldMsA > 500) {
    lastHoldMsA = millis();
    btnAHoldTriggered = true;

    if (uiPage == 1) {
      // Info mode: long press to modify ramp interval
      rampIdx = (rampIdx + 1) % (sizeof(RAMP_INTERVALS) / sizeof(RAMP_INTERVALS[0]));
      lastBPMUpdateMs = millis();
      uiNeedsRefresh = true;
    }
  }

  // Detect release (short press)
  if (StickCP2.BtnA.wasReleased()) {
    if (!btnAHoldTriggered) {
      // Short press
      if (uiPage == 0) {
        // Minimal mode: cycle BPM modes
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
        reachedMinBpmMs = 0;  // Reset auto pause/power off timer
        setRedLed(false);
      } else {
        // Info mode: short press to modify LED brightness
        ledBrightIdx = (ledBrightIdx + 1) % (sizeof(LED_BRIGHTS) / sizeof(LED_BRIGHTS[0]));
        uiNeedsRefresh = true;
      }
    }
    btnAHoldTriggered = false;
  }

  // ========== Button B ==========
  // Detect long press (adjust screen brightness)
  if (StickCP2.BtnB.wasHold() && millis() - lastHoldMsB > 500) {
    lastHoldMsB = millis();
    btnBHoldTriggered = true;

    // Long press B to adjust screen brightness (works in any mode)
    screenBrightIdx = (screenBrightIdx + 1) % (sizeof(SCREEN_BRIGHTS) / sizeof(SCREEN_BRIGHTS[0]));
    uiApplyScreenBrightness();
  }

  // Detect release (short press to switch page)
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

  // Startup self-test: red LED on for 2 seconds
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
