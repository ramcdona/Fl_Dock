//
// Dock split header file for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#ifndef Fl_Dock_Split_H
#define Fl_Dock_Split_H

#include <FL/Fl_Dock_Export.H>
#include <FL/Fl_Group.H>

/** The type() of an Fl_Dock_Split. */
#define FL_DOCK_SPLIT 0xD0

/**
  A node of an Fl_Dock's tree that lays its children out side by side or
  one above another, with a bar between each pair that the user can drag.

  Each child has a weight, its share of the space; the weights add up to 1.
  Resizing the split keeps the shares.  Fl_Dock creates and removes splits
  as panels are docked and closed; an application only meets them when it
  walks the tree.
*/
class FL_DOCK_EXPORT Fl_Dock_Split : public Fl_Group {
  int horizontal_;
  double *weights_;
  int nweights_;
  int dragging_;        // index of the bar being dragged, or -1
  int drag_offset_;

  static Fl_Widget *keep_center_;      // while set, splits holding it keep their other children's sizes

  int bar_at(int X, int Y) const;
  void sync_weights();
  void keep_sizes(int center_index, int total);
  void insert_node(Fl_Widget *w, int index, double weight, Fl_Widget *from = 0);
  void remove_node(Fl_Widget *w, Fl_Widget *to = 0);
  void replace_node(Fl_Widget *old_node, Fl_Widget *new_node);
  void absorb(Fl_Dock_Split *child_split);
  friend class Fl_Dock;

public:
  Fl_Dock_Split(int X, int Y, int W, int H, int horizontal);
  ~Fl_Dock_Split() FL_OVERRIDE;

  void resize(int X, int Y, int W, int H) FL_OVERRIDE;
  int handle(int event) FL_OVERRIDE;
  void draw() FL_OVERRIDE;

protected:
  void on_remove(int index) FL_OVERRIDE;

public:

  /** Returns 1 if the children are side by side, 0 if one above another. */
  int horizontal() const { return horizontal_; }

  double weight(int i) const;
  void weight(int i, double w);
  void layout();

  static int bar_size();
  static void bar_size(int s);
  static int min_size();
  static void min_size(int s);
};

#endif // !Fl_Dock_Split_H
