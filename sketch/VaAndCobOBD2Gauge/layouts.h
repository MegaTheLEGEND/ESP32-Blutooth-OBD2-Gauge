// layouts.h
#pragma once

#define GRID_MIXED 2  // left 160px numeric col + right two 80px vbar cols
// ============================================================
// GAUGE TYPE CONSTANTS
// ============================================================
#define GT_ARC     0
#define GT_NUMERIC 1
#define GT_VBAR    2
#define GT_SEG7    3
#define GT_SEGBAR  4
#define GT_NEEDLE  5
#define GT_C4BAR   6
#define GT_VFD     7
#define GT_DOT     8

// ============================================================
// GRID TYPE CONSTANTS
// ============================================================
#define GRID_6CELL 0  // two 160px cols, three 80px rows
#define GRID_4COL  1  // four 80px cols full height

// ============================================================
// STRUCTS
// ============================================================
struct CellDef {
  uint8_t pid;
  uint8_t gaugeType;
};

struct LayoutDef {
  const char* name;
  uint8_t     grid;
  uint8_t     cellCount;
  CellDef     cells[6];
  uint8_t     enginePid;
};

// ============================================================
// LAYOUT TABLE - add new layouts here, nothing else to change
// ============================================================
const LayoutDef layoutDefs[] = {

  // 0 - arc left, numeric right
  { "Arc + Numeric", GRID_6CELL, 6, {
    {0, GT_ARC}, {2, GT_ARC}, {3, GT_ARC},
    {1, GT_NUMERIC}, {5, GT_NUMERIC}, {6, GT_NUMERIC}
  }, 4 },

  // 1 - all arc
  { "All Arc", GRID_6CELL, 6, {
    {0, GT_ARC}, {2, GT_ARC}, {3, GT_ARC},
    {1, GT_ARC}, {5, GT_ARC}, {6, GT_ARC}
  }, 4 },

  // 2 - all numeric
  { "All Numeric", GRID_6CELL, 6, {
    {0, GT_NUMERIC}, {2, GT_NUMERIC}, {3, GT_NUMERIC},
    {1, GT_NUMERIC}, {5, GT_NUMERIC}, {6, GT_NUMERIC}
  }, 4 },

    // 3 - numeric left, vbar right
  { "Numeric + VBar", GRID_MIXED, 5, {
    {1, GT_NUMERIC}, {5, GT_NUMERIC}, {6, GT_NUMERIC},
    {3, GT_VBAR},    {2, GT_VBAR}
  }, 4 },

  // 4 - vbar left, numeric right  
  { "VBar + Numeric", GRID_MIXED, 5, {
    {6, GT_NUMERIC}, {1, GT_NUMERIC}, {5, GT_NUMERIC},
    {2, GT_VBAR},    {3, GT_VBAR}
  }, 4 },

  // 5 - two vbar left, three numeric right
  { "VBar Left", GRID_MIXED, 5, {
    {0, GT_NUMERIC}, {2, GT_NUMERIC}, {4, GT_NUMERIC},
    {1, GT_VBAR},    {3, GT_VBAR}
  }, 4 },

  // 6 - all vbar
  { "All VBar", GRID_4COL, 4, {
    {1, GT_VBAR}, {5, GT_VBAR}, {6, GT_VBAR}, {4, GT_VBAR}
  }, 4 },

  // 7 - all vbar alt
  { "All VBar Alt", GRID_4COL, 4, {
    {3, GT_VBAR}, {2, GT_VBAR}, {0, GT_VBAR}, {4, GT_VBAR}
  }, 4 },

  // 8 - needle left, numeric right
  { "Needle + Numeric", GRID_6CELL, 6, {
    {0, GT_NEEDLE}, {2, GT_NEEDLE}, {3, GT_NEEDLE},
    {1, GT_NUMERIC}, {5, GT_NUMERIC}, {6, GT_NUMERIC}
  }, 4 },

  // 9 - all 7seg
  { "All 7-Seg", GRID_6CELL, 6, {
    {0, GT_SEG7}, {2, GT_SEG7}, {3, GT_SEG7},
    {1, GT_SEG7}, {5, GT_SEG7}, {6, GT_SEG7}
  }, 4 },

  // 10 - all segbar
  { "All SegBar", GRID_4COL, 4, {
    {1, GT_SEGBAR}, {5, GT_SEGBAR}, {6, GT_SEGBAR}, {3, GT_SEGBAR}
  }, 4 },

  // 11 - C4 Corvette red LED
  { "C4 Red LED", GRID_4COL, 4, {
    {1, GT_C4BAR}, {5, GT_C4BAR}, {6, GT_C4BAR}, {3, GT_C4BAR}
  }, 4 },

  // 12 - DeLorean VFD
  { "DeLorean VFD", GRID_6CELL, 6, {
    {0, GT_VFD}, {2, GT_VFD}, {3, GT_VFD},
    {1, GT_VFD}, {5, GT_VFD}, {6, GT_VFD}
  }, 4 },

  // 13 - Amber dot matrix
  { "Dot Matrix", GRID_6CELL, 6, {
    {0, GT_DOT}, {2, GT_DOT}, {3, GT_DOT},
    {1, GT_DOT}, {5, GT_DOT}, {6, GT_DOT}
  }, 4 },
  
    // 14 - fuel & air
  { "Fuel & Air", GRID_6CELL, 6, {
    {7,  GT_ARC},     {9,  GT_ARC},     {10, GT_ARC},
    {11, GT_NUMERIC}, {12, GT_NUMERIC}, {13, GT_NUMERIC}
  }, 4 },

  // 15 - fuel & air numeric
  { "Fuel Numeric", GRID_6CELL, 6, {
    {7,  GT_NUMERIC}, {9,  GT_NUMERIC}, {10, GT_NUMERIC},
    {11, GT_NUMERIC}, {12, GT_NUMERIC}, {13, GT_NUMERIC}
  }, 4 },

  // 16 - engine vitals arc
  { "Engine Vitals", GRID_6CELL, 6, {
    {3,  GT_ARC}, {0,  GT_ARC}, {14, GT_ARC},
    {1,  GT_ARC}, {2,  GT_ARC}, {5,  GT_ARC}
  }, 4 },

  // 17 - engine vitals needle
  { "Engine Needle", GRID_6CELL, 6, {
    {3,  GT_NEEDLE}, {0,  GT_NEEDLE}, {14, GT_NEEDLE},
    {1,  GT_NEEDLE}, {2,  GT_NEEDLE}, {5,  GT_NEEDLE}
  }, 4 },

  // 18 - engine vitals 7seg
  { "Engine 7-Seg", GRID_6CELL, 6, {
    {3,  GT_SEG7}, {0,  GT_SEG7}, {14, GT_SEG7},
    {1,  GT_SEG7}, {2,  GT_SEG7}, {5,  GT_SEG7}
  }, 4 },

  // 19 - engine vitals VFD
  { "Engine VFD", GRID_6CELL, 6, {
    {3,  GT_VFD}, {0,  GT_VFD}, {14, GT_VFD},
    {1,  GT_VFD}, {2,  GT_VFD}, {5,  GT_VFD}
  }, 4 },

  // 20 - engine vitals dot matrix
  { "Engine Dot", GRID_6CELL, 6, {
    {3,  GT_DOT}, {0,  GT_DOT}, {14, GT_DOT},
    {1,  GT_DOT}, {2,  GT_DOT}, {5,  GT_DOT}
  }, 4 },

  // 21 - temps overview
  { "All Temps", GRID_6CELL, 6, {
    {1,  GT_ARC},     {5,  GT_ARC},     {6,  GT_ARC},
    {15, GT_NUMERIC}, {17, GT_NUMERIC}, {4,  GT_NUMERIC}
  }, 4 },

  // 22 - temps segbar
  { "Temps SegBar", GRID_4COL, 4, {
    {1,  GT_SEGBAR}, {5,  GT_SEGBAR}, {6,  GT_SEGBAR}, {15, GT_SEGBAR}
  }, 4 },

  // 23 - fuel system
  { "Fuel System", GRID_6CELL, 6, {
    {17, GT_ARC},     {9,  GT_ARC},     {10, GT_ARC},
    {7,  GT_NUMERIC}, {18, GT_NUMERIC}, {19, GT_NUMERIC}
  }, 4 },

  // 24 - throttle & load
  { "Throttle", GRID_6CELL, 6, {
    {7,  GT_NEEDLE}, {19, GT_NEEDLE}, {0,  GT_NEEDLE},
    {3,  GT_NUMERIC}, {8, GT_NUMERIC}, {18, GT_NUMERIC}
  }, 4 },

  // 25 - throttle & load vbar
  { "Throttle VBar", GRID_4COL, 4, {
    {7,  GT_VBAR}, {19, GT_VBAR}, {0,  GT_VBAR}, {3, GT_VBAR}
  }, 4 },

  // 26 - O2 & fuel trim
  { "O2 & Trim", GRID_6CELL, 6, {
    {12, GT_ARC},     {13, GT_ARC},     {9,  GT_ARC},
    {10, GT_NUMERIC}, {16, GT_NUMERIC}, {4,  GT_NUMERIC}
  }, 4 },

  // 27 - timing & load
  { "Timing", GRID_6CELL, 6, {
    {14, GT_NEEDLE}, {0,  GT_NEEDLE}, {2,  GT_NEEDLE},
    {3,  GT_NUMERIC}, {7, GT_NUMERIC}, {4,  GT_NUMERIC}
  }, 4 },

  // 28 - highway cruise
  { "Highway", GRID_6CELL, 6, {
    {8,  GT_ARC},     {3,  GT_ARC},     {4,  GT_ARC},
    {1,  GT_NUMERIC}, {9,  GT_NUMERIC}, {10, GT_NUMERIC}
  }, 4 },

  // 29 - highway cruise needle
  { "Highway Needle", GRID_6CELL, 6, {
    {8,  GT_NEEDLE}, {3,  GT_NEEDLE}, {4,  GT_NEEDLE},
    {1,  GT_NUMERIC}, {2, GT_NUMERIC}, {7,  GT_NUMERIC}
  }, 4 },

  // 30 - full C4 expanded
  { "C4 Full", GRID_4COL, 4, {
    {3,  GT_C4BAR}, {0,  GT_C4BAR}, {7,  GT_C4BAR}, {2, GT_C4BAR}
  }, 4 },

  // 31 - full segbar expanded
  { "SegBar Full", GRID_4COL, 4, {
    {3,  GT_SEGBAR}, {0,  GT_SEGBAR}, {7,  GT_SEGBAR}, {2, GT_SEGBAR}
  }, 4 },

  // 32 - mixed needle + vbar
  { "Needle + VBar", GRID_MIXED, 5, {
    {3,  GT_NEEDLE}, {0,  GT_NEEDLE}, {7,  GT_NEEDLE},
    {2,  GT_VBAR},   {1,  GT_VBAR}
  }, 4 },

  // 33 - mixed arc + vbar
  { "Arc + VBar", GRID_MIXED, 5, {
    {3,  GT_ARC}, {0,  GT_ARC}, {14, GT_ARC},
    {2,  GT_VBAR}, {1, GT_VBAR}
  }, 4 },

  // 34 - mixed VFD + vbar
  { "VFD + VBar", GRID_MIXED, 5, {
    {3,  GT_VFD}, {0,  GT_VFD}, {1,  GT_VFD},
    {2,  GT_VBAR}, {7, GT_VBAR}
  }, 4 },

  // 35 - all VFD expanded
  { "VFD Expanded", GRID_6CELL, 6, {
    {3,  GT_VFD}, {0,  GT_VFD}, {7,  GT_VFD},
    {8,  GT_VFD}, {2,  GT_VFD}, {14, GT_VFD}
  }, 4 },

  // 36 - all dot expanded
  { "Dot Expanded", GRID_6CELL, 6, {
    {3,  GT_DOT}, {0,  GT_DOT}, {7,  GT_DOT},
    {8,  GT_DOT}, {2,  GT_DOT}, {14, GT_DOT}
  }, 4 },

};

const uint8_t max_layout = sizeof(layoutDefs) / sizeof(layoutDefs[0]);