//
// Dock panel header file for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#ifndef Fl_Dock_Panel_H
#define Fl_Dock_Panel_H

#include <FL/Fl_Dock_Export.H>
#include <FL/Fl_Group.H>

class Fl_Dock;

/**
  Flags of an Fl_Dock_Panel, or'ed together.
  \see Fl_Dock_Panel::dock_flags()
*/
enum Fl_Dock_Flags {
  FL_DOCK_CLOSABLE  = 1,        ///< the user can close the panel
  FL_DOCK_FLOATABLE = 2,        ///< the user can float the panel in a window of its own
  FL_DOCK_MOVABLE   = 4,        ///< the user can drag the panel to another place
  FL_DOCK_DEFAULT_FLAGS = 7     ///< all of the above
};

/**
  How a panel is shown: docked in the dock's tree, in an inset window inside
  the dock's window, or in a floating window.  A
  closed panel keeps the mode it had, and Fl_Dock::show_panel() opens it that
  way again.
  \see Fl_Dock_Panel::mode()
*/
enum Fl_Dock_Mode {
  FL_DOCK_MODE_DOCKED = 0,      ///< in the dock's tree
  FL_DOCK_MODE_INSET,           ///< in an inset window, inside the dock's window
  FL_DOCK_MODE_FLOATING,        ///< in a floating window of its own
};

/** The type() of an Fl_Dock_Panel. */
#define FL_DOCK_PANEL 0xD2

/**
  A panel that the user can dock, tab, float and close.

  An Fl_Dock_Panel is an Fl_Group holding the application's widgets.  Its
  label is its title, shown on its tab.  Its name identifies it in saved
  layouts and must be unique and stable: it is the label unless name() is
  set, which an application whose labels are translated should do.

  \code
  Fl_Dock_Panel *files = new Fl_Dock_Panel(0, 0, 200, 300, "Files");
  files->name("files");
  // ... the panel's widgets ...
  files->end();
  \endcode  It is resized like any group, so
  give it a resizable() child as usual.

  Panels are created by the application and handed to an Fl_Dock with
  Fl_Dock::add_panel().  The dock moves them between its tab groups and windows
  but never deletes them while they belong to it; it deletes them with
  itself.  Fl_Dock::remove_panel() gives a panel back to the application,
  and deleting a panel, as any FLTK child may be deleted, takes it out of its
  dock first.
*/
class FL_DOCK_EXPORT Fl_Dock_Panel : public Fl_Group {
  char *name_;
  int dock_flags_;
  // Where the panel was before it was closed or floated, so it can go back.
  char *home_ref_;
  int home_where_;
  double home_fraction_;
  int home_relative_;
  char *home_ref2_;     // the neighbour on the other side, used when home_ref_ is gone
  int home_where2_;
  int home_edge_;       // 1 if the panel was at the end of its split, past home_ref()
  char *home_span_;     // the far end of the part home_ref() is the near end of, for home_edge_
  int mode_;
  int inset_rect_[4];   // the inset window it was last in, relative to the dock; w 0 if none
  int float_rect_[4];   // the floating window it was last in, on the screen; w 0 if none

  void fix_name();
  // Where the panel goes back to: next to the panel named home_ref(), the
  // center if it is "", or the whole dock if 0, on side home_where(), with
  // home_fraction() of the space; home_relative() is 1 if that is a
  // fraction of home_ref()'s space, as add_panel() takes, 0 if a share of
  // its split.  home_ref2() is the neighbour on the other side, used if
  // home_ref() is gone.
  const char *home_ref() const { return home_ref_; }
  int home_where() const { return home_where_; }
  double home_fraction() const { return home_fraction_; }
  int home_relative() const { return home_relative_; }
  void home(const char *ref, int where, double fraction, int relative = 0);
  const char *home_ref2() const { return home_ref2_; }
  int home_where2() const { return home_where2_; }
  void home2(const char *ref, int where);
  /** Returns 1 if the panel goes back to the end of the split it was in, past home_ref() and whatever has joined it. */
  int home_edge() const { return home_edge_; }
  /** \internal Returns the panel at the far end of the part next to the panel, "" for the center, or 0. */
  const char *home_span() const { return home_span_; }
  void home_edge(int e, const char *span = 0);
  friend class Fl_Dock;

public:
  Fl_Dock_Panel(int X, int Y, int W, int H, const char *L = 0);
  ~Fl_Dock_Panel() FL_OVERRIDE;

  const char *name() const;
  void name(const char *n);

  /** Returns the panel's Fl_Dock_Flags. */
  int dock_flags() const { return dock_flags_; }
  /** Sets the panel's Fl_Dock_Flags. */
  void dock_flags(int f) { dock_flags_ = f; }

  Fl_Dock *dock() const;
  int docked() const;

  /** Returns how the panel is shown, or how it opens again if it is closed: an Fl_Dock_Mode. */
  int mode() const { return mode_; }
  /** Sets how a closed panel opens again: an Fl_Dock_Mode.  Fl_Dock sets it as the panel moves. */
  void mode(int m) { mode_ = m; }
  void inset_rect(int X, int Y, int W, int H);
  int get_inset_rect(int &X, int &Y, int &W, int &H) const;
  void float_rect(int X, int Y, int W, int H);
  int get_float_rect(int &X, int &Y, int &W, int &H) const;

  /**
    Called when the user asks to close the panel; return 0 to refuse.  The
    default allows it.
  */
  virtual int can_close() { return 1; }
};

#endif // !Fl_Dock_Panel_H
