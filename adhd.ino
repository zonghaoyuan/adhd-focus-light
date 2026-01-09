#include "M5StickCPlus2.h"
#include <Preferences.h>

// ----------------- Time Constants -----------------
const uint32_t LOOP_DELAY_MS = 10;
const uint32_t BAT_READ_INTERVAL_MS = 30000;  // Read battery every 30 seconds
const uint32_t LONG_PRESS_INTERVAL_MS = 500;
const uint32_t STARTUP_LED_DURATION_MS = 2000;

// ----------------- Settings Persistence -----------------
Preferences prefs;

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

// ----------------- Auto Sleep Configuration -----------------
// Auto-pause delay after reaching 60 BPM (in minutes)
const uint16_t AUTO_PAUSE_DELAYS[] = {3, 5, 10, 0};  // 0 = OFF
const char* AUTO_PAUSE_NAMES[] = {"3m", "5m", "10m", "OFF"};
uint8_t autoPauseIdx = 1;  // Default 5 minutes

// Auto power-off delay after PAUSE mode (in minutes)
const uint16_t AUTO_OFF_DELAYS[] = {1, 2, 5, 0};  // 0 = OFF
const char* AUTO_OFF_NAMES[] = {"1m", "2m", "5m", "OFF"};
uint8_t autoOffIdx = 1;  // Default 2 minutes

uint32_t reachedMinBpmMs = 0;  // Timestamp when 60 BPM was reached (0 = not reached)

// ----------------- Screen Brightness -----------------
const uint8_t SCREEN_BRIGHTS[] = {20, 60, 120, 200};
uint8_t screenBrightIdx = 2;

// ----------------- Battery Cache -----------------
uint32_t lastBatReadMs = 0;
int cachedBatPct = 100;
bool cachedCharging = false;
bool lowBatWarningShown = false;

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

// ----------------- Settings Persistence Functions -----------------

void loadSettings() {
  prefs.begin("adhd-light", true);  // Read-only mode
  ledBrightIdx = prefs.getUChar("ledBright", 2);
  screenBrightIdx = prefs.getUChar("scrBright", 2);
  rampIdx = prefs.getUChar("rampIdx", 1);
  autoPauseIdx = prefs.getUChar("autoPause", 1);
  autoOffIdx = prefs.getUChar("autoOff", 1);
  prefs.end();

  // Validate loaded values
  if (ledBrightIdx >= sizeof(LED_BRIGHTS)) ledBrightIdx = 2;
  if (screenBrightIdx >= sizeof(SCREEN_BRIGHTS)) screenBrightIdx = 2;
  if (rampIdx >= sizeof(RAMP_INTERVALS) / sizeof(RAMP_INTERVALS[0])) rampIdx = 1;
  if (autoPauseIdx >= sizeof(AUTO_PAUSE_DELAYS) / sizeof(AUTO_PAUSE_DELAYS[0])) autoPauseIdx = 1;
  if (autoOffIdx >= sizeof(AUTO_OFF_DELAYS) / sizeof(AUTO_OFF_DELAYS[0])) autoOffIdx = 1;
}

void saveSetting(const char* key, uint8_t value) {
  prefs.begin("adhd-light", false);  // Read-write mode
  prefs.putUChar(key, value);
  prefs.end();
}

// ----------------- Battery Management -----------------

void updateBatteryCache() {
  uint32_t now = millis();
  if (now - lastBatReadMs >= BAT_READ_INTERVAL_MS || lastBatReadMs == 0) {
    lastBatReadMs = now;
    cachedBatPct = StickCP2.Power.getBatteryLevel();
    // isCharging() returns: 0=not charging, 1=charging, 2=charge complete
    cachedCharging = (StickCP2.Power.isCharging() == 1);
  }
}

// Apply power-saving measures based on battery level
void applyBatteryPowerPolicy() {
  if (cachedCharging) {
    lowBatWarningShown = false;
    return;  // No restrictions while charging
  }

  if (cachedBatPct < 5 && !lowBatWarningShown) {
    // Critical battery: force PAUSE and show warning
    lowBatWarningShown = true;
    if (currentMode != PAUSE) {
      currentMode = PAUSE;
      reachedMinBpmMs = millis();
      setRedLed(false);
    }
  } else if (cachedBatPct < 10) {
    // Very low: limit both LED and screen brightness
    if (ledBrightIdx > 1) {
      ledBrightIdx = 1;
      saveSetting("ledBright", ledBrightIdx);
    }
    if (screenBrightIdx > 0) {
      screenBrightIdx = 0;
      uiApplyScreenBrightness();
      saveSetting("scrBright", screenBrightIdx);
    }
  } else if (cachedBatPct < 20) {
    // Low: limit screen brightness only
    if (screenBrightIdx > 1) {
      screenBrightIdx = 1;
      uiApplyScreenBrightness();
      saveSetting("scrBright", screenBrightIdx);
    }
  }
}

// ----------------- UI Draw Functions -----------------

void uiApplyScreenBrightness() {
  StickCP2.Display.setBrightness(SCREEN_BRIGHTS[screenBrightIdx]);
}

void uiDrawFrame() {
  StickCP2.Display.fillScreen(BLACK);
  StickCP2.Display.setTextColor(WHITE);
  StickCP2.Display.setTextSize(2);
}

// Minimal mode: centered BPM or PAUSE
void uiDrawMinimal() {
  StickCP2.Display.fillScreen(BLACK);
  StickCP2.Display.setTextColor(WHITE);

  // Show low battery warning if critical
  if (lowBatWarningShown && cachedBatPct < 5) {
    StickCP2.Display.setTextSize(3);
    StickCP2.Display.setCursor(20, 20);
    StickCP2.Display.setTextColor(RED);
    StickCP2.Display.print("LOW BAT!");
    StickCP2.Display.setTextColor(WHITE);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setCursor(50, 70);
    StickCP2.Display.printf("%d%%", cachedBatPct);
    return;
  }

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
  // Left: battery with charging indicator
  StickCP2.Display.setCursor(2, 2);
  if (charging) {
    StickCP2.Display.setTextColor(GREEN);
    StickCP2.Display.printf("CHG:%d%%", batPct);
    StickCP2.Display.setTextColor(WHITE);
  } else if (batPct < 20) {
    StickCP2.Display.setTextColor(RED);
    StickCP2.Display.printf("BAT:%d%%", batPct);
    StickCP2.Display.setTextColor(WHITE);
  } else {
    StickCP2.Display.printf("BAT:%d%%", batPct);
  }
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
    bool needsRedraw = (ui.bpm != currentBPM || ui.mode != currentMode);
    // Also redraw if low battery warning state changed
    if (lowBatWarningShown && cachedBatPct < 5 && ui.batPct != cachedBatPct) {
      needsRedraw = true;
    }
    if (needsRedraw) {
      uiDrawMinimal();
      ui.bpm = currentBPM;
      ui.mode = currentMode;
      ui.batPct = cachedBatPct;
    }
    return;
  }

  // ===== Info mode (page 1): refresh only on button press =====
  if (!uiNeedsRefresh) return;
  uiNeedsRefresh = false;

  // Draw frame on first entry
  if (ui.mode == NUM_MODES) {
    uiDrawFrame();
  }

  // Top bar (use cached battery values)
  uiDrawTopBar(cachedBatPct, cachedCharging);
  ui.batPct = cachedBatPct;
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

  // In PAUSE mode: auto power off
  if (currentMode == PAUSE) {
    uint16_t autoOffDelay = AUTO_OFF_DELAYS[autoOffIdx];
    if (autoOffDelay > 0 && reachedMinBpmMs > 0) {
      uint32_t delayMs = (uint32_t)autoOffDelay * 60UL * 1000UL;
      if (now - reachedMinBpmMs >= delayMs) {
        StickCP2.Power.powerOff();
      }
    }
    return;
  }

  // Check if at 60 BPM for configured time, then auto pause
  uint16_t autoPauseDelay = AUTO_PAUSE_DELAYS[autoPauseIdx];
  if (autoPauseDelay > 0 && currentBPM <= 60 && reachedMinBpmMs > 0) {
    uint32_t delayMs = (uint32_t)autoPauseDelay * 60UL * 1000UL;
    if (now - reachedMinBpmMs >= delayMs) {
      currentMode = PAUSE;
      reachedMinBpmMs = now;  // Reset timer for auto power off
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
      // Just reached 60 BPM, start auto-pause timer
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
  if (StickCP2.BtnA.wasHold() && millis() - lastHoldMsA > LONG_PRESS_INTERVAL_MS) {
    lastHoldMsA = millis();
    btnAHoldTriggered = true;

    if (uiPage == 1) {
      // Info mode: long press to modify ramp interval
      rampIdx = (rampIdx + 1) % (sizeof(RAMP_INTERVALS) / sizeof(RAMP_INTERVALS[0]));
      lastBPMUpdateMs = millis();
      saveSetting("rampIdx", rampIdx);
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
        lowBatWarningShown = false;  // Allow resuming from low battery warning
        setRedLed(false);
      } else {
        // Info mode: short press to modify LED brightness
        ledBrightIdx = (ledBrightIdx + 1) % (sizeof(LED_BRIGHTS) / sizeof(LED_BRIGHTS[0]));
        saveSetting("ledBright", ledBrightIdx);
        uiNeedsRefresh = true;
      }
    }
    btnAHoldTriggered = false;
  }

  // ========== Button B ==========
  // Detect long press (adjust screen brightness)
  if (StickCP2.BtnB.wasHold() && millis() - lastHoldMsB > LONG_PRESS_INTERVAL_MS) {
    lastHoldMsB = millis();
    btnBHoldTriggered = true;

    // Long press B to adjust screen brightness (works in any mode)
    screenBrightIdx = (screenBrightIdx + 1) % (sizeof(SCREEN_BRIGHTS) / sizeof(SCREEN_BRIGHTS[0]));
    uiApplyScreenBrightness();
    saveSetting("scrBright", screenBrightIdx);
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

  // GPIO4 must stay HIGH to keep ESP32 powered (hardware requirement)
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  // Load saved settings from NVS
  loadSettings();

  StickCP2.Display.setRotation(3);
  StickCP2.Display.setTextSize(2);
  uiApplyScreenBrightness();
  uiDrawFrame();

  sessionStartMs = millis();
  lastBPMUpdateMs = sessionStartMs;
  beatSyncMs = sessionStartMs;

  // Initial battery read
  updateBatteryCache();

  // Startup self-test: red LED on for 2 seconds
  setRedLed(true);
  delay(STARTUP_LED_DURATION_MS);
  setRedLed(false);

  ui = UiCache{};
  ui.page = 255;
}

void loop() {
  updateBatteryCache();
  applyBatteryPowerPolicy();
  handleButtons();
  tickRamp();
  tickHeartbeat();
  tickUI();
  delay(LOOP_DELAY_MS);
}
