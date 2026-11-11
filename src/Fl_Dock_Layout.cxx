//
// Saving and restoring dock layouts for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#include <FL/Fl_Dock.H>
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock_Split.H>
#include <FL/Fl_Dock_Tabs.H>
#include <FL/Fl_Dock_Window.H>
#include <FL/Fl.H>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/*
  A layout is text, one node per line, indented one space per level, after a
  version line:

    fl_dock 1
    dock
     split h 0.25 0.75
      tabs 0 files
      split v 0.7 0.3
       center
       tabs 1 props log
    window 120 80 300 400
     tabs 0 notes
    inset 40 30 250 200
     tabs 0 find
    closed help find
    size 800 575

  "split h" lays its children side by side and "split v" one above another,
  each with its share; "tabs" gives the index of the tab shown, then the
  panels' names; "center" is the dock's center.  Each "window" is a floating
  window at x y w h on the screen, and each "inset" an inset window at x y w h
  in the dock.  "closed" lists the closed panels.
  "size" is the size of the dock's area, so that panels keep their sizes
  when the layout is restored in a dock of another size.
  Names are encoded so they hold no spaces.  Lines with a
  keyword this version does not know are ignored, with their children.
*/

static const int LAYOUT_VERSION = 1;

// Appends to a buffer that may be too small, counting what it would need.
struct Layout_Writer {
  char *buf;
  int size;
  int len;
  void put(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *at = 0;
    int room = 0;
    if (buf && len < size) {
      at = buf + len;
      room = size - len;
    }
    int n = vsnprintf(at, room, fmt, ap);
    va_end(ap);
    if (n > 0) len += n;
  }
  // A name, with spaces, control characters and '%' as %XX.
  void name(const char *s) {
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
      if (*c <= ' ' || *c == '%' || *c == 127) put("%%%02X", *c);
      else put("%c", *c);
    }
  }
};

// 1 if node writes anything: a tab group with panels, the center, or a
// split holding one of those.
static int writes(Fl_Widget *node) {
  if (Fl_Dock::is_tabs(node)) return ((Fl_Group *)node)->children() > 0;
  if (Fl_Dock::is_split(node)) {
    Fl_Group *g = (Fl_Group *)node;
    for (int i = 0; i < g->children(); i++) {
      if (writes(g->child(i))) return 1;
    }
    return 0;
  }
  return 1;
}

// Writes the tree at node.  A tree not yet put back in normal form, as
// while a deleted widget is taken out, is written as if it were.
static void write_node(Layout_Writer &w, Fl_Widget *node, int depth) {
  if (Fl_Dock::is_split(node)) {
    Fl_Dock_Split *s = (Fl_Dock_Split *)node;
    int n = 0;
    int only = -1;
    double sum = 0;
    for (int i = 0; i < s->children(); i++) {
      if (!writes(s->child(i))) continue;
      n++;
      only = i;
      sum += s->weight(i);
    }
    if (n == 0) return;
    if (n == 1) {
      write_node(w, s->child(only), depth);
      return;
    }
    // The shares are written as they are unless a child is left out, so a
    // layout read back is written the same.
    if (!(sum > 0) || n == s->children()) sum = 1;
    for (int i = 0; i < depth; i++) w.put(" ");
    if (s->horizontal()) w.put("split h");
    else w.put("split v");
    for (int i = 0; i < s->children(); i++) {
      if (!writes(s->child(i))) continue;
      double wt = s->weight(i) / sum;
      if (!(wt > 0)) wt = 1e-6;
      w.put(" %.6g", wt);
    }
    w.put("\n");
    for (int i = 0; i < s->children(); i++) {
      if (writes(s->child(i))) write_node(w, s->child(i), depth + 1);
    }
    return;
  }
  if (!writes(node)) return;
  for (int i = 0; i < depth; i++) w.put(" ");
  if (Fl_Dock::is_tabs(node)) {
    Fl_Dock_Tabs *t = (Fl_Dock_Tabs *)node;
    int sel = t->find(t->value());
    if (sel >= t->panels()) sel = 0;
    w.put("tabs %d", sel);
    for (int i = 0; i < t->panels(); i++) {
      w.put(" ");
      w.name(t->panel(i)->name());
    }
    w.put("\n");
  } else {
    w.put("center\n");
  }
}

/**
  Writes the layout of this dock, its floating windows and its closed panels
  into buf, as text ending in a nul, cutting it short if size is too small.
  \return the length of the whole text, without the nul; if it is size or
    more the text was cut short, and buf needs to be that plus one long
*/
int Fl_Dock::write_layout(char *buf, int size) const {
  const Fl_Dock *o = this;
  if (owner_) o = owner_;
  Layout_Writer w;
  w.buf = buf;
  w.size = size;
  w.len = 0;
  if (buf && size > 0) buf[0] = 0;
  w.put("fl_dock %d\n", LAYOUT_VERSION);
  w.put("dock\n");
  if (o->root_) write_node(w, o->root_, 1);
  for (int i = 0; i < o->nwindows_; i++) {
    Fl_Dock_Window *win = o->windows_[i];
    if (!win->dock()->root() || !writes(win->dock()->root())) continue;
    if (win->inset()) w.put("inset %d %d %d %d\n", win->x() - o->x(), win->y() - o->y(), win->w(), win->h());
    else w.put("window %d %d %d %d\n", win->x(), win->y(), win->w(), win->h());
    write_node(w, win->dock()->root(), 1);
  }
  int nclosed = o->store_->children();
  if (nclosed > 0) {
    w.put("closed");
    for (int i = 0; i < nclosed; i++) {
      w.put(" ");
      w.name(((Fl_Dock_Panel *)o->store_->child(i))->name());
    }
    w.put("\n");
  }
  int ax, ay, aw, ah;
  o->area(ax, ay, aw, ah);
  w.put("size %d %d\n", aw, ah);
  return w.len;
}

