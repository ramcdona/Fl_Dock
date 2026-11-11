//
// Dock stripe header file for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#ifndef Fl_Dock_Stripe_H
#define Fl_Dock_Stripe_H

#include <FL/Fl_Dock_Export.H>
#include <FL/Fl_Widget.H>

class Fl_Dock;
class Fl_Dock_Panel;

/**
  A row of buttons along one side of an Fl_Dock, one for each panel given a
  place on it with Fl_Dock::stripe_panel().

  A button shows its panel's icon, or else its title, and is drawn pressed
  while the panel is shown.  Clicking it shows or hides the panel, the right
  mouse button opens a menu to pin, unpin, move or remove it, and dragging
  it moves it to another place on this or another stripe, or docks its
  panel.  The buttons are in two groups, at the start and at the end of the
  stripe.  Fl_Dock creates its stripes; applications only meet them when
  they ask for them.
*/
class FL_DOCK_EXPORT Fl_Dock_Stripe : public Fl_Widget {
  Fl_Dock *dock_;
  int side_;                    // an Fl_Dock_Where
  Fl_Dock_Panel **items_[2];
  int nitems_[2];
  int pressed_group_, pressed_index_;
  int press_x_, press_y_;

  int length(Fl_Dock_Panel *p) const;
  int *button_boxes(int group) const;
  void show_menu(Fl_Dock_Panel *p);
  Fl_Dock_Stripe(Fl_Dock *dock, int side);
  void insert(Fl_Dock_Panel *p, int group, int index);
  int remove(Fl_Dock_Panel *p);
  void insert_place(int X, int Y, int &group, int &index) const;
  friend class Fl_Dock;
  friend class Fl_Dock_Drag;

public:
  ~Fl_Dock_Stripe() FL_OVERRIDE;

  int handle(int event) FL_OVERRIDE;
  void draw() FL_OVERRIDE;

  /** Returns the side of the dock the stripe is on, an Fl_Dock_Where. */
  int side() const { return side_; }
  /** Returns 1 if the stripe runs up and down, on the left or right of the dock. */
  int vertical() const { return side_ == 0 || side_ == 1; }
  /** Returns the number of buttons in group 0 or 1. */
  int buttons(int group) const {
    if (group < 0 || group > 1) return 0;
    return nitems_[group];
  }
  /** Returns the panel of button i in group 0 or 1, or 0. */
  Fl_Dock_Panel *button(int group, int i) const {
    if (group < 0 || group > 1 || i < 0 || i >= nitems_[group]) return 0;
    return items_[group][i];
  }
  int count() const;

  int find(Fl_Dock_Panel *p, int &group, int &index) const;
  void button_box(int group, int i, int &X, int &Y, int &W, int &H) const;
  Fl_Dock_Panel *button_at(int X, int Y) const;

  static int thickness();
  static void thickness(int t);
};

#endif // !Fl_Dock_Stripe_H
