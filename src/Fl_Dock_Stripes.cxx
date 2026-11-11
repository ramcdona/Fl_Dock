//
// Stripes, sliding panels and whole-dock commands for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
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
#include <FL/Fl_Menu_.H>
#include <FL/Fl_Preferences.H>
#include <FL/fl_string_functions.h>
#include "Fl_Dock_Drag.H"
#include <stdlib.h>
#include <string.h>

// The sliding window out, in any dock, and the dispatch function there was
// before the dock's.  The window is on FLTK's watch list, which clears the
// pointer if it is deleted.  The dispatch is installed while any dock
// exists.
static Fl_Widget *slide_out = 0;
static Fl_Event_Dispatch prev_dispatch = 0;
static int docks = 0;

// ---- Stripes ---------------------------------------------------------------

/**
  Sets the sides that show stripes, Fl_Dock_Sides or'ed together.  A side
  turned off keeps its buttons, and shows them again when turned back on.
*/
void Fl_Dock::stripes(int sides) {
  Fl_Dock *o = owner();
  o->stripe_sides_ = sides & FL_DOCK_SIDE_ALL;
  for (int s = 0; s < 4; s++) {
    if ((o->stripe_sides_ & (1 << s)) && !o->stripe_[s]) {
      Fl_Group *saved = Fl_Group::current();
      Fl_Group::current(0);
      o->stripe_[s] = new Fl_Dock_Stripe(o, s);
      Fl_Group::current(saved);
      o->Fl_Group::add(o->stripe_[s]);
    }
  }
  o->place_stripes();
}

/** Returns the stripe on a side, an Fl_Dock_Where, or 0 if that side has never shown one. */
Fl_Dock_Stripe *Fl_Dock::stripe(int side) const {
  const Fl_Dock *o = owner();
  if (side < 0 || side > 3) return 0;
  return o->stripe_[side];
}

// Shows the stripes that have buttons, or all when empty ones are shown, and
// places them; the side stripes run between the top and bottom ones.
void Fl_Dock::place_stripes() {
  int t = Fl_Dock_Stripe::thickness();
  int top = 0;
  int bottom = 0;
  for (int s = 0; s < 4; s++) {
    Fl_Dock_Stripe *st = stripe_[s];
    if (!st) continue;
    // Empty stripes show while a panel is dragged, as places to drop it.
    int on = (stripe_sides_ & (1 << s)) &&
             (st->count() > 0 || (stripe_options_ & FL_DOCK_STRIPE_SHOW_EMPTY) || Fl_Dock_Drag::active());
    if (on) st->set_visible();
    else st->clear_visible();
    if (on && s == FL_DOCK_TOP) top = t;
    if (on && s == FL_DOCK_BOTTOM) bottom = t;
  }
  if (stripe_[FL_DOCK_TOP]) stripe_[FL_DOCK_TOP]->resize(x(), y(), w(), t);
  if (stripe_[FL_DOCK_BOTTOM]) stripe_[FL_DOCK_BOTTOM]->resize(x(), y() + h() - t, w(), t);
  if (stripe_[FL_DOCK_LEFT]) stripe_[FL_DOCK_LEFT]->resize(x(), y() + top, t, h() - top - bottom);
  if (stripe_[FL_DOCK_RIGHT]) stripe_[FL_DOCK_RIGHT]->resize(x() + w() - t, y() + top, t, h() - top - bottom);
  fit_root(1);
  place_slide();
  for (int i = 0; i < nwindows_; i++) windows_[i]->keep_inside();
  redraw();
}

/**
  Gives a panel a button on the stripe on one side, in group 0 at its start
  or group 1 at its end, before button index or last.  A panel has one
  button: one it has elsewhere moves here, keeping its group when group is
  -1.  The panel stays where it is; its button shows and hides it.
  \return 1, or 0 if the panel is not this dock's or the side is not one
*/
int Fl_Dock::stripe_panel(Fl_Dock_Panel *p, Fl_Dock_Where side, int group, int index) {
  Fl_Dock *o = owner();
  if (!p || side < FL_DOCK_LEFT || side > FL_DOCK_BOTTOM) return 0;
  if (!p->dock() || p->dock()->owner() != o) return 0;
  o->drop_hidden();
  if (group < 0) group = p->stripe_group();
  if (group < 0 || group > 1) group = 0;
  if (!o->stripe_[side]) {
    // A stripe made for this side, which shows if the side is turned on.
    Fl_Group *saved = Fl_Group::current();
    Fl_Group::current(0);
    o->stripe_[side] = new Fl_Dock_Stripe(o, side);
    Fl_Group::current(saved);
    o->Fl_Group::add(o->stripe_[side]);
  }
  if (p->stripe_side() >= 0 && o->stripe_[p->stripe_side()]) o->stripe_[p->stripe_side()]->remove(p);
  o->stripe_[side]->insert(p, group, index);
  p->stripe(side, group);
  // A sliding panel slides out from its stripe's side.
  if (o->slide_ && o->slide_->dock()->panel(0) == p) o->close_slide();
  o->place_stripes();
  return 1;
}

/**
  Takes a panel's button off its stripe.  A sliding panel, which has no
  other place, is pinned first.
  \return 1 if it had a button
*/
int Fl_Dock::unstripe_panel(Fl_Dock_Panel *p) {
  Fl_Dock *o = owner();
  if (!p || p->stripe_side() < 0) return 0;
  o->drop_hidden();
  if (p->mode() == FL_DOCK_MODE_SLIDING) o->pin_panel(p);
  if (o->stripe_[p->stripe_side()]) o->stripe_[p->stripe_side()]->remove(p);
  p->stripe(-1, 0);
  o->place_stripes();
  return 1;
}

/**
  Returns 1 if the panel is shown: it is the tab shown of its group, docked,
  in a window, or sliding out.  A closed or sliding panel not out is not.
*/
int Fl_Dock::panel_shown(const Fl_Dock_Panel *p) const {
  if (!p || !p->docked()) return 0;
  return p->visible() != 0;
}

// Hides the other shown panels of p's stripe group, unless they may share;
// for the user, the application is told of each.
void Fl_Dock::hide_others_in_group(Fl_Dock_Panel *p, int user) {
  if (stripe_options_ & FL_DOCK_STRIPE_SHARED) return;
  int side = p->stripe_side();
  if (side < 0 || !stripe_[side]) return;
  Fl_Dock_Stripe *st = stripe_[side];
  int g = p->stripe_group();
  for (int i = st->buttons(g) - 1; i >= 0; i--) {
    Fl_Dock_Panel *q = st->button(g, i);
    if (q == p || !panel_shown(q)) continue;
    if (q->mode() == FL_DOCK_MODE_SLIDING) close_slide();
    else close_panel(q);
    if (user) notify(q, FL_REASON_CLOSED);
  }
}

/**
  Shows or hides a panel, as clicking its stripe button or pressing its
  dock_shortcut() does.  A hidden panel is shown, opened where it was or
  slid out, and given the keyboard focus, and the others in its stripe
  group are hidden.  A shown panel is hidden; with FL_DOCK_STRIPE_FOCUS_FIRST
  one without the focus is given it instead.  As for every call, the dock's
  callback is not called.
  \return 1 if the panel was shown, 0 if it was hidden or not this dock's
*/
int Fl_Dock::toggle_panel(Fl_Dock_Panel *p) {
  return toggle(p, 0);
}

// toggle_panel(), for the user if user is set: the application is told.
int Fl_Dock::toggle(Fl_Dock_Panel *p, int user) {
  Fl_Dock *o = owner();
  if (!p || !p->dock() || p->dock()->owner() != o) return 0;
  if (o->panel_shown(p)) {
    int has_focus = Fl::focus() && p->contains(Fl::focus());
    if ((o->stripe_options_ & FL_DOCK_STRIPE_FOCUS_FIRST) && !has_focus) {
      p->take_focus();
      return 1;
    }
    if (p->mode() == FL_DOCK_MODE_SLIDING) o->close_slide();
    else o->close_panel(p);
    o->place_stripes();
    if (user) o->notify(p, FL_REASON_CLOSED);
    return 0;
  }
  o->hide_others_in_group(p, user);
  if (p->mode() == FL_DOCK_MODE_SLIDING) o->open_slide(p);
  else o->show_panel(p);
  p->take_focus();
  o->place_stripes();
  if (user) o->notify(p, FL_REASON_OPENED);
  return 1;
}

// The side of the dock a panel is nearest, for a panel unpinned without a
// stripe of its own: the nearest side that may show a stripe.
int Fl_Dock::nearest_side(Fl_Dock_Panel *p) const {
  int cx = p->x() + p->w() / 2 - x();
  int cy = p->y() + p->h() / 2 - y();
  int d[4] = { cx, w() - cx, cy, h() - cy };
  int best = -1;
  for (int s = 0; s < 4; s++) {
    if (stripe_sides_ && !(stripe_sides_ & (1 << s))) continue;
    if (best < 0 || d[s] < d[best]) best = s;
  }
  if (best < 0) best = FL_DOCK_LEFT;
  return best;
}

/**
  Unpins a panel: it leaves the tree, keeping its place there, and slides
  out over the dock from its stripe when shown.  A panel without a stripe
  button is given one on the side it is nearest.
  \return 1 if the panel is now sliding
*/
int Fl_Dock::slide_panel(Fl_Dock_Panel *p) {
  Fl_Dock *o = owner();
  if (!p || !p->dock() || p->dock()->owner() != o) return 0;
  if (p->mode() == FL_DOCK_MODE_SLIDING) return 1;
  o->drop_hidden();
  if (p->stripe_side() < 0) o->stripe_panel(p, (Fl_Dock_Where)o->nearest_side(p));
  if (p->slide_size() <= 0) {
    int frame = 2 * Fl_Dock_Window::frame_size();
    if (p->stripe_side() < 2) p->slide_size(Fl_Dock_Tabs::outer_w(p->w()) + frame);
    else p->slide_size(Fl_Dock_Tabs::outer_h(p->h()) + frame);
  }
  if (p->docked()) {
    Fl_Dock *d = p->dock();
    if (d == o) o->remember_home(p);
    d->take(p);
    o->store_->add(p);
  }
  p->mode(FL_DOCK_MODE_SLIDING);
  o->place_stripes();
  return 1;
}

/**
  Pins a sliding panel: it is docked where it was before it was unpinned,
  or beside the dock on its stripe's side.
  \return 1 if the panel is now docked
*/
int Fl_Dock::pin_panel(Fl_Dock_Panel *p) {
  Fl_Dock *o = owner();
  if (!p || !p->dock() || p->dock()->owner() != o) return 0;
  if (p->mode() != FL_DOCK_MODE_SLIDING) return 0;
  o->drop_hidden();
  if (o->slide_ && o->slide_->dock()->panel(0) == p) o->close_slide();
  p->mode(FL_DOCK_MODE_DOCKED);
  int r;
  if (p->home_ref() || p->home_fraction() > 0) {
    r = o->restore_home(p);
  } else {
    int side = p->stripe_side();
    if (side < 0) side = FL_DOCK_LEFT;
    r = o->add_panel(p, (Fl_Dock_Where)side, 0, 0);
  }
  o->place_stripes();
  return r;
}

// Places the sliding window along its stripe's side of the tree's area.
void Fl_Dock::place_slide() {
  if (!slide_) return;
  int X, Y, W, H;
  area(X, Y, W, H);
  Fl_Dock_Panel *p = slide_->dock()->panel(0);
  int size = 200;
  if (p && p->slide_size() > 0) size = p->slide_size();
  int side = slide_->slide_side();
  if (side < 2) {
    if (size > W) size = W;
    if (side == FL_DOCK_LEFT) slide_->resize(X, Y, size, H);
    else slide_->resize(X + W - size, Y, size, H);
  } else {
    if (size > H) size = H;
    if (side == FL_DOCK_TOP) slide_->resize(X, Y, W, size);
    else slide_->resize(X, Y + H - size, W, size);
  }
}

/**
  Slides a sliding panel out over the dock, beside its stripe, closing the
  one out before.  It slides away again on a click elsewhere in the
  application, other than in a menu, a tooltip or while a modal window is
  up; on Escape that the widget with the focus does not use; or with
  close_slide().
  \return 1 if the panel slid out
*/
int Fl_Dock::open_slide(Fl_Dock_Panel *p) {
  Fl_Dock *o = owner();
  if (!p || p->mode() != FL_DOCK_MODE_SLIDING || !p->dock() || p->dock() != o) return 0;
  if (o->slide_) o->close_slide();
  int side = p->stripe_side();
  if (side < 0) side = FL_DOCK_LEFT;
  o->slide_ = o->new_window(o->x(), o->y(), 100, 100, 2);
  o->slide_->slide_side(side);
  o->slide_->copy_label(p->label());
  o->place_slide();
  o->store_->remove(p);
  o->slide_->dock()->add_panel(p, FL_DOCK_LEFT);
  p->mode(FL_DOCK_MODE_SLIDING);
  // Clicks elsewhere are seen through the event dispatch.
  slide_out = o->slide_;
  return 1;
}

/** Slides the sliding window away, if one is out; its panel stays sliding. */
void Fl_Dock::close_slide() {
  Fl_Dock *o = owner();
  Fl_Dock_Window *win = o->slide_;
  if (!win) return;
  o->slide_ = 0;
  if (slide_out == win) slide_out = 0;
  Fl_Dock_Panel *p = win->dock()->panel(0);
  if (p) {
    if (win->slide_side() < 2) p->slide_size(win->w());
    else p->slide_size(win->h());
    p->hide();
    win->dock()->take(p);
    o->store_->add(p);
    p->mode(FL_DOCK_MODE_SLIDING);
  }
  win->hide();
  win->owner_ = 0;
  win->dock()->owner_ = 0;
  if (win->parent()) win->parent()->remove(win);
  Fl::delete_widget(win);
  o->place_stripes();
}

// Sees every event, to slide a sliding window away on a click outside it and
// outside its own stripe button, and to follow the keyboard focus.
int Fl_Dock::dispatch_focus(int event, Fl_Window *w) {
  // The event's handlers may delete the widget with the focus.
  Fl_Widget_Tracker before(Fl::focus());
  int r = dispatch_event(event, w);
  // The tab strips of the groups the keyboard focus left and entered show it.
  Fl_Widget *focus_before = before.widget();
  Fl_Widget *focus_after = Fl::focus();
  if (focus_after != focus_before) {
    Fl_Widget *ends[2] = { focus_before, focus_after };
    for (int k = 0; k < 2; k++) {
      for (Fl_Widget *c = ends[k]; c; c = c->parent()) {
        if (Fl_Dock::is_tabs(c)) {
          c->damage(FL_DAMAGE_USER1);
          break;
        }
      }
    }
    // The dock is told which panel now has the focus.
    for (Fl_Widget *c = focus_after; c; c = c->parent()) {
      if (Fl_Dock::is_panel(c)) {
        Fl_Dock *d = ((Fl_Dock_Panel *)c)->dock();
        if (d) d->owner()->notify((Fl_Dock_Panel *)c, FL_REASON_GOT_FOCUS);
        break;
      }
    }
  }
  return r;
}

// Adds to X, Y the offsets that take a point in w to its top-level window,
// and returns that window.
static Fl_Window *to_top(Fl_Window *w, int &X, int &Y) {
  while (w && w->window()) {
    X += w->x();
    Y += w->y();
    w = w->window();
  }
  return w;
}

// The dispatch of one event, before the focus is looked at.  A click outside
// the sliding window slides it away, unless a menu, a tooltip or a modal
// window has the mouse.
int Fl_Dock::dispatch_event(int event, Fl_Window *w) {
  Fl_Dock_Window *slide = (Fl_Dock_Window *)slide_out;
  Fl_Dock *dock = 0;
  if (slide) dock = slide->owner();
  int outside = (dock && event == FL_PUSH && w && w != slide && !slide->contains(w));
  if (outside && (Fl::grab() || Fl::modal() || w->menu_window() || w->tooltip_window())) outside = 0;
  // A click in a subwindow may come to the window it is in, as on macOS.
  if (outside) {
    int X = Fl::event_x();
    int Y = Fl::event_y();
    int SX = 0;
    int SY = 0;
    if (to_top(w, X, Y) == to_top(slide, SX, SY) && X >= SX && Y >= SY && X < SX + slide->w() &&
        Y < SY + slide->h())
      outside = 0;
  }
  if (outside) {
    // A click on the panel's own stripe button is left to the button.
    Fl_Dock_Panel *p = slide->dock()->panel(0);
    Fl_Window *top = dock->window();
    int on_button = 0;
    if (top && p && p->stripe_side() >= 0 && dock->stripe_[p->stripe_side()] && (w == top || top->contains(w))) {
      int X = Fl::event_x();
      int Y = Fl::event_y();
      for (Fl_Window *c = w; c && c != top; c = c->window()) {
        X += c->x();
        Y += c->y();
      }
      if (dock->stripe_[p->stripe_side()]->button_at(X, Y) == p) on_button = 1;
    }
    if (!on_button) {
      dock->close_slide();
      if (p) dock->notify(p, FL_REASON_CLOSED);
    }
  }
  if (prev_dispatch) return prev_dispatch(event, w);
  return Fl::handle_(event, w);
}

// Puts the docks' event dispatch in front of the one set, as the first dock
// is made; it slides sliding windows away and follows the keyboard focus.
void Fl_Dock::install_dispatch() {
  static int watching = 0;
  if (!watching) {
    Fl::watch_widget_pointer(slide_out);
    watching = 1;
  }
  if (docks++ > 0 || Fl::event_dispatch() == dispatch) return;
  prev_dispatch = Fl::event_dispatch();
  Fl::event_dispatch(dispatch);
}

// Takes the docks' event dispatch out as the last dock goes, unless the
// application has set another since, which may call it.
void Fl_Dock::remove_dispatch() {
  if (--docks > 0) return;
  if (Fl::event_dispatch() != dispatch) return;
  Fl::event_dispatch(prev_dispatch);
  prev_dispatch = 0;
}

/**
  The event dispatch the docks put in front of the application's, as the
  first dock is made, and take out as the last one goes.  It slides a
  sliding panel away on a click outside it, and follows the keyboard focus
  for the tab strips and FL_REASON_GOT_FOCUS.  An application that sets its
  own Fl::event_dispatch() after making a dock must have its function end by
  calling the one set before it, which it reads with Fl::event_dispatch()
  before setting its own, as FLTK's documentation of Fl::event_dispatch()
  shows for Fl::handle_(); otherwise the docks stop seeing events.  This
  function may also be called from such a function directly.
*/
int Fl_Dock::dispatch(int event, Fl_Window *w) {
  return dispatch_focus(event, w);
}

// ---- Whole-dock commands -------------------------------------------------------

/**
  Hides every docked panel and the sliding window, leaving the center; a
  second call shows them all again exactly as they were.  Changing the dock
  in between, other than by selecting tabs and moving bars, keeps it as it
  is then and forgets what a second call would put back.
  \return 1 if they were hidden, 0 if they were shown again
*/
int Fl_Dock::hide_all() {
  Fl_Dock *o = owner();
  if (o->saved_kind_) {
    char *text = o->saved_layout_;
    o->saved_layout_ = 0;
    o->saved_kind_ = 0;
    o->read_layout(text);
    free(text);
    return 0;
  }
  // Taken before anything changes and kept after, as changing the dock
  // otherwise forgets it.
  int n = o->write_layout(0, 0);
  char *text = (char *)malloc(n + 1);
  o->write_layout(text, n + 1);
  o->close_slide();
  int np;
  Fl_Dock_Panel **all = o->all_panels(np);
  for (int i = 0; i < np; i++) {
    if (all[i]->docked() && all[i]->dock() == o) o->close_panel(all[i]);
  }
  free(all);
  o->place_stripes();
  o->saved_layout_ = text;
  o->saved_kind_ = 1;
  return 1;
}

/**
  Gives one panel's tab group the whole dock: the other panels are hidden,
  and the center too.  A second call, with any panel, or hide_all(), puts
  everything back exactly as it was.  As for hide_all(), changing the dock
  in between forgets what would be put back.
  \return 1 if the panel was maximized, 0 if everything was put back
*/
int Fl_Dock::maximize_panel(Fl_Dock_Panel *p) {
  Fl_Dock *o = owner();
  if (o->saved_kind_) return o->hide_all();
  if (!p || !p->docked() || p->dock() != o) return 0;
  int n = o->write_layout(0, 0);
  char *text = (char *)malloc(n + 1);
  o->write_layout(text, n + 1);
  o->close_slide();
  Fl_Group *keep = p->parent();
  int np;
  Fl_Dock_Panel **all = o->all_panels(np);
  for (int i = 0; i < np; i++) {
    Fl_Dock_Panel *r = all[i];
    if (r->docked() && r->dock() == o && r->parent() != keep) o->close_panel(r);
  }
  free(all);
  // The center leaves the tree while the panel has it all.
  Fl_Widget *c = o->center_;
  if (c && c->parent() && c != o->root_ && is_split(c->parent())) {
    c->hide();
    ((Fl_Dock_Split *)c->parent())->remove_node(c);
    o->normalize();
    o->fit_root();
  }
  o->redraw();
  o->saved_layout_ = text;
  o->saved_kind_ = 2;
  return 1;
}

// ---- Named layouts -------------------------------------------------------------

/**
  Saves the layout in prefs under name, in a group "layouts", so that an
  application can keep several and switch between them.
  \return 1 if it was saved
*/
int Fl_Dock::save_layout(Fl_Preferences &prefs, const char *name) const {
  if (!name || !name[0]) return 0;
  int n = write_layout(0, 0);
  char *text = (char *)malloc(n + 1);
  write_layout(text, n + 1);
  Fl_Preferences layouts(prefs, "layouts");
  int ok = layouts.set(name, text);
  free(text);
  return ok;
}

/**
  Restores the layout saved in prefs under name.
  \return 0 if it was restored, -1 if there is none or it could not be read
*/
int Fl_Dock::load_layout(Fl_Preferences &prefs, const char *name) {
  if (!name || !name[0]) return -1;
  Fl_Preferences layouts(prefs, "layouts");
  if (!layouts.entryExists(name)) return -1;
  char *text = 0;
  layouts.get(name, text, "");
  int r = -1;
  if (text && text[0]) r = read_layout(text);
  free(text);
  return r;
}

// ---- A menu of panels ------------------------------------------------------------

void Fl_Dock::panel_menu_cb(Fl_Widget *, void *data) {
  Fl_Dock_Panel *p = (Fl_Dock_Panel *)data;
  Fl_Dock *d = p->dock();
  if (d) d->owner()->toggle(p, 1);
}

/**
  Puts an item for every panel in menu under path, such as "&View/Panels",
  checked while the panel is shown; choosing one shows or hides it as its
  stripe button does.  The dock keeps the items up to date, and rebuilds
  them when called again, for panels added since.
*/
void Fl_Dock::panel_menu(Fl_Menu_ *menu, const char *path) {
  Fl_Dock *o = owner();
  o->panel_menu_ = menu;
  free(o->panel_menu_path_);
  o->panel_menu_path_ = 0;
  if (!menu || !path) return;
  o->panel_menu_path_ = fl_strdup(path);
  int sub = menu->find_index(path);
  if (sub >= 0) menu->clear_submenu(sub);
  int n;
  Fl_Dock_Panel **all = o->all_panels(n);
  for (int i = 0; i < n; i++) {
    Fl_Dock_Panel *p = all[i];
    if (!p->label()) continue;
    // A slash in a title is not a submenu.
    int len = (int)strlen(path) + 2 * (int)strlen(p->label()) + 2;
    char *item = (char *)malloc(len);
    int k = snprintf(item, len, "%s/", path);
    for (const char *c = p->label(); *c; c++) {
      if (*c == '/' || *c == '\\') item[k++] = '\\';
      item[k++] = *c;
    }
    item[k] = 0;
    menu->add(item, 0, panel_menu_cb, p, FL_MENU_TOGGLE);
    free(item);
  }
  free(all);
  o->update_panel_menu();
}

// Checks the items of the panels that are shown.
void Fl_Dock::update_panel_menu() {
  if (!panel_menu_ || !panel_menu_path_) return;
  const Fl_Menu_Item *items = panel_menu_->menu();
  if (!items) return;
  int n = panel_menu_->size();
  for (int i = 0; i < n; i++) {
    Fl_Menu_Item *it = (Fl_Menu_Item *)&items[i];
    if (it->callback() != panel_menu_cb || !it->user_data()) continue;
    Fl_Dock_Panel *p = (Fl_Dock_Panel *)it->user_data();
    if (panel_shown(p)) it->set();
    else it->clear();
  }
}

// Removes the item of a panel that leaves the dock.
void Fl_Dock::remove_menu_item(Fl_Dock_Panel *p) {
  if (!panel_menu_ || !panel_menu_->menu()) return;
  for (int i = panel_menu_->size() - 1; i >= 0; i--) {
    const Fl_Menu_Item &it = panel_menu_->menu()[i];
    if (it.label() && it.callback() == panel_menu_cb && it.user_data() == p) panel_menu_->remove(i);
  }
}
