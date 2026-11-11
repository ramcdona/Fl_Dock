//
// Dock stripe widget for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#include <FL/Fl_Dock_Stripe.H>
#include <FL/Fl_Dock.H>
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Tooltip.H>
#include <FL/fl_draw.H>
#include "Fl_Dock_Drag.H"
#include <stdlib.h>
#include <string.h>

static int stripe_thickness = 24;
static const int PAD = 6;       // around a button's title or icon
static const int GAP = 2;       // between buttons

/** Returns the width of a side stripe, or the height of a top or bottom one. */
int Fl_Dock_Stripe::thickness() { return stripe_thickness; }

/** Sets the width of side stripes, and the height of top and bottom ones, for all docks. */
void Fl_Dock_Stripe::thickness(int t) { if (t > 8) stripe_thickness = t; }

/** \internal Creates the stripe on one side of a dock. */
Fl_Dock_Stripe::Fl_Dock_Stripe(Fl_Dock *dock, int side)
  : Fl_Widget(0, 0, 0, 0) {
  dock_ = dock;
  side_ = side;
  for (int g = 0; g < 2; g++) {
    items_[g] = 0;
    nitems_[g] = 0;
  }
  pressed_group_ = -1;
  pressed_index_ = -1;
  press_x_ = press_y_ = 0;
  box(FL_FLAT_BOX);
  color(fl_darker(FL_BACKGROUND_COLOR));
}

/** Deletes the stripe, ending a drag of one of its buttons. */
Fl_Dock_Stripe::~Fl_Dock_Stripe() {
  Fl_Dock_Drag::source_gone(this);
  for (int g = 0; g < 2; g++) free(items_[g]);
}

/** Returns the number of buttons. */
int Fl_Dock_Stripe::count() const { return nitems_[0] + nitems_[1]; }

/** \internal Puts a button for p before button index of group, at the end if index is -1. */
void Fl_Dock_Stripe::insert(Fl_Dock_Panel *p, int group, int index) {
  if (group < 0 || group > 1) group = 0;
  int n = nitems_[group];
  if (index < 0 || index > n) index = n;
  items_[group] = (Fl_Dock_Panel **)realloc(items_[group], sizeof(Fl_Dock_Panel *) * (n + 1));
  for (int i = n; i > index; i--) items_[group][i] = items_[group][i - 1];
  items_[group][index] = p;
  nitems_[group] = n + 1;
  redraw();
}

/** \internal Removes p's button; returns 1 if it had one. */
int Fl_Dock_Stripe::remove(Fl_Dock_Panel *p) {
  int g, i;
  if (!find(p, g, i)) return 0;
  for (int k = i; k < nitems_[g] - 1; k++) items_[g][k] = items_[g][k + 1];
  nitems_[g]--;
  pressed_group_ = -1;
  pressed_index_ = -1;
  redraw();
  return 1;
}

/** Finds p's button: returns 1 and its group and index, or 0. */
int Fl_Dock_Stripe::find(Fl_Dock_Panel *p, int &group, int &index) const {
  for (int g = 0; g < 2; g++) {
    for (int i = 0; i < nitems_[g]; i++) {
      if (items_[g][i] == p) {
        group = g;
        index = i;
        return 1;
      }
    }
  }
  return 0;
}

// The length of p's button along the stripe: its icon's, or its title's.
int Fl_Dock_Stripe::length(Fl_Dock_Panel *p) const {
  Fl_Image *icon = p->dock_icon();
  if (icon) {
    if (vertical()) return icon->h() + 2 * PAD;
    return icon->w() + 2 * PAD;
  }
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  int len = 0;
  if (p->label()) len = (int)fl_width(p->label());
  return len + 2 * PAD;
}

// The boxes of a group's buttons, X, Y, W and H for each, measured once, in
// a list the caller frees.
int *Fl_Dock_Stripe::button_boxes(int group) const {
  int n = nitems_[group];
  int *b = (int *)malloc(sizeof(int) * 4 * (n + 1));
  int total = w();
  if (vertical()) total = h();
  int pos = 0;
  for (int i = 0; i < n; i++) {
    int len = length(items_[group][i]);
    int at = pos;
    if (group == 1) at = total - pos - len;
    if (vertical()) {
      b[4 * i] = x();
      b[4 * i + 1] = y() + at;
      b[4 * i + 2] = w();
      b[4 * i + 3] = len;
    } else {
      b[4 * i] = x() + at;
      b[4 * i + 1] = y();
      b[4 * i + 2] = len;
      b[4 * i + 3] = h();
    }
    pos += len + GAP;
  }
  return b;
}

/**
  Gets the box of button i of a group.  Group 0 runs from the start of the
  stripe, the top or the left, and group 1 back from its end.
*/
void Fl_Dock_Stripe::button_box(int group, int i, int &X, int &Y, int &W, int &H) const {
  int pos = 0;
  for (int k = 0; k < i; k++) pos += length(items_[group][k]) + GAP;
  int len = length(items_[group][i]);
  if (group == 1) {
    int total = w();
    if (vertical()) total = h();
    pos = total - pos - len;
  }
  if (vertical()) {
    X = x();
    Y = y() + pos;
    W = w();
    H = len;
  } else {
    X = x() + pos;
    Y = y();
    W = len;
    H = h();
  }
}

/** Returns the panel whose button is at X, Y, or 0. */
Fl_Dock_Panel *Fl_Dock_Stripe::button_at(int X, int Y) const {
  Fl_Dock_Panel *p = 0;
  for (int g = 0; g < 2 && !p; g++) {
    int *b = button_boxes(g);
    for (int i = 0; i < nitems_[g]; i++) {
      int *r = b + 4 * i;
      if (X >= r[0] && X < r[0] + r[2] && Y >= r[1] && Y < r[1] + r[3]) {
        p = items_[g][i];
        break;
      }
    }
    free(b);
  }
  return p;
}

/**
  \internal Finds where a button dropped at X, Y would go: the group of the
  half of the stripe it is in, before the button it is over.
*/
void Fl_Dock_Stripe::insert_place(int X, int Y, int &group, int &index) const {
  int along = Y - y();
  int total = h();
  if (!vertical()) {
    along = X - x();
    total = w();
  }
  group = 0;
  if (along > total / 2) group = 1;
  index = nitems_[group];
  int *b = button_boxes(group);
  for (int i = 0; i < nitems_[group]; i++) {
    int bx = b[4 * i];
    int by = b[4 * i + 1];
    int bw = b[4 * i + 2];
    int bh = b[4 * i + 3];
    int mid = by + bh / 2;
    int at = Y;
    if (!vertical()) {
      mid = bx + bw / 2;
      at = X;
    }
    // Group 0 runs forward and group 1 backward along the stripe.
    if ((group == 0 && at < mid) || (group == 1 && at > mid)) {
      index = i;
      break;
    }
  }
  free(b);
}

/** Draws the buttons, each pressed while its panel is shown. */
void Fl_Dock_Stripe::draw() {
  draw_box();
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  for (int g = 0; g < 2; g++) {
    int *b = button_boxes(g);
    for (int i = 0; i < nitems_[g]; i++) {
      Fl_Dock_Panel *p = items_[g][i];
      int bx = b[4 * i];
      int by = b[4 * i + 1];
      int bw = b[4 * i + 2];
      int bh = b[4 * i + 3];
      int shown = dock_->panel_shown(p);
      if (shown) draw_box(FL_DOWN_BOX, bx, by, bw, bh, fl_lighter(color()));
      else if (g == pressed_group_ && i == pressed_index_) draw_box(FL_THIN_DOWN_BOX, bx, by, bw, bh, color());
      Fl_Color text = FL_FOREGROUND_COLOR;
      if (!active_r()) text = fl_inactive(text);
      fl_color(text);
      Fl_Image *icon = p->dock_icon();
      if (icon) {
        icon->draw(bx + (bw - icon->w()) / 2, by + (bh - icon->h()) / 2);
      } else if (p->label()) {
        int asc = fl_height() - fl_descent();
        if (side_ == 0) {
          // Read from the bottom up, as on the left of most applications.
          fl_draw(90, p->label(), bx + (bw + asc) / 2 - 1, by + bh - PAD);
        } else if (side_ == 1) {
          fl_draw(270, p->label(), bx + (bw - asc) / 2 + 1, by + PAD);
        } else {
          fl_draw(p->label(), bx + PAD, by + (bh + asc) / 2 - 1);
        }
      }
    }
    free(b);
  }
}

// The menu of a button: pin or unpin its panel, move it, or take it off.
void Fl_Dock_Stripe::show_menu(Fl_Dock_Panel *p) {
  enum { PIN = 1, SLIDE, LEFT, RIGHT, TOP, BOTTOM, REMOVE, HIDE };
  static const char *side_names[4] = { "Move to Left", "Move to Right", "Move to Top", "Move to Bottom" };
  Fl_Menu_Item items[10];
  memset(items, 0, sizeof(items));
  int k = 0;
  if (p->mode() == FL_DOCK_MODE_SLIDING) {
    items[k].label("Pin");
    items[k].argument(PIN);
  } else {
    items[k].label("Slide");
    items[k].argument(SLIDE);
  }
  items[k].flags = FL_MENU_DIVIDER;
  k++;
  for (int s = 0; s < 4; s++) {
    if (s == side_ || !(dock_->stripes() & (1 << s))) continue;
    items[k].label(side_names[s]);
    items[k].argument(LEFT + s);
    k++;
  }
  if (k > 1) items[k - 1].flags = FL_MENU_DIVIDER;
  items[k].label("Remove from Stripe");
  items[k].argument(REMOVE);
  k++;
  if (dock_->panel_shown(p)) {
    items[k].label("Hide");
    items[k].argument(HIDE);
    k++;
  }
  const Fl_Menu_Item *m = items->popup(Fl::event_x(), Fl::event_y());
  if (!m) return;
  long a = m->argument();
  Fl_Dock *d = dock_;
  if (a == HIDE) {
    d->toggle(p, 1);
    return;
  }
  int done = 0;
  if (a == PIN) done = d->pin_panel(p);
  else if (a == SLIDE) done = d->slide_panel(p);
  else if (a >= LEFT && a <= BOTTOM) done = d->stripe_panel(p, (Fl_Dock_Where)(a - LEFT));
  else if (a == REMOVE) done = d->unstripe_panel(p);
  if (done) d->notify(p, FL_REASON_DRAGGED);
}

/** Handles the mouse on the buttons; see the class description. */
int Fl_Dock_Stripe::handle(int event) {
  switch (event) {
    case FL_ENTER:
    case FL_MOVE: {
      Fl_Dock_Panel *p = button_at(Fl::event_x(), Fl::event_y());
      if (p) {
        int g, i, bx, by, bw, bh;
        find(p, g, i);
        button_box(g, i, bx, by, bw, bh);
        Fl_Tooltip::enter_area(this, bx - x(), by - y(), bw, bh, p->label());
      } else {
        Fl_Tooltip::exit(this);
      }
      return 1;
    }
    case FL_LEAVE:
      Fl_Tooltip::exit(this);
      return 1;
    case FL_PUSH: {
      Fl_Tooltip::exit(this);
      Fl_Dock_Panel *p = button_at(Fl::event_x(), Fl::event_y());
      if (!p) return 1;
      if (Fl::event_button() == FL_RIGHT_MOUSE) {
        show_menu(p);
        return 1;
      }
      find(p, pressed_group_, pressed_index_);
      press_x_ = Fl::event_x();
      press_y_ = Fl::event_y();
      redraw();
      return 1;
    }
    case FL_DRAG: {
      if (pressed_group_ < 0) return 1;
      if (Fl_Dock_Drag::active()) {
        Fl_Dock_Drag::drag();
        return 1;
      }
      int dx = Fl::event_x() - press_x_;
      int dy = Fl::event_y() - press_y_;
      if (dx * dx + dy * dy > 25) {
        Fl_Dock_Panel *p = items_[pressed_group_][pressed_index_];
        if (p->dock_flags() & FL_DOCK_MOVABLE) Fl_Dock_Drag::begin_button(this, p);
      }
      return 1;
    }
    case FL_RELEASE: {
      if (pressed_group_ < 0) return 1;
      Fl_Dock_Panel *p = items_[pressed_group_][pressed_index_];
      pressed_group_ = -1;
      pressed_index_ = -1;
      redraw();
      if (Fl_Dock_Drag::active()) {
        Fl_Dock_Drag::release();
        return 1;
      }
      if (button_at(Fl::event_x(), Fl::event_y()) == p) dock_->toggle(p, 1);
      return 1;
    }
    case FL_KEYBOARD:
      if (Fl::event_key() == FL_Escape && Fl_Dock_Drag::active()) {
        Fl_Dock_Drag::cancel();
        pressed_group_ = -1;
        redraw();
        return 1;
      }
      break;
    default:
      break;
  }
  return Fl_Widget::handle(event);
}
