//
// Dock split widget for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#include <FL/Fl_Dock_Split.H>
#include <FL/Fl_Dock.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <stdlib.h>
#include <string.h>

static int split_bar_size = 6;
static int split_min_size = 40;
// The share of a split's room the child holding the center keeps before the
// others are made smaller.
static const double CENTER_MIN_SHARE = 1.0 / 3;

Fl_Widget *Fl_Dock_Split::keep_center_ = 0;

/**
  Creates an empty split.
  \param[in] X, Y, W, H position and size
  \param[in] horizontal 1 to lay children out side by side, 0 one above another
*/
Fl_Dock_Split::Fl_Dock_Split(int X, int Y, int W, int H, int horizontal)
  : Fl_Group(X, Y, W, H) {
  type(FL_DOCK_SPLIT);
  box(FL_FLAT_BOX);
  horizontal_ = horizontal;
  weights_ = 0;
  nweights_ = 0;
  dragging_ = -1;
  drag_offset_ = 0;
  end();
}

Fl_Dock_Split::~Fl_Dock_Split() {
  free(weights_);
}

/** Returns the width, or height, of the bar between two children. */
int Fl_Dock_Split::bar_size() { return split_bar_size; }

/** Sets the width, or height, of the bar between two children for all splits. */
void Fl_Dock_Split::bar_size(int s) { if (s > 0) split_bar_size = s; }

/** Returns the smallest width, or height, a child is dragged to. */
int Fl_Dock_Split::min_size() { return split_min_size; }

/** Sets the smallest width, or height, a child is dragged to for all splits. */
void Fl_Dock_Split::min_size(int s) { if (s >= 0) split_min_size = s; }

// Keeps one weight per child: a child added without one gets an equal share.
void Fl_Dock_Split::sync_weights() {
  int n = children();
  if (n == nweights_) return;
  double *w = (double *)malloc(sizeof(double) * (n + 1));
  for (int i = 0; i < n; i++) {
    if (i < nweights_) w[i] = weights_[i];
    else w[i] = 1.0 / n;
  }
  free(weights_);
  weights_ = w;
  nweights_ = n;
  double sum = 0;
  for (int i = 0; i < n; i++) sum += weights_[i];
  if (sum > 0) {
    for (int i = 0; i < n; i++) weights_[i] /= sum;
  }
}

/** Returns child i's share of the space. */
double Fl_Dock_Split::weight(int i) const {
  if (i < 0 || i >= nweights_) return 0;
  return weights_[i];
}

/**
  Sets child i's share of the space.  The other weights are not changed, so
  the caller keeps the sum at 1.
*/
void Fl_Dock_Split::weight(int i, double w) {
  sync_weights();
  if (i < 0 || i >= nweights_ || !(w > 0)) return;
  weights_[i] = w;
}

/**
  Inserts a node before child index with the given share.  The share is
  taken from child from if it is given and has room for it, and otherwise
  from all the other children in proportion.
*/
void Fl_Dock_Split::insert_node(Fl_Widget *w, int index, double weight, Fl_Widget *from) {
  sync_weights();
  if (index < 0) index = 0;
  if (index > children()) index = children();
  if (!(weight > 0) || weight >= 1) weight = 1.0 / (children() + 1);
  int fi = -1;
  if (from) {
    fi = find(from);
    if (fi >= children() || !(weights_[fi] > weight * 1.25)) fi = -1;
  }
  insert(*w, index);
  double *nw = (double *)malloc(sizeof(double) * children());
  int j = 0;
  for (int i = 0; i < children(); i++) {
    if (i == index) {
      nw[i] = weight;
    } else if (fi >= 0) {
      nw[i] = weights_[j];
      if (j == fi) nw[i] -= weight;
      j++;
    } else {
      nw[i] = weights_[j++] * (1 - weight);
    }
  }
  free(weights_);
  weights_ = nw;
  nweights_ = children();
}

// The dock the split is in, or 0.
static Fl_Dock *dock_of(Fl_Widget *w) {
  for (Fl_Widget *p = w->parent(); p; p = p->parent()) {
    if (p->type() == FL_DOCK && p->as_group()) return (Fl_Dock *)p;
  }
  return 0;
}

/**
  Called by FLTK as a child leaves the split.
  A child leaving the split, however it leaves, takes its weight with it;
  the others share its space in proportion.  The dock is told, in case the
  child was its center and is being deleted.
*/
void Fl_Dock_Split::on_remove(int index) {
  if (nweights_ == children() && index >= 0 && index < nweights_) {
    double gone = weights_[index];
    for (int i = index; i < nweights_ - 1; i++) weights_[i] = weights_[i + 1];
    nweights_--;
    if (gone < 1) {
      for (int i = 0; i < nweights_; i++) weights_[i] /= (1 - gone);
    }
  }
  Fl_Dock *d = dock_of(this);
  if (d) d->child_gone();
}

/**
  Removes a node.  Its share goes to child to if it is given, and otherwise
  to the others in proportion to theirs.
*/
void Fl_Dock_Split::remove_node(Fl_Widget *w, Fl_Widget *to) {
  sync_weights();
  int index = find(w);
  if (index >= children()) return;
  int n = nweights_;
  double *old = (double *)malloc(sizeof(double) * n);
  memcpy(old, weights_, sizeof(double) * n);
  double gone = old[index];
  remove(index);
  int k = 0;
  for (int i = 0; i < n; i++) {
    if (i != index) weights_[k++] = old[i];
  }
  free(old);
  int ti = -1;
  if (to && to != w) ti = find(to);
  if (ti >= 0 && ti < nweights_) {
    weights_[ti] += gone;
  } else if (gone < 1) {
    for (int i = 0; i < nweights_; i++) weights_[i] /= (1 - gone);
  }
}

/** Puts new_node in old_node's place, with its share. */
void Fl_Dock_Split::replace_node(Fl_Widget *old_node, Fl_Widget *new_node) {
  sync_weights();
  int index = find(old_node);
  if (index >= children()) return;
  int n = nweights_;
  double *w = (double *)malloc(sizeof(double) * (n + 1));
  memcpy(w, weights_, sizeof(double) * n);
  remove(index);
  insert(*new_node, index);
  free(weights_);
  weights_ = w;
  nweights_ = n;
}

/**
  Replaces a child split of the same direction by its children, each taking
  its share of the child's share, so nothing moves.
*/
void Fl_Dock_Split::absorb(Fl_Dock_Split *c) {
  sync_weights();
  c->sync_weights();
  int index = find(c);
  if (index >= children()) return;
  int n = c->children();
  if (n == 0) {
    remove_node(c);
    return;
  }
  double wc = weights_[index];
  int total = children() - 1 + n;
  double *nw = (double *)malloc(sizeof(double) * total);
  for (int i = 0; i < index; i++) nw[i] = weights_[i];
  for (int k = 0; k < n; k++) nw[index + k] = c->weights_[k] * wc;
  for (int i = index + 1; i < children(); i++) nw[i - 1 + n] = weights_[i];
  remove(index);
  for (int k = 0; k < n; k++) insert(*c->child(0), index + k);
  free(weights_);
  weights_ = nw;
  nweights_ = total;
  c->nweights_ = 0;
}

/** Places the children by their shares. */
void Fl_Dock_Split::layout() {
  sync_weights();
  int n = children();
  if (n == 0) return;
  // As for Fl_Dock_Tabs::layout(): not outside a window.
  if (!window()) return;
  int bar = split_bar_size;
  int total = h();
  int pos = y();
  if (horizontal_) {
    total = w();
    pos = x();
  }
  int room = total - bar * (n - 1);
  if (room < 0) room = 0;
  // Each boundary is placed by rounding the shares before it, so a child's
  // size depends on its own share and not on rounding in the others.
  double sum = 0;
  int used = 0;
  for (int i = 0; i < n; i++) {
    sum += weights_[i];
    int end = room;
    if (i < n - 1) end = (int)(sum * room + 0.5);
    if (end > room) end = room;
    int size = end - used;
    if (size < 0) size = 0;
    Fl_Widget *c = child(i);
    if (horizontal_) c->resize(pos, y(), size, h());
    else c->resize(x(), pos, w(), size);
    pos += size + bar;
    used += size;
  }
}

/**
  Resizes the split, keeping each child's share; or, when the dock is
  resized and keeps its panels' sizes, keeping the size of each child but
  the one holding the center.
*/
void Fl_Dock_Split::resize(int X, int Y, int W, int H) {
  int ci = -1;
  if (keep_center_) {
    for (Fl_Widget *c = keep_center_; c; c = c->parent()) {
      if (c->parent() == this) {
        ci = find(c);
        break;
      }
    }
  }
  int old_total = h();
  int new_total = H;
  if (horizontal_) {
    old_total = w();
    new_total = W;
  }
  Fl_Widget::resize(X, Y, W, H);
  if (ci >= 0 && ci < children() && old_total > 0 && new_total != old_total && window()) keep_sizes(ci, new_total);
  layout();
}

// Gives the children other than child ci the sizes they have now in a split
// total long, and child ci the rest.  If that leaves child ci less than its
// minimum share, the others share what is left in proportion to their sizes.
void Fl_Dock_Split::keep_sizes(int ci, int total) {
  sync_weights();
  int n = children();
  int room = total - split_bar_size * (n - 1);
  if (room <= 0 || n < 2) return;
  double others = 0;
  for (int i = 0; i < n; i++) {
    if (i == ci) continue;
    if (horizontal_) others += child(i)->w();
    else others += child(i)->h();
  }
  double cmin = room * CENTER_MIN_SHARE;
  if (cmin < split_min_size) cmin = split_min_size;
  if (cmin > room) cmin = room;
  double scale = 1;
  if (others > room - cmin && others > 0) scale = (room - cmin) / others;
  double used = 0;
  for (int i = 0; i < n; i++) {
    if (i == ci) continue;
    double s = child(i)->h();
    if (horizontal_) s = child(i)->w();
    s *= scale;
    if (!(s > 0)) s = 1;
    weights_[i] = s / room;
    used += s;
  }
  double rest = room - used;
  if (rest < 1) rest = 1;
  weights_[ci] = rest / room;
  double sum = 0;
  for (int i = 0; i < n; i++) sum += weights_[i];
  for (int i = 0; i < n; i++) weights_[i] /= sum;
}

// Returns the index of the child before the bar at X, Y, or -1.
int Fl_Dock_Split::bar_at(int X, int Y) const {
  if (X < x() || X >= x() + w() || Y < y() || Y >= y() + h()) return -1;
  for (int i = 0; i < children() - 1; i++) {
    Fl_Widget *c = child(i);
    if (horizontal_) {
      int e = c->x() + c->w();
      if (X >= e && X < e + split_bar_size) return i;
    } else {
      int e = c->y() + c->h();
      if (Y >= e && Y < e + split_bar_size) return i;
    }
  }
  return -1;
}

/** Lets the user drag the bars between the children, which changes their shares. */
int Fl_Dock_Split::handle(int event) {
  switch (event) {
    case FL_ENTER:
    case FL_MOVE: {
      int b = bar_at(Fl::event_x(), Fl::event_y());
      if (b >= 0) {
        if (horizontal_) window()->cursor(FL_CURSOR_WE);
        else window()->cursor(FL_CURSOR_NS);
        Fl_Group::handle(event);
        return 1;
      }
      if (dragging_ < 0) window()->cursor(FL_CURSOR_DEFAULT);
      break;
    }
    case FL_LEAVE:
      if (dragging_ < 0) window()->cursor(FL_CURSOR_DEFAULT);
      break;
    case FL_PUSH: {
      int b = bar_at(Fl::event_x(), Fl::event_y());
      if (b >= 0 && Fl::event_button() == FL_LEFT_MOUSE) {
        dragging_ = b;
        Fl_Widget *c = child(b + 1);
        if (horizontal_) drag_offset_ = Fl::event_x() - (c->x() - split_bar_size);
        else drag_offset_ = Fl::event_y() - (c->y() - split_bar_size);
        return 1;
      }
      break;
    }
    case FL_DRAG:
      if (dragging_ >= 0) {
        // Move the bar between children dragging_ and dragging_ + 1, keeping
        // both at least min_size(), and give them new shares of the room.
        int i = dragging_;
        Fl_Widget *a = child(i);
        Fl_Widget *b = child(i + 1);
        int start, end;
        if (horizontal_) { start = a->x(); end = b->x() + b->w(); }
        else { start = a->y(); end = b->y() + b->h(); }
        int pos = Fl::event_y() - drag_offset_;
        if (horizontal_) pos = Fl::event_x() - drag_offset_;
        int lo = start + split_min_size;
        int hi = end - split_bar_size - split_min_size;
        if (pos > hi) pos = hi;
        if (pos < lo) pos = lo;
        if (lo > hi) return 1;
        int pair = end - start - split_bar_size;
        int sa = pos - start;
        double both = weights_[i] + weights_[i + 1];
        if (pair > 0) {
          weights_[i] = both * sa / pair;
          weights_[i + 1] = both - weights_[i];
        }
        layout();
        redraw();
        // The dock's callback, with no panel, as the bar moves and when it
        // is let go, as when() asks.
        Fl_Dock *d = dock_of(this);
        if (d) d->notify(0, FL_REASON_CHANGED, 1);
        return 1;
      }
      break;
    case FL_RELEASE:
      if (dragging_ >= 0) {
        dragging_ = -1;
        if (bar_at(Fl::event_x(), Fl::event_y()) < 0) window()->cursor(FL_CURSOR_DEFAULT);
        Fl_Dock *d = dock_of(this);
        if (d) d->notify(0, FL_REASON_CHANGED, 2);
        return 1;
      }
      break;
    default:
      break;
  }
  return Fl_Group::handle(event);
}

/** Draws the children and a grip in the middle of each bar between them. */
void Fl_Dock_Split::draw() {
  Fl_Group::draw();
  if (!(damage() & ~FL_DAMAGE_CHILD)) return;
  // A grip of three dots in the middle of each bar.
  fl_color(fl_darker(color()));
  for (int i = 0; i < children() - 1; i++) {
    Fl_Widget *c = child(i);
    for (int k = -1; k <= 1; k++) {
      if (horizontal_) {
        int bx = c->x() + c->w() + split_bar_size / 2 - 1;
        int by = y() + h() / 2 + 5 * k - 1;
        fl_rectf(bx, by, 2, 2);
      } else {
        int bx = x() + w() / 2 + 5 * k - 1;
        int by = c->y() + c->h() + split_bar_size / 2 - 1;
        fl_rectf(bx, by, 2, 2);
      }
    }
  }
}
