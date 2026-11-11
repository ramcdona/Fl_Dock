//
// Dragging panels for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#ifndef Fl_Dock_Drag_H
#define Fl_Dock_Drag_H

#include <FL/Fl_Dock.H>

class Fl_Dock_Tabs;
class Fl_Dock_Panel;
class Fl_Window;

/**
  \internal Where a dragged panel would go.
*/
struct Fl_Dock_Target {
  Fl_Dock *dock;        ///< the dock it goes into, 0 for none
  Fl_Widget *node;      ///< the leaf it goes beside or into, or 0 for the whole dock
  int where;            ///< an Fl_Dock_Where
  int index;            ///< for FL_DOCK_TAB, the tab it goes before
  double fraction;      ///< for a side, the share of the space it takes
  int x, y, w, h;       ///< where it would be shown, in its window's coordinates
};

/**
  \internal The drag of a panel, or of a whole tab group, between places in
  docks.  There is at most one at a time.  An Fl_Dock_Tabs starts it and
  passes it its FL_DRAG and FL_RELEASE events.
*/
class FL_DOCK_EXPORT Fl_Dock_Drag {
public:
  static int active();
  static void begin(Fl_Dock_Tabs *tabs, Fl_Dock_Panel *panel);
  static void drag();
  static void release();
  static void cancel();
  static void forget(Fl_Dock_Panel *p);
  static void source_gone(Fl_Widget *w);
  static void notify_selected(Fl_Dock_Tabs *tabs, Fl_Dock_Panel *p);
  static void close(Fl_Dock_Tabs *tabs, Fl_Dock_Panel *p);

  static int hit(Fl_Dock *d, int X, int Y, Fl_Dock_Tabs *source, Fl_Dock_Panel *panel, Fl_Dock_Target &t);
  static int drop(Fl_Dock_Tabs *source, Fl_Dock_Panel *panel, const Fl_Dock_Target &t);
  static int drop_list(Fl_Dock_Tabs *source, Fl_Dock_Panel *panel, Fl_Dock_Panel **list, int n,
                       const Fl_Dock_Target &t);
  static Fl_Dock *dock_of(Fl_Widget *w);
};

#endif // !Fl_Dock_Drag_H
