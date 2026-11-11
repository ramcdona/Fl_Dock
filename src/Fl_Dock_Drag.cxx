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

#include "Fl_Dock_Drag.H"
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock_Split.H>
#include <FL/Fl_Dock_Tabs.H>
#include <FL/Fl_Dock_Window.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_config.h>
#if defined(FLTK_USE_WAYLAND)
#  include <FL/platform.H>
#endif
#include <stdlib.h>

// The band along a dock's edge that docks beside the whole dock.
static const int EDGE_BAND = 24;
// The share a panel dropped beside a leaf, or beside the whole dock, takes.
static const double LEAF_SHARE = 0.5;
static const double EDGE_SHARE = 0.25;
// The width of the frame that shows where a panel would go.
static const int FRAME = 3;

static Fl_Dock_Tabs *drag_tabs = 0;     // the tab group dragged from

// What the drag started on.
static Fl_Widget *drag_source() {
  return drag_tabs;
}
static Fl_Dock_Panel *drag_panel = 0;   // the panel dragged, 0 for the whole group
static Fl_Dock_Target drag_target;
// The window the frame is in, and the frame's four windows.  They are on
// FLTK's watch list, which clears them if the window is deleted.
static Fl_Widget *frame_window = 0;
static Fl_Widget *frame[4] = { 0, 0, 0, 0 };

static void watch_frame() {
  static int watching = 0;
  if (watching) return;
  watching = 1;
  Fl::watch_widget_pointer(frame_window);
  for (int i = 0; i < 4; i++) Fl::watch_widget_pointer(frame[i]);
}

/** \internal Returns the dock w is in, or 0. */
Fl_Dock *Fl_Dock_Drag::dock_of(Fl_Widget *w) {
  for (Fl_Widget *p = w; p; p = p->parent()) {
    if (p->type() == FL_DOCK && p->as_group()) return (Fl_Dock *)p;
  }
  return 0;
}

/** \internal Returns 1 while a panel is being dragged. */
int Fl_Dock_Drag::active() {
  return drag_source() != 0;
}

static void hide_frame() {
  for (int i = 0; i < 4; i++) {
    if (frame[i]) {
      if (frame_window) ((Fl_Window *)frame_window)->remove(frame[i]);
      frame[i]->hide();
      Fl::delete_widget(frame[i]);
      frame[i] = 0;
    }
  }
  frame_window = 0;
}

// Shows the frame around X, Y, W, H in win.  The frame is four child windows,
// which stack above any other child, an OpenGL view included, on every
// platform and need neither a compositor nor placing a top-level window.
static void show_frame(Fl_Window *win, int X, int Y, int W, int H) {
  watch_frame();
  if (win != frame_window) {
    hide_frame();
    frame_window = win;
    Fl_Group *saved = Fl_Group::current();
    Fl_Group::current(0);
    for (int i = 0; i < 4; i++) {
      Fl_Window *f = new Fl_Window(0, 0, 1, 1);
      f->box(FL_FLAT_BOX);
      f->color(FL_SELECTION_COLOR);
      f->end();
      win->add(f);
      frame[i] = f;
    }
    Fl_Group::current(saved);
  }
  frame[0]->resize(X, Y, W, FRAME);
  frame[1]->resize(X, Y + H - FRAME, W, FRAME);
  frame[2]->resize(X, Y, FRAME, H);
  frame[3]->resize(X + W - FRAME, Y, FRAME, H);
  for (int i = 0; i < 4; i++) {
    if (win->shown()) frame[i]->show();
  }
}

/**
  \internal Starts dragging a panel, or the whole group if panel is 0.  Does
  nothing if what would be dragged cannot move.
*/
void Fl_Dock_Drag::begin(Fl_Dock_Tabs *tabs, Fl_Dock_Panel *panel) {
  if (panel) {
    if (!(panel->dock_flags() & FL_DOCK_MOVABLE)) return;
  } else {
    for (int i = 0; i < tabs->panels(); i++) {
      if (!(tabs->panel(i)->dock_flags() & FL_DOCK_MOVABLE)) return;
    }
  }
  drag_tabs = tabs;
  drag_panel = panel;
  drag_target.dock = 0;
  Fl::focus(tabs);
  if (tabs->window()) tabs->window()->cursor(FL_CURSOR_MOVE);
}

// Where window w is in its top-level window.
static void offset_in_top(Fl_Window *w, int &X, int &Y) {
  X = 0;
  Y = 0;
  while (w->parent()) {
    X += w->x();
    Y += w->y();
    w = w->window();
  }
}

// 1 if top-level windows know where they are on the screen, which they do
// everywhere but on Wayland.
static int global_positions() {
#if defined(FLTK_USE_WAYLAND)
  if (fl_wl_display()) return 0;
#endif
  return 1;
}

// Converts the mouse to coordinates in window win.  The window the drag
// started in has the event's own coordinates, and so, through their offsets,
// do the other windows in the same top-level window, such as inset windows.
// Other top-level windows need screen positions, which Wayland does not
// give, so there they are not used.
static int mouse_in(Fl_Window *src, Fl_Window *win, int &X, int &Y) {
  if (!win || !win->shown()) return 0;
  if (win == src) {
    X = Fl::event_x();
    Y = Fl::event_y();
  } else if (win->top_window() == src->top_window()) {
    int sx, sy, wx, wy;
    offset_in_top(src, sx, sy);
    offset_in_top(win, wx, wy);
    X = Fl::event_x() + sx - wx;
    Y = Fl::event_y() + sy - wy;
  } else {
    if (!global_positions()) return 0;
    X = Fl::event_x_root() - win->x_root();
    Y = Fl::event_y_root() - win->y_root();
  }
  return X >= 0 && Y >= 0 && X < win->w() && Y < win->h();
}

// The leaf of d's tree at X, Y.
static Fl_Widget *leaf_at(Fl_Dock *d, int X, int Y) {
  Fl_Widget *n = d->root();
  while (Fl_Dock::is_split(n)) {
    Fl_Group *g = (Fl_Group *)n;
    Fl_Widget *next = g->child(g->children() - 1);
    int horizontal = ((Fl_Dock_Split *)g)->horizontal();
    // A child owns the bar after it.
    for (int i = 0; i < g->children(); i++) {
      Fl_Widget *c = g->child(i);
      int end = c->y() + c->h() + Fl_Dock_Split::bar_size();
      int at = Y;
      if (horizontal) {
        end = c->x() + c->w() + Fl_Dock_Split::bar_size();
        at = X;
      }
      if (at < end) {
        next = c;
        break;
      }
    }
    n = next;
  }
  return n;
}

// The part of node's rectangle a node put on that side of it, taking that
// share, would get.
static void side_rect(Fl_Widget *node, int where, double share, int &X, int &Y, int &W, int &H) {
  X = node->x(); Y = node->y(); W = node->w(); H = node->h();
  int sw = (int)(node->w() * share);
  int sh = (int)(node->h() * share);
  if (where == FL_DOCK_LEFT) W = sw;
  else if (where == FL_DOCK_RIGHT) { X += node->w() - sw; W = sw; }
  else if (where == FL_DOCK_TOP) H = sh;
  else if (where == FL_DOCK_BOTTOM) { Y += node->h() - sh; H = sh; }
}

/**
  \internal Finds where a panel dragged from source would go if dropped at X,
  Y in dock d's window, as Lumino's DockPanel does: a band along the dock's
  edge docks beside the whole dock; over a tab strip it becomes a tab there;
  over a leaf, the outer third on any side docks beside it, and the middle
  makes it a tab.  A drop that would change nothing is no target.
  \return 1 and the target in t, or 0
*/
int Fl_Dock_Drag::hit(Fl_Dock *d, int X, int Y, Fl_Dock_Tabs *source, Fl_Dock_Panel *panel, Fl_Dock_Target &t) {
  t.dock = d;
  t.node = 0;
  t.index = -1;
  t.fraction = EDGE_SHARE;
  if (X < d->x() || Y < d->y() || X >= d->x() + d->w() || Y >= d->y() + d->h()) return 0;
  int alone = (!panel || (source && source->panels() == 1));
  if (!d->root()) {
    t.where = FL_DOCK_LEFT;
    t.x = d->x(); t.y = d->y(); t.w = d->w(); t.h = d->h();
    return 1;
  }
  Fl_Widget *leaf = leaf_at(d, X, Y);
  if (!leaf) return 0;
  int is_tabs = Fl_Dock::is_tabs(leaf);
  // A tab strip: a tab at the place under the mouse.  It comes before the
  // dock's edges, which a strip along the top would otherwise lie under.
  if (is_tabs && Y < leaf->y() + Fl_Dock_Tabs::tab_height()) {
    Fl_Dock_Tabs *lt = (Fl_Dock_Tabs *)leaf;
    if (leaf == source && alone) return 0;
    t.node = leaf;
    t.fraction = LEAF_SHARE;
    t.where = FL_DOCK_TAB;
    t.index = lt->tab_insert_index(X);
    t.x = leaf->x(); t.y = leaf->y(); t.w = leaf->w(); t.h = leaf->h();
    return 1;
  }
  // The dock's own edges, unless the dragged group is all there is.
  int edge = -1;
  if (X < d->x() + EDGE_BAND) edge = FL_DOCK_LEFT;
  else if (X >= d->x() + d->w() - EDGE_BAND) edge = FL_DOCK_RIGHT;
  else if (Y < d->y() + EDGE_BAND) edge = FL_DOCK_TOP;
  else if (Y >= d->y() + d->h() - EDGE_BAND) edge = FL_DOCK_BOTTOM;
  if (edge >= 0 && !(alone && d->root() == source)) {
    t.where = edge;
    side_rect(d, edge, EDGE_SHARE, t.x, t.y, t.w, t.h);
    return 1;
  }
  t.node = leaf;
  t.fraction = LEAF_SHARE;
  int lw = leaf->w();
  int lh = leaf->h();
  if (lw < 1) lw = 1;
  if (lh < 1) lh = 1;
  double fx = (double)(X - leaf->x()) / lw;
  double fy = (double)(Y - leaf->y()) / lh;
  double dl = fx, dr = 1 - fx, dt = fy, db = 1 - fy;
  int side = FL_DOCK_LEFT;
  double m = dl;
  if (dr < m) { m = dr; side = FL_DOCK_RIGHT; }
  if (dt < m) { m = dt; side = FL_DOCK_TOP; }
  if (db < m) { m = db; side = FL_DOCK_BOTTOM; }
  if (m >= 1.0 / 3 && is_tabs) {
    if (leaf == source) return 0;
    t.where = FL_DOCK_TAB;
    t.index = ((Fl_Dock_Tabs *)leaf)->panels();
    t.x = leaf->x(); t.y = leaf->y(); t.w = leaf->w(); t.h = leaf->h();
    return 1;
  }
  // Beside the group it came from changes nothing when it is all of the group.
  if (leaf == source && alone) return 0;
  t.where = side;
  side_rect(leaf, side, LEAF_SHARE, t.x, t.y, t.w, t.h);
  return 1;
}

/** \internal Follows the mouse: finds the target under it and shows where the panel would go. */
void Fl_Dock_Drag::drag() {
  Fl_Widget *source = drag_source();
  if (!source) return;
  Fl_Dock *src_dock = dock_of(source);
  Fl_Window *src = source->window();
  if (!src_dock || !src) { cancel(); return; }
  Fl_Dock *o = src_dock->owner();
  drag_target.dock = 0;
  // The windows in the order they lie on the screen, top first: the inset
  // windows, over the main dock, then the floating windows, which stay
  // above the main window, each last made on top; then the main dock.
  int nw = o->windows();
  Fl_Dock **docks = (Fl_Dock **)malloc(sizeof(Fl_Dock *) * (nw + 1));
  int n = 0;
  for (int i = nw - 1; i >= 0; i--) {
    if (o->window_at(i)->inset()) docks[n++] = o->window_at(i)->dock();
  }
  for (int i = nw - 1; i >= 0; i--) {
    if (!o->window_at(i)->inset()) docks[n++] = o->window_at(i)->dock();
  }
  docks[n++] = o;
  for (int i = 0; i < n; i++) {
    Fl_Dock *d = docks[i];
    Fl_Window *win = d->window();
    int X, Y;
    if (!mouse_in(src, win, X, Y)) continue;
    Fl_Dock_Target t;
    if (hit(d, X, Y, drag_tabs, drag_panel, t)) {
      drag_target = t;
      show_frame(win, t.x, t.y, t.w, t.h);
      free(docks);
      return;
    }
    // Over a window with no target: nowhere, not a window behind it.
    break;
  }
  free(docks);
  hide_frame();
}

/**
  \internal Moves the panel, or the whole group, to target t.
  \return 1 if anything moved
*/
int Fl_Dock_Drag::drop(Fl_Dock_Tabs *source, Fl_Dock_Panel *panel, const Fl_Dock_Target &t) {
  Fl_Dock *d = t.dock;
  if (!d) return 0;
  int max = 1;
  if (!panel) max = source->panels() + 1;
  Fl_Dock_Panel **list = (Fl_Dock_Panel **)malloc(sizeof(Fl_Dock_Panel *) * max);
  int n = 0;
  if (panel) list[n++] = panel;
  else {
    for (int i = 0; i < source->panels(); i++) list[n++] = source->panel(i);
  }
  int r = drop_list(source, panel, list, n, t);
  free(list);
  return r;
}

// Moves the n panels of list to target t.
int Fl_Dock_Drag::drop_list(Fl_Dock_Tabs *source, Fl_Dock_Panel *panel, Fl_Dock_Panel **list, int n,
                            const Fl_Dock_Target &t) {
  Fl_Dock *d = t.dock;
  Fl_Dock_Panel *shown = 0;
  if (source) shown = source->value();
  if (t.where == FL_DOCK_TAB) {
    Fl_Dock_Tabs *target = (Fl_Dock_Tabs *)t.node;
    int index = t.index;
    for (int i = 0; i < n; i++) {
      Fl_Dock_Panel *p = list[i];
      if (p->parent() == target) {
        // Moving within the strip: the index counts the tab being moved.
        int old = target->find(p);
        if (old < index) index--;
        target->remove_panel(p);
        target->insert_panel(p, index);
      } else {
        Fl_Dock *from = p->dock();
        if (from) from->take(p);
        target->insert_panel(p, index);
      }
      index++;
    }
    if (!panel && shown) target->value(shown);
    d->fit_root();
    d->redraw();
    return 1;
  }
  // add_panel() takes the node beside which to dock by naming a panel in it, the
  // center, or 0 for the whole dock; it finds the node before moving anything.
  Fl_Widget *ref = t.node;
  if (Fl_Dock::is_tabs(ref)) ref = ((Fl_Dock_Tabs *)ref)->panel(0);
  if (!d->add_panel(list[0], (Fl_Dock_Where)t.where, ref, t.fraction)) return 0;
  for (int i = 1; i < n; i++) d->add_panel(list[i], FL_DOCK_TAB, list[0]);
  if (!panel && shown && Fl_Dock::is_tabs(shown->parent())) ((Fl_Dock_Tabs *)shown->parent())->value(shown);
  return 1;
}

/** \internal Ends the drag where the mouse is: docks the panel there, floats it outside every window, or does nothing. */
void Fl_Dock_Drag::release() {
  Fl_Widget *source = drag_source();
  if (!source) return;
  Fl_Dock_Tabs *tabs = drag_tabs;
  Fl_Dock_Panel *panel = drag_panel;
  Fl_Dock_Target t = drag_target;
  Fl_Window *src = source->window();
  Fl_Dock *src_dock = dock_of(source);
  drag_tabs = 0;
  drag_panel = 0;
  hide_frame();
  if (src) src->cursor(FL_CURSOR_DEFAULT);
  if (!src_dock) return;
  Fl_Dock *o = src_dock->owner();
  Fl_Dock_Panel *notify_panel = panel;
  if (!notify_panel && tabs) notify_panel = tabs->value();
  if (t.dock) {
    if (drop(tabs, panel, t)) o->notify(notify_panel, FL_REASON_DRAGGED);
    return;
  }
  // Released outside the drag's top-level window, over no dock: float, if
  // it may and it is not floating already.
  if (src) {
    Fl_Window *top = src->top_window();
    int X, Y;
    if (mouse_in(src, top, X, Y)) return;
  }
  Fl_Dock_Panel *p = panel;
  if (!p) p = tabs->value();
  if (!p || !(p->dock_flags() & FL_DOCK_FLOATABLE)) return;
  Fl_Dock_Window *from = 0;
  if (src_dock != o) from = (Fl_Dock_Window *)src_dock->window();
  if (from && !from->inset()) return;
  Fl_Dock_Window *win = o->float_panel(p, Fl::event_x_root() - p->w() / 2, Fl::event_y_root() - 10);
  if (!win) return;
  if (!panel) {
    // The rest of the group goes with it, as tabs.
    while (tabs->panels() > 0 && Fl_Dock_Drag::dock_of(tabs) == src_dock) {
      Fl_Dock_Panel *q = tabs->panel(0);
      if (q->dock_flags() & FL_DOCK_FLOATABLE) win->dock()->add_panel(q, FL_DOCK_TAB, p);
      else break;
    }
  }
  o->notify(p, FL_REASON_DRAGGED);
}

/** \internal Ends the drag without moving anything. */
void Fl_Dock_Drag::cancel() {
  Fl_Widget *source = drag_source();
  if (source && source->window()) source->window()->cursor(FL_CURSOR_DEFAULT);
  drag_tabs = 0;
  drag_panel = 0;
  drag_target.dock = 0;
  hide_frame();
}

/** \internal Ends a drag that moves panel p, which something else is moving. */
void Fl_Dock_Drag::forget(Fl_Dock_Panel *p) {
  if (!drag_source()) return;
  if (drag_panel == p || (drag_tabs && p->parent() == drag_tabs)) cancel();
}

/** \internal Ends a drag that started on w, a tab group being deleted. */
void Fl_Dock_Drag::source_gone(Fl_Widget *w) {
  if (!w || w != drag_tabs) return;
  drag_tabs = 0;
  drag_panel = 0;
  drag_target.dock = 0;
  hide_frame();
}

/** \internal Tells the application that the user selected a tab. */
void Fl_Dock_Drag::notify_selected(Fl_Dock_Tabs *tabs, Fl_Dock_Panel *p) {
  Fl_Dock *d = dock_of(tabs);
  if (d) d->owner()->notify(p, FL_REASON_SELECTED);
}

/** \internal Closes a panel the user asked to close, if it agrees. */
void Fl_Dock_Drag::close(Fl_Dock_Tabs *tabs, Fl_Dock_Panel *p) {
  Fl_Dock *d = dock_of(tabs);
  if (!d || !p->can_close()) return;
  Fl_Dock *o = d->owner();
  if (o->close_panel(p)) o->notify(p, FL_REASON_CLOSED);
}
