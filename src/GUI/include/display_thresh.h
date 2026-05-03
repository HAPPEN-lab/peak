// Layout and element definitions for the Thresholds screen.

#ifndef DISPLAY_THRESH_H
#define DISPLAY_THRESH_H

#include "display_home.h"
#include "display_bal_prio.h"

//  - Register page title/menu element references for shared screen chrome updates.
// Inputs:
//  - screen: logical screen identifier.
//  - titleRef: title text element for that screen.
//  - menuRef: footer menu text element for that screen.
// Outputs:
//  - none; stores references in `main.cpp` managed state.
void RegisterPageChrome(ScreenId screen, gslc_tsElemRef* titleRef, gslc_tsElemRef* menuRef);

//  - GUIslice page ID assigned to the Thresholds page.
// Used by:
//  - `InitThresholdPage()` and `ScreenToPage()` in `main.cpp`.
constexpr int16_t kThresholdPageId = 2;

//  - Stable GUIslice element IDs for Thresholds widgets.
// Used by:
//  - `InitThresholdPage()` during creation and reference caching.
enum class ThresholdElemId : uint16_t {
  Background = 1999,
  Title = 2000,
  DeviceALabel,
  DeviceAFill,
  DeviceAFrame,
  DeviceAPercent,
  DeviceBLabel,
  DeviceBFill,
  DeviceBFrame,
  DeviceBPercent,
  DeviceCLabel,
  DeviceCFill,
  DeviceCFrame,
  DeviceCPercent,
  DeviceDLabel,
  DeviceDFill,
  DeviceDFrame,
  DeviceDPercent,
  Menu
};

//  - Storage backing for Thresholds page elements, frame geometry, and fill refs.
// Used by:
//  - `InitThresholdPage()` for construction.
//  - `RenderThresholdScreen()` in `main.cpp` for dynamic bar updates.
constexpr size_t kThresholdElemCount = 19;
gslc_tsElem g_thresholdElems[kThresholdElemCount];
gslc_tsElemRef g_thresholdElemRefs[kThresholdElemCount];
gslc_tsRect g_thresholdBarFrames[kDeviceCount];
gslc_tsElemRef* g_thresholdBarFillRefs[kDeviceCount] = {};
gslc_tsElemRef* g_thresholdBarFrameRefs[kDeviceCount] = {};
gslc_tsRect g_thresholdFocusRects[kDeviceCount] = {};
char g_thresholdPercentBuffers[kDeviceCount][8];
gslc_tsElemRef* g_thresholdPercentRefs[kDeviceCount] = {};

/*
 * Objective:
 *  - Build the Thresholds page with per-device bars and register chrome references.
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; creates widgets on threshold page and caches bar fill refs for runtime updates.
 */
void InitThresholdPage();

// ------------------------------------------------
// Create Thresholds page elements
// ------------------------------------------------
inline void InitThresholdPage() {
  gslc_tsElemRef* elemRef = nullptr;
  gslc_tsElemRef* pageTitleRef = nullptr;
  gslc_tsElemRef* menuRef = nullptr;

  gslc_PageAdd(&m_gui, kThresholdPageId, g_thresholdElems, kThresholdElemCount,
               g_thresholdElemRefs, kThresholdElemCount);

  // Full-screen background to ensure the page clears any previous content.
  elemRef = gslc_ElemCreateBox(&m_gui, static_cast<int>(ThresholdElemId::Background), kThresholdPageId,
                               (gslc_tsRect){0, 0, 800, 480});
  gslc_ElemSetCol(&m_gui, elemRef, kColBackground, kColBackground, kColBackground);
  gslc_ElemSetFrameEn(&m_gui, elemRef, false);

  elemRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(ThresholdElemId::Title), kThresholdPageId,
                               (gslc_tsRect){10, 10, 560, 40},
                               (char*)"Per-Device Availability", 0, E_DOSIS_BOLD24);
  gslc_ElemSetTxtCol(&m_gui, elemRef, kColAccent);
  gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
  pageTitleRef = elemRef;

  const int16_t barWidth = 140;
  const int16_t barHeight = 240;
  const int16_t barTop = 120;
  const int16_t labelY = 75;
  const int16_t percentY = 372;
  const int16_t barPositions[kDeviceCount] = {30, 230, 430, 630};

  for (size_t i = 0; i < kDeviceCount; ++i) {
    const int labelId = static_cast<int>(ThresholdElemId::DeviceALabel) + (static_cast<int>(i) * 4);
    const int fillId = labelId + 1;
    const int frameId = labelId + 2;
    const int percentId = labelId + 3;
    const int16_t x = barPositions[i];

    elemRef = gslc_ElemCreateTxt(&m_gui, labelId, kThresholdPageId,
                                 (gslc_tsRect){x, labelY, barWidth, 30},
                                 (char*)g_devices[i].label, 0, E_DOSIS_BOOK18);
    gslc_ElemSetTxtCol(&m_gui, elemRef, kColPrimaryLabel);
    gslc_ElemSetCol(&m_gui, elemRef, kColBorder, kColBackground, kColGlowOff);
    gslc_ElemSetTxtAlign(&m_gui, elemRef, GSLC_ALIGN_TOP_MID);

    g_thresholdBarFrames[i] = (gslc_tsRect){x, barTop, barWidth, barHeight};

    elemRef = gslc_ElemCreateBox(&m_gui, frameId, kThresholdPageId, g_thresholdBarFrames[i]);
    gslc_ElemSetCol(&m_gui, elemRef, kColBarTrack, kColBarTrack, kColGlowOff);
    gslc_ElemSetFrameEn(&m_gui, elemRef, true);
    g_thresholdBarFrameRefs[i] = elemRef;
    g_thresholdFocusRects[i] = g_thresholdBarFrames[i];

    // Create the fill box after the frame so it renders on top of the white frame background.
    gslc_tsRect fillRect = {static_cast<int16_t>(x + 6),
                            static_cast<int16_t>(barTop + barHeight - 6),
                            static_cast<int16_t>(barWidth - 12),
                            1};
    g_thresholdBarFillRefs[i] = gslc_ElemCreateBox(&m_gui, fillId, kThresholdPageId, fillRect);
    gslc_ElemSetCol(&m_gui, g_thresholdBarFillRefs[i], kColBarFill, kColBarFill, kColBarFill);
    gslc_ElemSetFrameEn(&m_gui, g_thresholdBarFillRefs[i], false);

    g_thresholdPercentBuffers[i][0] = '\0';
    g_thresholdPercentRefs[i] = gslc_ElemCreateTxt(&m_gui, percentId, kThresholdPageId,
                                                   (gslc_tsRect){x, percentY, barWidth, 26},
                                                   g_thresholdPercentBuffers[i], sizeof(g_thresholdPercentBuffers[i]), E_DOSIS_BOOK18);
    gslc_ElemSetTxtCol(&m_gui, g_thresholdPercentRefs[i], kColPrimaryLabel);
    gslc_ElemSetCol(&m_gui, g_thresholdPercentRefs[i], kColBorder, kColBackground, kColGlowOff);
    gslc_ElemSetTxtAlign(&m_gui, g_thresholdPercentRefs[i], GSLC_ALIGN_MID_MID);
  }

  menuRef = gslc_ElemCreateTxt(&m_gui, static_cast<int>(ThresholdElemId::Menu), kThresholdPageId,
                               (gslc_tsRect){0, 446, 800, 27},
                               (char*)"Home   Adjust Balance   [Availability]   Settings", 0, E_DOSIS_BOOK14);
  gslc_ElemSetTxtAlign(&m_gui, menuRef, GSLC_ALIGN_MID_MID);
  gslc_ElemSetTxtCol(&m_gui, menuRef, kColAccent);
  gslc_ElemSetCol(&m_gui, menuRef, kColBorder, kColBackground, kColGlowOff);

  RegisterPageChrome(ScreenId::Thresholds, pageTitleRef, menuRef);

  gslc_SetPageCur(&m_gui, E_PG_MAIN);
}

#endif // DISPLAY_THRESH_H