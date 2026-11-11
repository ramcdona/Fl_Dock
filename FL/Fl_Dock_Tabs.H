//
// Dock tabs header file for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#ifndef Fl_Dock_Tabs_H
#define Fl_Dock_Tabs_H

#include <FL/Fl_Dock_Export.H>
#include <FL/Fl_Group.H>

class Fl_Dock_Panel;

/** The type() of an Fl_Dock_Tabs. */
#define FL_DOCK_TABS 0xD1

/**
  A leaf of an Fl_Dock's tree: one or more panels stacked as tabs, one of
  them shown.

  The tab strip along the top has a tab for each panel, and a button at the
  right of the strip opens a menu to select a panel or close the shown one.
  A thin line goes around the group, see border().  Fl_Dock creates and
  removes tab groups as panels move.
*/
class FL_DOCK_EXPORT Fl_Dock_Tabs : public Fl_Group {
  int value_;           // index of the shown panel

  int tab_at(int X, int Y) const;
  int *tab_widths() const;
  void draw_strip();
  int on_menu_box(int X, int Y) const;
  void show_menu();
  void insert_panel(Fl_Dock_Panel *p, int index);
  void remove_panel(Fl_Dock_Panel *p);
  friend class Fl_Dock;

public:
  Fl_Dock_Tabs(int X, int Y, int W, int H);

  void resize(int X, int Y, int W, int H) FL_OVERRIDE;
  int handle(int event) FL_OVERRIDE;
  void draw() FL_OVERRIDE;

protected:
  void on_remove(int index) FL_OVERRIDE;

public:

  /** Returns the number of panels. */
  int panels() const { return children(); }
  Fl_Dock_Panel *panel(int i) const;
  Fl_Dock_Panel *value() const;
  void value(Fl_Dock_Panel *p);
  void layout();

  static int tab_height();
  static void tab_height(int h);
  static int border();
  static void border(int b);
  static int outer_w(int W);
  static int outer_h(int H);
};

#endif // !Fl_Dock_Tabs_H
