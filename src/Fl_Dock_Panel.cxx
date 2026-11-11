//
// Dock panel widget for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock.H>
#include <FL/fl_string_functions.h>
#include <stdlib.h>
#include <string.h>

/**
  Creates a panel.  Like any group, it becomes the current group, so the
  widgets created after it are its children until end() is called.
  \param[in] X, Y, W, H position and size
  \param[in] L the label, the title on the panel's tab, which is not copied,
    as for any widget; it is also the panel's name unless name() is set
*/
Fl_Dock_Panel::Fl_Dock_Panel(int X, int Y, int W, int H, const char *L)
  : Fl_Group(X, Y, W, H, L) {
  type(FL_DOCK_PANEL);
  box(FL_FLAT_BOX);
  name_ = 0;
  dock_flags_ = FL_DOCK_DEFAULT_FLAGS;
  home_ref_ = 0;
  home_where_ = FL_DOCK_LEFT;
  home_fraction_ = 0.25;
  home_relative_ = 0;
  home_ref2_ = 0;
  home_where2_ = FL_DOCK_LEFT;
  home_edge_ = 0;
  home_span_ = 0;
  for (int i = 0; i < 4; i++) {
    inset_rect_[i] = 0;
    float_rect_[i] = 0;
  }
}

/**
  Sets where the panel's inset window goes when the panel is next shown in
  one: X and Y relative to the dock.  Fl_Dock sets it when the panel leaves
  an inset window.
*/
void Fl_Dock_Panel::inset_rect(int X, int Y, int W, int H) {
  inset_rect_[0] = X;
  inset_rect_[1] = Y;
  inset_rect_[2] = W;
  inset_rect_[3] = H;
}

/** Gets where the panel's inset window goes; returns 0 if it has none. */
int Fl_Dock_Panel::get_inset_rect(int &X, int &Y, int &W, int &H) const {
  X = inset_rect_[0];
  Y = inset_rect_[1];
  W = inset_rect_[2];
  H = inset_rect_[3];
  return W > 0;
}

/**
  Sets where the panel's floating window goes when the panel is next shown in
  one, on the screen.  Fl_Dock sets it when the panel leaves a floating window.
*/
void Fl_Dock_Panel::float_rect(int X, int Y, int W, int H) {
  float_rect_[0] = X;
  float_rect_[1] = Y;
  float_rect_[2] = W;
  float_rect_[3] = H;
}

/** Gets where the panel's floating window goes; returns 0 if it has none. */
int Fl_Dock_Panel::get_float_rect(int &X, int &Y, int &W, int &H) const {
  X = float_rect_[0];
  Y = float_rect_[1];
  W = float_rect_[2];
  H = float_rect_[3];
  return W > 0;
}

/**
  Deletes the panel, taking it out of its dock first, as Fl_Dock::remove_panel()
  does.
*/
Fl_Dock_Panel::~Fl_Dock_Panel() {
  Fl_Dock *d = dock();
  if (d) {
    Fl_Dock *o = d->owner();
    // A dock deleting its panels forgets them itself.
    if (!o->destroying_ && !d->destroying_) o->remove_panel(this);
  }
  free(name_);
  free(home_ref_);
  free(home_ref2_);
  free(home_span_);
}

/**
  Returns the panel's name, which identifies it in saved layouts: the one
  set with name(const char *), or else its label.  A dock takes the label as
  the name when it is given the panel, so the name stays the same if the
  label changes later, as when it is translated.
*/
const char *Fl_Dock_Panel::name() const {
  if (name_) return name_;
  return label();
}

/**
  Sets the panel's name, which must not be empty and must be unique in its
  dock, and should be the same from one run of the application to the next.
  It is copied.  A name a panel of the same dock has is not taken.
*/
void Fl_Dock_Panel::name(const char *n) {
  if (!n || !n[0]) return;
  Fl_Dock *d = dock();
  if (d) {
    Fl_Dock_Panel *q = d->find(n);
    if (q && q != this) return;
  }
  char *s = fl_strdup(n);
  free(name_);
  name_ = s;
}

// Keeps the name the panel has now, its label if it has none of its own.
void Fl_Dock_Panel::fix_name() {
  if (!name_ && label()) name_ = fl_strdup(label());
}

/** Returns the dock the panel belongs to, docked or closed, or 0. */
Fl_Dock *Fl_Dock_Panel::dock() const {
  for (Fl_Widget *w = parent(); w; w = w->parent()) {
    if (w->type() == FL_DOCK && w->as_group()) return (Fl_Dock *)w;
  }
  return 0;
}

/** Returns 1 if the panel is in a tab group of a dock, 0 if it is closed or was never docked. */
int Fl_Dock_Panel::docked() const {
  return Fl_Dock::is_tabs(parent());
}

/**
  \internal Sets where the panel goes back to when it is shown again: beside
  or as a tab of the panel named ref, beside the center if ref is "", or
  beside the whole dock if ref is 0.
*/
void Fl_Dock_Panel::home(const char *ref, int where, double fraction, int relative) {
  home_relative_ = relative;
  free(home_ref2_);
  home_ref2_ = 0;
  free(home_ref_);
  home_ref_ = 0;
  if (ref) home_ref_ = fl_strdup(ref);
  home_where_ = where;
  home_edge(0);
  if (fraction >= 0 && fraction < 1) home_fraction_ = fraction;     // 0: the panel's own size
}

/**
  \internal Sets the neighbour on the other side of the panel's home, which
  it goes back beside, on side where, if home_ref() is no longer docked.
  home() clears it.
*/
void Fl_Dock_Panel::home2(const char *ref, int where) {
  free(home_ref2_);
  home_ref2_ = 0;
  if (ref) home_ref2_ = fl_strdup(ref);
  home_where2_ = where;
}

/**
  \internal Sets whether the panel goes back to the end of its split, and
  span, the panel or "" for the center at the far end of the part of the
  split beside it: back, it spans that whole part again.  home() clears it.
*/
void Fl_Dock_Panel::home_edge(int e, const char *span) {
  home_edge_ = e;
  free(home_span_);
  home_span_ = 0;
  if (e && span) home_span_ = fl_strdup(span);
}
