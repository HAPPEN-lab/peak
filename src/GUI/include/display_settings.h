// Layout and element definitions for the Settings screen.

#ifndef DISPLAY_SETTINGS_H
#define DISPLAY_SETTINGS_H

#include "display_home.h"

//  - Register page title/menu element references for shared screen chrome updates.
// Inputs:
//  - screen: logical screen identifier.
//  - titleRef: title text element for that screen.
//  - menuRef: footer menu text element for that screen.
// Outputs:
//  - none; stores references in `main.cpp` managed state.
void RegisterPageChrome(ScreenId screen, gslc_tsElemRef* titleRef, gslc_tsElemRef* menuRef);

//  - GUIslice page ID assigned to the Settings page.
// Used by:
//  - `InitSettingsPage()` and `ScreenToPage()` in `main.cpp`.
constexpr int16_t kSettingsPageId = 3;

//  - Stable GUIslice element IDs for Settings widgets.
// Used by:
//  - `InitSettingsPage()` and runtime updates in `RenderSettingsScreen()`.
enum class SettingsElemId : uint16_t {
  Background = 2999,
  Title = 3000,
  Summary,
  ClockHeader,
  TimeLabel,
  HoursValue,
  TimeSeparator,
  MinutesValue,
  DateLabel,
  YearLabel,
  YearValue,
  MonthLabel,
  MonthValue,
  DayLabel,
  DayValue,
  Line1,
  Line2,
  Line3,
  Menu
};

//  - Storage backing for Settings page elements and direct-access references.
// Used by:
//  - `InitSettingsPage()` for construction.
//  - `RenderSettingsScreen()` and focus-highlighting helpers in `main.cpp` for updates.
constexpr size_t kSettingsElemCount = 20;
gslc_tsElem g_settingsElems[kSettingsElemCount];
gslc_tsElemRef g_settingsElemRefs[kSettingsElemCount];
gslc_tsElemRef* g_settingsSummaryRef = nullptr;
gslc_tsElemRef* g_settingsLineRefs[3] = {};
gslc_tsElemRef* g_settingsHoursRef = nullptr;
gslc_tsElemRef* g_settingsMinutesRef = nullptr;
gslc_tsElemRef* g_settingsYearRef = nullptr;
gslc_tsElemRef* g_settingsMonthRef = nullptr;
gslc_tsElemRef* g_settingsDayRef = nullptr;
gslc_tsRect g_settingsFocusRects[5] = {};

/*
 * Objective:
 *  - Build the Settings page layout, cache editable field refs, and register page chrome.
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; creates widgets on settings page and stores refs used by runtime render/focus logic.
 */
inline void InitSettingsPage() {
  gslc_tsElemRef* elemRef = nullptr;
  gslc_tsElemRef* pageTitleRef = nullptr;
  gslc_tsElemRef* menuRef = nullptr;

  gslc_PageAdd(&m_gui, kSettingsPageId, g_settingsElems, kSettingsElemCount,
               g_settingsElemRefs, kSettingsElemCount);

  elemRef = gslc_ElemCreateBox(&m_gui, static_cast<int>(SettingsElemId::Background), kSettingsPageId,
                               (gslc_tsRect){0, 0, 800, 480});
  gslc_ElemSetCol(&m_gui, elemRef, kColBackground, kColBackground, kColBackground);
  gslc_ElemSetFrameEn(&m_gui, elemRef, false);

  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::Title), kSettingsPageId,
                               (gslc_tsRect){10, 10, 320, 40},
                               (char*)"SETTINGS", 0, E_DOSIS_BOLD24);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColAccent);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  pageTitleRef = elemRef;

  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::Summary), kSettingsPageId,
                               (gslc_tsRect){10, 60, 500, 30},
                               (char*)"Basic device behavior", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColSecondaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  g_settingsSummaryRef = elemRef;

  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::ClockHeader), kSettingsPageId,
                               (gslc_tsRect){10, 110, 300, 32},
                               (char*)"Device Clock", 0, E_DOSIS_BOLD22);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColAccent);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);

  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::TimeLabel), kSettingsPageId,
                               (gslc_tsRect){10, 150, 100, 32},
                               (char*)"Time:", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);

  // Hours input field
  static char s_hoursBuffer[4] = "";
  const gslc_tsRect hoursValueRect = {110, 150, 70, 32};
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::HoursValue), kSettingsPageId,
                               hoursValueRect,
                               s_hoursBuffer, sizeof(s_hoursBuffer), E_DOSIS_BOOK18);
  gslc_ElemSetTxtMargin(&m_gui, elemRef, 6);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColSurface, kColGlowOff);
  gslc_ElemSetFrameEn(&m_gui, elemRef, true);
  g_settingsHoursRef = elemRef;
  g_settingsFocusRects[0] = hoursValueRect;

  // ":" separator
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::TimeSeparator), kSettingsPageId,
                               (gslc_tsRect){182, 150, 16, 32},
                               (char*)":", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColSecondaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  gslc_ElemSetTxtAlign(&m_gui, elemRef, GSLC_ALIGN_MID_MID);

  // Minutes input field
  static char s_minutesBuffer[4] = "";
  const gslc_tsRect minutesValueRect = {200, 150, 70, 32};
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::MinutesValue), kSettingsPageId,
                               minutesValueRect,
                               s_minutesBuffer, sizeof(s_minutesBuffer), E_DOSIS_BOOK18);
  gslc_ElemSetTxtMargin(&m_gui, elemRef, 6);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColSurface, kColGlowOff);
  gslc_ElemSetFrameEn(&m_gui, elemRef, true);
  g_settingsMinutesRef = elemRef;
  g_settingsFocusRects[1] = minutesValueRect;

  // Date section header label
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::DateLabel), kSettingsPageId,
                               (gslc_tsRect){10, 195, 100, 32},
                               (char*)"Date:", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);

  // Year label + value
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::YearLabel), kSettingsPageId,
                               (gslc_tsRect){110, 195, 24, 32},
                               (char*)"Y:", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColSecondaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);

  static char s_yearBuffer[6] = "";
  const gslc_tsRect yearValueRect = {136, 195, 80, 32};
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::YearValue), kSettingsPageId,
                               yearValueRect,
                               s_yearBuffer, sizeof(s_yearBuffer), E_DOSIS_BOOK18);
  gslc_ElemSetTxtMargin(&m_gui, elemRef, 6);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColSurface, kColGlowOff);
  gslc_ElemSetFrameEn(&m_gui, elemRef, true);
  g_settingsYearRef = elemRef;
  g_settingsFocusRects[2] = yearValueRect;

  // Month label + value
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::MonthLabel), kSettingsPageId,
                               (gslc_tsRect){224, 195, 28, 32},
                               (char*)"M:", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColSecondaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);

  static char s_monthBuffer[4] = "";
  const gslc_tsRect monthValueRect = {254, 195, 50, 32};
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::MonthValue), kSettingsPageId,
                               monthValueRect,
                               s_monthBuffer, sizeof(s_monthBuffer), E_DOSIS_BOOK18);
  gslc_ElemSetTxtMargin(&m_gui, elemRef, 6);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColSurface, kColGlowOff);
  gslc_ElemSetFrameEn(&m_gui, elemRef, true);
  g_settingsMonthRef = elemRef;
  g_settingsFocusRects[3] = monthValueRect;

  // Day label + value
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::DayLabel), kSettingsPageId,
                               (gslc_tsRect){312, 195, 24, 32},
                               (char*)"D:", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColSecondaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);

  static char s_dayBuffer[4] = "";
  const gslc_tsRect dayValueRect = {338, 195, 50, 32};
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::DayValue), kSettingsPageId,
                               dayValueRect,
                               s_dayBuffer, sizeof(s_dayBuffer), E_DOSIS_BOOK18);
  gslc_ElemSetTxtMargin(&m_gui, elemRef, 6);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColSurface, kColGlowOff);
  gslc_ElemSetFrameEn(&m_gui, elemRef, true);
  g_settingsDayRef = elemRef;
  g_settingsFocusRects[4] = dayValueRect;

  const gslc_tsRect lineRects[3] = {
    {10, 270, 760, 30},
    {10, 320, 760, 30},
    {10, 370, 760, 30}
  };

  for (int i = 0; i < 3; ++i) {
    elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::Line1) + i, kSettingsPageId,
                                 lineRects[i], (char*)"", 0, E_DOSIS_BOOK18);
    gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
    gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
    g_settingsLineRefs[i] = elemRef;
  }

  menuRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(SettingsElemId::Menu), kSettingsPageId,
                               (gslc_tsRect){0, 446, 800, 27},
                               (char*)"Home   Adjust Balance   Availability   [Settings]", 0, E_DOSIS_BOOK14);
  gslc_ElemSetTxtAlign(&m_gui, menuRef, GSLC_ALIGN_MID_MID);
  gslc_ElemSetTxtCol(&m_gui, menuRef, kColAccent);
  gslc_ElemSetCol(&m_gui, menuRef, kColBorder, kColBackground, kColGlowOff);

  RegisterPageChrome(ScreenId::Settings, pageTitleRef, menuRef);

  gslc_SetPageCur(&m_gui, E_PG_MAIN);
}

#endif // DISPLAY_SETTINGS_H
