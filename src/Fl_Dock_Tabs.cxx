//
// Dock tabs widget for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#include <FL/Fl_Dock_Tabs.H>
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock.H>
#include <FL/Fl_Dock_Window.H>
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include <stdlib.h>

static int tabs_height = 24;
static int border_size = 1;

// Sizes within the tab strip.
static const int TAB_PAD = 10;          // space either side of a tab's title
static const int TAB_MIN_W = 36;
static const int MENU_W = 20;           // the menu button at the right
static const int GRAB_W = 40;           // strip kept free of tabs to move an inset window by

/** Creates an empty tab group. */
Fl_Dock_Tabs::Fl_Dock_Tabs(int X, int Y, int W, int H)
  : Fl_Group(X, Y, W, H) {
  type(FL_DOCK_TABS);
  box(FL_FLAT_BOX);
  value_ = 0;
  end();
}

/** Returns the height of the tab strip. */
int Fl_Dock_Tabs::tab_height() { return tabs_height; }

/** Sets the height of the tab strip for all tab groups. */
void Fl_Dock_Tabs::tab_height(int h) { if (h > 0) tabs_height = h; }

/** Returns the width of the line around each tab group and its panel. */
int Fl_Dock_Tabs::border() { return border_size; }

/** Sets the width of the line around each tab group and its panel, 0 for none, for all tab groups. */
void Fl_Dock_Tabs::border(int b) { if (b >= 0) border_size = b; }

/** Returns the width of a tab group whose panels are W wide. */
int Fl_Dock_Tabs::outer_w(int W) { return W + 2 * border_size; }

/** Returns the height of a tab group whose panels are H high. */
int Fl_Dock_Tabs::outer_h(int H) { return H + tabs_height + border_size; }

// The window of the dock this group is in, if it is an inset one, or 0.
// Its tab strips are its title bar.
static Fl_Dock_Window *inset_window_of(const Fl_Widget *w) {
  for (Fl_Widget *p = w->parent(); p; p = p->parent()) {
    if (Fl_Dock::is_split(p) || Fl_Dock::is_tabs(p)) continue;
    Fl_Dock *d = (Fl_Dock *)p;
    if (d->owner() == d || !d->window()) return 0;
    Fl_Dock_Window *win = (Fl_Dock_Window *)d->window();
    if (win->inset()) return win;
    return 0;
  }
  return 0;
}

/** Returns panel i, or 0. */
Fl_Dock_Panel *Fl_Dock_Tabs::panel(int i) const {
  if (i < 0 || i >= children()) return 0;
  return (Fl_Dock_Panel *)child(i);
}

/** Returns the panel shown, or 0 if there is none. */
Fl_Dock_Panel *Fl_Dock_Tabs::value() const {
  return panel(value_);
}

/** Shows panel p, which must be one of this group's. */
void Fl_Dock_Tabs::value(Fl_Dock_Panel *p) {
  int i = find(p);
  if (i >= children()) return;
  value_ = i;
  // Outside a window the panels stay hidden, as Fl_Dock keeps them there;
  // the dock shows this one when it places the group.
  if (!window()) return;
  for (int k = 0; k < children(); k++) {
    if (k == value_) child(k)->show();
    else child(k)->hide();
  }
  redraw();
}

/** Adds panel p as the tab before index, and shows it. */
void Fl_Dock_Tabs::insert_panel(Fl_Dock_Panel *p, int index) {
  if (index < 0 || index > children()) index = children();
  insert(*p, index);
  layout();
  value(p);
}

/** Removes panel p; the tab next to it is shown. */
void Fl_Dock_Tabs::remove_panel(Fl_Dock_Panel *p) {
  int i = find(p);
  if (i >= children()) return;
  remove(i);
  if (children() > 0) value(panel(value_));
  redraw();
}

/**
  Called by FLTK as a panel leaves the group.
  However a panel leaves, the index of the one shown stays on it, or on the
  tab next to it.
*/
void Fl_Dock_Tabs::on_remove(int index) {
  if (index < value_) value_--;
  if (value_ >= children() - 1) value_ = children() - 2;
  if (value_ < 0) value_ = 0;
}

/** Places every panel below the tab strip. */
void Fl_Dock_Tabs::layout() {
  // A panel may hold a subwindow, which cannot be resized outside a window
  // (on macOS it crashes); the dock lays the group out once it is placed.
  if (!window()) return;
  int th = tabs_height;
  int b = border_size;
  for (int i = 0; i < children(); i++) {
    int pw = w() - 2 * b;
    int ph = h() - th - b;
    if (pw < 0) pw = 0;
    if (ph < 0) ph = 0;
    child(i)->resize(x() + b, y() + th, pw, ph);
  }
}

/** Resizes the group and lays out its strip and panels. */
void Fl_Dock_Tabs::resize(int X, int Y, int W, int H) {
  Fl_Widget::resize(X, Y, W, H);
  layout();
}

// The width of every tab, in a list the caller frees.  The tabs share the
// strip, less the menu button: each as wide as its title needs, all shrunk
// alike when they do not fit.
int *Fl_Dock_Tabs::tab_widths() const {
  int n = children();
  int *ws = (int *)malloc(sizeof(int) * (n + 1));
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  int room = w() - MENU_W;
  if (inset_window_of(this)) room -= GRAB_W;
  int total = 0;
  for (int k = 0; k < n; k++) {
    Fl_Widget *c = child(k);
    int ww = 0;
    if (c->label()) ww = (int)fl_width(c->label());
    ww += 2 * TAB_PAD;
    if (ww < TAB_MIN_W) ww = TAB_MIN_W;
    ws[k] = ww;
    total += ww;
  }
  if (total > room && total > 0) {
    for (int k = 0; k < n; k++) ws[k] = ws[k] * room / total;
  }
  return ws;
}

// The index of the tab at X, Y; -1 for the strip beside the tabs; -2 elsewhere.
int Fl_Dock_Tabs::tab_at(int X, int Y) const {
  if (Y < y() || Y >= y() + tabs_height || X < x() || X >= x() + w()) return -2;
  int *ws = tab_widths();
  int tx = x();
  int at = -1;
  for (int i = 0; i < children() && at < 0; i++) {
    if (X >= tx && X < tx + ws[i]) at = i;
    tx += ws[i];
  }
  free(ws);
  return at;
}

int Fl_Dock_Tabs::on_menu_box(int X, int Y) const {
  return X >= x() + w() - MENU_W && X < x() + w() && Y >= y() && Y < y() + tabs_height;
}

/** \internal Returns 1 if X, Y is on the tab strip, beside the tabs and the menu button. */
int Fl_Dock_Tabs::on_strip_space(int X, int Y) const {
  return tab_at(X, Y) == -1 && !on_menu_box(X, Y);
}

// The menu at the right of the strip: the panels, to show one whose tab is
// too narrow to read, then what can be done with the one shown.
void Fl_Dock_Tabs::show_menu() {
  Fl_Dock_Panel *p = value();
  Fl_Dock *d = 0;
  for (Fl_Widget *w = parent(); w; w = w->parent()) {
    if (Fl_Dock::is_split(w) || Fl_Dock::is_tabs(w)) continue;
    d = (Fl_Dock *)w;
    break;
  }
  if (!p || !d) return;
  enum { FLOAT = 1000, INSET, DOCK, CLOSE };
  int n = children();
  Fl_Menu_Item *items = (Fl_Menu_Item *)calloc(n + 8, sizeof(Fl_Menu_Item));
  int k = 0;
  for (int i = 0; i < n; i++) {
    items[k].label(child(i)->label());
    items[k].argument(i);
    items[k].flags = FL_MENU_RADIO;
    if (i == value_) items[k].flags |= FL_MENU_VALUE;
    if (i == n - 1) items[k].flags |= FL_MENU_DIVIDER;
    k++;
  }
  // Docked, floating in a window of its own, or floating inside the dock's
  // window: offer the other two.
  int floating = (d->owner() != d);
  int inset = 0;
  if (floating && d->window()) {
    inset = ((Fl_Dock_Window *)d->window())->inset();
  }
  int floatable = (p->dock_flags() & FL_DOCK_FLOATABLE);
  if (floatable && (!floating || inset)) {
    items[k].label("Float");
    items[k].argument(FLOAT);
    k++;
  }
  if (floatable && !inset && d->owner()->window()) {
    items[k].label("Float Inside");
    items[k].argument(INSET);
    k++;
  }
  if (floating) {
    items[k].label("Dock");
    items[k].argument(DOCK);
    k++;
  }
  if (p->dock_flags() & FL_DOCK_CLOSABLE) {
    items[k].label("Close");
    items[k].argument(CLOSE);
    k++;
  }
  const Fl_Menu_Item *m = items->popup(x() + w() - MENU_W, y() + tabs_height);
  long a = -1;
  if (m) a = m->argument();
  free(items);
  if (a < 0) return;
  Fl_Dock *owner = d->owner();
  if (a < n) {
    value(panel((int)a));
    owner->notify(panel((int)a), FL_REASON_SELECTED);
  } else if (a == FLOAT) {
    if (owner->float_panel(p)) owner->notify(p, FL_REASON_DRAGGED);
  } else if (a == INSET) {
    if (owner->inset_panel(p)) owner->notify(p, FL_REASON_DRAGGED);
  } else if (a == DOCK) {
    if (owner->dock_panel(p)) owner->notify(p, FL_REASON_DRAGGED);
  } else if (a == CLOSE) {
    if (p->can_close() && owner->close_panel(p)) owner->notify(p, FL_REASON_CLOSED);
  }
}

/** Handles the mouse on the tab strip; see the class description. */
int Fl_Dock_Tabs::handle(int event) {
  int ex = Fl::event_x();
  int ey = Fl::event_y();
  switch (event) {
    case FL_PUSH: {
      int t = tab_at(ex, ey);
      if (t == -2) break;
      if (Fl::event_button() != FL_LEFT_MOUSE) return 1;
      if (on_menu_box(ex, ey)) {
        show_menu();
        return 1;
      }
      return 1;
    }
    default:
      break;
  }
  return Fl_Group::handle(event);
}

// Draws the tab strip, and the line around the group.
void Fl_Dock_Tabs::draw_strip() {
  draw_box(box(), x(), y(), w(), tabs_height, color());
  int *ws = tab_widths();
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  int tx = x();
  for (int i = 0; i < children(); i++) {
    Fl_Dock_Panel *p = panel(i);
    int tw = ws[i];
    int sel = (i == value_);
    Fl_Color bg = color();
    if (sel) bg = fl_lighter(color());
    if (sel) draw_box(FL_UP_BOX, tx, y(), tw, tabs_height, bg);
    else draw_box(FL_THIN_UP_BOX, tx, y() + 2, tw, tabs_height - 2, bg);
    int lw = tw - 2 * TAB_PAD;
    if (active_r()) fl_color(labelcolor());
    else fl_color(fl_inactive(labelcolor()));
    if (lw < 0) lw = 0;
    fl_push_clip(tx + TAB_PAD, y(), lw, tabs_height);
    if (p->label()) fl_draw(p->label(), tx + TAB_PAD, y(), lw, tabs_height, FL_ALIGN_LEFT);
    fl_pop_clip();
    tx += tw;
  }
  free(ws);
  // The menu button: three bars.
  int mx = x() + w() - MENU_W / 2 - 5;
  int my = y() + tabs_height / 2;
  fl_color(labelcolor());
  for (int k = -1; k <= 1; k++) fl_rectf(mx, my + 4 * k - 1, 10, 2);
  // The line under the strip, broken by the shown tab.
  fl_color(fl_darker(color()));
  fl_xyline(x(), y() + tabs_height - 1, x() + w() - 1);
  // The line around the group, setting it apart from its neighbours.
  fl_color(fl_color_average(FL_FOREGROUND_COLOR, color(), 0.3f));
  for (int k = 0; k < border_size; k++) fl_rect(x() + k, y() + k, w() - 2 * k, h() - 2 * k);
}

/**
  Draws the tab strip and the shown panel.
*/
void Fl_Dock_Tabs::draw() {
  Fl_Widget *c = value();
  if (damage() & ~FL_DAMAGE_CHILD) {
    draw_strip();
    if (c) draw_child(*c);
    return;
  }
  if (c) update_child(*c);
}
