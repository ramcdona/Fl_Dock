//
// Floating dock window for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#include <FL/Fl_Dock_Window.H>
#include <FL/Fl_Dock.H>
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock_Split.H>
#include <FL/Fl_Dock_Tabs.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>

// An inset window's frame.  Its tab strips are its title bar.
static const int FRAME = 3;
static const int GRIP = 8;          // the corner that resizes both ways
static const int MIN_W = 100;
static const int MIN_H = 60;

// What a press does: move the window, or size it by one edge or two.
enum { NONE = 0, SIZE_LEFT = 1, SIZE_RIGHT = 2, SIZE_TOP = 4, SIZE_BOTTOM = 8, MOVE = 16 };

/** Returns the width of the frame around an inset window. */
int Fl_Dock_Window::frame_size() { return FRAME; }

// Creates a window for panels of the dock owner: a floating one, at X, Y on
// the screen, or an inset one inside the owner's window, at X, Y
// in that window.
Fl_Dock_Window::Fl_Dock_Window(int X, int Y, int W, int H, Fl_Dock *owner, Kind kind)
  : Fl_Double_Window(X, Y, W, H) {
  owner_ = owner;
  kind_ = kind;
  action_ = NONE;
  press_x_ = press_y_ = press_w_ = press_h_ = press_mx_ = press_my_ = 0;
  if (kind_ != FLOATING) dock_ = new Fl_Dock(FRAME, FRAME, W - 2 * FRAME, H - 2 * FRAME);
  else dock_ = new Fl_Dock(0, 0, W, H);
  dock_->owner_ = owner;
  end();
  resizable(dock_);
  callback(close_cb, this);
  if (kind_ != FLOATING) {
    box(FL_UP_BOX);
  } else {
    set_non_modal();
  }
}

Fl_Dock_Window::~Fl_Dock_Window() {
  // The dock goes first, while it still belongs to the owner, so the owner
  // forgets its panels.
  if (dock_) {
    Fl_Dock *d = dock_;
    dock_ = 0;
    delete d;
  }
  // An inset window may go with the window it is in, before its owner.
  if (owner_) {
    owner_->remove_window(this);
  }
}

// Closing the window closes its panels, or docks those that cannot be
// closed; the window goes when its last panel leaves.
void Fl_Dock_Window::close_cb(Fl_Widget *, void *data) {
  Fl_Dock_Window *win = (Fl_Dock_Window *)data;
  Fl_Dock *owner = win->owner_;
  if (!owner) return;
  Fl_Dock *d = win->dock_;
  for (;;) {
    if (d->panels() == 0) break;
    Fl_Dock_Panel *p = d->panel(0);
    if ((p->dock_flags() & FL_DOCK_CLOSABLE) && p->can_close()) {
      owner->close_panel(p);
      owner->notify(p, FL_REASON_CLOSED);
    } else {
      owner->dock_panel(p);
      owner->notify(p, FL_REASON_DRAGGED);
    }
    // The window was deleted with its last panel.
    if (owner->windows() == 0) break;
    int still = 0;
    for (int i = 0; i < owner->windows(); i++) {
      if (owner->window_at(i) == win) still = 1;
    }
    if (!still) break;
  }
}

/**
  Keeps an inset window inside its owner's area: moved back in, and made
  smaller if it does not fit.
*/
void Fl_Dock_Window::keep_inside() {
  if (kind_ == FLOATING || !owner_) return;
  int ax, ay, aw, ah;
  owner_->area(ax, ay, aw, ah);
  int W = w();
  int H = h();
  if (W > aw) W = aw;
  if (H > ah) H = ah;
  int X = x();
  int Y = y();
  if (X + W > ax + aw) X = ax + aw - W;
  if (Y + H > ay + ah) Y = ay + ah - H;
  if (X < ax) X = ax;
  if (Y < ay) Y = ay;
  if (X != x() || Y != y() || W != w() || H != h()) resize(X, Y, W, H);
}

// The tab group under X, Y in node, or 0.
static Fl_Dock_Tabs *tabs_at(Fl_Widget *node, int X, int Y) {
  if (!node || X < node->x() || Y < node->y() || X >= node->x() + node->w() || Y >= node->y() + node->h())
    return 0;
  if (Fl_Dock::is_tabs(node)) return (Fl_Dock_Tabs *)node;
  if (!Fl_Dock::is_split(node)) return 0;
  Fl_Group *g = (Fl_Group *)node;
  for (int i = 0; i < g->children(); i++) {
    Fl_Dock_Tabs *t = tabs_at(g->child(i), X, Y);
    if (t) return t;
  }
  return 0;
}

// What a press at X, Y in the window does: move it or size it.  An inset
// window sizes from any edge, and from both edges near a corner, and moves
// by the part of a tab strip beside its tabs.
int Fl_Dock_Window::action_at(int X, int Y) const {
  if (kind_ == FLOATING) return NONE;
  if (X >= w() - FRAME - GRIP && Y >= h() - FRAME - GRIP) return SIZE_RIGHT | SIZE_BOTTOM;
  int a = NONE;
  if (X < FRAME) a |= SIZE_LEFT;
  if (X >= w() - FRAME) a |= SIZE_RIGHT;
  if (Y < FRAME) a |= SIZE_TOP;
  if (Y >= h() - FRAME) a |= SIZE_BOTTOM;
  if (a & (SIZE_LEFT | SIZE_RIGHT)) {
    if (Y < FRAME + GRIP) a |= SIZE_TOP;
    if (Y >= h() - FRAME - GRIP) a |= SIZE_BOTTOM;
  }
  if (a & (SIZE_TOP | SIZE_BOTTOM)) {
    if (X < FRAME + GRIP) a |= SIZE_LEFT;
    if (X >= w() - FRAME - GRIP) a |= SIZE_RIGHT;
  }
  if (a != NONE) return a;
  Fl_Dock_Tabs *t = tabs_at(dock_->root(), X, Y);
  if (t && t->on_strip_space(X, Y)) return MOVE;
  return NONE;
}

// The mouse in the coordinates of the window the inset window is in, which
// stay put while the inset window moves under the mouse.
void Fl_Dock_Window::parent_mouse(int &X, int &Y) const {
  X = Fl::event_x() + x();
  Y = Fl::event_y() + y();
}

/** Lets the user move and resize the window by its frame; see the class description. */
int Fl_Dock_Window::handle(int event) {
  if (kind_ == FLOATING) return Fl_Double_Window::handle(event);
  switch (event) {
    case FL_MOVE:
    case FL_ENTER: {
      int a = action_at(Fl::event_x(), Fl::event_y());
      Fl_Cursor c = FL_CURSOR_DEFAULT;
      if (a == (SIZE_LEFT | SIZE_TOP) || a == (SIZE_RIGHT | SIZE_BOTTOM)) c = FL_CURSOR_NWSE;
      else if (a == (SIZE_RIGHT | SIZE_TOP) || a == (SIZE_LEFT | SIZE_BOTTOM)) c = FL_CURSOR_NESW;
      else if (a & (SIZE_LEFT | SIZE_RIGHT)) c = FL_CURSOR_WE;
      else if (a & (SIZE_TOP | SIZE_BOTTOM)) c = FL_CURSOR_NS;
      if (a != NONE || action_ == NONE) cursor(c);
      if (a != NONE) return 1;
      break;
    }
    case FL_LEAVE:
      if (action_ == NONE) cursor(FL_CURSOR_DEFAULT);
      break;
    case FL_PUSH: {
      int a = action_at(Fl::event_x(), Fl::event_y());
      if (a == NONE || Fl::event_button() != FL_LEFT_MOUSE) break;
      action_ = a;
      press_x_ = x();
      press_y_ = y();
      press_w_ = w();
      press_h_ = h();
      parent_mouse(press_mx_, press_my_);
      return 1;
    }
    case FL_DRAG: {
      if (action_ == NONE) break;
      int mx, my;
      parent_mouse(mx, my);
      int dx = mx - press_mx_;
      int dy = my - press_my_;
      int X = press_x_;
      int Y = press_y_;
      int W = press_w_;
      int H = press_h_;
      if (action_ == MOVE) {
        X += dx;
        Y += dy;
      }
      if (action_ & SIZE_RIGHT) W += dx;
      if (action_ & SIZE_BOTTOM) H += dy;
      if (action_ & SIZE_LEFT) {
        X += dx;
        W -= dx;
      }
      if (action_ & SIZE_TOP) {
        Y += dy;
        H -= dy;
      }
      if (W < MIN_W) {
        if (action_ & SIZE_LEFT) X -= MIN_W - W;
        W = MIN_W;
      }
      if (H < MIN_H) {
        if (action_ & SIZE_TOP) Y -= MIN_H - H;
        H = MIN_H;
      }
      // A left or top edge stops at the dock's area; the opposite edge stays.
      if (owner_ && action_ != MOVE) {
        int ax, ay, aw, ah;
        owner_->area(ax, ay, aw, ah);
        if (X < ax) {
          W -= ax - X;
          X = ax;
        }
        if (Y < ay) {
          H -= ay - Y;
          Y = ay;
        }
      }
      resize(X, Y, W, H);
      keep_inside();
      if (owner_) {
        Fl_Callback_Reason r = FL_REASON_CHANGED;
        if (action_ == MOVE) r = FL_REASON_DRAGGED;
        owner_->notify(dock_->panel(0), r, 1);
      }
      return 1;
    }
    case FL_RELEASE: {
      int a = action_;
      action_ = NONE;
      if (a == NONE) break;
      if (owner_) {
        Fl_Dock_Panel *p = dock_->panel(0);
        // Moving the window moves its panels; sizing it changes them.
        Fl_Callback_Reason r = FL_REASON_CHANGED;
        if (a == MOVE) r = FL_REASON_DRAGGED;
        owner_->notify(p, r, 2);
      }
      return 1;
    }
    default:
      break;
  }
  return Fl_Double_Window::handle(event);
}

/** Draws the window, and the grip in the corner of an inset window. */
void Fl_Dock_Window::draw() {
  Fl_Double_Window::draw();
  // A grip in the corner that sizes an inset window.
  if (kind_ != INSET) return;
  fl_color(fl_darker(color()));
  for (int k = 0; k < 3; k++) {
    int o = 3 + 3 * k;
    fl_line(w() - FRAME - o, h() - FRAME - 1, w() - FRAME - 1, h() - FRAME - o);
  }
}
