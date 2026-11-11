//
// Unit tests for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

// These tests build docks in windows that are never shown, so they need no
// display and run anywhere, as CTest does.  Layouts are compared as text:
// the text a dock writes is the record of its tree.

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Dock.H>
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock_Split.H>
#include <FL/Fl_Dock_Tabs.H>
#include <FL/Fl_Dock_Window.H>
#include "../src/Fl_Dock_Drag.H"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- A small test harness, in the style of FLTK's unittests ---------------

typedef bool (*Test_Function)();

struct Test_Case {
  const char *suite;
  const char *name;
  Test_Function fn;
};

static const int MAX_TEST_CASES = 400;
static Test_Case test_cases[MAX_TEST_CASES];
static int num_test_cases = 0;

struct Test_Register {
  Test_Register(const char *suite, const char *name, Test_Function fn) {
    if (num_test_cases >= MAX_TEST_CASES) {
      fprintf(stderr, "more than %d test cases\n", MAX_TEST_CASES);
      exit(1);
    }
    test_cases[num_test_cases].suite = suite;
    test_cases[num_test_cases].name = name;
    test_cases[num_test_cases].fn = fn;
    num_test_cases++;
  }
};

#define TEST(SUITE, CASE) \
  static bool test_##SUITE##_##CASE(); \
  static Test_Register register_##SUITE##_##CASE(#SUITE, #CASE, test_##SUITE##_##CASE); \
  static bool test_##SUITE##_##CASE()

#define EXPECT_TRUE(COND) \
  do { \
    if (!(COND)) { \
      fprintf(stderr, "%s:%d: expected %s\n", __FILE__, __LINE__, #COND); \
      return false; \
    } \
  } while (0)

#define EXPECT_EQ(A, B) \
  do { \
    long a_ = (long)(A); \
    long b_ = (long)(B); \
    if (a_ != b_) { \
      fprintf(stderr, "%s:%d: %s is %ld, expected %ld\n", __FILE__, __LINE__, #A, a_, b_); \
      return false; \
    } \
  } while (0)

#define EXPECT_NEAR(A, B, TOL) \
  do { \
    double a_ = (A); \
    double b_ = (B); \
    if (fabs(a_ - b_) > (TOL)) { \
      fprintf(stderr, "%s:%d: %s is %g, expected %g\n", __FILE__, __LINE__, #A, a_, b_); \
      return false; \
    } \
  } while (0)

#define EXPECT_STREQ(A, B) \
  do { \
    const char *a_ = (A); \
    const char *b_ = (B); \
    if (!a_ || !b_ || strcmp(a_, b_)) { \
      fprintf(stderr, "%s:%d: %s is\n%s\nexpected\n%s\n", __FILE__, __LINE__, #A, a_, b_); \
      return false; \
    } \
  } while (0)

// ---- Fixture ---------------------------------------------------------------

static Fl_Window *win = 0;
static Fl_Dock *dock = 0;
static Fl_Box *view = 0;

static Fl_Dock_Panel *make_panel(const char *name) {
  Fl_Group::current(0);
  Fl_Dock_Panel *p = new Fl_Dock_Panel(0, 0, 100, 100);
  p->copy_label(name);
  p->end();
  return p;
}

// A dock of 800 by 600 holding a center view.
static void setup(int with_center) {
  if (win) {
    Fl::do_widget_deletion();
    delete win;
  }
  Fl_Group::current(0);
  win = new Fl_Window(800, 600);
  dock = new Fl_Dock(0, 0, 800, 600);
  win->end();
  win->resizable(dock);
  view = 0;
  if (with_center) {
    Fl_Group::current(0);
    view = new Fl_Box(0, 0, 10, 10, "view");
    dock->center(view);
  }
}

// The dock's layout, kept in a static buffer.
static const char *text() {
  static char buf[4096];
  dock->write_layout(buf, sizeof(buf));
  return buf;
}

// True if the dock reads back the layout it writes, and writes it again the
// same.
static char *written() {
  int n = dock->write_layout(0, 0);
  char *buf = (char *)malloc(n + 1);
  dock->write_layout(buf, n + 1);
  return buf;
}

static bool rereads() {
  char *before = written();
  bool ok = true;
  if (dock->read_layout(before) != 0) {
    fprintf(stderr, "own layout not read:\n%s", before);
    ok = false;
  }
  Fl::do_widget_deletion();
  char *after = written();
  if (ok && strcmp(after, before)) {
    fprintf(stderr, "own layout read as\n%sfrom\n%s", after, before);
    ok = false;
  }
  free(before);
  free(after);
  return ok;
}

// Sends a mouse event to w at X, Y with the left button.
static int mouse(Fl_Widget *w, int event, int X, int Y) {
  Fl::e_x = X;
  Fl::e_y = Y;
  Fl::e_keysym = FL_Button + FL_LEFT_MOUSE;
  Fl::e_state = FL_BUTTON1;
  if (event == FL_RELEASE) Fl::e_state = 0;
  return w->handle(event);
}

// The standard arrangement: a left of everything, b below a, c a tab of b.
static void arrange() {
  setup(1);
  dock->add_panel(make_panel("a"), FL_DOCK_LEFT, 0, 0.25);
  dock->add_panel(make_panel("b"), FL_DOCK_BOTTOM, dock->find("a"), 0.5);
  dock->add_panel(make_panel("c"), FL_DOCK_TAB, dock->find("b"));
}

static const char *ARRANGED =
  "fl_dock 1\n"
  "dock\n"
  " split h 0.25 0.75\n"
  "  split v 0.5 0.5\n"
  "   tabs 0 a\n"
  "   tabs 1 b c\n"
  "  center\n"
  "size 800 600\n";

// True if no tab group is empty, no split has one child, and no split holds
// a split of the same direction.
static bool normal(Fl_Widget *n) {
  if (Fl_Dock::is_tabs(n)) return ((Fl_Group *)n)->children() > 0;
  if (!Fl_Dock::is_split(n)) return true;
  Fl_Dock_Split *s = (Fl_Dock_Split *)n;
  if (s->children() < 2) return false;
  double sum = 0;
  for (int i = 0; i < s->children(); i++) {
    Fl_Widget *c = s->child(i);
    if (Fl_Dock::is_split(c) && ((Fl_Dock_Split *)c)->horizontal() == s->horizontal()) return false;
    if (!normal(c)) return false;
    sum += s->weight(i);
  }
  return fabs(sum - 1) < 1e-4;
}

// ---- Tree ------------------------------------------------------------------

TEST(Tree, Arrange) {
  arrange();
  EXPECT_STREQ(text(), ARRANGED);
  EXPECT_EQ(dock->panels(), 3);
  EXPECT_TRUE(normal(dock->root()));
  // Child sizes follow the shares.
  Fl_Dock_Split *s = (Fl_Dock_Split *)dock->root();
  EXPECT_EQ(s->child(0)->w(), (int)(0.25 * (800 - Fl_Dock_Split::bar_size()) + 0.5));
  EXPECT_EQ(s->child(1)->x() + s->child(1)->w(), 800);
  return true;
}

TEST(Tree, CloseNormalizes) {
  arrange();
  dock->close_panel(dock->find("a"));
  EXPECT_TRUE(normal(dock->root()));
  EXPECT_STREQ(text(),
    "fl_dock 1\n"
    "dock\n"
    " split h 0.25 0.75\n"
    "  tabs 1 b c\n"
    "  center\n"
    "closed a\n"
    "size 800 600\n");
  EXPECT_EQ(dock->panels(), 3);
  EXPECT_TRUE(!dock->find("a")->docked());
  EXPECT_TRUE(!dock->find("a")->visible());
  return true;
}

TEST(Tree, CloseShowReturns) {
  arrange();
  const char *names[] = { "a", "b", "c" };
  for (int i = 0; i < 3; i++) {
    dock->close_panel(dock->find(names[i]));
    dock->show_panel(dock->find(names[i]));
    EXPECT_TRUE(dock->find(names[i])->docked());
    EXPECT_TRUE(normal(dock->root()));
  }
  // a and b go back beside their neighbours, c as a tab of b; the tab shown
  // may differ, so compare the shape.
  char buf[4096];
  dock->write_layout(buf, sizeof(buf));
  EXPECT_TRUE(strstr(buf, "tabs 0 a\n") != 0);
  EXPECT_TRUE(strstr(buf, " b c\n") != 0);
  EXPECT_TRUE(strstr(buf, "closed") == 0);
  return true;
}

TEST(Tree, ApiVersion) {
  // The library is the version of the headers it was built with.
  EXPECT_EQ(Fl_Dock::api_version(), FL_DOCK_API_VERSION);
  EXPECT_EQ(FL_DOCK_API_VERSION, FL_DOCK_MAJOR_VERSION * 10000 + FL_DOCK_MINOR_VERSION * 100 + FL_DOCK_PATCH_VERSION);
  return true;
}

TEST(Tree, EdgePanelReturnsToEdge) {
  // A panel at the end of its split goes back to the end, past a panel
  // docked beside the center while it was closed.
  setup(1);
  dock->add_panel(make_panel("b"), FL_DOCK_RIGHT, dock->center(), 0.25);
  dock->close_panel(dock->find("b"));
  dock->add_panel(make_panel("v"), FL_DOCK_RIGHT, dock->center(), 0.25);
  dock->show_panel(dock->find("b"));
  Fl_Dock_Panel *b = dock->find("b");
  Fl_Dock_Panel *v = dock->find("v");
  EXPECT_TRUE(b->x() > v->x());
  EXPECT_EQ(b->parent()->x() + b->parent()->w(), dock->x() + dock->w());
  // Likewise at the start.
  setup(1);
  dock->add_panel(make_panel("b"), FL_DOCK_LEFT, dock->center(), 0.25);
  dock->close_panel(dock->find("b"));
  dock->add_panel(make_panel("v"), FL_DOCK_LEFT, dock->center(), 0.25);
  dock->show_panel(dock->find("b"));
  EXPECT_TRUE(dock->find("b")->x() < dock->find("v")->x());
  EXPECT_EQ(dock->find("b")->parent()->x(), dock->x());
  return true;
}

TEST(Tree, EdgePanelReturnsBesideItsPart) {
  // A panel at the end of a split of two goes back beside the part it was
  // beside, not to the end of a split farther out.
  setup(1);
  dock->add_panel(make_panel("log"), FL_DOCK_BOTTOM, view, 0.25);
  dock->add_panel(make_panel("gl"), FL_DOCK_RIGHT, view, 0.25);
  dock->add_panel(make_panel("shape"), FL_DOCK_BOTTOM, dock->find("gl"), 0.5);
  char *before = strdup(text());
  dock->close_panel(dock->find("shape"));
  dock->show_panel(dock->find("shape"));
  EXPECT_STREQ(text(), before);
  free(before);
  // One that spanned a whole part goes back across all of it.
  setup(0);
  dock->add_panel(make_panel("a"), FL_DOCK_LEFT, 0, 0.5);
  dock->add_panel(make_panel("b"), FL_DOCK_RIGHT, dock->find("a"), 0.5);
  dock->add_panel(make_panel("p"), FL_DOCK_BOTTOM, 0, 0.25);
  before = strdup(text());
  dock->close_panel(dock->find("p"));
  dock->show_panel(dock->find("p"));
  EXPECT_STREQ(text(), before);
  free(before);
  return true;
}

TEST(Tree, CloseShowKeepsSizes) {
  // Closing and showing a panel in a split of three puts every size back.
  setup(1);
  Fl_Dock_Panel *a = make_panel("a");
  Fl_Dock_Panel *b = make_panel("b");
  dock->add_panel(a, FL_DOCK_LEFT, 0, 0.2);
  dock->add_panel(b, FL_DOCK_RIGHT, 0, 0.3);
  dock->root()->resize(0, 0, 800, 600);
  int aw = a->w();
  int bw = b->w();
  int vw = view->w();
  for (int k = 0; k < 3; k++) {
    dock->close_panel(b);
    dock->show_panel(b);
    dock->close_panel(a);
    dock->show_panel(a);
  }
  EXPECT_EQ(a->w(), aw);
  EXPECT_EQ(b->w(), bw);
  EXPECT_EQ(view->w(), vw);
  return true;
}

// Resizes the window and its dock, as the user does.
static void resize_dock(int W, int H) {
  win->size(W, H);
  dock->resize(0, 0, W, H);
}

TEST(Tree, PanelsKeepSizes) {
  // The center takes what the window gains or loses; the panels keep their
  // sizes until the center would have less than a third.
  arrange();
  dock->add_panel(make_panel("d"), FL_DOCK_BOTTOM, view, 0.3);
  Fl_Dock_Panel *a = dock->find("a");
  Fl_Dock_Panel *d = dock->find("d");
  int aw = a->w();
  int dh = d->h();
  resize_dock(1600, 1200);
  EXPECT_EQ(a->w(), aw);
  EXPECT_EQ(d->h(), dh);
  EXPECT_TRUE(normal(dock->root()));
  resize_dock(700, 500);
  EXPECT_EQ(a->w(), aw);
  EXPECT_EQ(d->h(), dh);
  // Too small: the panels shrink in proportion, the center keeps a third.
  resize_dock(300, 200);
  EXPECT_TRUE(a->w() < aw);
  EXPECT_NEAR(view->w(), (300 - Fl_Dock_Split::bar_size()) / 3.0, 2);
  EXPECT_TRUE(normal(dock->root()));
  EXPECT_TRUE(rereads());
  // Without keep_sizes() every part keeps its share.
  setup(1);
  dock->keep_sizes(0);
  dock->add_panel(make_panel("a"), FL_DOCK_LEFT, 0, 0.25);
  resize_dock(1600, 1200);
  EXPECT_NEAR(dock->find("a")->parent()->w(), 0.25 * (1600 - Fl_Dock_Split::bar_size()), 1);
  resize_dock(800, 600);
  return true;
}

TEST(Tree, BorderAroundPanels) {
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  Fl_Dock_Tabs *t = (Fl_Dock_Tabs *)a->parent();
  int b = Fl_Dock_Tabs::border();
  EXPECT_EQ(b, 1);
  EXPECT_EQ(a->x(), t->x() + b);
  EXPECT_EQ(a->w(), t->w() - 2 * b);
  EXPECT_EQ(a->y(), t->y() + Fl_Dock_Tabs::tab_height());
  EXPECT_EQ(a->y() + a->h(), t->y() + t->h() - b);
  Fl_Dock_Tabs::border(0);
  t->layout();
  EXPECT_EQ(a->w(), t->w());
  Fl_Dock_Tabs::border(1);
  t->layout();
  return true;
}

TEST(Tree, MoveMergesSameDirection) {
  arrange();
  // c to the right of the center makes a third child of the root split,
  // not a split inside it.
  dock->add_panel(dock->find("c"), FL_DOCK_RIGHT, view, 0.5);
  EXPECT_TRUE(normal(dock->root()));
  EXPECT_EQ(((Fl_Group *)dock->root())->children(), 3);
  Fl_Dock_Split *s = (Fl_Dock_Split *)dock->root();
  EXPECT_NEAR(s->weight(0), 0.25, 1e-9);
  EXPECT_NEAR(s->weight(1), 0.375, 1e-9);
  EXPECT_NEAR(s->weight(2), 0.375, 1e-9);
  return true;
}

TEST(Tree, MoveIntoOwnGroupIsRefused) {
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  EXPECT_EQ(dock->add_panel(a, FL_DOCK_LEFT, a), 0);
  EXPECT_STREQ(text(), ARRANGED);
  return true;
}

TEST(Tree, BesideOwnGroupIsRefused) {
  // A panel alone in its group cannot go beside or into that group.
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  EXPECT_EQ(dock->add_panel(a, FL_DOCK_RIGHT, a, 0.3), 0);
  EXPECT_EQ(dock->add_panel(a, FL_DOCK_TAB, a), 0);
  Fl::do_widget_deletion();
  EXPECT_STREQ(text(), ARRANGED);
  EXPECT_TRUE(a->docked());
  // One of two can go beside its own group.
  Fl_Dock_Panel *b = dock->find("b");
  EXPECT_EQ(dock->add_panel(b, FL_DOCK_RIGHT, b, 0.3), 1);
  Fl::do_widget_deletion();
  EXPECT_TRUE(b->docked());
  EXPECT_TRUE(b->parent() != dock->find("c")->parent());
  EXPECT_EQ(dock->panels(), 3);
  EXPECT_TRUE(normal(dock->root()));
  return true;
}

TEST(Tree, AbsorbKeepsSizes) {
  // Removing the middle of three vertical groups inside a horizontal split
  // inside a vertical split: the inner splits collapse and nothing else moves.
  setup(1);
  Fl_Dock_Panel *a = make_panel("a");
  Fl_Dock_Panel *b = make_panel("b");
  dock->add_panel(a, FL_DOCK_TOP, 0, 0.2);
  dock->add_panel(b, FL_DOCK_BOTTOM, 0, 0.2);
  int ah = a->h();
  int bh = b->h();
  int vh = view->h();
  Fl_Dock_Panel *c = make_panel("c");
  dock->add_panel(c, FL_DOCK_RIGHT, view, 0.5);
  dock->close_panel(c);
  EXPECT_TRUE(normal(dock->root()));
  EXPECT_EQ(a->h(), ah);
  EXPECT_EQ(b->h(), bh);
  EXPECT_EQ(view->h(), vh);
  EXPECT_EQ(((Fl_Group *)dock->root())->children(), 3);
  return true;
}

TEST(Tree, TabRemovalKeepsShown) {
  setup(0);
  Fl_Dock_Panel *a = make_panel("a");
  Fl_Dock_Panel *b = make_panel("b");
  Fl_Dock_Panel *c = make_panel("c");
  dock->add_panel(a);
  dock->add_panel(b, FL_DOCK_TAB, a);
  dock->add_panel(c, FL_DOCK_TAB, a);
  Fl_Dock_Tabs *t = (Fl_Dock_Tabs *)a->parent();
  t->value(b);
  dock->close_panel(a);
  EXPECT_TRUE(t->value() == b);
  EXPECT_TRUE(b->visible());
  EXPECT_TRUE(!c->visible());
  return true;
}

// ---- Floating ----------------------------------------------------------------

TEST(Float, FloatAndDock) {
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  Fl_Dock_Window *w = dock->float_panel(a, 100, 100, 300, 200);
  EXPECT_TRUE(w != 0);
  EXPECT_EQ(dock->windows(), 1);
  EXPECT_TRUE(a->dock() == w->dock());
  EXPECT_TRUE(dock->find("a") == a);
  EXPECT_EQ(dock->panels(), 3);
  EXPECT_TRUE(normal(dock->root()));
  EXPECT_EQ(dock->dock_panel(a), 1);
  Fl::do_widget_deletion();
  EXPECT_EQ(dock->windows(), 0);
  EXPECT_TRUE(a->dock() == dock);
  EXPECT_TRUE(strstr(text(), "tabs 0 a\n") != 0);
  return true;
}

TEST(Float, CloseFromWindow) {
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  dock->float_panel(a, 100, 100, 300, 200);
  EXPECT_EQ(dock->close_panel(a), 1);
  Fl::do_widget_deletion();
  EXPECT_EQ(dock->windows(), 0);
  EXPECT_TRUE(a->dock() == dock);
  EXPECT_TRUE(!a->docked());
  EXPECT_TRUE(strstr(text(), "closed a\n") != 0);
  return true;
}

TEST(Float, InsetStaysInside) {
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  Fl_Dock_Window *w = dock->inset_panel(a, 700, 500, 300, 200);
  EXPECT_TRUE(w != 0);
  EXPECT_TRUE(w->inset());
  EXPECT_TRUE(w->parent() == dock);
  EXPECT_TRUE(a->dock() == w->dock());
  // Placed past the corner, it is moved back inside the dock.
  EXPECT_EQ(w->x() + w->w(), 800);
  EXPECT_EQ(w->y() + w->h(), 600);
  // A smaller dock moves and shrinks it to fit.
  dock->resize(0, 0, 250, 150);
  EXPECT_EQ(w->x(), 0);
  EXPECT_EQ(w->w(), 250);
  EXPECT_EQ(w->h(), 150);
  dock->resize(0, 0, 800, 600);
  // Docking it again removes the window.
  EXPECT_EQ(dock->dock_panel(a), 1);
  Fl::do_widget_deletion();
  EXPECT_EQ(dock->windows(), 0);
  EXPECT_TRUE(strstr(text(), "tabs 0 a\n") != 0);
  return true;
}

TEST(Float, InsetTitleIsTabStrip) {
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  int pw = a->w();
  int ph = a->h();
  Fl_Dock_Window *w = dock->inset_panel(a);
  EXPECT_TRUE(w != 0);
  // No title bar: the tab strip is at the top, inside the frame.
  int f = Fl_Dock_Window::frame_size();
  Fl_Dock_Tabs *t = (Fl_Dock_Tabs *)a->parent();
  EXPECT_EQ(t->y(), f);
  EXPECT_EQ(w->h(), Fl_Dock_Tabs::outer_h(ph) + 2 * f);
  // Opened at the panel's own size.
  EXPECT_EQ(a->w(), pw);
  EXPECT_EQ(a->h(), ph);
  // The strip beside the tab moves the window.
  int y = t->y() + Fl_Dock_Tabs::tab_height() / 2;
  int wx = w->x();
  mouse(w, FL_PUSH, t->x() + t->w() - 30, y);
  mouse(w, FL_DRAG, t->x() + t->w() - 40, y);
  mouse(w, FL_RELEASE, t->x() + t->w() - 40, y);
  EXPECT_EQ(w->x(), wx - 10);
  EXPECT_EQ(dock->dock_panel(a), 1);
  Fl::do_widget_deletion();
  return true;
}

// Sends a press at X, Y in w, a drag by DX, DY and a release, with the
// mouse given in w's coordinates as it is while w moves under it.
static void drag_window(Fl_Window *w, int X, int Y, int DX, int DY) {
  int px = w->x() + X;
  int py = w->y() + Y;
  mouse(w, FL_PUSH, X, Y);
  mouse(w, FL_DRAG, px + DX - w->x(), py + DY - w->y());
  mouse(w, FL_RELEASE, px + DX - w->x(), py + DY - w->y());
}

TEST(Float, InsetSizesFromEveryEdge) {
  arrange();
  Fl_Dock_Window *w = dock->inset_panel(dock->find("a"), 200, 150, 300, 200);
  int f = Fl_Dock_Window::frame_size();
  // The left edge, the opposite edge staying put.
  drag_window(w, 1, w->h() / 2, -20, 0);
  EXPECT_EQ(w->x(), 180);
  EXPECT_EQ(w->w(), 320);
  EXPECT_EQ(w->y(), 150);
  // The top edge.
  drag_window(w, w->w() / 2, 1, 0, -30);
  EXPECT_EQ(w->y(), 120);
  EXPECT_EQ(w->h(), 230);
  EXPECT_EQ(w->x(), 180);
  // Near the top-left corner, both.
  drag_window(w, 1, f + 2, -10, -10);
  EXPECT_EQ(w->x(), 170);
  EXPECT_EQ(w->y(), 110);
  EXPECT_EQ(w->w(), 330);
  EXPECT_EQ(w->h(), 240);
  // Near the bottom-left corner, the left and the bottom.
  drag_window(w, 1, w->h() - 2, -10, 10);
  EXPECT_EQ(w->x(), 160);
  EXPECT_EQ(w->w(), 340);
  EXPECT_EQ(w->h(), 250);
  EXPECT_EQ(w->y(), 110);
  // The left edge stops at the dock's area, the right edge staying put.
  int right = w->x() + w->w();
  drag_window(w, 1, w->h() / 2, -1000, 0);
  EXPECT_EQ(w->x(), dock->x());
  EXPECT_EQ(w->x() + w->w(), right);
  dock->dock_panel(dock->find("a"));
  Fl::do_widget_deletion();
  return true;
}


TEST(Float, InsetFromFloating) {
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  dock->float_panel(a, 100, 100, 300, 200);
  Fl_Dock_Window *w = dock->inset_panel(a, 10, 10, 200, 150);
  Fl::do_widget_deletion();
  EXPECT_TRUE(w != 0);
  EXPECT_EQ(dock->windows(), 1);
  EXPECT_TRUE(dock->window_at(0)->inset());
  return true;
}

TEST(Float, DockGoesFirst) {
  // A window waiting to be deleted outlives the dock it belonged to.
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  dock->inset_panel(a, 10, 10, 200, 150);
  dock->dock_panel(a);
  delete win;
  win = 0;
  Fl::do_widget_deletion();
  setup(1);
  return true;
}

// ---- Layout ------------------------------------------------------------------

TEST(Layout, RoundTrip) {
  arrange();
  dock->float_panel(dock->find("a"), 120, 80, 300, 400);
  dock->close_panel(dock->find("b"));
  char saved[4096];
  dock->write_layout(saved, sizeof(saved));
  EXPECT_STREQ(saved,
    "fl_dock 1\n"
    "dock\n"
    " split h 0.25 0.75\n"
    "  tabs 0 c\n"
    "  center\n"
    "window 120 80 300 400\n"
    " tabs 0 a\n"
    "closed b\n"
    "size 800 600\n");
  // Put everything somewhere else, then restore.
  dock->dock_panel(dock->find("a"));
  dock->show_panel(dock->find("b"));
  dock->add_panel(dock->find("c"), FL_DOCK_BOTTOM, view, 0.3);
  Fl::do_widget_deletion();
  EXPECT_EQ(dock->read_layout(saved), 0);
  Fl::do_widget_deletion();
  EXPECT_STREQ(text(), saved);
  EXPECT_EQ(dock->windows(), 1);
  EXPECT_TRUE(normal(dock->root()));
  return true;
}

TEST(Layout, InsetRoundTrip) {
  arrange();
  dock->inset_panel(dock->find("c"), 40, 30, 250, 200);
  char saved[4096];
  dock->write_layout(saved, sizeof(saved));
  EXPECT_TRUE(strstr(saved, "inset 40 30 250 200\n tabs 0 c\n") != 0);
  dock->dock_panel(dock->find("c"));
  Fl::do_widget_deletion();
  EXPECT_EQ(dock->windows(), 0);
  EXPECT_EQ(dock->read_layout(saved), 0);
  Fl::do_widget_deletion();
  EXPECT_STREQ(text(), saved);
  EXPECT_TRUE(dock->window_at(0)->inset());
  // An inset holding the center is refused.
  EXPECT_EQ(dock->read_layout("fl_dock 1\ndock\n tabs 0 a\ninset 0 0 100 100\n center\n"), -1);
  return true;
}

TEST(Layout, RestoreAtOtherSize) {
  // A layout saved in a larger dock is restored with the panels at the
  // sizes they had there, the center taking the difference.
  arrange();
  resize_dock(1600, 1200);
  dock->add_panel(make_panel("d"), FL_DOCK_BOTTOM, view, 0.2);
  int aw = dock->find("a")->w();
  int dh = dock->find("d")->h();
  char saved[4096];
  dock->write_layout(saved, sizeof(saved));
  EXPECT_TRUE(strstr(saved, "size 1600 1200\n") != 0);
  resize_dock(800, 600);
  dock->close_panel(dock->find("a"));
  EXPECT_EQ(dock->read_layout(saved), 0);
  EXPECT_EQ(dock->find("a")->w(), aw);
  EXPECT_EQ(dock->find("d")->h(), dh);
  EXPECT_TRUE(normal(dock->root()));
  // Without the size, or with keep_sizes() off, the shares are kept.
  dock->keep_sizes(0);
  EXPECT_EQ(dock->read_layout(saved), 0);
  int bar = Fl_Dock_Split::bar_size();
  EXPECT_NEAR(dock->find("a")->parent()->w(), Fl_Dock_Tabs::outer_w(aw) * (800.0 - bar) / (1600 - bar), 2);
  dock->keep_sizes(1);
  return true;
}

static Fl_Dock_Panel *same_panel(Fl_Dock *d, const char *name, void *data) {
  return (Fl_Dock_Panel *)data;
}

TEST(Layout, FactoryChecked) {
  // A factory returning one panel for two names, or a panel of the dock,
  // is not believed.
  arrange();
  Fl_Dock_Panel *zz = make_panel("zz");
  dock->panel_factory(same_panel, zz);
  EXPECT_EQ(dock->read_layout("fl_dock 1\ndock\n split h 0.5 0.5\n  tabs 0 x\n  tabs 0 y\n"), 0);
  EXPECT_TRUE(zz->dock() == 0);
  dock->panel_factory(same_panel, dock->find("a"));
  EXPECT_EQ(dock->read_layout("fl_dock 1\ndock\n tabs 0 x\n"), 0);
  dock->panel_factory(0);
  EXPECT_TRUE(rereads());
  delete zz;
  return true;
}

TEST(Layout, DeepTrees) {
  // Panels docked one inside the other as deep as add_panel() allows write
  // a layout that reads back.
  setup(1);
  Fl_Dock_Panel *last = 0;
  int refused = 0;
  for (int i = 0; i < Fl_Dock::MAX_DEPTH + 10 && !refused; i++) {
    char name[16];
    snprintf(name, sizeof(name), "p%d", i);
    Fl_Dock_Panel *p = make_panel(name);
    Fl_Dock_Where where = FL_DOCK_RIGHT;
    if (i % 2) where = FL_DOCK_BOTTOM;
    if (!dock->add_panel(p, where, last, 0.5)) {
      refused = 1;
      delete p;
    }
    last = p;
  }
  EXPECT_TRUE(refused);
  EXPECT_TRUE(rereads());
  return true;
}

TEST(Layout, NameAndLabel) {
  // A panel's name is its label unless set, and stays when the label
  // changes once the dock has it.
  setup(1);
  Fl_Group::current(0);
  Fl_Dock_Panel *p = new Fl_Dock_Panel(0, 0, 100, 100, "Files");
  p->end();
  EXPECT_STREQ(p->name(), "Files");
  dock->add_panel(p);
  p->label("Dateien");
  EXPECT_STREQ(p->name(), "Files");
  EXPECT_TRUE(dock->find("Files") == p);
  Fl_Dock_Panel *q = new Fl_Dock_Panel(0, 0, 100, 100, "Files");
  q->end();
  q->name("other");
  EXPECT_STREQ(q->name(), "other");
  EXPECT_EQ(dock->add_panel(q), 1);
  // A name the dock has is not taken.
  q->name("Files");
  EXPECT_STREQ(q->name(), "other");
  EXPECT_TRUE(rereads());
  return true;
}

TEST(Layout, ShortBuffer) {
  arrange();
  char small[10];
  int n = dock->write_layout(small, sizeof(small));
  EXPECT_EQ(n, (int)strlen(ARRANGED));
  EXPECT_EQ((int)strlen(small), 9);
  EXPECT_EQ(dock->write_layout(0, 0), n);
  return true;
}

TEST(Layout, EncodedNames) {
  setup(0);
  dock->add_panel(make_panel("two words"));
  dock->add_panel(make_panel("100%"), FL_DOCK_TAB, dock->find("two words"));
  char saved[4096];
  dock->write_layout(saved, sizeof(saved));
  EXPECT_TRUE(strstr(saved, "tabs 1 two%20words 100%25\n") != 0);
  dock->close_panel(dock->find("two words"));
  EXPECT_EQ(dock->read_layout(saved), 0);
  EXPECT_STREQ(text(), saved);
  return true;
}

TEST(Layout, BadTextChangesNothing) {
  arrange();
  const char *bad[] = {
    "",
    "fl_dock 2\ndock\n",                                    // a later version
    "fl_dock 1\n",                                          // no dock
    "fl_dock 1\ndock\n split h 0.5\n  tabs 0 a\n",          // one share
    "fl_dock 1\ndock\n split h 0.5 0.5\n  tabs 0 a\n",      // a missing child
    "fl_dock 1\ndock\n split h 0.5 -1\n  tabs 0 a\n  center\n",
    "fl_dock 1\ndock\n split x 0.5 0.5\n  tabs 0 a\n  center\n",
    "fl_dock 1\ndock\n split h 0.5 0.5\n  tabs 0 a\n  tabs 0 a\n",   // a twice
    "fl_dock 1\ndock\n split h 0.5 0.5\n  center\n  center\n",
    "fl_dock 1\ndock\n tabs 0 a\nwindow 0 0 100 100\n center\n",     // center floating
    "fl_dock 1\ndock\n tabs x a\n",
    "fl_dock 1\ndock\n tabs 0\n",
    "fl_dock 1\ndock\n  tabs 0 a\n",                        // too deep
    "fl_dock 1\ndock\n tabs 0 a\n  tabs 0 b\n",             // a child of tabs
    "fl_dock 1\ndock\nwindow 0 0 0 100\n tabs 0 a\n",
    0
  };
  for (int i = 0; bad[i]; i++) {
    if (dock->read_layout(bad[i]) != -1) {
      fprintf(stderr, "bad layout %d accepted\n", i);
      return false;
    }
    EXPECT_STREQ(text(), ARRANGED);
  }
  EXPECT_EQ(dock->read_layout((const char *)0), -1);
  return true;
}

TEST(Layout, UnknownSectionsIgnored) {
  arrange();
  char saved[4096];
  dock->write_layout(saved, sizeof(saved));
  char later[4096];
  snprintf(later, sizeof(later), "%sstatus bar 3\n extra 1 2\n", saved);
  EXPECT_EQ(dock->read_layout(later), 0);
  EXPECT_STREQ(text(), ARRANGED);
  return true;
}

TEST(Layout, UnmentionedPanelsStay) {
  arrange();
  // A layout from before d existed: d keeps its place.
  char saved[4096];
  dock->write_layout(saved, sizeof(saved));
  dock->add_panel(make_panel("d"), FL_DOCK_RIGHT, view, 0.3);
  EXPECT_EQ(dock->read_layout(saved), 0);
  EXPECT_TRUE(dock->find("d")->docked());
  EXPECT_TRUE(normal(dock->root()));
  return true;
}

static int factory_calls = 0;
static Fl_Dock_Panel *factory(Fl_Dock *d, const char *name, void *data) {
  factory_calls++;
  if (strcmp(name, "made")) return 0;
  return make_panel(name);
}

TEST(Layout, Factory) {
  arrange();
  factory_calls = 0;
  dock->panel_factory(factory);
  EXPECT_EQ(dock->read_layout(
    "fl_dock 1\n"
    "dock\n"
    " split h 0.25 0.75\n"
    "  tabs 0 a made\n"
    "  center\n"
    "closed b c\n"), 0);
  dock->panel_factory(0);
  EXPECT_EQ(factory_calls, 1);
  EXPECT_TRUE(dock->find("made") != 0);
  EXPECT_TRUE(dock->find("made")->docked());
  EXPECT_STREQ(text(),
    "fl_dock 1\n"
    "dock\n"
    " split h 0.25 0.75\n"
    "  tabs 0 a made\n"
    "  center\n"
    "closed b c\n"
    "size 800 600\n");
  return true;
}

TEST(Layout, ReadForgetsEdges) {
  // v closed last, at the end of its split, goes back beside the center
  // after a layout is read, not past b, which the layout put at the end.
  setup(1);
  dock->add_panel(make_panel("b"), FL_DOCK_RIGHT, dock->center(), 0.25);
  char *saved = strdup(text());
  dock->add_panel(make_panel("v"), FL_DOCK_LEFT, dock->find("b"), 0.25);
  dock->close_panel(dock->find("b"));
  dock->close_panel(dock->find("v"));
  EXPECT_EQ(dock->read_layout(saved), 0);
  free(saved);
  dock->show_panel(dock->find("v"));
  EXPECT_TRUE(dock->find("v")->x() < dock->find("b")->x());
  return true;
}

TEST(Layout, KeptPlaces) {
  // A layout naming panels the dock does not have yet: place_panel() puts
  // each where the layout had it, whatever it is asked.
  arrange();
  EXPECT_EQ(dock->read_layout(
    "fl_dock 1\n"
    "dock\n"
    " split h 0.2 0.5 0.3\n"
    "  tabs 0 a later1\n"
    "  center\n"
    "  tabs 0 later2\n"
    "closed b c\n"), 0);
  dock->place_panel(make_panel("later1"), FL_DOCK_BOTTOM);
  dock->place_panel(make_panel("later2"), FL_DOCK_BOTTOM);
  dock->place_panel(make_panel("other"), FL_DOCK_BOTTOM, 0, 0.1);
  EXPECT_TRUE(normal(dock->root()));
  Fl_Dock_Panel *l1 = dock->find("later1");
  Fl_Dock_Panel *l2 = dock->find("later2");
  EXPECT_TRUE(l1->parent() == dock->find("a")->parent());
  // later2 is right of the center with its share of the width, 0.3.
  EXPECT_TRUE(l2->x() > view->x() + view->w());
  EXPECT_NEAR((double)l2->w() / 800, 0.3, 0.02);
  // other is below everything.
  Fl_Dock_Panel *o = dock->find("other");
  EXPECT_EQ(o->parent()->y() + o->parent()->h(), 600);
  return true;
}

// ---- Drop targets ------------------------------------------------------------

TEST(Drop, Targets) {
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  Fl_Dock_Panel *b = dock->find("b");
  Fl_Dock_Tabs *ta = (Fl_Dock_Tabs *)a->parent();
  Fl_Dock_Tabs *tb = (Fl_Dock_Tabs *)b->parent();
  Fl_Dock_Target t;
  // The band along the dock's edge docks beside everything.
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, 790, 300, tb, b, t), 1);
  EXPECT_EQ(t.where, FL_DOCK_RIGHT);
  EXPECT_TRUE(t.node == 0);
  // The middle of the center goes beside it, by the nearest side.
  int cx = view->x() + view->w() / 2;
  int cy = view->y() + view->h() / 2;
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, cx, view->y() + 40, tb, b, t), 1);
  EXPECT_EQ(t.where, FL_DOCK_TOP);
  EXPECT_TRUE(t.node == view);
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, view->x() + 40, cy, tb, b, t), 1);
  EXPECT_EQ(t.where, FL_DOCK_LEFT);
  // The middle of a tab group, or its tab strip, makes a tab.
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, ta->x() + ta->w() / 2, ta->y() + ta->h() / 2, tb, b, t), 1);
  EXPECT_EQ(t.where, FL_DOCK_TAB);
  EXPECT_TRUE(t.node == ta);
  // A tab strip along the dock's edge is still a tab strip.
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, ta->x() + 5, ta->y() + 5, tb, b, t), 1);
  EXPECT_EQ(t.where, FL_DOCK_TAB);
  EXPECT_EQ(t.index, 0);
  // Below it, the band along the dock's edge.
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, ta->x() + 5, ta->y() + 60, tb, b, t), 1);
  EXPECT_EQ(t.where, FL_DOCK_LEFT);
  EXPECT_TRUE(t.node == 0);
  // A lone panel dropped on its own group changes nothing.
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, ta->x() + ta->w() / 2, ta->y() + ta->h() / 2, ta, a, t), 0);
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, ta->x() + 30, ta->y() + ta->h() / 2, ta, a, t), 0);
  // One of two panels can go beside its own group.
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, tb->x() + 30, tb->y() + tb->h() / 2, tb, b, t), 1);
  EXPECT_EQ(t.where, FL_DOCK_LEFT);
  // Outside the dock is no target.
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, 900, 300, tb, b, t), 0);
  return true;
}

TEST(Drop, LargeGroup) {
  // A group of many tabs, dragged whole, moves whole.
  arrange();
  Fl_Dock_Panel *b = dock->find("b");
  for (int i = 0; i < 300; i++) {
    char name[16];
    snprintf(name, sizeof(name), "t%d", i);
    dock->add_panel(make_panel(name), FL_DOCK_TAB, b);
  }
  Fl_Dock_Tabs *t = (Fl_Dock_Tabs *)b->parent();
  EXPECT_EQ(t->panels(), 302);
  Fl_Dock_Target tg;
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, 790, 300, t, 0, tg), 1);
  EXPECT_EQ(Fl_Dock_Drag::drop(t, 0, tg), 1);
  Fl::do_widget_deletion();
  EXPECT_EQ(((Fl_Dock_Tabs *)b->parent())->panels(), 302);
  EXPECT_TRUE(dock->find("t299")->parent() == b->parent());
  EXPECT_TRUE(normal(dock->root()));
  return true;
}

TEST(Drop, DropGroupBesideCenter) {
  arrange();
  Fl_Dock_Panel *b = dock->find("b");
  Fl_Dock_Tabs *tb = (Fl_Dock_Tabs *)b->parent();
  Fl_Dock_Target t;
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, view->x() + view->w() - 30, view->y() + view->h() / 2, tb, 0, t), 1);
  EXPECT_EQ(t.where, FL_DOCK_RIGHT);
  // Dropping the whole group keeps b and c together.
  EXPECT_EQ(Fl_Dock_Drag::drop(tb, 0, t), 1);
  EXPECT_TRUE(normal(dock->root()));
  EXPECT_TRUE(dock->find("b")->parent() == dock->find("c")->parent());
  EXPECT_TRUE(b->x() > view->x());
  return true;
}

TEST(Drop, SourceDeletedDuringDrag) {
  // A drag whose tab group goes away, as a shortcut closing the dragged
  // panel does, ends.
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  Fl_Dock_Tabs *ta = (Fl_Dock_Tabs *)a->parent();
  Fl_Dock_Drag::begin(ta, a);
  EXPECT_EQ(Fl_Dock_Drag::active(), 1);
  dock->close_panel(a);
  Fl::do_widget_deletion();
  EXPECT_EQ(Fl_Dock_Drag::active(), 0);
  Fl_Dock_Drag::cancel();
  // A drag of a group whose tab group is deleted while another of its
  // panels stays.
  arrange();
  Fl_Dock_Tabs *tb = (Fl_Dock_Tabs *)dock->find("b")->parent();
  Fl_Dock_Drag::begin(tb, 0);
  dock->close_panel(dock->find("c"));
  EXPECT_EQ(Fl_Dock_Drag::active(), 0);
  return true;
}

// ---- The callback ---------------------------------------------------------------

static int calls = 0;
static Fl_Callback_Reason last_reason = FL_REASON_UNKNOWN;
static Fl_Dock_Panel *last_panel = 0;
static void count_cb(Fl_Widget *w, void *) {
  calls++;
  last_reason = Fl::callback_reason();
  last_panel = ((Fl_Dock *)w)->callback_panel();
}

TEST(Callback, BarDrag) {
  // Dragging the bar beside the center calls the callback when it is let
  // go, or as it moves with FL_WHEN_CHANGED, with no panel.
  arrange();
  dock->callback(count_cb);
  Fl_Dock_Split *s = (Fl_Dock_Split *)dock->root();
  int bx = s->child(0)->x() + s->child(0)->w() + 1;
  int aw = s->child(0)->w();
  calls = 0;
  mouse(s, FL_PUSH, bx, 300);
  mouse(s, FL_DRAG, bx + 20, 300);
  mouse(s, FL_DRAG, bx + 40, 300);
  EXPECT_EQ(calls, 0);
  mouse(s, FL_RELEASE, bx + 40, 300);
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(last_reason, FL_REASON_CHANGED);
  EXPECT_TRUE(last_panel == 0);
  EXPECT_EQ(s->child(0)->w(), aw + 40);
  dock->when(FL_WHEN_CHANGED);
  calls = 0;
  bx += 40;
  mouse(s, FL_PUSH, bx, 300);
  mouse(s, FL_DRAG, bx - 20, 300);
  mouse(s, FL_DRAG, bx - 40, 300);
  mouse(s, FL_RELEASE, bx - 40, 300);
  EXPECT_EQ(calls, 2);
  dock->when(FL_WHEN_RELEASE);
  dock->callback((Fl_Callback *)0);
  return true;
}

TEST(Callback, InsetMoveAndSize) {
  arrange();
  dock->callback(count_cb);
  Fl_Dock_Panel *a = dock->find("a");
  Fl_Dock_Window *w = dock->inset_panel(a, 100, 100, 300, 200);
  calls = 0;
  // The corner grip sizes it.
  int ww = w->w();
  mouse(w, FL_PUSH, w->w() - 4, w->h() - 4);
  mouse(w, FL_DRAG, w->w() - 4 + 30, w->h() - 4);
  mouse(w, FL_RELEASE, w->w() - 4, w->h() - 4);
  EXPECT_EQ(w->w(), ww + 30);
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(last_reason, FL_REASON_CHANGED);
  EXPECT_TRUE(last_panel == a);
  // The strip beside the tab moves it.
  Fl_Dock_Tabs *t = (Fl_Dock_Tabs *)a->parent();
  int wx = w->x();
  int y = t->y() + Fl_Dock_Tabs::tab_height() / 2;
  mouse(w, FL_PUSH, t->x() + t->w() - 30, y);
  mouse(w, FL_DRAG, t->x() + t->w() - 30 - 25, y);
  mouse(w, FL_RELEASE, t->x() + t->w() - 30 - 25, y);
  EXPECT_EQ(w->x(), wx - 25);
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(last_reason, FL_REASON_DRAGGED);
  dock->callback((Fl_Callback *)0);
  return true;
}

// A panel docked along an edge of the whole dock, as its tab menu's Dock Left
// does, leaves the panels it moved away from their sizes.
TEST(Move, DockAlongAnEdgeKeepsTheOthers) {
  setup(1);
  Fl_Dock_Panel *browser = make_panel("browser");
  browser->size(120, 100);
  Fl_Dock_Panel *geom = make_panel("geom");
  geom->size(200, 100);
  EXPECT_TRUE(dock->add_panel(browser, FL_DOCK_RIGHT, 0, 0));
  EXPECT_TRUE(dock->add_panel(geom, FL_DOCK_RIGHT, view, 0));
  int bw = browser->parent()->w();
  int gw = geom->parent()->w();
  EXPECT_NEAR(bw, 120, 1);
  EXPECT_NEAR(gw, 200, 1);
  EXPECT_TRUE(dock->add_panel(geom, FL_DOCK_LEFT, 0, 0));
  EXPECT_EQ(geom->parent()->x(), 0);
  // Its own size, less a split bar it is measured with
  EXPECT_NEAR(geom->parent()->w(), gw, 4);
  EXPECT_EQ(browser->parent()->w(), bw);
  EXPECT_EQ(browser->parent()->x() + browser->parent()->w(), 800);
  return true;
}

// ---- Main ----------------------------------------------------------------------

int main(int argc, char **argv) {
  int failed = 0;
  for (int i = 0; i < num_test_cases; i++) {
    Test_Case &c = test_cases[i];
    if (argc > 1 && strcmp(argv[1], c.suite)) continue;
    bool ok = c.fn();
    if (ok) {
      printf("ok     %s.%s\n", c.suite, c.name);
    } else {
      printf("FAILED %s.%s\n", c.suite, c.name);
      failed++;
    }
  }
  if (win) delete win;
  printf("%d of %d failed\n", failed, num_test_cases);
  return failed > 0;
}
