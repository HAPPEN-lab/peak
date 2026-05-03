// Layout and element definitions for the Balance/Priorities screen.

#ifndef DISPLAY_BAL_PRIO_H
#define DISPLAY_BAL_PRIO_H

#include "display_home.h"

//  - Register page title/menu element references for shared screen chrome updates.
// Inputs:
//  - screen: logical screen identifier.
//  - titleRef: title text element for that screen.
//  - menuRef: footer menu text element for that screen.
// Outputs:
//  - none; stores references in `main.cpp` managed state.
void RegisterPageChrome(ScreenId screen, gslc_tsElemRef* titleRef, gslc_tsElemRef* menuRef);

//  - Fixed number of managed devices displayed on balance and threshold pages.
// Used by:
//  - Page element arrays in this header and threshold header.
constexpr size_t kDeviceCount = 4;

//  - Shared device model storing display label, priority rank, and threshold utilization.
// Used by:
//  - `RenderBalanceScreen()`, `RenderThresholdScreen()`, and device reordering logic in `main.cpp`.
struct DeviceAllocation {
  const char* label;
  int priorityPercent;
  int thresholdUsedPercent;
};

extern DeviceAllocation g_devices[kDeviceCount];

//  - Per-slot device assignment: g_slotDevice[rank-1] holds the device index assigned to that rank.
// Used by:
//  - `AssignDeviceToSlot()`, `RefreshPriorityFields()`, and sync helpers in `main.cpp`.
extern int g_slotDevice[kDeviceCount];

//  - Stable GUIslice element IDs for widgets on the Balance/Priorities page.
// Used by:
//  - `InitBalancePage()` during widget creation and later updates through cached refs.
enum class BalPrioElemId : uint16_t {
  Background = 90,
  Title = 100,
  Subtitle,
  BalanceLabel,
  TimeLabel,
  BalanceValue,
  TimeValue,
  DollarSign,
  DollarsInput,
  DotSeparator,
  CentsInput,
  CentsSign,
  SlotRank0,      // rank label "#1"
  SlotDevice0,    // device name text for slot 0
  SlotRank1,
  SlotDevice1,
  SlotRank2,
  SlotDevice2,
  SlotRank3,
  SlotDevice3,
  TimeUnits,
  Menu
};

//  - Storage backing for Balance page GUIslice elements, text buffers, and direct-access refs.
// Used by:
//  - `InitBalancePage()` for construction.
//  - `RenderBalanceScreen()` and focus-highlighting helpers in `main.cpp` for updates.
constexpr size_t kBalPrioElemCount = 27;
gslc_tsElem g_balPrioElems[kBalPrioElemCount];
gslc_tsElemRef g_balPrioElemRefs[kBalPrioElemCount];
char g_balValueBuffer[32] = "";
char g_balTimeBuffer[32] = "";
gslc_tsElemRef* g_balValueRef = nullptr;
gslc_tsElemRef* g_balTimeRef = nullptr;
gslc_tsElemRef* g_balDollarsInputRef = nullptr;
gslc_tsElemRef* g_balCentsInputRef = nullptr;
gslc_tsElemRef* g_balTimeLabelRef = nullptr;
char g_balPriorityBuffers[kDeviceCount][16];
gslc_tsElemRef* g_balPriorityRefs[kDeviceCount] = {};
gslc_tsRect g_balFocusRects[6] = {};

/*
 * Objective:
 *  - Build the Balance/Priorities page and register key element references for runtime updates.
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; creates widgets on `E_PG_BALPRIO`, caches refs, and registers page chrome.
 */
void InitBalancePage();

// ------------------------------------------------
// Create Balance/Priorities page elements
// ------------------------------------------------
inline void InitBalancePage() {
  gslc_tsElemRef* elemRef = nullptr;
  gslc_tsElemRef* pageTitleRef = nullptr;
  gslc_tsElemRef* menuRef = nullptr;

  gslc_PageAdd(&m_gui, E_PG_BALPRIO, g_balPrioElems, kBalPrioElemCount,
               g_balPrioElemRefs, kBalPrioElemCount);

  // Full-screen background to ensure the page clears any previous content.
  elemRef = gslc_ElemCreateBox(&m_gui, static_cast<int>(BalPrioElemId::Background), E_PG_BALPRIO,
                               (gslc_tsRect){0, 0, 800, 480});
  gslc_ElemSetCol(&m_gui, elemRef, kColBackground, kColBackground, kColBackground);
  gslc_ElemSetFrameEn(&m_gui, elemRef, false);

  // Top section titles
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::Title), E_PG_BALPRIO,
                               (gslc_tsRect){10, 10, 320, 40},
                               (char*)"Adjust Balance", 0, E_DOSIS_BOLD24);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColAccent);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  pageTitleRef = elemRef;

  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::Subtitle), E_PG_BALPRIO,
                               (gslc_tsRect){10, 200, 360, 40},
                               (char*)"Adjust Priorities", 0, E_DOSIS_BOLD24);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColAccent);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);

  // Labels for balance/time sections
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::BalanceLabel), E_PG_BALPRIO,
                               (gslc_tsRect){10, 100, 240, 32},
                               (char*)"Current Balance: $", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);

  static char s_timeLabelBuffer[16] = "Time Added:";
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::TimeLabel), E_PG_BALPRIO,
                               (gslc_tsRect){10, 160, 220, 32},
                               s_timeLabelBuffer, sizeof(s_timeLabelBuffer), E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  g_balTimeLabelRef = elemRef;

  // Value fields
  g_balValueBuffer[0] = '\0';
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::BalanceValue), E_PG_BALPRIO,
                               (gslc_tsRect){260, 100, 190, 32},
                               g_balValueBuffer, sizeof(g_balValueBuffer), E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  gslc_ElemSetTxtAlign(&m_gui, elemRef, GSLC_ALIGN_MID_LEFT);
  g_balValueRef = elemRef;

  g_balTimeBuffer[0] = '\0';
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::TimeValue), E_PG_BALPRIO,
                               (gslc_tsRect){260, 160, 190, 32},
                               g_balTimeBuffer, sizeof(g_balTimeBuffer), E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  gslc_ElemSetTxtAlign(&m_gui, elemRef, GSLC_ALIGN_MID_LEFT);
  g_balTimeRef = elemRef;

  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::TimeUnits), E_PG_BALPRIO,
                               (gslc_tsRect){455, 160, 100, 32},
                               (char*)"minutes", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColSecondaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  gslc_ElemSetTxtAlign(&m_gui, elemRef, GSLC_ALIGN_MID_LEFT);

  // "$" sign label
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::DollarSign), E_PG_BALPRIO,
                               (gslc_tsRect){460, 100, 20, 32},
                               (char*)"$", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColSecondaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  gslc_ElemSetTxtAlign(&m_gui, elemRef, GSLC_ALIGN_MID_LEFT);

  // Dollars input field
  static char s_dollarsBuffer[12] = "";
  const gslc_tsRect dollarsInputRect = {480, 100, 80, 32};
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::DollarsInput), E_PG_BALPRIO,
                               dollarsInputRect,
                               s_dollarsBuffer, sizeof(s_dollarsBuffer), E_DOSIS_BOOK18);
  gslc_ElemSetTxtMargin(&m_gui, elemRef, 5);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColSurface, kColGlowOff);
  gslc_ElemSetFrameEn(&m_gui, elemRef, true);
  g_balDollarsInputRef = elemRef;
  g_balFocusRects[0] = dollarsInputRect;

  // "." separator — shifted down 10px so it sits lower than the input boxes
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::DotSeparator), E_PG_BALPRIO,
                               (gslc_tsRect){562, 110, 12, 32},
                               (char*)".", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColSecondaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  gslc_ElemSetTxtAlign(&m_gui, elemRef, GSLC_ALIGN_MID_MID);

  // "c" cents-sign label before the cents input
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::CentsSign), E_PG_BALPRIO,
                               (gslc_tsRect){576, 100, 18, 32},
                               (char*)"c", 0, E_DOSIS_BOOK18);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColSecondaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  gslc_ElemSetTxtAlign(&m_gui, elemRef, GSLC_ALIGN_MID_MID);

  // Cents input field — shifted right to make room for the cents-sign label
  static char s_centsBuffer[8] = "";
  const gslc_tsRect centsInputRect = {596, 100, 60, 32};
  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::CentsInput), E_PG_BALPRIO,
                               centsInputRect,
                               s_centsBuffer, sizeof(s_centsBuffer), E_DOSIS_BOOK18);
  gslc_ElemSetTxtMargin(&m_gui, elemRef, 5);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColSurface, kColGlowOff);
  gslc_ElemSetFrameEn(&m_gui, elemRef, true);
  g_balCentsInputRef = elemRef;
  g_balFocusRects[1] = centsInputRect;

  //  - Vertical slot list: each row shows a rank label and an editable device name.
  // Layout: 4 rows stacked vertically, 48px apart, starting at y=250.
  constexpr int16_t kSlotStartY = 250;
  constexpr int16_t kSlotRowHeight = 44;
  constexpr int16_t kRankLabelX = 30;
  constexpr int16_t kRankLabelW = 55;
  constexpr int16_t kDeviceNameX = 95;
  constexpr int16_t kDeviceNameW = 220;
  constexpr int16_t kSlotH = 34;

  for (size_t i = 0; i < kDeviceCount; ++i) {
    const int rankId = static_cast<int>(BalPrioElemId::SlotRank0) + static_cast<int>(i) * 2;
    const int deviceId = rankId + 1;
    const int16_t rowY = kSlotStartY + static_cast<int16_t>(i) * kSlotRowHeight;

    // Rank label (static string literals — must not be local variables since GUIslice
    // stores the pointer directly when len=0).
    static const char* const kRankLabels[kDeviceCount] = {"#1", "#2", "#3", "#4"};
    elemRef = gslc_ElemCreateTxt(&m_gui, rankId, E_PG_BALPRIO,
                                 (gslc_tsRect){kRankLabelX, rowY, kRankLabelW, kSlotH},
                                 (char*)kRankLabels[i], 0, E_DOSIS_BOLD22);
    gslc_ElemSetTxtCol(&m_gui, elemRef, kColAccent);
    gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);

    // Device name (editable via encoder cycling)
    g_balPriorityBuffers[i][0] = '\0';
    const gslc_tsRect deviceRect = {kDeviceNameX, rowY, kDeviceNameW, kSlotH};
    elemRef = gslc_ElemCreateTxt(&m_gui, deviceId, E_PG_BALPRIO,
                                 deviceRect,
                                 g_balPriorityBuffers[i], sizeof(g_balPriorityBuffers[i]), E_DOSIS_BOOK20);
    gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
    gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColSurface, kColGlowOff);
    gslc_ElemSetFrameEn(&m_gui, elemRef, true);
    gslc_ElemSetTxtAlign(&m_gui, elemRef, GSLC_ALIGN_MID_MID);
    g_balPriorityRefs[i] = elemRef;
    g_balFocusRects[i + 2] = deviceRect;
  }


  menuRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(BalPrioElemId::Menu), E_PG_BALPRIO,
                               (gslc_tsRect){0, 446, 800, 27},
                               (char*)"Home   [Adjust Balance]   Availability   Settings", 0, E_DOSIS_BOOK14);
  gslc_ElemSetTxtAlign(&m_gui, menuRef, GSLC_ALIGN_MID_MID);
  gslc_ElemSetTxtCol(&m_gui, menuRef, kColAccent);
  gslc_ElemSetCol(&m_gui, menuRef, kColBorder, kColBackground, kColGlowOff);

  RegisterPageChrome(ScreenId::Balance, pageTitleRef, menuRef);

  // Default back to the main dashboard after constructing the new page.
  gslc_SetPageCur(&m_gui, E_PG_MAIN);
}

#endif // DISPLAY_BAL_PRIO_H