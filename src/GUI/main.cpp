#include <Arduino.h>
#include <SPI.h>
#include <cstdio>

#include "display_home.h"
#include "display_bal_prio.h"
#include "display_thresh.h"
#include "display_settings.h"
#include "physical_controls.h"


// GUIslice Builder element refs defined as extern in the generated header
gslc_tsElemRef* m_pElemTextbox1_6 = nullptr;
gslc_tsElemRef* m_pElemTextbox2 = nullptr;
gslc_tsElemRef* m_pElemTextbox3 = nullptr;
gslc_tsElemRef* m_pTextSlider1 = nullptr;

//  - Shared in-memory model of device labels, priority ordering, and threshold usage.
// Used by:
//  - `RenderBalanceScreen()` to populate priority labels.
//  - `RenderThresholdScreen()` to compute bar fill percentages.
//  - `ReorderDevice()` to update interactive ordering.
DeviceAllocation g_devices[kDeviceCount] = {
  {"Device A", 1, 75},
  {"Device B", 2, 30},
  {"Device C", 3, 90},
  {"Device D", 4, 55}
};

//  - Per-slot device assignment: g_slotDevice[rank-1] holds the device index for that rank.
// Used by:
//  - `AssignDeviceToSlot()`, `RefreshPriorityFields()`, and `SyncSlotsFromDevices()`.
int g_slotDevice[kDeviceCount] = {0, 1, 2, 3};

//  - Global banner registry keyed by `ScreenId` to keep title/menu refs accessible across page builders.
// Used by:
//  - `RegisterPageChrome()` to cache refs.
//  - `SetScreenTitle()` and `RenderScreen()` to update active page chrome.
constexpr size_t kScreenCount = static_cast<size_t>(ScreenId::Count);
const char* const kScreenNames[kScreenCount] = {
  "HOME",
  "ADJUST BALANCE",
  "Per-Device Availability",
  "SETTINGS"
};
gslc_tsElemRef* g_pageTitleRefs[kScreenCount] = {};
gslc_tsElemRef* g_menuLabels[kScreenCount] = {};
ScreenId g_activeScreen = ScreenId::Home;

// Forward declaration for shared menu title updates.
void UpdateMenuBanner(ScreenId active);

/*
 *  - Cache title and menu element references for a screen so shared chrome updates can target them.
 * Inputs:
 *  - screen: logical page identifier used as cache index.
 *  - titleRef: title text element for that page (nullable).
 *  - menuRef: footer menu text element for that page (nullable).
 * Outputs:
 *  - none; updates global reference arrays when pointers are non-null.
 */
void RegisterPageChrome(ScreenId screen,
                        gslc_tsElemRef* titleRef,
                        gslc_tsElemRef* menuRef) {
  const size_t idx = static_cast<size_t>(screen);
  if (idx >= static_cast<size_t>(ScreenId::Count)) {
    return;
  }

  if (titleRef) {
    g_pageTitleRefs[idx] = titleRef;
  }

  if (menuRef) {
    g_menuLabels[idx] = menuRef;
  }
}

/*
 *  - Provide compatibility hook for banner updates in layouts that still call this helper.
 * Inputs:
 *  - active: currently active screen identifier (unused).
 * Outputs:
 *  - none.
 */
void UpdateMenuBanner(ScreenId) {}

/*
 * Sends GUIslice debug output over Serial.
 * Inputs:
 *  - ch: character to write to the serial port
 * Outputs:
 *  - returns 0 after the character is sent
 */
static int16_t DebugOut(char ch) { Serial.write(ch); return 0; }

namespace {

constexpr uint32_t kSerialBaud = 115200;

// Demo values that drive each screen segment
int currentBalanceCents = 120050; // 1200.50 credits
int remainingTimeMinutes = (8 * 60) + 24;
int powerUsageWatts = 425;
int warningMinutes = 90;
int cutoffWatts = 1500;
int pendingTopUpCents = 0;

using ScreenRenderFn = void (*)();

// kScreenCount/kScreenNames now defined at global scope

/*
 * Replaces the contents of a GUIslice textbox with a new string.
 * Inputs:
 *  - textbox: textbox element to update
 *  - value: null-terminated string to display
 * Outputs:
 *  - none
 */
void SetTextboxValue(gslc_tsElemRef* textbox, const char* value) {
  if (!textbox || !value) { return; }
  gslc_ElemXTextboxReset(&m_gui, textbox);
  gslc_ElemXTextboxAdd(&m_gui, textbox, const_cast<char*>(value));
  gslc_ElemSetRedraw(&m_gui, textbox, GSLC_REDRAW_FULL);
}

/*
 * Formats an integer number of cents into a dollar string.
 * Inputs:
 *  - cents: currency value expressed in cents
 *  - buffer: destination character buffer for the formatted value
 *  - length: size of the destination buffer
 * Outputs:
 *  - buffer is populated with a "d.cc" formatted string
 */
void FormatCurrency(int cents, char* buffer, size_t length) {
  const int dollars = cents / 100;
  const int fractional = cents % 100;
  snprintf(buffer, length, "%d.%02d", dollars, fractional);
}

/*
 * Formats minutes into an hours and minutes string.
 * Inputs:
 *  - minutes: total minutes to convert
 *  - buffer: destination character buffer for the formatted value
 *  - length: size of the destination buffer
 * Outputs:
 *  - buffer is populated with a "HHh MMm" formatted string
 */
void FormatTime(int minutes, char* buffer, size_t length) {
  const int hours = minutes / 60;
  const int mins = minutes % 60;
  snprintf(buffer, length, "%02dh %02dm", hours, mins);
}

/*
 * Sets the title text for a given screen if its reference is cached.
 * Inputs:
 *  - screen: logical screen identifier
 *  - title: string to place in the title element
 * Outputs:
 *  - none
 */
void SetScreenTitle(ScreenId screen, const char* title) {
  const size_t idx = static_cast<size_t>(screen);
  if (idx >= kScreenCount) {
    return;
  }

  gslc_tsElemRef* titleRef = g_pageTitleRefs[idx];
  if (titleRef && title) {
    gslc_ElemSetTxtStr(&m_gui, titleRef, title);
  }
}

//  - Enumerates interactive focus targets on the Balance/Priority screen.
// Used by:
//  - `HandleBalanceInput()`, `AdvanceBalFocus()`, `MoveBalFocus()`, and highlight helpers.
enum class BalPrioFocus : uint8_t {
  Dollars = 0,
  Cents,
  Slot0,
  Slot1,
  Slot2,
  Slot3
};

//  - Enumerates focusable controls on the Settings screen.
// Used by:
//  - `HandleSettingsInput()`, `AdvanceSettingsFocus()`, `MoveSettingsFocus()`, and highlight helpers.
enum class SettingsFocus : uint8_t {
  Hours = 0,
  Minutes,
  Year,
  Month,
  Day,
  Line1,
  Line2,
  Line3,
  Pairing
};

enum class ThresholdFocus : uint8_t {
  Device0 = 0,
  Device1,
  Device2,
  Device3
};

constexpr int kTopUpDollarStepCents = 100;  // $1 per encoder detent on dollars field
constexpr int kTopUpCentsStep = 5;           // 5 cents per encoder detent on cents field
constexpr int kTopUpMinCents = -50000;
constexpr int kTopUpMaxCents = 50000;    // $500 cap for demo safety
constexpr int kTopUpRatioDollars = 1;
constexpr int kTopUpRatioMinutes = 30;

/*
 *  - Convert top-up currency (cents) into added service minutes using configured conversion ratio.
 * Inputs:
 *  - cents: top-up amount in cents.
 * Outputs:
 *  - returns rounded minute value; returns 0 for non-positive input.
 */
int TopUpCentsToMinutes(int cents) {
  if (cents == 0) {
    return 0;
  }

  const int absCents = cents < 0 ? -cents : cents;
  const int numerator = absCents * kTopUpRatioMinutes;
  const int denominator = 100 * kTopUpRatioDollars;
  int minutes = (numerator + (denominator / 2)) / denominator;
  return cents < 0 ? -minutes : minutes;
}

BalPrioFocus g_balFocus = BalPrioFocus::Dollars;
SettingsFocus g_settingsFocus = SettingsFocus::Hours;
ThresholdFocus g_thresholdFocus = ThresholdFocus::Device0;

int settingsHour = 12;
int settingsMinute = 0;
int settingsYear = 2026;
int settingsMonth = 2;
int settingsDay = 23;

void RenderHomeScreen();
void RenderBalanceScreen();
void RenderThresholdScreen();
void RenderSettingsScreen();
int16_t ScreenToPage(ScreenId screen);

//  - Screen-to-render-function dispatch table.
// Used by:
//  - `RenderScreen()` to execute the matching page refresh routine.
const ScreenRenderFn kScreenRenderers[kScreenCount] = {
  RenderHomeScreen,
  RenderBalanceScreen,
  RenderThresholdScreen,
  RenderSettingsScreen
};

/*
 *  - Apply visual focus styling (frame + text color) to one element.
 * Inputs:
 *  - ref: target UI element reference.
 *  - active: true to apply focused style, false to clear it.
 * Outputs:
 *  - none; updates the target element appearance when reference is valid.
 */
void SetFocusHighlight(gslc_tsElemRef* ref, const gslc_tsRect* baseRect, bool active) {
  if (!ref) { return; }
  (void)baseRect;

  gslc_ElemSetFrameEn(&m_gui, ref, active);
  // Active:   white text on solid-blue fill — two simultaneous cues (hue + luminance).
  // Inactive: dimmer secondary-gray text on dark-gray fill — clearly "dormant".
  gslc_ElemSetTxtCol(&m_gui, ref, active ? kColPrimaryLabel : kColSecondaryLabel);
  if (active) {
    gslc_ElemSetCol(&m_gui, ref, kColFocusAccent, kColFocusFill, kColFocusAccent);
  } else {
    gslc_ElemSetCol(&m_gui, ref, kColBorder, kColSurface, kColGlowOff);
  }
}

void SetThresholdBarHighlight(size_t index, bool active) {
  if (index >= kDeviceCount || !g_thresholdBarFrameRefs[index]) {
    return;
  }

  gslc_ElemSetRect(&m_gui, g_thresholdBarFrameRefs[index], g_thresholdFocusRects[index]);
  gslc_ElemSetFrameEn(&m_gui, g_thresholdBarFrameRefs[index], true);
  if (active) {
    gslc_ElemSetCol(&m_gui, g_thresholdBarFrameRefs[index], kColBarFocusBorder, kColBarTrack, kColBarFocusBorder);
  } else {
    gslc_ElemSetCol(&m_gui, g_thresholdBarFrameRefs[index], kColBorder, kColBarTrack, kColGlowOff);
  }
}

/*
 *  - Move Balance screen highlight from the previous focus target to a new one.
 * Inputs:
 *  - newFocus: next balance control focus state.
 * Outputs:
 *  - none; updates element frame/text colors for old and new focus elements.
 */
void UpdateBalanceFocusHighlight(BalPrioFocus newFocus) {
  static BalPrioFocus lastFocus = BalPrioFocus::Dollars;

  auto elemForFocus = [](BalPrioFocus focus) -> gslc_tsElemRef* {
    switch (focus) {
      case BalPrioFocus::Dollars: return g_balDollarsInputRef;
      case BalPrioFocus::Cents: return g_balCentsInputRef;
      case BalPrioFocus::Slot0: return g_balPriorityRefs[0];
      case BalPrioFocus::Slot1: return g_balPriorityRefs[1];
      case BalPrioFocus::Slot2: return g_balPriorityRefs[2];
      case BalPrioFocus::Slot3: return g_balPriorityRefs[3];
      default: return nullptr;
    }
  };

  auto rectForFocus = [](BalPrioFocus focus) -> const gslc_tsRect* {
    switch (focus) {
      case BalPrioFocus::Dollars: return &g_balFocusRects[0];
      case BalPrioFocus::Cents: return &g_balFocusRects[1];
      case BalPrioFocus::Slot0: return &g_balFocusRects[2];
      case BalPrioFocus::Slot1: return &g_balFocusRects[3];
      case BalPrioFocus::Slot2: return &g_balFocusRects[4];
      case BalPrioFocus::Slot3: return &g_balFocusRects[5];
      default: return nullptr;
    }
  };

  SetFocusHighlight(elemForFocus(lastFocus), rectForFocus(lastFocus), false);
  SetFocusHighlight(elemForFocus(newFocus), rectForFocus(newFocus), true);
  lastFocus = newFocus;
}

/*
 *  - Move Settings screen highlight from the previous focus target to a new one.
 * Inputs:
 *  - newFocus: next settings control focus state.
 * Outputs:
 *  - none; updates element frame/text colors for old and new focus elements.
 */
void UpdateSettingsFocusHighlight(SettingsFocus newFocus) {
  static SettingsFocus lastFocus = SettingsFocus::Hours;

  auto elemForFocus = [](SettingsFocus focus) -> gslc_tsElemRef* {
    switch (focus) {
      case SettingsFocus::Hours: return g_settingsHoursRef;
      case SettingsFocus::Minutes: return g_settingsMinutesRef;
      case SettingsFocus::Year: return g_settingsYearRef;
      case SettingsFocus::Month: return g_settingsMonthRef;
      case SettingsFocus::Day: return g_settingsDayRef;
      case SettingsFocus::Line1: return g_settingsLineRefs[0];
      case SettingsFocus::Line2: return g_settingsLineRefs[1];
      case SettingsFocus::Line3: return g_settingsLineRefs[2];
      case SettingsFocus::Pairing: return g_settingsLineRefs[2];
      default: return nullptr;
    }
  };

  auto rectForFocus = [](SettingsFocus focus) -> const gslc_tsRect* {
    switch (focus) {
      case SettingsFocus::Hours: return &g_settingsFocusRects[0];
      case SettingsFocus::Minutes: return &g_settingsFocusRects[1];
      case SettingsFocus::Year: return &g_settingsFocusRects[2];
      case SettingsFocus::Month: return &g_settingsFocusRects[3];
      case SettingsFocus::Day: return &g_settingsFocusRects[4];
      default: return nullptr;
    }
  };

  SetFocusHighlight(elemForFocus(lastFocus), rectForFocus(lastFocus), false);
  SetFocusHighlight(elemForFocus(newFocus), rectForFocus(newFocus), true);
  lastFocus = newFocus;
}

void UpdateThresholdFocusHighlight(ThresholdFocus newFocus) {
  static ThresholdFocus lastFocus = ThresholdFocus::Device0;
  SetThresholdBarHighlight(static_cast<size_t>(lastFocus), false);
  SetThresholdBarHighlight(static_cast<size_t>(newFocus), true);
  lastFocus = newFocus;
}

/*
 *  - Commit the pending top-up amount into total balance and remaining time.
 * Inputs:
 *  - none (uses `pendingTopUpCents` global state).
 * Outputs:
 *  - none; updates balance/time globals, clears pending value, and re-renders balance page.
 */
/*
 *  - Refresh only the balance amount input fields (dollars, cents) and time-added display.
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; updates only the three amount-related text elements on the balance page.
 */
void RefreshBalanceAmountFields() {
  char value[32];
  const char* signStr = pendingTopUpCents >= 0 ? "+" : "-";
  const int absCents = pendingTopUpCents < 0 ? -pendingTopUpCents : pendingTopUpCents;

  if (g_balDollarsInputRef) {
    snprintf(value, sizeof(value), "%s%d", signStr, absCents / 100);
    gslc_ElemSetTxtStr(&m_gui, g_balDollarsInputRef, value);
    gslc_ElemSetRedraw(&m_gui, g_balDollarsInputRef, GSLC_REDRAW_INC);
  }

  if (g_balCentsInputRef) {
    snprintf(value, sizeof(value), "%s%02d", signStr, absCents % 100);
    gslc_ElemSetTxtStr(&m_gui, g_balCentsInputRef, value);
    gslc_ElemSetRedraw(&m_gui, g_balCentsInputRef, GSLC_REDRAW_INC);
  }

  if (g_balTimeLabelRef) {
    gslc_ElemSetTxtStr(&m_gui, g_balTimeLabelRef,
                       pendingTopUpCents >= 0 ? "Time Added:" : "Time Removed:");
    gslc_ElemSetRedraw(&m_gui, g_balTimeLabelRef, GSLC_REDRAW_INC);
  }

  if (g_balTimeRef) {
    int minutes = TopUpCentsToMinutes(pendingTopUpCents);
    snprintf(value, sizeof(value), "%d", minutes < 0 ? -minutes : minutes);
    gslc_ElemSetTxtStr(&m_gui, g_balTimeRef, value);
    gslc_ElemSetRedraw(&m_gui, g_balTimeRef, GSLC_REDRAW_INC);
  }
}

/*
 *  - Rebuild g_slotDevice[] from g_devices[].priorityPercent.
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; populates g_slotDevice so g_slotDevice[rank-1] = device index.
 */
void SyncSlotsFromDevices() {
  for (size_t i = 0; i < kDeviceCount; ++i) {
    int rank = g_devices[i].priorityPercent;
    if (rank < 1 || rank > static_cast<int>(kDeviceCount)) {
      rank = static_cast<int>(i) + 1;
    }
    g_slotDevice[rank - 1] = static_cast<int>(i);
  }
}

/*
 *  - Write g_devices[].priorityPercent back from g_slotDevice[].
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; each device's priorityPercent is set to its 1-based rank.
 */
void SyncDevicesFromSlots() {
  for (size_t slot = 0; slot < kDeviceCount; ++slot) {
    int devIdx = g_slotDevice[slot];
    if (devIdx >= 0 && devIdx < static_cast<int>(kDeviceCount)) {
      g_devices[devIdx].priorityPercent = static_cast<int>(slot) + 1;
    }
  }
}

/*
 *  - Refresh the slot-based priority display: each row shows the device name assigned to that rank.
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; updates the four slot text elements on the balance page.
 */
void RefreshPriorityFields() {
  for (size_t i = 0; i < kDeviceCount; ++i) {
    if (!g_balPriorityRefs[i]) { continue; }
    int devIdx = g_slotDevice[i];
    if (devIdx < 0 || devIdx >= static_cast<int>(kDeviceCount)) {
      devIdx = static_cast<int>(i);
    }
    gslc_ElemSetTxtStr(&m_gui, g_balPriorityRefs[i], g_devices[devIdx].label);
    gslc_ElemSetRedraw(&m_gui, g_balPriorityRefs[i], GSLC_REDRAW_INC);
  }
}

void ApplyTopUp() {
  int addedMinutes = TopUpCentsToMinutes(pendingTopUpCents);
  int newBalance = currentBalanceCents + pendingTopUpCents;
  int newTime = remainingTimeMinutes + addedMinutes;
  if (newBalance < 0) { newBalance = 0; }
  if (newTime < 0) { newTime = 0; }
  currentBalanceCents = newBalance;
  remainingTimeMinutes = newTime;
  pendingTopUpCents = 0;
  // Full render needed because current balance display also changes
  RenderBalanceScreen();
}

/*
 *  - Cycle the device assigned to a priority slot and auto-swap to maintain unique assignments.
 * Inputs:
 *  - slotIndex: priority slot (0 = rank #1, 3 = rank #4).
 *  - delta: encoder movement; positive cycles forward through devices.
 * Outputs:
 *  - none; mutates g_slotDevice[], syncs g_devices[].priorityPercent, refreshes display.
 */
void AssignDeviceToSlot(size_t slotIndex, int delta) {
  if (slotIndex >= kDeviceCount || delta == 0) {
    return;
  }

  int oldDevIdx = g_slotDevice[slotIndex];
  int newDevIdx = oldDevIdx + delta;
  // Wrap around device indices
  while (newDevIdx < 0) { newDevIdx += static_cast<int>(kDeviceCount); }
  newDevIdx %= static_cast<int>(kDeviceCount);

  if (newDevIdx == oldDevIdx) {
    return;
  }

  // Find which slot currently holds the new device and swap
  for (size_t s = 0; s < kDeviceCount; ++s) {
    if (s == slotIndex) { continue; }
    if (g_slotDevice[s] == newDevIdx) {
      g_slotDevice[s] = oldDevIdx;
      break;
    }
  }
  g_slotDevice[slotIndex] = newDevIdx;

  SyncDevicesFromSlots();
  RefreshPriorityFields();
}

/*
 *  - Increment or decrement pending top-up dollars (whole-dollar portion).
 * Inputs:
 *  - deltaSteps: number of $1 increments to apply (can be negative).
 * Outputs:
 *  - none; updates `pendingTopUpCents` within bounds and re-renders balance screen on change.
 */
void AdjustPendingDollars(int deltaSteps) {
  const int deltaCents = deltaSteps * kTopUpDollarStepCents;
  int next = pendingTopUpCents + deltaCents;
  if (next < kTopUpMinCents) { next = kTopUpMinCents; }
  if (next > kTopUpMaxCents) { next = kTopUpMaxCents; }
  if (next != pendingTopUpCents) {
    pendingTopUpCents = next;
    RefreshBalanceAmountFields();
  }
}

/*
 *  - Increment or decrement pending top-up cents (fractional portion only).
 * Inputs:
 *  - deltaSteps: number of 5-cent increments to apply (can be negative).
 * Outputs:
 *  - none; updates `pendingTopUpCents` within bounds and re-renders balance screen on change.
 */
void AdjustPendingCents(int deltaSteps) {
  bool negative = pendingTopUpCents < 0;
  int absCents = negative ? -pendingTopUpCents : pendingTopUpCents;
  int centsOnly = absCents % 100;
  centsOnly += deltaSteps * kTopUpCentsStep;
  if (centsOnly < 0) { centsOnly = 0; }
  if (centsOnly > 95) { centsOnly = 95; }
  int nextAbs = (absCents / 100) * 100 + centsOnly;
  int next = negative ? -nextAbs : nextAbs;
  if (next < kTopUpMinCents) { next = kTopUpMinCents; }
  if (next > kTopUpMaxCents) { next = kTopUpMaxCents; }
  if (next != pendingTopUpCents) {
    pendingTopUpCents = next;
    RefreshBalanceAmountFields();
  }
}

/*
 *  - Cycle focus forward through all Balance screen interactive targets.
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; updates `g_balFocus` and highlight state.
 */
void AdvanceBalFocus() {
  const int focusCount = 6;
  int next = (static_cast<int>(g_balFocus) + 1) % focusCount;
  Serial.printf("[DBG] AdvanceBalFocus %d -> %d\n", static_cast<int>(g_balFocus), next);
  g_balFocus = static_cast<BalPrioFocus>(next);
  UpdateBalanceFocusHighlight(g_balFocus);
}

/*
 *  - Move Balance focus by a signed offset without wrapping.
 * Inputs:
 *  - delta: signed step amount to shift focus index.
 * Outputs:
 *  - none; clamps and updates `g_balFocus`/highlight when movement is valid.
 */
void MoveBalFocus(int delta) {
  const int minIdx = 0;
  const int maxIdx = 5;
  int next = static_cast<int>(g_balFocus) + delta;
  if (next < minIdx) next = minIdx;
  if (next > maxIdx) next = maxIdx;
  if (next != static_cast<int>(g_balFocus)) {
    g_balFocus = static_cast<BalPrioFocus>(next);
    UpdateBalanceFocusHighlight(g_balFocus);
  }
}

/*
 *  - Adjust the configured hour value with 24-hour wraparound.
 * Inputs:
 *  - delta: signed number of hours to apply.
 * Outputs:
 *  - none; mutates `settingsHour`, wraps 0–23.
 */
void AdjustSettingsHour(int delta) {
  settingsHour += delta;
  while (settingsHour < 0) { settingsHour += 24; }
  settingsHour %= 24;
}

/*
 *  - Adjust the configured minute value with 60-minute wraparound.
 * Inputs:
 *  - delta: signed number of minutes to apply.
 * Outputs:
 *  - none; mutates `settingsMinute`, wraps 0–59.
 */
void AdjustSettingsMinute(int delta) {
  settingsMinute += delta;
  while (settingsMinute < 0) { settingsMinute += 60; }
  settingsMinute %= 60;
}

/*
 *  - Adjust the configured year value within a bounded range.
 * Inputs:
 *  - delta: signed year increment/decrement.
 * Outputs:
 *  - none; mutates `settingsYear`, clamps to [2020, 2099].
 */
void AdjustSettingsYear(int delta) {
  settingsYear += delta;
  if (settingsYear < 2020) { settingsYear = 2020; }
  if (settingsYear > 2099) { settingsYear = 2099; }
}

/*
 *  - Adjust the configured month value with 1–12 wraparound.
 * Inputs:
 *  - delta: signed month increment/decrement.
 * Outputs:
 *  - none; mutates `settingsMonth`, wraps 1–12; clamps day to valid range for new month.
 */
void AdjustSettingsMonth(int delta) {
  settingsMonth += delta;
  while (settingsMonth < 1) { settingsMonth += 12; }
  while (settingsMonth > 12) { settingsMonth -= 12; }
  // Clamp day to valid range for the new month
  const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int maxDay = daysInMonth[settingsMonth];
  // Simple leap year check for February
  if (settingsMonth == 2 && ((settingsYear % 4 == 0 && settingsYear % 100 != 0) || settingsYear % 400 == 0)) {
    maxDay = 29;
  }
  if (settingsDay > maxDay) { settingsDay = maxDay; }
}

/*
 *  - Shift configured day value within a bounded day range for the current month.
 * Inputs:
 *  - deltaDays: signed day increment/decrement.
 * Outputs:
 *  - none; mutates `settingsDay` and clamps to [1, daysInMonth].
 */
void AdjustSettingsDate(int deltaDays) {
  const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int maxDay = daysInMonth[settingsMonth];
  if (settingsMonth == 2 && ((settingsYear % 4 == 0 && settingsYear % 100 != 0) || settingsYear % 400 == 0)) {
    maxDay = 29;
  }
  settingsDay += deltaDays;
  if (settingsDay < 1) { settingsDay = 1; }
  if (settingsDay > maxDay) { settingsDay = maxDay; }
}

/*
 *  - Emit pairing-request signal/log for long-press settings action.
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; writes a status line to serial output.
 */
void TriggerPairing() {
  Serial.println("Pairing requested (encoder long-press)");
}

/*
 *  - Cycle focus through actionable Settings controls.
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; updates `g_settingsFocus` and highlight state.
 */
void AdvanceSettingsFocus() {
  // Only cycle through actionable fields; skip empty info rows.
  constexpr SettingsFocus kCycleOrder[] = {
    SettingsFocus::Hours,
    SettingsFocus::Minutes,
    SettingsFocus::Year,
    SettingsFocus::Month,
    SettingsFocus::Day
  };

  int idx = 0;
  for (size_t i = 0; i < sizeof(kCycleOrder) / sizeof(kCycleOrder[0]); ++i) {
    if (g_settingsFocus == kCycleOrder[i]) {
      idx = static_cast<int>(i);
      break;
    }
  }

  idx = (idx + 1) % (sizeof(kCycleOrder) / sizeof(kCycleOrder[0]));
  g_settingsFocus = kCycleOrder[idx];
  UpdateSettingsFocusHighlight(g_settingsFocus);
}

/*
 *  - Move Settings focus by a signed offset within constrained actionable fields.
 * Inputs:
 *  - delta: signed step amount to shift focus index.
 * Outputs:
 *  - none; clamps and updates `g_settingsFocus`/highlight when movement is valid.
 */
void MoveSettingsFocus(int delta) {
  const int minIdx = 0;
  const int maxIdx = 4; // constrain held-rotation navigation to Hours/Minutes/Year/Month/Day
  int next = static_cast<int>(g_settingsFocus) + delta;
  if (next < minIdx) next = minIdx;
  if (next > maxIdx) next = maxIdx;
  if (next != static_cast<int>(g_settingsFocus)) {
    g_settingsFocus = static_cast<SettingsFocus>(next);
    UpdateSettingsFocusHighlight(g_settingsFocus);
  }
}

void AdvanceThresholdFocus() {
  const int focusCount = static_cast<int>(kDeviceCount);
  int next = (static_cast<int>(g_thresholdFocus) + 1) % focusCount;
  g_thresholdFocus = static_cast<ThresholdFocus>(next);
  UpdateThresholdFocusHighlight(g_thresholdFocus);
}

void MoveThresholdFocus(int delta) {
  const int minIdx = 0;
  const int maxIdx = static_cast<int>(kDeviceCount) - 1;
  int next = static_cast<int>(g_thresholdFocus) + delta;
  if (next < minIdx) next = minIdx;
  if (next > maxIdx) next = maxIdx;
  if (next != static_cast<int>(g_thresholdFocus)) {
    g_thresholdFocus = static_cast<ThresholdFocus>(next);
    UpdateThresholdFocusHighlight(g_thresholdFocus);
  }
}

void AdjustThresholdForFocus(int delta) {
  const size_t idx = static_cast<size_t>(g_thresholdFocus);
  if (idx >= kDeviceCount || delta == 0) {
    return;
  }

  int next = g_devices[idx].thresholdUsedPercent + delta;
  if (next < 0) { next = 0; }
  if (next > 100) { next = 100; }
  if (next != g_devices[idx].thresholdUsedPercent) {
    g_devices[idx].thresholdUsedPercent = next;
    RenderThresholdScreen();
  }
}

void HandleThresholdInput(int encoderDelta, bool shortPress) {
  if (encoderDelta != 0) {
    AdjustThresholdForFocus(encoderDelta);
  }

  if (shortPress) {
    AdvanceThresholdFocus();
  }
}

/*
 * Maps a logical screen to the GUIslice page identifier.
 * Inputs:
 *  - screen: logical screen identifier
 * Outputs:
 *  - returns the GUIslice page ID corresponding to the screen
 */
int16_t ScreenToPage(ScreenId screen) {
  switch (screen) {
    case ScreenId::Home: return E_PG_MAIN;
    case ScreenId::Balance: return E_PG_BALPRIO;
    case ScreenId::Thresholds: return kThresholdPageId;
    case ScreenId::Settings: return kSettingsPageId;
    default: return E_PG_MAIN;
  }
}

/*
 * Switches to a target screen and invokes its render routine.
 * Inputs:
 *  - screen: logical screen to display
 * Outputs:
 *  - active GUI page and banner are updated; screen render callback is executed
 */
void RenderScreen(ScreenId screen) {
  const size_t index = static_cast<size_t>(screen);
  if (index >= kScreenCount) {
    return;
  }

  gslc_SetPageCur(&m_gui, ScreenToPage(screen));
  // Force a full redraw so stale pixels from prior pages are cleared.
  gslc_PageRedrawSet(&m_gui, true);

  g_activeScreen = screen;
  SetScreenTitle(screen, kScreenNames[index]);
  ::UpdateMenuBanner(screen);

  if (screen == ScreenId::Balance) {
    g_balFocus = BalPrioFocus::Dollars;
  } else if (screen == ScreenId::Settings) {
    g_settingsFocus = SettingsFocus::Hours;
  } else if (screen == ScreenId::Thresholds) {
    g_thresholdFocus = ThresholdFocus::Device0;
  }

  if (kScreenRenderers[index]) {
    kScreenRenderers[index]();
  }
}

/*
 * Renders the home dashboard values for balance, time, and power usage.
 * Inputs:
 *  - none
 * Outputs:
 *  - home screen textboxes are refreshed with current demo values
 */
void RenderHomeScreen() {
  char value[32];

  FormatCurrency(currentBalanceCents, value, sizeof(value));
  SetTextboxValue(m_pElemTextbox1_6, value);

  FormatTime(remainingTimeMinutes, value, sizeof(value));
  SetTextboxValue(m_pElemTextbox2, value);

  snprintf(value, sizeof(value), "%d W", powerUsageWatts);
  SetTextboxValue(m_pElemTextbox3, value);
}

/*
 * Renders the balance screen with current credit, time, and device priorities.
 * Inputs:
 *  - none
 * Outputs:
 *  - balance page textboxes and inputs are updated with demo data
 */
void RenderBalanceScreen() {
  char value[32];
  const char* signStr = pendingTopUpCents >= 0 ? "+" : "-";
  const int absCents = pendingTopUpCents < 0 ? -pendingTopUpCents : pendingTopUpCents;

  if (g_balValueRef) {
    FormatCurrency(currentBalanceCents, value, sizeof(value));
    gslc_ElemSetTxtStr(&m_gui, g_balValueRef, value);
    gslc_ElemSetRedraw(&m_gui, g_balValueRef, GSLC_REDRAW_FULL);
  }

  if (g_balTimeLabelRef) {
    gslc_ElemSetTxtStr(&m_gui, g_balTimeLabelRef,
                       pendingTopUpCents >= 0 ? "Time Added:" : "Time Removed:");
    gslc_ElemSetRedraw(&m_gui, g_balTimeLabelRef, GSLC_REDRAW_FULL);
  }

  if (g_balTimeRef) {
    int minutes = TopUpCentsToMinutes(pendingTopUpCents);
    snprintf(value, sizeof(value), "%d", minutes < 0 ? -minutes : minutes);
    gslc_ElemSetTxtStr(&m_gui, g_balTimeRef, value);
    gslc_ElemSetRedraw(&m_gui, g_balTimeRef, GSLC_REDRAW_FULL);
  }

  if (g_balDollarsInputRef) {
    snprintf(value, sizeof(value), "%s%d", signStr, absCents / 100);
    gslc_ElemSetTxtStr(&m_gui, g_balDollarsInputRef, value);
    gslc_ElemSetRedraw(&m_gui, g_balDollarsInputRef, GSLC_REDRAW_FULL);
  }

  if (g_balCentsInputRef) {
    snprintf(value, sizeof(value), "%s%02d", signStr, absCents % 100);
    gslc_ElemSetTxtStr(&m_gui, g_balCentsInputRef, value);
    gslc_ElemSetRedraw(&m_gui, g_balCentsInputRef, GSLC_REDRAW_FULL);
  }

  for (size_t i = 0; i < kDeviceCount; ++i) {
    if (!g_balPriorityRefs[i]) {
      continue;
    }
    int devIdx = g_slotDevice[i];
    if (devIdx < 0 || devIdx >= static_cast<int>(kDeviceCount)) {
      devIdx = static_cast<int>(i);
    }
    gslc_ElemSetTxtStr(&m_gui, g_balPriorityRefs[i], g_devices[devIdx].label);
    gslc_ElemSetRedraw(&m_gui, g_balPriorityRefs[i], GSLC_REDRAW_FULL);
  }

  UpdateBalanceFocusHighlight(g_balFocus);
}

/*
 * Renders the thresholds screen showing cutoff and warning limits.
 * Inputs:
 *  - none
 * Outputs:
 *  - threshold labels and notes are updated with current limits
 */
void RenderThresholdScreen() {
  const int16_t inset = 6;

  for (size_t i = 0; i < kDeviceCount; ++i) {
    gslc_tsElemRef* fillRef = g_thresholdBarFillRefs[i];
    if (!fillRef) {
      continue;
    }

    const gslc_tsRect frame = g_thresholdBarFrames[i];
    const int16_t usableHeight = frame.h - (2 * inset);
    if (usableHeight <= 0) {
      continue;
    }

    int percentUsed = g_devices[i].thresholdUsedPercent;
    if (percentUsed < 0) { percentUsed = 0; }
    if (percentUsed > 100) { percentUsed = 100; }

    int16_t filled = static_cast<int16_t>((usableHeight * percentUsed) / 100);
    if (filled <= 0 && percentUsed > 0) {
      filled = 1;
    }

    gslc_tsRect fillRect = {
      static_cast<int16_t>(frame.x + inset),
      static_cast<int16_t>(frame.y + inset + (usableHeight - filled)),
      static_cast<uint16_t>(frame.w - (2 * inset)),
      static_cast<uint16_t>(filled)
    };

    // Ensure we keep the bar anchored to the bottom even when empty
    if (filled == 0) {
      fillRect.y = static_cast<int16_t>(frame.y + frame.h - inset);
      fillRect.h = 1;
    }

    gslc_ElemSetRect(&m_gui, fillRef, fillRect);

    if (g_thresholdPercentRefs[i]) {
      char value[8];
      snprintf(value, sizeof(value), "%d%%", percentUsed);
      gslc_ElemSetTxtStr(&m_gui, g_thresholdPercentRefs[i], value);
      gslc_ElemSetRedraw(&m_gui, g_thresholdPercentRefs[i], GSLC_REDRAW_FULL);
    }
  }

  UpdateThresholdFocusHighlight(g_thresholdFocus);
}

/*
 * Renders the settings screen with quick shortcut descriptions.
 * Inputs:
 *  - none
 * Outputs:
 *  - settings summary and lines are populated with static guidance text
 */
void RenderSettingsScreen() {
  if (!g_settingsSummaryRef) {
    return;
  }

  gslc_ElemSetTxtStr(&m_gui, g_settingsSummaryRef, "Quick shortcuts and IO");

  if (g_settingsHoursRef) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", settingsHour);
    gslc_ElemSetTxtStr(&m_gui, g_settingsHoursRef, buf);
  }

  if (g_settingsMinutesRef) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", settingsMinute);
    gslc_ElemSetTxtStr(&m_gui, g_settingsMinutesRef, buf);
  }

  if (g_settingsYearRef) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%04d", settingsYear);
    gslc_ElemSetTxtStr(&m_gui, g_settingsYearRef, buf);
  }

  if (g_settingsMonthRef) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", settingsMonth);
    gslc_ElemSetTxtStr(&m_gui, g_settingsMonthRef, buf);
  }

  if (g_settingsDayRef) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", settingsDay);
    gslc_ElemSetTxtStr(&m_gui, g_settingsDayRef, buf);
  }

  const char* lines[3] = {
    "Encoder nudges priority sliders",
    "Buttons 1-4 jump to Home, Balance, Availability, Settings",
    "Hold encoder for 3s to enter pairing"
  };

  for (size_t i = 0; i < 3; ++i) {
    if (g_settingsLineRefs[i]) {
      gslc_ElemSetTxtStr(&m_gui, g_settingsLineRefs[i], lines[i]);
    }
  }

  UpdateSettingsFocusHighlight(g_settingsFocus);
}

/*
 * Maps a physical button index to the corresponding screen selection.
 * Inputs:
 *  - buttonIndex: zero-based button identifier from the controls module
 * Outputs:
 *  - returns the target screen ID, or the current screen if unmapped
 */
ScreenId ButtonToScreen(int buttonIndex) {
  switch (buttonIndex) {
    case 0: return ScreenId::Home;
    case 1: return ScreenId::Balance;
    case 2: return ScreenId::Thresholds;
    case 3: return ScreenId::Settings;
    default: return g_activeScreen;
  }
}

/*
 *  - Route encoder rotation/press actions to the active Balance screen control.
 * Inputs:
 *  - encoderDelta: signed rotary movement since last loop.
 *  - shortPress: true when encoder short press event occurred.
 * Outputs:
 *  - none; updates pending top-up/device priority/focus and may trigger render updates.
 */
void HandleBalanceInput(int encoderDelta, bool shortPress) {
  if (encoderDelta != 0) {
    switch (g_balFocus) {
      case BalPrioFocus::Dollars:
        AdjustPendingDollars(encoderDelta);
        break;
      case BalPrioFocus::Cents:
        AdjustPendingCents(encoderDelta);
        break;
      case BalPrioFocus::Slot0:
      case BalPrioFocus::Slot1:
      case BalPrioFocus::Slot2:
      case BalPrioFocus::Slot3: {
        const size_t slotIdx = static_cast<size_t>(static_cast<int>(g_balFocus) - static_cast<int>(BalPrioFocus::Slot0));
        AssignDeviceToSlot(slotIdx, encoderDelta);
        break;
      }
      default:
        break;
    }
  }

  if (shortPress) {
    Serial.printf("[DBG] HandleBalanceInput shortPress, focus=%d\n", static_cast<int>(g_balFocus));
    if (g_balFocus == BalPrioFocus::Cents) {
      ApplyTopUp();
    }
    AdvanceBalFocus();
  }
}

/*
 *  - Route encoder actions to time/date adjustment or pairing trigger on Settings screen.
 * Inputs:
 *  - encoderDelta: signed rotary movement since last loop.
 *  - shortPress: true when encoder short press event occurred.
 *  - longPress: true when encoder long press event occurred.
 * Outputs:
 *  - none; updates settings fields/focus and may emit pairing request.
 */
void HandleSettingsInput(int encoderDelta, bool shortPress, bool longPress) {
  if (encoderDelta != 0) {
    switch (g_settingsFocus) {
      case SettingsFocus::Hours:
        AdjustSettingsHour(encoderDelta);
        RenderSettingsScreen();
        break;
      case SettingsFocus::Minutes:
        AdjustSettingsMinute(encoderDelta);
        RenderSettingsScreen();
        break;
      case SettingsFocus::Year:
        AdjustSettingsYear(encoderDelta);
        RenderSettingsScreen();
        break;
      case SettingsFocus::Month:
        AdjustSettingsMonth(encoderDelta);
        RenderSettingsScreen();
        break;
      case SettingsFocus::Day:
        AdjustSettingsDate(encoderDelta);
        RenderSettingsScreen();
        break;
      default:
        break;
    }
  }

  if (longPress && g_settingsFocus == SettingsFocus::Pairing) {
    TriggerPairing();
    return;
  }

  if (shortPress) {
    Serial.printf("[DBG] HandleSettingsInput shortPress, focus=%d\n", static_cast<int>(g_settingsFocus));
    if (g_settingsFocus == SettingsFocus::Pairing) {
      TriggerPairing();
    }
    AdvanceSettingsFocus();
  }
}

}  // namespace

/*
 * Initializes serial, GUIslice, page layouts, and physical controls, then shows Home.
 * Inputs:
 *  - none
 * Outputs:
 *  - hardware, UI pages, and controls are initialized and the Home screen is rendered
 */
void setup() {
  Serial.begin(kSerialBaud);
  delay(200);

  Serial.println("\nBooting Prepaid Energy Manager UI");

  gslc_InitDebug(&DebugOut);

  SPI.begin(ADAGFX_PIN_SCK, ADAGFX_PIN_MISO, ADAGFX_PIN_MOSI, ADAGFX_PIN_CS);

  InitDisplayHome();
  InitBalancePage();
  InitThresholdPage();
  InitSettingsPage();

  // Build slot-device mapping from initial priority data.
  SyncSlotsFromDevices();

  // Propagate the home banner text to all registered pages at startup.
  UpdateMenuBanner(ScreenId::Home);

  initializeControls();
  Serial.println("Physical controls ready");

  // Ensure the main page is fully drawn after controls init
  RenderScreen(ScreenId::Home);
}

/*
 * Main loop that updates the GUI, reads inputs, and navigates between screens.
 * Inputs:
 *  - none
 * Outputs:
 *  - GUI state is refreshed; navigation changes are applied based on user inputs
 */
void loop() {
  gslc_Update(&m_gui);

  const ButtonEvent buttonEvent = updateButtons();
  const int encoderDelta = readRotaryDelta();

  if (buttonEvent.navButton >= 0) {
    const ScreenId nextScreen = ButtonToScreen(buttonEvent.navButton);
    if (nextScreen != g_activeScreen || buttonEvent.navButton == 0) {
      RenderScreen(nextScreen);
    }
  }

  if (buttonEvent.encoderShortPress) {
    Serial.printf("[DBG] shortPress detected, screen=%d, held=%d, delta=%d\n",
                  static_cast<int>(g_activeScreen), buttonEvent.encoderHeld, encoderDelta);
  }

  switch (g_activeScreen) {
    case ScreenId::Balance:
      if (buttonEvent.encoderHeld && encoderDelta != 0) {
        MoveBalFocus(encoderDelta);
      } else {
        HandleBalanceInput(encoderDelta, buttonEvent.encoderShortPress);
      }
      break;
    case ScreenId::Thresholds:
      if (buttonEvent.encoderHeld && encoderDelta != 0) {
        MoveThresholdFocus(encoderDelta);
      } else {
        HandleThresholdInput(encoderDelta, buttonEvent.encoderShortPress);
      }
      break;
    case ScreenId::Settings:
      if (buttonEvent.encoderHeld && encoderDelta != 0) {
        MoveSettingsFocus(encoderDelta);
      } else {
        HandleSettingsInput(encoderDelta, buttonEvent.encoderShortPress, buttonEvent.encoderLongPress);
      }
      break;
    default:
      break;
  }

  delay(10);
}