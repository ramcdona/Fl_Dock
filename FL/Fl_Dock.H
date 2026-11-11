//
// Dock header file for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#ifndef Fl_Dock_H
#define Fl_Dock_H

#include <FL/Fl_Dock_Export.H>
#include <FL/Fl_Group.H>

class Fl_Dock_Panel;
class Fl_Dock_Tabs;
class Fl_Dock_Split;
class Fl_Dock_Window;
class Fl_Dock_Stripe;

/** The version of Fl_Dock: 10000 * major + 100 * minor + patch. */
#define FL_DOCK_VERSION 100

/** The type() of an Fl_Dock. */
#define FL_DOCK 0xD3

/**
  Where a panel goes relative to another, for Fl_Dock::add_panel().
*/
enum Fl_Dock_Where {
  FL_DOCK_LEFT = 0,     ///< beside it, on the left
  FL_DOCK_RIGHT,        ///< beside it, on the right
  FL_DOCK_TOP,          ///< above it
  FL_DOCK_BOTTOM,       ///< below it
  FL_DOCK_TAB           ///< as a tab of its tab group
};

/**
  The sides of a dock that may show stripes, or'ed together.
  \see Fl_Dock::stripes()
*/
enum Fl_Dock_Sides {
  FL_DOCK_SIDE_LEFT   = 1,      ///< the left side
  FL_DOCK_SIDE_RIGHT  = 2,      ///< the right side
  FL_DOCK_SIDE_TOP    = 4,      ///< the top
  FL_DOCK_SIDE_BOTTOM = 8,      ///< the bottom
  FL_DOCK_SIDE_ALL    = 15      ///< all four
};

/**
  How the stripes behave, or'ed together.
  \see Fl_Dock::stripe_options()
*/
enum Fl_Dock_Stripe_Options {
  FL_DOCK_STRIPE_FOCUS_FIRST = 1,   ///< clicking the button of a shown panel without the keyboard focus gives it the focus, rather than hiding it
  FL_DOCK_STRIPE_SHARED      = 2,   ///< panels in one group of a stripe may be shown together; otherwise showing one hides the others
  FL_DOCK_STRIPE_SHOW_EMPTY  = 4    ///< a stripe with no buttons is shown; otherwise it takes no space
};

/**
  Arranges an application's panels, which the user can drag to dock them side
  by side, stack them as tabs, float them in windows of their own and close
  them, and saves and restores the arrangement.

  An Fl_Dock is a group holding a tree: its inner nodes are Fl_Dock_Split
  groups that lay their children side by side or one above another, and its
  leaves are Fl_Dock_Tabs groups that stack Fl_Dock_Panel widgets as tabs.  A
  dock may also have a center, one widget such as a 3D view, which is a leaf of
  its own and is never closed or floated.  After every change the tree is put
  in a normal form: no empty tab groups, no split with one child, and no split
  inside a split of the same direction.

  A dock and the windows its panels float in share their panels: find() and
  write_layout() cover them all.

  \code
  Fl_Dock *dock = new Fl_Dock(0, 25, 800, 575);
  dock->center(view);
  dock->add_panel(files, FL_DOCK_LEFT, 0, 0.25);      // left of everything
  dock->add_panel(props, FL_DOCK_BOTTOM, files, 0.5); // below files
  dock->add_panel(notes, FL_DOCK_TAB, props);         // a tab beside props
  \endcode

  The dock's callback tells the application what the user did, as for other
  FLTK widgets: changes made by calling the dock's functions do not call it.
  Each change sets the dock's changed() flag, and calls the callback, with
  Fl::callback_reason() set and callback_panel() returning the panel, if
  when() has FL_WHEN_CHANGED or FL_WHEN_RELEASE, the default.  The reasons:

  - FL_REASON_DRAGGED: the user moved a panel: dropped it after a drag,
    floated, inset or docked it from a menu, unpinned or pinned it, moved
    its stripe button, or moved its inset window.
  - FL_REASON_CLOSED: the user closed a panel, from its tab, its window, its
    stripe button or its shortcut, or a sliding panel slid away.
  - FL_REASON_OPENED: the user showed a closed panel, or slid one out.
  - FL_REASON_SELECTED: the user selected a panel's tab.
  - FL_REASON_GOT_FOCUS: a panel took the keyboard focus.
  - FL_REASON_CHANGED: the user resized something: dragged a bar between two
    parts of the dock, for which callback_panel() is 0, resized an inset or
    sliding window.

  Dragging a bar or resizing an inset window calls the callback as it moves
  with FL_WHEN_CHANGED and when it is let go with FL_WHEN_RELEASE.  An
  application that saves its layout when it changes can save on every call.

  The dock deletes its panels, open or closed, its center and its floating
  windows with itself.  remove_panel() gives a panel back to the
  application, and deleting a panel is safe at any time.  A layout may be
  read before the dock's window is shown; floating windows show and hide
  with that window.  The docks see every event through the function
  dispatch(), which an application that sets its own Fl::event_dispatch()
  must call.
*/
class FL_DOCK_EXPORT Fl_Dock : public Fl_Group {
public:
  /**
    Called for a panel name in a layout that the dock does not know, so the
    application can create the panel; return 0 to leave it out.
  */
  typedef Fl_Dock_Panel *(*Panel_Factory)(Fl_Dock *dock, const char *name, void *data);

  /**
    The deepest tree a layout may hold, and so the deepest a dock builds:
    add_panel() refuses a panel that would make the tree deeper.
  */
  enum { MAX_DEPTH = 1000 };

private:
  Fl_Widget *root_;             // the tree's root node, or 0
  Fl_Widget *center_;           // the center widget, or 0
  Fl_Group *store_;             // hidden: closed panels
  Fl_Dock *owner_;              // the dock this one's window was floated from, or 0
  Fl_Dock_Window **windows_;    // floating windows, for the owning dock
  int nwindows_;
  Fl_Dock_Panel *callback_panel_;
  Panel_Factory factory_;
  void *factory_data_;
  struct Kept;                  // a place kept for a panel the dock does not have yet
  Kept *kept_;
  int nkept_;
  Fl_Dock_Stripe *stripe_[4];   // by Fl_Dock_Where, or 0
  int stripe_sides_;
  int stripe_options_;
  Fl_Dock_Window *slide_;       // the sliding window shown, or 0
  int keep_sizes_;              // 1 if panels keep their sizes as the dock is resized
  int reading_;                 // 1 while a layout is read
  int has_center_;              // 1 while a center is set; center_ is cleared if it is deleted
  int destroying_;              // 1 while the dock deletes its children

  int can_take(const Fl_Dock_Panel *p) const;
  Fl_Dock_Panel **all_panels(int &n) const;
  Fl_Widget *node_of(Fl_Widget *ref) const;
  void put(Fl_Widget *node, Fl_Dock_Where where, Fl_Widget *target, double fraction, int share = 0);
  int dock_at(Fl_Dock_Panel *p, Fl_Dock_Where where, Fl_Widget *target, double fraction, int share);
  int restore(Fl_Dock_Panel *p, const char *ref, int where, double share, int relative = 0);
  int restore_home(Fl_Dock_Panel *p);
  void take(Fl_Dock_Panel *p);
  void forget(Fl_Dock_Panel *p);
  void detach_center();
  void child_gone();
  static void repair_cb(void *data);
  void normalize();
  static Fl_Widget *normal_form(Fl_Widget *node, Fl_Widget *center);
  void set_root(Fl_Widget *node);
  void fit_root(int keep = 0);
  void remember_home(Fl_Dock_Panel *p);
  void refresh();
  void notify(Fl_Dock_Panel *p, Fl_Callback_Reason reason, int step = 0);
  int toggle(Fl_Dock_Panel *p, int user);
  void add_window(Fl_Dock_Window *w);
  void remove_window(Fl_Dock_Window *w);
  Fl_Dock_Window *new_window(int X, int Y, int W, int H, int kind);
  void show_window(Fl_Dock_Window *win);
  Fl_Dock_Window *float_at(Fl_Dock_Panel *p, int given, int X, int Y, int W, int H);
  Fl_Dock_Window *inset_at(Fl_Dock_Panel *p, int given, int X, int Y, int W, int H);
  Fl_Widget *build_node(void *node);
  void keep_missing(void *node, void *parent_node, int index);
  void keep_place(const char *name, const char *ref, int where, double fraction);
  int kept_place(const char *name, const char **ref, int *where, double *fraction) const;
  void clear_kept();
  void place_stripes();
  void place_slide();
  void hide_others_in_group(Fl_Dock_Panel *p, int user);
  int nearest_side(Fl_Dock_Panel *p) const;
  static int dispatch_focus(int event, Fl_Window *w);
  static int dispatch_event(int event, Fl_Window *w);
  static void install_dispatch();
  static void remove_dispatch();

  friend class Fl_Dock_Tabs;
  friend class Fl_Dock_Window;
  friend class Fl_Dock_Drag;
  friend class Fl_Dock_Stripe;
  friend class Fl_Dock_Split;
  friend class Fl_Dock_Panel;

public:
  Fl_Dock(int X, int Y, int W, int H, const char *L = 0);
  ~Fl_Dock() FL_OVERRIDE;

  void resize(int X, int Y, int W, int H) FL_OVERRIDE;
  void draw() FL_OVERRIDE;
  int handle(int event) FL_OVERRIDE;

protected:
  void on_remove(int index) FL_OVERRIDE;

public:
  /** Returns the root node of the tree, or 0 if the dock is empty. */
  Fl_Widget *root() const { return root_; }

  /** Returns the center widget, or 0. */
  Fl_Widget *center() const { return center_; }
  void center(Fl_Widget *w);

  int add_panel(Fl_Dock_Panel *p, Fl_Dock_Where where = FL_DOCK_LEFT, Fl_Widget *ref = 0, double fraction = 0.25);
  int close_panel(Fl_Dock_Panel *p);
  int remove_panel(Fl_Dock_Panel *p);
  int show_panel(Fl_Dock_Panel *p);
  int place_panel(Fl_Dock_Panel *p, Fl_Dock_Where where = FL_DOCK_LEFT, Fl_Widget *ref = 0, double fraction = 0.25);
  int store_panel(Fl_Dock_Panel *p, Fl_Dock_Where where = FL_DOCK_LEFT, Fl_Widget *ref = 0, double fraction = 0.25);
  Fl_Dock_Window *float_panel(Fl_Dock_Panel *p);
  Fl_Dock_Window *float_panel(Fl_Dock_Panel *p, int X, int Y, int W = 0, int H = 0);
  Fl_Dock_Window *inset_panel(Fl_Dock_Panel *p);
  Fl_Dock_Window *inset_panel(Fl_Dock_Panel *p, int X, int Y, int W = 0, int H = 0);
  int dock_panel(Fl_Dock_Panel *p);

  Fl_Dock_Panel *find(const char *name) const;
  int panels() const;
  Fl_Dock_Panel *panel(int i) const;

  /** Returns the dock this one belongs to: itself, or the dock its floating window was floated from. */
  Fl_Dock *owner() { if (owner_) return owner_; return this; }
  /** Returns the dock this one belongs to. */
  const Fl_Dock *owner() const { if (owner_) return owner_; return this; }
  /** Returns the number of floating windows. */
  int windows() const { return nwindows_; }
  /** Returns floating window i, or 0. */
  Fl_Dock_Window *window_at(int i) const {
    if (i < 0 || i >= nwindows_) return 0;
    return windows_[i];
  }

  int write_layout(char *buf, int size) const;
  int read_layout(const char *text);

  void stripes(int sides);
  /** Returns the sides that show stripes, Fl_Dock_Sides or'ed together. */
  int stripes() const { return stripe_sides_; }
  /** Sets how the stripes behave, Fl_Dock_Stripe_Options or'ed together. */
  void stripe_options(int o) { stripe_options_ = o; }
  /** Returns how the stripes behave. */
  int stripe_options() const { return stripe_options_; }
  Fl_Dock_Stripe *stripe(int side) const;
  int stripe_panel(Fl_Dock_Panel *p, Fl_Dock_Where side, int group = -1, int index = -1);
  int unstripe_panel(Fl_Dock_Panel *p);
  int toggle_panel(Fl_Dock_Panel *p);
  int panel_shown(const Fl_Dock_Panel *p) const;
  int slide_panel(Fl_Dock_Panel *p);
  int pin_panel(Fl_Dock_Panel *p);
  int open_slide(Fl_Dock_Panel *p);
  void close_slide();
  /** Returns the sliding window shown, or 0. */
  Fl_Dock_Window *sliding() const { return slide_; }
  void area(int &X, int &Y, int &W, int &H) const;

  /**
    Sets whether docked panels keep their sizes as the dock is resized, as
    in most applications with docking: the part of the dock holding the
    center grows and shrinks, and the panels beside it keep their widths and
    heights until the center would get less than a third of the room.  When
    off, every part keeps its share of the dock.  On by default.  It applies
    to restored layouts too: a layout saved at another size is restored with
    the panels at the sizes they had.
  */
  void keep_sizes(int on) { owner()->keep_sizes_ = on; }
  /** Returns 1 if docked panels keep their sizes as the dock is resized. */
  int keep_sizes() const { return owner()->keep_sizes_; }

  /** Sets the function that creates panels a layout names but the dock does not know. */
  void panel_factory(Panel_Factory f, void *data = 0) { factory_ = f; factory_data_ = data; }

  /** Returns the panel the last callback was about, or 0 if it was about none. */
  Fl_Dock_Panel *callback_panel() const { return callback_panel_; }

  static int dispatch(int event, Fl_Window *w);

  static int is_split(const Fl_Widget *w);
  static int is_tabs(const Fl_Widget *w);
  static int is_panel(const Fl_Widget *w);
  static int api_version();
};

#endif // !Fl_Dock_H
