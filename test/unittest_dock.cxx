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

TEST(Layout, ShortBuffer) {
  arrange();
  char small[10];
  int n = dock->write_layout(small, sizeof(small));
  EXPECT_EQ(n, (int)strlen(ARRANGED));
  EXPECT_EQ((int)strlen(small), 9);
  EXPECT_EQ(dock->write_layout(0, 0), n);
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
