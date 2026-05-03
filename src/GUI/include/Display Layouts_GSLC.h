//<File !Start!>
// FILE: [Display Layouts_GSLC.h]
// Created by GUIslice Builder version: [0.17.b41]
//
// GUIslice Builder Generated GUI Framework File
//
// For the latest guides, updates and support view:
// https://github.com/ImpulseAdventure/GUIslice
//
//<File !End!>

#ifndef _GUISLICE_GEN_H
#define _GUISLICE_GEN_H

// ------------------------------------------------
// Headers to include
// ------------------------------------------------
#include "GUIslice.h"
#include "GUIslice_drv.h"

// Include any extended elements
//<Includes !Start!>
// Include extended elements
#include "elem/XTextbox.h"
//<Includes !End!>

// ------------------------------------------------
// Headers and Defines for fonts
// Note that font files are located within the Adafruit-GFX library folder:
// ------------------------------------------------
//<Fonts !Start!>
#if defined(DRV_DISP_TFT_ESPI)
  #error E_PROJECT_OPTIONS tab->Graphics Library should be TFT_eSPI
#endif
#include <Adafruit_GFX.h>
#include "dosis_bold22pt7b.h"
#include "dosis_bold24pt7b.h"
#include "dosis_book14pt7b.h"
#include "dosis_book18pt7b.h"
#include "dosis_book20pt7b.h"
//<Fonts !End!>

// ------------------------------------------------
// Defines for resources
// ------------------------------------------------
//<Resources !Start!>
//<Resources !End!>

// ------------------------------------------------
// Enumerations for pages, elements, fonts, images
// ------------------------------------------------
//<Enum !Start!>
//  - Page identifiers for generated layouts.
// Used by:
//  - `InitGUIslice_gen()` when adding and selecting pages.
enum {E_PG_MAIN,E_PG_BALPRIO};
//  - Element identifiers for widgets on `E_PG_MAIN`.
// Used by:
//  - `InitGUIslice_gen()` during widget creation and reference assignment.
enum {E_ELEM_TEXT1,E_ELEM_TEXT3,E_ELEM_TEXT5,E_ELEM_TEXT6,E_ELEM_TEXT7
      ,E_ELEM_TEXTBOX4,E_ELEM_TEXTBOX5,E_ELEM_TEXTBOX6};
// Must use separate enum for fonts with MAX_FONT at end to use gslc_FontSet.
//  - Font slot identifiers loaded into GUIslice font table.
// Used by:
//  - `InitGUIslice_gen()` font initialization calls.
enum {E_BUILTIN5X8,E_DOSIS_BOLD22,E_DOSIS_BOLD24,E_DOSIS_BOOK14,E_DOSIS_BOOK18
  ,E_DOSIS_BOOK20,MAX_FONT};
//<Enum !End!>

// ------------------------------------------------
// Instantiate the GUI
// ------------------------------------------------

// ------------------------------------------------
// Define the maximum number of elements and pages
// ------------------------------------------------
//<ElementDefines !Start!>
#define MAX_PAGE                4

#define MAX_ELEM_PG_MAIN 8 // # Elems total on page
#define MAX_ELEM_PG_MAIN_RAM MAX_ELEM_PG_MAIN // # Elems in RAM
//<ElementDefines !End!>

// ------------------------------------------------
// Create element storage
// ------------------------------------------------
//  - Core GUI state, page storage, and generated widget buffers.
// Used by:
//  - `InitGUIslice_gen()` for layout construction and runtime widget updates.
gslc_tsGui                      m_gui;
gslc_tsDriver                   m_drv;
gslc_tsFont                     m_asFont[MAX_FONT];
gslc_tsPage                     m_asPage[MAX_PAGE];

//<GUI_Extra_Elements !Start!>
gslc_tsElem                     m_asPage1Elem[MAX_ELEM_PG_MAIN_RAM];
gslc_tsElemRef                  m_asPage1ElemRef[MAX_ELEM_PG_MAIN];
gslc_tsXTextbox                 m_sTextbox4;
char                            m_acTextboxBuf4[168]; // NRows=6 NCols=28
gslc_tsXTextbox                 m_sTextbox5;
char                            m_acTextboxBuf5[168]; // NRows=6 NCols=28
gslc_tsXTextbox                 m_sTextbox6;
char                            m_acTextboxBuf6[168]; // NRows=6 NCols=28

#define MAX_STR                 100

//<GUI_Extra_Elements !End!>

// ------------------------------------------------
// Program Globals
// ------------------------------------------------

// Element References for direct access
//<Extern_References !Start!>
extern gslc_tsElemRef* m_pElemTextbox1_6;
extern gslc_tsElemRef* m_pElemTextbox2;
extern gslc_tsElemRef* m_pElemTextbox3;
extern gslc_tsElemRef* m_pTextSlider1;
extern gslc_tsElemRef* m_pTextSlider1;
extern gslc_tsElemRef* m_pTextSlider1;
//<Extern_References !End!>

// Define debug message function
//  - Send debug characters from GUIslice to the active logging sink.
// Inputs:
//  - ch: character to emit.
// Outputs:
//  - returns status code consumed by GUIslice debug subsystem.
static int16_t DebugOut(char ch);

// ------------------------------------------------
// Callback Methods
// ------------------------------------------------
//  - Declare GUIslice event handlers used by generated controls.
// Inputs:
//  - Parameters follow GUIslice callback signatures for control-specific events.
// Outputs:
//  - Boolean return indicates whether each event was handled.
bool CbBtnCommon(void* pvGui,void *pvElemRef,gslc_teTouch eTouch,int16_t nX,int16_t nY);
bool CbCheckbox(void* pvGui, void* pvElemRef, int16_t nSelId, bool bState);
bool CbDrawScanner(void* pvGui,void* pvElemRef,gslc_teRedrawType eRedraw);
bool CbKeypad(void* pvGui, void *pvElemRef, int16_t nState, void* pvData);
bool CbListbox(void* pvGui, void* pvElemRef, int16_t nSelId);
bool CbSlidePos(void* pvGui,void* pvElemRef,int16_t nPos);
bool CbSpinner(void* pvGui, void *pvElemRef, int16_t nState, void* pvData);
bool CbTickScanner(void* pvGui,void* pvScope);

// ------------------------------------------------
// Create page elements
// ------------------------------------------------
/*
 * Objective:
 *  - Initialize generated GUI pages, fonts, and widget instances.
 * Inputs:
 *  - none.
 * Outputs:
 *  - none; populates global GUI/page/element storage for the generated layout.
 */
void InitGUIslice_gen()
{
  gslc_tsElemRef* pElemRef = NULL;

  if (!gslc_Init(&m_gui,&m_drv,m_asPage,MAX_PAGE,m_asFont,MAX_FONT)) { return; }

  // ------------------------------------------------
  // Load Fonts
  // ------------------------------------------------
//<Load_Fonts !Start!>
    if (!gslc_FontSet(&m_gui,E_BUILTIN5X8,GSLC_FONTREF_PTR,NULL,1)) { return; }
    if (!gslc_FontSet(&m_gui,E_DOSIS_BOLD22,GSLC_FONTREF_PTR,&dosis_bold22pt7b,1)) { return; }
    if (!gslc_FontSet(&m_gui,E_DOSIS_BOLD24,GSLC_FONTREF_PTR,&dosis_bold24pt7b,1)) { return; }
    if (!gslc_FontSet(&m_gui,E_DOSIS_BOOK14,GSLC_FONTREF_PTR,&dosis_book14pt7b,1)) { return; }
    if (!gslc_FontSet(&m_gui,E_DOSIS_BOOK18,GSLC_FONTREF_PTR,&dosis_book18pt7b,1)) { return; }
    if (!gslc_FontSet(&m_gui,E_DOSIS_BOOK20,GSLC_FONTREF_PTR,&dosis_book20pt7b,1)) { return; }
//<Load_Fonts !End!>

//<InitGUI !Start!>
  gslc_PageAdd(&m_gui,E_PG_MAIN,m_asPage1Elem,MAX_ELEM_PG_MAIN_RAM,m_asPage1ElemRef,MAX_ELEM_PG_MAIN);

  // NOTE: The current page defaults to the first page added. Here we explicitly
  //       ensure that the main page is the correct page no matter the add order.
  gslc_SetPageCur(&m_gui,E_PG_MAIN);
  
  // Set Background to a flat color
  gslc_SetBkgndColor(&m_gui,GSLC_COL_GRAY_LT3);

  // -----------------------------------
  // PAGE: E_PG_MAIN
  
  
  // Create E_ELEM_TEXT1 text label
  pElemRef = gslc_ElemCreateTxt(&m_gui,E_ELEM_TEXT1,E_PG_MAIN,(gslc_tsRect){10,10,100,32},
    (char*)"HOME",0,E_DOSIS_BOLD22);
  gslc_ElemSetTxtCol(&m_gui,pElemRef,GSLC_COL_BLUE_DK4);
  gslc_ElemSetCol(&m_gui,pElemRef,GSLC_COL_GRAY,GSLC_COL_GRAY_LT3,GSLC_COL_BLACK);
  
  // Create E_ELEM_TEXT3 text label
  pElemRef = gslc_ElemCreateTxt(&m_gui,E_ELEM_TEXT3,E_PG_MAIN,(gslc_tsRect){0,446,800,27},
    (char*)"[Home]      Adust Balance & Priorities       View Availability       Settings",0,E_DOSIS_BOOK14);
  gslc_ElemSetTxtAlign(&m_gui,pElemRef,GSLC_ALIGN_MID_MID);
  gslc_ElemSetTxtCol(&m_gui,pElemRef,GSLC_COL_BLUE_DK4);
  gslc_ElemSetCol(&m_gui,pElemRef,GSLC_COL_GRAY,GSLC_COL_GRAY_LT3,GSLC_COL_BLACK);
  
  // Create E_ELEM_TEXT5 text label
  pElemRef = gslc_ElemCreateTxt(&m_gui,E_ELEM_TEXT5,E_PG_MAIN,(gslc_tsRect){10,100,280,39},
    (char*)"Current Balance: $",0,E_DOSIS_BOOK20);
  gslc_ElemSetTxtCol(&m_gui,pElemRef,GSLC_COL_GRAY_DK3);
  gslc_ElemSetCol(&m_gui,pElemRef,GSLC_COL_GRAY,GSLC_COL_GRAY_LT3,GSLC_COL_BLACK);
  
  // Create E_ELEM_TEXT6 text label
  pElemRef = gslc_ElemCreateTxt(&m_gui,E_ELEM_TEXT6,E_PG_MAIN,(gslc_tsRect){9,204,243,38},
    (char*)"Time Remaining:",0,E_DOSIS_BOOK20);
  gslc_ElemSetTxtCol(&m_gui,pElemRef,GSLC_COL_GRAY_DK3);
  gslc_ElemSetCol(&m_gui,pElemRef,GSLC_COL_GRAY,GSLC_COL_GRAY_LT3,GSLC_COL_BLACK);
  
  // Create E_ELEM_TEXT7 text label
  pElemRef = gslc_ElemCreateTxt(&m_gui,E_ELEM_TEXT7,E_PG_MAIN,(gslc_tsRect){10,300,310,29},
    (char*)"Current Power Draw: ",0,E_DOSIS_BOOK20);
  gslc_ElemSetTxtCol(&m_gui,pElemRef,GSLC_COL_GRAY_DK3);
  gslc_ElemSetCol(&m_gui,pElemRef,GSLC_COL_GRAY,GSLC_COL_GRAY_LT3,GSLC_COL_BLACK);
   
  // Create textbox
  pElemRef = gslc_ElemXTextboxCreate(&m_gui,E_ELEM_TEXTBOX4,E_PG_MAIN,&m_sTextbox4,
    (gslc_tsRect){260,200,150,58},E_DOSIS_BOOK18,
    (char*)&m_acTextboxBuf4,6,28);
  gslc_ElemXTextboxWrapSet(&m_gui,pElemRef,false);
  gslc_ElemSetTxtCol(&m_gui,pElemRef,GSLC_COL_GRAY_DK3);
  gslc_ElemSetCol(&m_gui,pElemRef,GSLC_COL_GRAY,GSLC_COL_GRAY_LT3,GSLC_COL_BLACK);
  m_pElemTextbox2 = pElemRef;
   
  // Create textbox
  pElemRef = gslc_ElemXTextboxCreate(&m_gui,E_ELEM_TEXTBOX5,E_PG_MAIN,&m_sTextbox5,
    (gslc_tsRect){330,300,150,58},E_DOSIS_BOOK18,
    (char*)&m_acTextboxBuf5,6,28);
  gslc_ElemXTextboxWrapSet(&m_gui,pElemRef,false);
  gslc_ElemSetTxtCol(&m_gui,pElemRef,GSLC_COL_GRAY_DK3);
  gslc_ElemSetCol(&m_gui,pElemRef,GSLC_COL_GRAY,GSLC_COL_GRAY_LT3,GSLC_COL_BLACK);
  m_pElemTextbox3 = pElemRef;
   
  // Create textbox
  pElemRef = gslc_ElemXTextboxCreate(&m_gui,E_ELEM_TEXTBOX6,E_PG_MAIN,&m_sTextbox6,
    (gslc_tsRect){300,100,150,58},E_BUILTIN5X8,
    (char*)&m_acTextboxBuf6,6,28);
  gslc_ElemXTextboxWrapSet(&m_gui,pElemRef,false);
  gslc_ElemSetTxtCol(&m_gui,pElemRef,GSLC_COL_GRAY_DK3);
  gslc_ElemSetCol(&m_gui,pElemRef,GSLC_COL_GRAY,GSLC_COL_GRAY_LT3,GSLC_COL_BLACK);
  m_pElemTextbox1_6 = pElemRef;
//<InitGUI !End!>

//<Startup !Start!>
//<Startup !End!>

}

#endif // end _GUISLICE_GEN_H
