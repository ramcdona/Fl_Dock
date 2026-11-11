//
// Dock window header file for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#ifndef Fl_Dock_Window_H
#define Fl_Dock_Window_H

#include <FL/Fl_Dock_Export.H>
#include <FL/Fl_Double_Window.H>

class Fl_Dock;

/**
  A window holding panels floated out of an Fl_Dock.

  It holds a dock of its own, so panels can be docked into it beside and on
  top of each other as in the main one.  It belongs to the dock it was
  floated from, closes when its last panel leaves, and closing it closes
  its panels.  Applications get one from Fl_Dock::float_panel() or
  Fl_Dock::inset_panel() and do not create it themselves.

  A floating window is a top-level window that moves freely.  An inset
  window instead floats inside the window of the dock it belongs to: it is
  a subwindow with a frame of its own and no title bar: the user moves it by
  its tab strip, beside the tabs, and resizes it within the dock's area,
  and it moves, hides and minimizes with that window.  A sliding window is an inset window holding an unpinned
  panel, along the side of the dock where its stripe is, which hides again
  when the user clicks elsewhere.
*/
class FL_DOCK_EXPORT Fl_Dock_Window : public Fl_Double_Window {
  // What a window is for.
  enum Kind {
    FLOATING = 0,       // a top-level window
    INSET,              // a subwindow of the owner's window
    SLIDING             // a subwindow holding a panel slid out from a stripe
  };
  Fl_Dock *dock_;
  Fl_Dock *owner_;
  Kind kind_;
  int slide_side_;      // for a sliding window, the side of its stripe
  int action_;          // what a press on the title bar or frame does
  int press_x_, press_y_, press_w_, press_h_, press_mx_, press_my_;

  static void close_cb(Fl_Widget *w, void *data);
  friend class Fl_Dock;
  int action_at(int X, int Y) const;
  void parent_mouse(int &X, int &Y) const;
  Fl_Dock_Window(int X, int Y, int W, int H, Fl_Dock *owner, Kind kind);
  void slide_side(int s) { slide_side_ = s; }

public:
  ~Fl_Dock_Window() FL_OVERRIDE;

  int handle(int event) FL_OVERRIDE;
  void draw() FL_OVERRIDE;

  /** Returns the dock in this window. */
  Fl_Dock *dock() const { return dock_; }
  /** Returns the dock this window was floated from. */
  Fl_Dock *owner() const { return owner_; }
  /** Returns 1 for an inset window, inside the owner's window, 0 for a floating or sliding one. */
  int inset() const { return kind_ == INSET; }
  /** Returns 1 for a sliding window, holding an unpinned panel beside its stripe. */
  int sliding() const { return kind_ == SLIDING; }
  /** Returns the side of the dock a sliding window slides out from, an Fl_Dock_Where. */
  int slide_side() const { return slide_side_; }
  void keep_inside();

  static int frame_size();
};

#endif // !Fl_Dock_Window_H
