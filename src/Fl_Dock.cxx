//
// Dock widget for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
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
#include <FL/Fl_Dock_Stripe.H>
#include <FL/Fl_Dock_Tabs.H>
#include <FL/Fl_Dock_Window.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include "Fl_Dock_Drag.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** The type() of the hidden group holding a dock's closed panels. */
#define FL_DOCK_STORE 0xD4

// Widgets made while the dock rearranges itself must not land in whatever
// group the application has open.
class Fl_Dock_Current {
  Fl_Group *saved_;
public:
  Fl_Dock_Current() { saved_ = Fl_Group::current(); Fl_Group::current(0); }
  ~Fl_Dock_Current() { Fl_Group::current(saved_); }
};

// The child of split s that is or holds w, or 0.
static Fl_Widget *child_holding(Fl_Group *s, Fl_Widget *w) {
  for (Fl_Widget *c = w; c; c = c->parent()) {
    if (c->parent() == s) return c;
  }
  return 0;
}

// The widget a home names: a docked panel, the center, or 0 for the whole dock.
static Fl_Widget *home_target(Fl_Dock *d, const char *ref) {
  if (!ref) return 0;
  if (!ref[0]) return d->center();
  Fl_Dock_Panel *r = d->find(ref);
  if (r && r->docked() && r->dock() == d) return r;
  return 0;
}

// For a panel that was at an end of its split: the first or last node of
// the split that target is in, along where, so the panel goes back past
// whatever has docked beside target since.  A target in a split the other
// way, or in none, is the part the panel's own split was made of.
static Fl_Widget *edge_node(Fl_Widget *target, int where) {
  int horizontal = (where == FL_DOCK_LEFT || where == FL_DOCK_RIGHT);
  if (!target || !Fl_Dock::is_split(target->parent())) return target;
  Fl_Dock_Split *s = (Fl_Dock_Split *)target->parent();
  if (s->horizontal() != horizontal) return target;
  if (where == FL_DOCK_LEFT || where == FL_DOCK_TOP) return s->child(0);
  return s->child(s->children() - 1);
}

// The smallest node of d's tree holding both a and b, or a.
static Fl_Widget *common_node(Fl_Dock *d, Fl_Widget *a, Fl_Widget *b) {
  if (!b) return a;
  for (Fl_Widget *x = a; x && x != d; x = x->parent()) {
    for (Fl_Widget *y = b; y && y != d; y = y->parent()) {
      if (x == y) return x;
    }
  }
  return a;
}

// The name of the panel nearest one end of node: "" for the center.
static const char *end_name(Fl_Widget *node, int last) {
  while (Fl_Dock::is_split(node)) {
    Fl_Group *g = (Fl_Group *)node;
    node = g->child(0);
    if (last) node = g->child(g->children() - 1);
  }
  if (Fl_Dock::is_tabs(node)) return ((Fl_Dock_Tabs *)node)->panel(0)->name();
  return "";
}

/**
  Creates an empty dock.  Unlike other groups it does not become the current
  group: panels are given to it with add_panel(), not by creating them inside it.
*/
Fl_Dock::Fl_Dock(int X, int Y, int W, int H, const char *L)
  : Fl_Group(X, Y, W, H, L) {
  type(FL_DOCK);
  box(FL_FLAT_BOX);
  root_ = 0;
  center_ = 0;
  owner_ = 0;
  windows_ = 0;
  nwindows_ = 0;
  callback_panel_ = 0;
  factory_ = 0;
  factory_data_ = 0;
  kept_ = 0;
  nkept_ = 0;
  for (int s = 0; s < 4; s++) stripe_[s] = 0;
  stripe_sides_ = 0;
  stripe_options_ = 0;
  slide_ = 0;
  saved_layout_ = 0;
  saved_kind_ = 0;
  panel_menu_ = 0;
  panel_menu_path_ = 0;
  last_placed_ = 0;
  nlast_placed_ = 0;
  keep_sizes_ = 1;
  reading_ = 0;
  has_center_ = 0;
  destroying_ = 0;
  // A deleted center clears center_.
  Fl::watch_widget_pointer(center_);
  store_ = new Fl_Group(X, Y, W, H);
  store_->type(FL_DOCK_STORE);
  store_->end();
  // Closed panels keep their own size as the dock is resized.
  store_->resizable(0);
  store_->hide();
  end();
  install_dispatch();
}

/**
  Deletes the dock, its panels, open or closed, its center and its floating
  windows.
*/
Fl_Dock::~Fl_Dock() {
  Fl::remove_timeout(repair_cb, this);
  // Only the dock windows belong to has a sliding window; a window's dock may
  // outlive that dock.
  if (!owner_) close_slide();
  // The panels of a floating window's dock leave its owner's stripes and
  // records; the owner's own panels go with everything else.
  if (owner_ && !owner_->destroying_) {
    int n;
    Fl_Dock_Panel **all = all_panels(n);
    for (int i = n - 1; i >= 0; i--) owner_->forget(all[i]);
    free(all);
    owner_->place_stripes();
  }
  destroying_ = 1;
  while (nwindows_ > 0) {
    Fl_Dock_Window *w = windows_[nwindows_ - 1];
    nwindows_--;
    delete w;
  }
  // The children are deleted while the dock is whole, so that panels can see
  // that their dock is deleting them.
  clear();
  Fl::release_widget_pointer(center_);
  remove_dispatch();
  free(windows_);
  clear_kept();
  free(saved_layout_);
  free(panel_menu_path_);
  free(last_placed_);
}

// The last panel of a place group to close, or 0.
Fl_Dock_Panel *Fl_Dock::last_placed(int group) const {
  for (int i = 0; i < nlast_placed_; i++) {
    if (last_placed_[i]->place_group() == group) return last_placed_[i];
  }
  return 0;
}

// Notes p as the last of its place group to close.
void Fl_Dock::set_last_placed(Fl_Dock_Panel *p) {
  if (!p->place_group()) return;
  for (int i = 0; i < nlast_placed_; i++) {
    if (last_placed_[i]->place_group() == p->place_group()) {
      last_placed_[i] = p;
      return;
    }
  }
  last_placed_ = (Fl_Dock_Panel **)realloc(last_placed_, sizeof(Fl_Dock_Panel *) * (nlast_placed_ + 1));
  last_placed_[nlast_placed_++] = p;
}

/** Returns 1 if w is an Fl_Dock_Split. */
int Fl_Dock::is_split(const Fl_Widget *w) {
  return w && w->type() == FL_DOCK_SPLIT && w->as_group() != 0;
}

/** Returns 1 if w is an Fl_Dock_Tabs. */
int Fl_Dock::is_tabs(const Fl_Widget *w) {
  return w && w->type() == FL_DOCK_TABS && w->as_group() != 0;
}

/** Returns 1 if w is an Fl_Dock_Panel. */
int Fl_Dock::is_panel(const Fl_Widget *w) {
  return w && w->type() == FL_DOCK_PANEL && w->as_group() != 0;
}

/**
  Returns the version of the Fl_Dock library the application is linked with,
  FL_DOCK_API_VERSION as it was when the library was built.  Compare it with
  FL_DOCK_API_VERSION to check that the headers match the library, as
  Fl::api_version() does for FLTK.
*/
int Fl_Dock::api_version() { return FL_DOCK_API_VERSION; }

/** Resizes the dock and lays out what it holds in the new area; see keep_sizes(). */
void Fl_Dock::resize(int X, int Y, int W, int H) {
  int dx = X - x();
  int dy = Y - y();
  Fl_Widget::resize(X, Y, W, H);
  store_->resize(X, Y, W, H);
  // Inset windows move with the dock; placing the stripes keeps them inside.
  for (int i = 0; i < nwindows_; i++) {
    Fl_Dock_Window *win = windows_[i];
    if (win->inset() && (dx || dy)) win->position(win->x() + dx, win->y() + dy);
  }
  place_stripes();
}

/** Draws the dock, or only its box and label while it holds nothing. */
void Fl_Dock::draw() {
  if (!root_) {
    draw_box();
    draw_label();
    return;
  }
  Fl_Group::draw();
}

/**
  Shows or hides a panel whose dock_shortcut() was pressed.  The dock's
  floating windows are shown and hidden with the dock, so they appear when
  the application's window is first shown and go when it is closed.
*/
int Fl_Dock::handle(int event) {
  // Escape that nothing else used slides the sliding window away.
  if (event == FL_SHORTCUT && !owner_ && slide_ && Fl::event_key() == FL_Escape) {
    Fl_Dock_Panel *p = slide_->dock()->panel(0);
    close_slide();
    if (center_) center_->take_focus();
    if (p) notify(p, FL_REASON_CLOSED);
    return 1;
  }
  if ((event == FL_SHOW || event == FL_HIDE) && !owner_) {
    int r = Fl_Group::handle(event);
    Fl_Window *top = window();
    for (int i = 0; i < nwindows_; i++) {
      Fl_Dock_Window *win = windows_[i];
      if (win->inset() || !win->dock()->root()) continue;
      if (event == FL_SHOW && top && top->shown() && visible_r()) show_window(win);
      else if (event == FL_HIDE) win->hide();
    }
    return r;
  }
  if (event == FL_SHORTCUT && !owner_) {
    int n;
    Fl_Dock_Panel **all = all_panels(n);
    Fl_Dock_Panel *hit = 0;
    for (int i = 0; i < n && !hit; i++) {
      if (all[i]->dock_shortcut() && Fl::test_shortcut(all[i]->dock_shortcut())) hit = all[i];
    }
    free(all);
    if (hit) {
      toggle(hit, 1);
      return 1;
    }
  }
  return Fl_Group::handle(event);
}

/**
  Gets the area the dock's tree takes: the dock less its stripes.  Stripes
  with no buttons take no space unless FL_DOCK_STRIPE_SHOW_EMPTY is set.
*/
void Fl_Dock::area(int &X, int &Y, int &W, int &H) const {
  X = x();
  Y = y();
  W = w();
  H = h();
  int t = Fl_Dock_Stripe::thickness();
  for (int s = 0; s < 4; s++) {
    Fl_Dock_Stripe *st = stripe_[s];
    if (!st || !st->visible()) continue;
    if (s == FL_DOCK_LEFT) {
      X += t;
      W -= t;
    } else if (s == FL_DOCK_RIGHT) {
      W -= t;
    } else if (s == FL_DOCK_TOP) {
      Y += t;
      H -= t;
    } else {
      H -= t;
    }
  }
  if (W < 0) W = 0;
  if (H < 0) H = 0;
}

// Fits the tree to the dock's area.  With keep set the area itself changed,
// and the splits holding the center keep their other children's sizes.
void Fl_Dock::fit_root(int keep) {
  if (!root_) return;
  int X, Y, W, H;
  area(X, Y, W, H);
  if (keep && !reading_ && keep_sizes() && center_ && center_->parent()) Fl_Dock_Split::keep_center_ = center_;
  root_->resize(X, Y, W, H);
  Fl_Dock_Split::keep_center_ = 0;
}

// Makes a node visible.  Before its window is shown it is only marked so:
// show() on a subwindow, such as an Fl_Gl_Window center, whose window is not
// shown yet leaves it unable to draw on macOS; the window maps it when shown.
static void show_node(Fl_Widget *node) {
  Fl_Window *win = node->window();
  if (win && win->shown()) node->show();
  else node->set_visible();
}

// Shows the panel each tab group under node shows.  Panels are shown only
// inside a window, where any subwindows they hold can be mapped.
static void reveal(Fl_Widget *node) {
  if (Fl_Dock::is_tabs(node)) {
    Fl_Dock_Tabs *t = (Fl_Dock_Tabs *)node;
    if (t->value()) t->value(t->value());
  } else if (Fl_Dock::is_split(node)) {
    Fl_Group *g = (Fl_Group *)node;
    for (int i = 0; i < g->children(); i++) reveal(g->child(i));
  }
}

void Fl_Dock::set_root(Fl_Widget *node) {
  if (root_ && root_->parent() == this) remove(root_);
  root_ = node;
  if (node) {
    add(node);
    show_node(node);
    fit_root();
    reveal(node);
  }
  redraw();
}

/**
  Sets the center: a widget, such as a 3D view, that is a leaf of its own and
  is never closed or floated.  Set it before adding panels; the dock takes it
  as a child and deletes it with itself.  A center set before is hidden,
  taken out of the dock and given back to the caller, who then owns it, and a
  new one takes its place; center(0) leaves the dock without one.  Deleting the center
  takes it out of the dock.
*/
void Fl_Dock::center(Fl_Widget *w) {
  if (w == center_) return;
  drop_hidden();
  Fl_Widget *old = center_;
  if (old && w && old->parent() && (old == root_ || is_split(old->parent()))) {
    center_ = w;
    old->hide();
    if (old == root_) {
      remove(old);
      root_ = 0;
      set_root(w);
    } else {
      ((Fl_Dock_Split *)old->parent())->replace_node(old, w);
      w->resize(old->x(), old->y(), old->w(), old->h());
      show_node(w);
      fit_root();
      redraw();
    }
    return;
  }
  if (center_) detach_center();
  if (!w) return;
  center_ = w;
  has_center_ = 1;
  if (!root_) set_root(w);
  else put(w, FL_DOCK_RIGHT, 0, 0.75);
}

// Takes the center out of the tree and forgets it.
void Fl_Dock::detach_center() {
  Fl_Widget *c = center_;
  center_ = 0;
  has_center_ = 0;
  c->hide();
  Fl_Group *p = c->parent();
  if (is_split(p)) ((Fl_Dock_Split *)p)->remove_node(c);
  else if (p) p->remove(c);
  normalize();
  fit_root();
  redraw();
}

/** Called by FLTK as a child leaves the dock. */
void Fl_Dock::on_remove(int index) {
  if (child(index) == root_) root_ = 0;
  child_gone();
}

// A widget left the tree.  If it was a deleted center, the tree is put in
// normal form again once the deletion is over.
void Fl_Dock::child_gone() {
  if (has_center_ && !center_ && !destroying_) Fl::add_timeout(0.0, repair_cb, this);
}

void Fl_Dock::repair_cb(void *data) {
  Fl_Dock *d = (Fl_Dock *)data;
  d->normalize();
  d->fit_root();
  d->redraw();
}

// The depth of the tree at node: 1 for a leaf.
static int tree_depth(Fl_Widget *node) {
  if (!Fl_Dock::is_split(node)) return 1;
  Fl_Group *g = (Fl_Group *)node;
  int d = 0;
  for (int i = 0; i < g->children(); i++) {
    int c = tree_depth(g->child(i));
    if (c > d) d = c;
  }
  return d + 1;
}

// 1 if p may join this dock: it has a name, and no other panel of the dock
// has that name, so every layout the dock writes can be read back.
int Fl_Dock::can_take(const Fl_Dock_Panel *p) const {
  if (!p || !p->name() || !p->name()[0]) return 0;
  Fl_Dock_Panel *q = owner()->find(p->name());
  return !q || q == p;
}

// The node a reference widget names: the root for 0, the center, or the tab
// group of a panel docked here.
Fl_Widget *Fl_Dock::node_of(Fl_Widget *ref) const {
  if (!ref) return root_;
  if (ref == center_) return center_;
  if (is_panel(ref) && is_tabs(ref->parent()) && ((Fl_Dock_Panel *)ref)->dock() == this) return ref->parent();
  return 0;
}

// Puts node beside target, or beside the whole dock if target is 0, taking
// that fraction of the target's space; or, with share set, taking that share
// of the whole split it joins from all its children in proportion, which
// undoes take() exactly.  The tree is normalized afterwards.
void Fl_Dock::put(Fl_Widget *node, Fl_Dock_Where where, Fl_Widget *target, double fraction, int share) {
  Fl_Dock_Current keep;
  if (!(fraction > 0 && fraction < 1)) fraction = 0.25;
  if (!root_) {
    set_root(node);
    return;
  }
  if (!target) target = root_;
  int horizontal = (where == FL_DOCK_LEFT || where == FL_DOCK_RIGHT);
  int before = (where == FL_DOCK_LEFT || where == FL_DOCK_TOP);
  // Along an edge of the whole dock, in a split of that direction already: the node goes at its
  // end, taking its share from the side holding the center, so the others keep their sizes
  if (target == root_ && is_split(root_) && ((Fl_Dock_Split *)root_)->horizontal() == horizontal) {
    Fl_Dock_Split *s = (Fl_Dock_Split *)root_;
    int at = s->children();
    if (before) at = 0;
    s->insert_node(node, at, fraction, child_holding(s, center_));
    s->layout();
    show_node(node);
    normalize();
    fit_root();
    reveal(node);
    redraw();
    return;
  }
  Fl_Group *p = target->parent();
  if (is_split(p) && ((Fl_Dock_Split *)p)->horizontal() == horizontal) {
    Fl_Dock_Split *s = (Fl_Dock_Split *)p;
    int i = s->find(target);
    double wt = s->weight(i);
    int at = i + 1;
    if (before) at = i;
    if (share) {
      // Taken from the side holding the center, which took it when the node left.
      s->insert_node(node, at, fraction, child_holding(s, center_));
      s->layout();
      show_node(node);
      normalize();
      fit_root();
      reveal(node);
      redraw();
      return;
    }
    // The new node takes its share from the target alone.
    s->insert_node(node, at, 0.5);
    int ti = s->find(target);
    int ni = s->find(node);
    double others = 0;
    for (int k = 0; k < s->children(); k++) {
      if (k != ti && k != ni) others += s->weight(k);
    }
    // Restore the others' shares, scaled back up, then split wt between the two.
    double scale = 1;
    if (others > 0) scale = (1 - wt) / others;
    for (int k = 0; k < s->children(); k++) {
      if (k != ti && k != ni) s->weight(k, s->weight(k) * scale);
    }
    s->weight(ni, wt * fraction);
    s->weight(ti, wt * (1 - fraction));
    s->layout();
  } else {
    Fl_Dock_Split *s = new Fl_Dock_Split(target->x(), target->y(), target->w(), target->h(), horizontal);
    if (is_split(p)) ((Fl_Dock_Split *)p)->replace_node(target, s);
    else set_root(s);
    if (before) {
      s->insert_node(node, 0, 0.5);
      s->insert_node(target, 1, 0.5);
      s->weight(0, fraction);
      s->weight(1, 1 - fraction);
    } else {
      s->insert_node(target, 0, 0.5);
      s->insert_node(node, 1, 0.5);
      s->weight(0, 1 - fraction);
      s->weight(1, fraction);
    }
  }
  show_node(node);
  normalize();
  fit_root();
  reveal(node);
  redraw();
}

// Removes panel p from its tab group, and the group if that leaves it empty.
// It is hidden first, while it is still in a window.  A panel leaving a
// floating or inset window notes where that window was, to open there again.
void Fl_Dock::take(Fl_Dock_Panel *p) {
  // A drag of the panel, or of its group, ends when the panel is moved.
  Fl_Dock_Drag::forget(p);
  Fl_Window *own = window();
  if (owner_ && own && is_tabs(p->parent())) {
    Fl_Dock_Window *dw = (Fl_Dock_Window *)own;
    if (dw->inset()) p->inset_rect(dw->x() - owner_->x(), dw->y() - owner_->y(), dw->w(), dw->h());
    else p->float_rect(dw->x(), dw->y(), dw->w(), dw->h());
  }
  p->hide();
  Fl_Group *t = p->parent();
  if (is_tabs(t)) {
    ((Fl_Dock_Tabs *)t)->remove_panel(p);
  } else if (t) {
    t->remove(p);
  }
  normalize();
  fit_root();
  redraw();
  // A floating window goes when its last panel leaves.
  if (owner_ && !root_) {
    Fl_Window *win = window();
    for (int i = 0; i < owner_->nwindows_; i++) {
      if (owner_->windows_[i] == win) {
        owner_->remove_window((Fl_Dock_Window *)win);
        win->hide();
        // An inset window leaves the dock now, so deleting the dock before
        // the deferred deletion cannot delete it twice.
        if (win->parent()) win->parent()->remove(win);
        Fl::delete_widget(win);
        break;
      }
    }
    // A sliding window whose panel was dragged out of it goes too.
    if (owner_ && owner_->slide_ == win) owner_->close_slide();
  }
}

// Puts the subtree at node in normal form and returns what should take its
// place: itself, one of its descendants, or 0 if nothing is left.  The space
// of a node that goes is given to the side holding the center when there is
// one, as the center is what takes up the space panels leave.
Fl_Widget *Fl_Dock::normal_form(Fl_Widget *node, Fl_Widget *center) {
  if (Fl_Dock::is_tabs(node)) {
    if (((Fl_Group *)node)->children() == 0) return 0;
    return node;
  }
  if (!Fl_Dock::is_split(node)) return node;
  Fl_Dock_Split *s = (Fl_Dock_Split *)node;
  for (int i = s->children() - 1; i >= 0; i--) {
    Fl_Widget *c = s->child(i);
    Fl_Widget *r = normal_form(c, center);
    if (!r) {
      s->remove_node(c, child_holding(s, center));
      Fl::delete_widget(c);
    } else if (r != c) {
      s->replace_node(c, r);
      Fl::delete_widget(c);
    }
  }
  // A split in a split of the same direction gives its children to the parent.
  for (int i = 0; i < s->children(); i++) {
    Fl_Widget *c = s->child(i);
    if (Fl_Dock::is_split(c) && ((Fl_Dock_Split *)c)->horizontal() == s->horizontal()) {
      int n = ((Fl_Group *)c)->children();
      s->absorb((Fl_Dock_Split *)c);
      Fl::delete_widget(c);
      i += n - 1;
    }
  }
  if (s->children() == 0) return 0;
  if (s->children() == 1) return s->child(0);
  return s;
}

void Fl_Dock::normalize() {
  if (has_center_ && !center_) has_center_ = 0;
  if (!root_) {
    refresh();
    return;
  }
  Fl_Widget *old = root_;
  Fl_Widget *r = normal_form(old, center_);
  if (r == old) {
    refresh();
    return;
  }
  if (r) {
    set_root(r);
  } else {
    remove(old);
    root_ = 0;
  }
  if (old != center_ && !is_panel(old)) Fl::delete_widget(old);
  refresh();
}

// Brings what shows the panels' state up to date: the panel menu's checks
// and the stripes' buttons.
void Fl_Dock::refresh() {
  Fl_Dock *o = owner();
  if (o->destroying_) return;
  o->update_panel_menu();
  for (int s = 0; s < 4; s++) {
    if (o->stripe_[s]) o->stripe_[s]->redraw();
  }
}

// Tells the application that the user changed the dock: the changed flag
// is set, and the callback called if when() asks for it.  A change made
// step by step, as a bar is dragged, calls it each step with
// FL_WHEN_CHANGED and at the end with FL_WHEN_RELEASE; any other calls it
// once with either.
void Fl_Dock::notify(Fl_Dock_Panel *p, Fl_Callback_Reason reason, int step) {
  Fl_Dock *o = owner();
  o->refresh();
  o->callback_panel_ = p;
  o->set_changed();
  int w = o->when();
  int call = (w & (FL_WHEN_CHANGED | FL_WHEN_RELEASE)) != 0;
  if (step == 1) call = (w & FL_WHEN_CHANGED) != 0;
  else if (step == 2) call = (w & FL_WHEN_RELEASE) != 0;
  if (call) o->do_callback(reason);
}

/**
  Docks a panel: beside the widget ref on the given side, taking that share of
  ref's space, or the panel's own width or height if fraction is 0, or as a
  tab of ref's tab group for FL_DOCK_TAB.  ref is a panel
  docked here, the center, or 0 for the whole dock.  A panel already in the
  dock, or in one of its windows, is moved; one in another dock is taken out
  of that dock first.  A panel alone in its tab group cannot go beside or
  into its own group.
  \return 1 if the panel was docked; 0 if ref is not in this dock or is the
    panel's own group, if the panel has no name or another panel of the dock
    has its name, or if the tree is as deep as a layout may be
*/
int Fl_Dock::add_panel(Fl_Dock_Panel *p, Fl_Dock_Where where, Fl_Widget *ref, double fraction) {
  if (!can_take(p)) return 0;
  p->fix_name();
  Fl_Widget *target = node_of(ref);
  if (ref && !target) return 0;
  // Taking p out would delete the group it is to go beside.
  if (target && target == p->parent() && ((Fl_Dock_Tabs *)target)->panels() == 1) return 0;
  Fl_Dock *from = p->dock();
  if (from && from->owner() != owner()) from->owner()->remove_panel(p);
  return dock_at(p, where, target, fraction, 0);
}

// Docks p beside or into the node target; see put() for fraction and share.
int Fl_Dock::dock_at(Fl_Dock_Panel *p, Fl_Dock_Where where, Fl_Widget *target, double fraction, int share) {
  Fl_Dock_Current keep;
  drop_hidden();
  // A new group can make the tree one deeper.
  if (where != FL_DOCK_TAB && root_ && tree_depth(root_) >= MAX_DEPTH) return 0;
  // No fraction: the panel's own size, which is what it had when last docked.
  if (!share && !(fraction > 0) && where != FL_DOCK_TAB && root_) {
    Fl_Widget *t = target;
    if (!t) t = root_;
    double size = Fl_Dock_Tabs::outer_h(p->h());
    double avail = t->h();
    if (where == FL_DOCK_LEFT || where == FL_DOCK_RIGHT) {
      size = Fl_Dock_Tabs::outer_w(p->w());
      avail = t->w();
    }
    fraction = 0.25;
    if (avail > 0) fraction = size / avail;
    if (fraction < 0.05) fraction = 0.05;
    if (fraction > 0.9) fraction = 0.9;
  }
  if (where == FL_DOCK_TAB && !is_tabs(target)) {
    if (target) where = FL_DOCK_RIGHT;
    else where = FL_DOCK_LEFT;
  }
  Fl_Dock *from = p->dock();
  if (from) from->take(p);
  else p->hide();
  if (where == FL_DOCK_TAB) {
    ((Fl_Dock_Tabs *)target)->insert_panel(p, -1);
    fit_root();
    redraw();
    if (!owner_) p->mode(FL_DOCK_MODE_DOCKED);
    return 1;
  }
  Fl_Dock_Tabs *t = new Fl_Dock_Tabs(x(), y(), w(), h());
  t->insert_panel(p, 0);
  put(t, where, target, fraction, share);
  if (!owner_) p->mode(FL_DOCK_MODE_DOCKED);
  return 1;
}

// Docks p at a place noted by remember_home() or a layout: beside or as a tab
// of the panel named ref, beside the center if ref is "", or beside the whole
// dock if ref is 0, with share its share of the split it rejoins.
int Fl_Dock::restore(Fl_Dock_Panel *p, const char *ref, int where, double share, int relative) {
  Fl_Widget *target = 0;
  if (ref && !ref[0]) {
    target = center_;
  } else if (ref) {
    Fl_Dock_Panel *r = find(ref);
    if (r && r->docked() && r->dock() == this) target = r->parent();
  }
  if (where == FL_DOCK_TAB && !target) where = FL_DOCK_LEFT;
  if (relative) return dock_at(p, (Fl_Dock_Where)where, target, share, 0);
  return dock_at(p, (Fl_Dock_Where)where, target, share, 1);
}

// Docks p at its home, beside the other neighbour if the first is not docked.
int Fl_Dock::restore_home(Fl_Dock_Panel *p) {
  const char *ref = p->home_ref();
  int where = p->home_where();
  int edge = p->home_edge();
  if (ref && !home_target(this, ref) && p->home_ref2() && home_target(this, p->home_ref2())) {
    ref = p->home_ref2();
    where = p->home_where2();
    edge = 0;
  }
  Fl_Widget *t = home_target(this, ref);
  if (edge && t && !p->home_relative() && where != FL_DOCK_TAB) {
    if (is_panel(t)) t = t->parent();
    Fl_Widget *far = home_target(this, p->home_span());
    if (far && is_panel(far)) far = far->parent();
    t = common_node(this, t, far);
    return dock_at(p, (Fl_Dock_Where)where, edge_node(t, where), p->home_fraction(), 1);
  }
  return restore(p, ref, where, p->home_fraction(), p->home_relative());
}

/**
  Gives the dock a panel that starts closed: show_panel() docks it as
  add_panel() would with these arguments, at that time.  Use it for panels
  that are shown and hidden as the application needs them, so that giving
  them to the dock changes nothing on the screen.  ref is a panel or the
  center, and is named, so it may be closed or docked elsewhere by then.
  \return 1 if the panel was taken, 0 if ref is not this dock's or the
    panel's name is empty or taken, as for add_panel()
*/
int Fl_Dock::store_panel(Fl_Dock_Panel *p, Fl_Dock_Where where, Fl_Widget *ref, double fraction) {
  Fl_Dock *o = owner();
  if (!can_take(p)) return 0;
  p->fix_name();
  drop_hidden();
  const char *name = 0;
  if (ref && ref == o->center_) name = "";
  else if (ref && is_panel(ref) && ((Fl_Dock_Panel *)ref)->dock() && ((Fl_Dock_Panel *)ref)->dock()->owner() == o) name = ((Fl_Dock_Panel *)ref)->name();
  else if (ref) return 0;
  Fl_Dock *from = p->dock();
  if (from && from->owner() != o) {
    from->owner()->remove_panel(p);
    from = 0;
  }
  if (from && p->docked()) from->take(p);
  else p->hide();
  o->store_->add(p);
  p->home(name, where, fraction, 1);
  return 1;
}

// Notes where a docked panel is, so show_panel() or dock_panel() can put it back.
void Fl_Dock::remember_home(Fl_Dock_Panel *p) {
  Fl_Group *t = p->parent();
  if (!is_tabs(t)) return;
  Fl_Dock_Tabs *tabs = (Fl_Dock_Tabs *)t;
  if (tabs->panels() > 1) {
    Fl_Dock_Panel *other = tabs->panel(0);
    if (other == p) other = tabs->panel(1);
    p->home(other->name(), FL_DOCK_TAB, 0.25);
    return;
  }
  Fl_Group *s = tabs->parent();
  if (!is_split(s)) {
    p->home(0, FL_DOCK_LEFT, 0.25);
    return;
  }
  Fl_Dock_Split *sp = (Fl_Dock_Split *)s;
  int i = sp->find(tabs);
  int horizontal = sp->horizontal();
  int before = FL_DOCK_TOP;     // the panel goes before its next neighbour
  int after = FL_DOCK_BOTTOM;   // and after its previous one
  if (horizontal) {
    before = FL_DOCK_LEFT;
    after = FL_DOCK_RIGHT;
  }
  // Each neighbour is named by its panel nearest this one, or "" for the
  // center; its share of the split is taken back when it returns.
  const char *next = 0;
  const char *prev = 0;
  if (i + 1 < sp->children()) next = end_name(sp->child(i + 1), 0);
  if (i > 0) prev = end_name(sp->child(i - 1), 1);
  if (next) {
    p->home(next, before, sp->weight(i));
    if (prev) p->home2(prev, after);
    else p->home_edge(1, end_name(sp->child(i + 1), 1));
  } else {
    p->home(prev, after, sp->weight(i));
    p->home_edge(1, end_name(sp->child(i - 1), 0));
  }
}


/**
  Closes a panel: it is hidden and kept, with where it was, so show_panel() can put
  it back.  The panel's can_close() is not asked; the dock's callback is not
  called.
  \return 1 if the panel was docked here or in one of this dock's windows
*/
int Fl_Dock::close_panel(Fl_Dock_Panel *p) {
  if (!p) return 0;
  Fl_Dock *d = p->dock();
  if (!d || d->owner() != owner() || !p->docked()) return 0;
  drop_hidden();
  if (d == owner()) owner()->remember_home(p);
  d->take(p);
  owner()->store_->add(p);
  owner()->set_last_placed(p);
  return 1;
}

/**
  Takes a panel out of the dock for good: out of the tree, its windows and its
  closed panels, off its stripe and out of the panel menu.  The panel is
  hidden, has no parent, and belongs to the caller again, who may delete it
  or give it to a dock.  Deleting a panel does this first.  The dock's
  callback is not called.
  \return 1 if the panel was this dock's, 0 if not
*/
int Fl_Dock::remove_panel(Fl_Dock_Panel *p) {
  Fl_Dock *o = owner();
  if (!p || !p->dock() || p->dock()->owner() != o) return 0;
  drop_hidden();
  if (o->slide_ && o->slide_->dock()->panel(0) == p) o->close_slide();
  Fl_Dock *d = p->dock();
  if (p->docked()) {
    d->take(p);
  } else {
    p->hide();
    if (p->parent()) p->parent()->remove(p);
  }
  o->forget(p);
  if (!o->destroying_) o->place_stripes();
  return 1;
}

// Forgets what hide_all() or maximize_panel() would put back, as the dock
// is changed while they are in effect.
void Fl_Dock::drop_hidden() {
  Fl_Dock *o = owner();
  if (!o->saved_kind_) return;
  free(o->saved_layout_);
  o->saved_layout_ = 0;
  o->saved_kind_ = 0;
}

// Forgets every pointer to p the dock keeps outside its tree: drags, the
// stripes, place groups, the last callback and the panel menu.
void Fl_Dock::forget(Fl_Dock_Panel *p) {
  Fl_Dock_Drag::forget(p);
  for (int s = 0; s < 4; s++) {
    if (stripe_[s]) stripe_[s]->remove(p);
  }
  p->stripe(-1, 0);
  int k = 0;
  for (int i = 0; i < nlast_placed_; i++) {
    if (last_placed_[i] != p) last_placed_[k++] = last_placed_[i];
  }
  nlast_placed_ = k;
  if (callback_panel_ == p) callback_panel_ = 0;
  remove_menu_item(p);
}

/**
  Shows a panel: a closed one goes back where it was, and a docked one has its
  tab selected and its window raised.
  \return 1 if the panel belongs to this dock
*/
int Fl_Dock::show_panel(Fl_Dock_Panel *p) {
  if (!p) return 0;
  Fl_Dock *d = p->dock();
  if (!d || d->owner() != owner()) return 0;
  drop_hidden();
  if (p->docked()) {
    ((Fl_Dock_Tabs *)p->parent())->value(p);
    Fl_Window *win = d->window();
    if (win && d != owner()) win->show();
    refresh();
    return 1;
  }
  Fl_Dock *o = owner();
  // A panel of a place group takes the turn of the one shown, and its place.
  if (p->place_group()) {
    int n;
    Fl_Dock_Panel **all = o->all_panels(n);
    for (int i = 0; i < n; i++) {
      Fl_Dock_Panel *q = all[i];
      if (q != p && q->place_group() == p->place_group() && q->docked()) o->close_panel(q);
    }
    free(all);
    Fl_Dock_Panel *last = o->last_placed(p->place_group());
    if (last && last != p) p->copy_place(last);
  }
  // A closed panel opens the way it was shown before.
  if (p->mode() == FL_DOCK_MODE_INSET && o->window()) {
    if (o->inset_panel(p)) return 1;
  } else if (p->mode() == FL_DOCK_MODE_FLOATING) {
    int X, Y, W, H;
    if (p->get_float_rect(X, Y, W, H) && o->float_panel(p)) return 1;
  }
  o->restore_home(p);
  return 1;
}

/**
  Docks a panel the dock does not have yet: where the last layout read put a
  panel of that name, or else as add_panel() would with these arguments.  Use it
  instead of add_panel() for panels created after a layout is restored, so they go
  back where the user had them.
  \return 1 if the panel was docked, 0 if not, as for add_panel()
*/
int Fl_Dock::place_panel(Fl_Dock_Panel *p, Fl_Dock_Where where, Fl_Widget *ref, double fraction) {
  Fl_Dock *o = owner();
  if (!o->can_take(p)) return 0;
  p->fix_name();
  const char *kref;
  int kwhere;
  double kfraction;
  if (o->kept_place(p->name(), &kref, &kwhere, &kfraction)) {
    if (!kref || home_target(o, kref)) return o->restore(p, kref, kwhere, kfraction);
  }
  return o->add_panel(p, where, ref, fraction);
}

/**
  Floats a panel in a window of its own: where it floated last, or by the
  mouse at the panel's size if it has not floated before.  On Wayland the
  window's place is the compositor's choice.
  \return the window, or 0 if the panel is not docked here or cannot float
*/
Fl_Dock_Window *Fl_Dock::float_panel(Fl_Dock_Panel *p) {
  return float_at(p, 0, 0, 0, 0, 0);
}

/**
  Floats a panel in a window of its own at X, Y on the screen, W by H, or at
  the panel's size if W or H is 0.  Screens left of or above the main one
  have negative coordinates, so X and Y may be.  The window is kept on the
  screens when it is shown.
  \return the window, or 0 if the panel is not docked here or cannot float
*/
Fl_Dock_Window *Fl_Dock::float_panel(Fl_Dock_Panel *p, int X, int Y, int W, int H) {
  return float_at(p, 1, X, Y, W, H);
}

// Floats p at X, Y, W, H if given is set, else where it floated last.
Fl_Dock_Window *Fl_Dock::float_at(Fl_Dock_Panel *p, int given, int X, int Y, int W, int H) {
  Fl_Dock *o = owner();
  if (!p || !p->dock() || p->dock()->owner() != o) return 0;
  if (!p->docked() && p->dock() != o) return 0;
  drop_hidden();
  if (!given) {
    if (p->get_float_rect(X, Y, W, H)) {
      given = 1;
    } else {
      W = H = 0;
    }
  }
  if (W <= 0) W = Fl_Dock_Tabs::outer_w(p->w());
  if (H <= 0) H = Fl_Dock_Tabs::outer_h(p->h());
  if (W < 100) W = 100;
  if (H < 80) H = 80;
  if (!given) {
    X = Fl::event_x_root() - W / 2;
    Y = Fl::event_y_root() - 10;
  }
  if (p->dock() == o) o->remember_home(p);
  p->dock()->take(p);
  Fl_Dock_Window *win = o->new_window(X, Y, W, H, 0);
  win->copy_label(p->label());
  win->dock()->add_panel(p, FL_DOCK_LEFT);
  p->mode(FL_DOCK_MODE_FLOATING);
  return win;
}

/**
  Floats a panel in an inset window: one inside this dock's window, over the
  dock, that moves, hides and minimizes with it.  It goes where the panel
  was last inset, or in the middle of the dock at the panel's size if it has
  not been inset before.
  \return the window, or 0 if the panel is not docked here or cannot float
*/
Fl_Dock_Window *Fl_Dock::inset_panel(Fl_Dock_Panel *p) {
  return inset_at(p, 0, 0, 0, 0, 0);
}

/**
  Floats a panel in an inset window at X, Y relative to the dock, W by H, or
  at the panel's size if W or H is 0.  The window is kept inside the dock.
  \return the window, or 0 if the panel is not docked here or cannot float
*/
Fl_Dock_Window *Fl_Dock::inset_panel(Fl_Dock_Panel *p, int X, int Y, int W, int H) {
  return inset_at(p, 1, X, Y, W, H);
}

// Insets p at X, Y, W, H if given is set, else where it was inset last.
Fl_Dock_Window *Fl_Dock::inset_at(Fl_Dock_Panel *p, int given, int X, int Y, int W, int H) {
  Fl_Dock *o = owner();
  if (!p || !p->dock() || p->dock()->owner() != o || !o->window()) return 0;
  if (!p->docked() && p->dock() != o) return 0;
  drop_hidden();
  if (!given) {
    if (p->get_inset_rect(X, Y, W, H)) {
      given = 1;
    } else {
      W = H = 0;
    }
  }
  int frame = 2 * Fl_Dock_Window::frame_size();
  if (W <= 0) W = Fl_Dock_Tabs::outer_w(p->w()) + frame;
  if (H <= 0) H = Fl_Dock_Tabs::outer_h(p->h()) + frame;
  if (W < 100) W = 100;
  if (H < 80) H = 80;
  if (!given) {
    X = (o->w() - W) / 2;
    Y = (o->h() - H) / 2;
  }
  if (p->dock() == o) o->remember_home(p);
  p->dock()->take(p);
  Fl_Dock_Window *win = o->new_window(o->x() + X, o->y() + Y, W, H, 1);
  win->copy_label(p->label());
  win->dock()->add_panel(p, FL_DOCK_LEFT);
  win->keep_inside();
  p->mode(FL_DOCK_MODE_INSET);
  return win;
}

// Moves and sizes a floating window to lie on the screen nearest its middle,
// inside that screen's work area, so a window saved on a screen that is
// gone opens on one that is there.
static void fit_to_screen(Fl_Window *win) {
  int cx = win->x() + win->w() / 2;
  int cy = win->y() + win->h() / 2;
  int best = 0;
  double best_d = -1;
  for (int i = 0; i < Fl::screen_count(); i++) {
    int sx, sy, sw, sh;
    Fl::screen_work_area(sx, sy, sw, sh, i);
    double dx = 0;
    double dy = 0;
    if (cx < sx) dx = sx - cx;
    else if (cx >= sx + sw) dx = cx - (sx + sw - 1);
    if (cy < sy) dy = sy - cy;
    else if (cy >= sy + sh) dy = cy - (sy + sh - 1);
    double d = dx * dx + dy * dy;
    if (best_d < 0 || d < best_d) {
      best_d = d;
      best = i;
    }
  }
  int sx, sy, sw, sh;
  Fl::screen_work_area(sx, sy, sw, sh, best);
  int X = win->x();
  int Y = win->y();
  int W = win->w();
  int H = win->h();
  if (W > sw) W = sw;
  if (H > sh) H = sh;
  if (X + W > sx + sw) X = sx + sw - W;
  if (Y + H > sy + sh) Y = sy + sh - H;
  if (X < sx) X = sx;
  if (Y < sy) Y = sy;
  if (X != win->x() || Y != win->y() || W != win->w() || H != win->h()) win->resize(X, Y, W, H);
}

// Shows a floating window, on the screens.
void Fl_Dock::show_window(Fl_Dock_Window *win) {
  if (!win->shown()) fit_to_screen(win);
  win->show();
}

// Makes a window for this dock's panels, floating or inset, and shows it if
// the dock's window is shown.  An inset window is a subwindow and a child of
// this dock, which places it itself rather than letting a group resize it.
Fl_Dock_Window *Fl_Dock::new_window(int X, int Y, int W, int H, int kind) {
  Fl_Dock_Current keep;
  Fl_Dock_Window *win = new Fl_Dock_Window(X, Y, W, H, this, (Fl_Dock_Window::Kind)kind);
  // A sliding window is the dock's own, not one of its floating windows.
  if (kind != 2) add_window(win);
  if (kind) Fl_Group::add(win);
  Fl_Window *top = window();
  if (top && top->shown()) {
    if (kind) win->show();
    else show_window(win);
  } else if (kind) {
    win->set_visible();
  }
  return win;
}

/**
  Docks a floating panel back where it was before it floated.
  \return 1 if the panel was floating in one of this dock's windows
*/
int Fl_Dock::dock_panel(Fl_Dock_Panel *p) {
  Fl_Dock *o = owner();
  if (!p || !p->dock() || p->dock() == o || p->dock()->owner() != o) return 0;
  return o->restore_home(p);
}

void Fl_Dock::add_window(Fl_Dock_Window *w) {
  windows_ = (Fl_Dock_Window **)realloc(windows_, sizeof(Fl_Dock_Window *) * (nwindows_ + 1));
  windows_[nwindows_++] = w;
}

// The window no longer belongs to this dock, which may go before it does.
void Fl_Dock::remove_window(Fl_Dock_Window *w) {
  w->owner_ = 0;
  if (w->dock_) w->dock_->owner_ = 0;
  for (int i = 0; i < nwindows_; i++) {
    if (windows_[i] == w) {
      for (int k = i; k < nwindows_ - 1; k++) windows_[k] = windows_[k + 1];
      nwindows_--;
      return;
    }
  }
}

// Counts the panels under node, in order, putting the first max in out if it is given.
static int walk_panels(Fl_Widget *node, Fl_Dock_Panel **out, int n, int max) {
  if (!node) return n;
  if (Fl_Dock::is_panel(node)) {
    if (out && n < max) out[n] = (Fl_Dock_Panel *)node;
    return n + 1;
  }
  if (Fl_Dock::is_tabs(node) || Fl_Dock::is_split(node) || node->type() == FL_DOCK_STORE) {
    Fl_Group *g = (Fl_Group *)node;
    for (int i = 0; i < g->children(); i++) n = walk_panels(g->child(i), out, n, max);
  }
  return n;
}

// The panels of this dock only: docked, then closed, then sliding out.  A
// floating window's dock has no closed panels; its owner keeps them.
static int own_panels(const Fl_Dock *d, Fl_Group *store, Fl_Dock_Panel **out, int max) {
  int n = walk_panels(d->root(), out, 0, max);
  if (d->owner() == d) {
    n = walk_panels(store, out, n, max);
    if (d->sliding()) n = walk_panels(d->sliding()->dock()->root(), out, n, max);
  }
  return n;
}

/**
  Returns the number of panels: docked in this dock and its windows, closed,
  and sliding out.  A floating window's dock counts only its own.
*/
int Fl_Dock::panels() const {
  int n = own_panels(this, store_, 0, 0);
  if (!owner_) {
    for (int i = 0; i < nwindows_; i++) n += windows_[i]->dock()->panels();
  }
  return n;
}

// Every panel of panels(), in order, in one walk; the caller frees the list.
Fl_Dock_Panel **Fl_Dock::all_panels(int &n) const {
  n = panels();
  Fl_Dock_Panel **all = (Fl_Dock_Panel **)malloc(sizeof(Fl_Dock_Panel *) * (n + 1));
  int k = own_panels(this, store_, all, n);
  if (!owner_) {
    for (int i = 0; i < nwindows_ && k < n; i++) k += own_panels(windows_[i]->dock(), 0, all + k, n - k);
  }
  return all;
}

/**
  Returns panel i of panels(), or 0.  Each call walks the dock, so to visit
  every panel, find() or a walk of the tree may be quicker.
*/
Fl_Dock_Panel *Fl_Dock::panel(int i) const {
  int n;
  Fl_Dock_Panel **all = all_panels(n);
  Fl_Dock_Panel *p = 0;
  if (i >= 0 && i < n) p = all[i];
  free(all);
  return p;
}

// The panel named name under node, or 0.
static Fl_Dock_Panel *find_under(Fl_Widget *node, const char *name) {
  if (!node) return 0;
  if (Fl_Dock::is_panel(node)) {
    Fl_Dock_Panel *p = (Fl_Dock_Panel *)node;
    if (p->name() && !strcmp(p->name(), name)) return p;
    return 0;
  }
  if (Fl_Dock::is_tabs(node) || Fl_Dock::is_split(node) || node->type() == FL_DOCK_STORE) {
    Fl_Group *g = (Fl_Group *)node;
    for (int i = 0; i < g->children(); i++) {
      Fl_Dock_Panel *p = find_under(g->child(i), name);
      if (p) return p;
    }
  }
  return 0;
}

/** Returns the panel with this name, docked, floating or closed, or 0. */
Fl_Dock_Panel *Fl_Dock::find(const char *name) const {
  if (!name) return 0;
  const Fl_Dock *o = this;
  if (owner_) o = owner_;
  Fl_Dock_Panel *p = find_under(o->root_, name);
  if (!p) p = find_under(o->store_, name);
  for (int i = 0; !p && i < o->nwindows_; i++) p = find_under(o->windows_[i]->dock()->root(), name);
  if (!p && o->slide_) p = find_under(o->slide_->dock()->root(), name);
  return p;
}
