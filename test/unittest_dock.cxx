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
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Dock.H>
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock_Split.H>
#include <FL/Fl_Dock_Tabs.H>
#include <FL/Fl_Dock_Window.H>
#include <FL/Fl_Dock_Stripe.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Window.H>
#include <FL/Fl_Preferences.H>
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

static void arrange_stripes();

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

TEST(Tree, StoreChangesNothing) {
  // Panels given closed leave the dock as it is, and open where they were put.
  arrange();
  for (int i = 0; i < 20; i++) {
    char name[16];
    snprintf(name, sizeof(name), "s%d", i);
    EXPECT_EQ(dock->store_panel(make_panel(name), FL_DOCK_RIGHT, view, 0.4), 1);
  }
  EXPECT_EQ(strncmp(text(), ARRANGED, strlen(ARRANGED) - strlen("size 800 600\n")), 0);
  int vw = view->w();
  Fl_Dock_Panel *s3 = dock->find("s3");
  EXPECT_TRUE(!s3->docked());
  dock->show_panel(s3);
  EXPECT_TRUE(s3->docked());
  EXPECT_TRUE(s3->x() > view->x());
  EXPECT_NEAR((double)s3->parent()->w() / vw, 0.4, 0.02);
  EXPECT_EQ(dock->store_panel(make_panel("bad"), FL_DOCK_LEFT, s3->parent()), 0);
  return true;
}

TEST(Tree, ClosedPanelsKeepTheirSize) {
  // A closed panel is not scaled with the dock, and opens at its own size.
  arrange();
  Fl_Dock_Panel *s = make_panel("s");
  s->size(300, 200);
  dock->store_panel(s, FL_DOCK_RIGHT, view, 0);
  dock->close_panel(dock->find("a"));
  int aw = dock->find("a")->w();
  int ah = dock->find("a")->h();
  win->size(300, 250);
  dock->resize(0, 0, 300, 250);
  win->size(1600, 1200);
  dock->resize(0, 0, 1600, 1200);
  EXPECT_EQ(s->w(), 300);
  EXPECT_EQ(s->h(), 200);
  EXPECT_EQ(dock->find("a")->w(), aw);
  EXPECT_EQ(dock->find("a")->h(), ah);
  dock->show_panel(s);
  EXPECT_NEAR(s->w(), 300, 2);
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

TEST(Tree, SwappingEditorsKeepsPlaces) {
  // An application shows one of several editors beside a browser at the
  // right of the center, showing the new one before hiding the old.  The
  // browser keeps its width, and each editor opens between the center and
  // the browser.
  setup(1);
  Fl_Dock_Panel *browser = make_panel("browser");
  dock->add_panel(browser, FL_DOCK_RIGHT, 0, 0.15);
  Fl_Dock_Panel *ed[2];
  ed[0] = make_panel("text");
  ed[1] = make_panel("image");
  for (int i = 0; i < 2; i++) {
    ed[i]->size(140 + 60 * i, 100);
    dock->store_panel(ed[i], FL_DOCK_RIGHT, view, 0);
  }
  dock->show_panel(ed[0]);
  int bw = browser->w();
  int ew[2];
  ew[0] = ed[0]->w();
  ew[1] = 200;
  EXPECT_NEAR(ew[0], 140, 2);
  int order[] = { 1, 0, 1, 0, 1 };
  for (int k = 0; k < 5; k++) {
    Fl_Dock_Panel *show = ed[order[k]];
    Fl_Dock_Panel *hide = ed[1 - order[k]];
    dock->show_panel(show);
    dock->close_panel(hide);
    EXPECT_EQ(browser->w(), bw);
    EXPECT_NEAR(show->w(), ew[order[k]], 2);
    if (k == 0) ew[1] = show->w();
    EXPECT_TRUE(show->x() > view->x() + view->w());
    EXPECT_TRUE(show->x() + show->w() < browser->x());
    EXPECT_TRUE(normal(dock->root()));
  }
  return true;
}

TEST(Tree, SwappingEditorsHideFirst) {
  // As above, but the old editor is hidden before the new one is shown.
  setup(1);
  Fl_Dock_Panel *browser = make_panel("browser");
  dock->add_panel(browser, FL_DOCK_RIGHT, 0, 0.15);
  Fl_Dock_Panel *ed[2];
  ed[0] = make_panel("text");
  ed[1] = make_panel("image");
  for (int i = 0; i < 2; i++) {
    ed[i]->size(140 + 60 * i, 100);
    dock->store_panel(ed[i], FL_DOCK_RIGHT, view, 0);
  }
  dock->show_panel(ed[0]);
  int order[] = { 1, 0, 1, 0, 1 };
  for (int k = 0; k < 5; k++) {
    Fl_Dock_Panel *show = ed[order[k]];
    Fl_Dock_Panel *hide = ed[1 - order[k]];
    dock->close_panel(hide);
    dock->show_panel(show);
    if (!(show->x() > view->x() + view->w() && show->x() + show->w() < browser->x())) {
      fprintf(stderr, "step %d %s at %d, view %d..%d, browser %d\n%s", k, show->name(), show->x(), view->x(),
              view->x() + view->w(), browser->x(), text());
    }
    EXPECT_TRUE(show->x() > view->x() + view->w());
    EXPECT_TRUE(show->x() + show->w() < browser->x());
  }
  return true;
}

TEST(Tree, SwappingEditorsInIndexOrder) {
  // The application walks its editors in a fixed order, showing the one
  // wanted and hiding the rest, so going from the first to the second hides
  // first, and going back shows first.
  setup(1);
  Fl_Dock_Panel *browser = make_panel("browser");
  dock->add_panel(browser, FL_DOCK_RIGHT, 0, 0.15);
  Fl_Dock_Panel *ed[2];
  ed[0] = make_panel("text");
  ed[1] = make_panel("image");
  for (int i = 0; i < 2; i++) {
    ed[i]->size(140 + 60 * i, 100);
    dock->store_panel(ed[i], FL_DOCK_RIGHT, view, 0);
  }
  int wanted[] = { 0, 1, 0, 1, 0, 1 };
  for (int k = 0; k < 6; k++) {
    for (int i = 0; i < 2; i++) {
      if (i == wanted[k]) dock->show_panel(ed[i]);
      else dock->close_panel(ed[i]);
    }
    Fl_Dock_Panel *show = ed[wanted[k]];
    if (!(show->x() > view->x() + view->w() && show->x() + show->w() < browser->x())) {
      fprintf(stderr, "step %d %s at %d, view %d..%d, browser %d\n%s", k, show->name(), show->x(), view->x(),
              view->x() + view->w(), browser->x(), text());
    }
    EXPECT_TRUE(show->x() > view->x() + view->w());
    EXPECT_TRUE(show->x() + show->w() < browser->x());
  }
  return true;
}

TEST(Tree, PlaceGroup) {
  // Editors of a place group take turns in one place, wherever the last was.
  setup(1);
  Fl_Dock_Panel *browser = make_panel("browser");
  dock->add_panel(browser, FL_DOCK_RIGHT, 0, 0.15);
  Fl_Dock_Panel *ed[3];
  const char *names[3] = { "e0", "e1", "e2" };
  for (int i = 0; i < 3; i++) {
    ed[i] = make_panel(names[i]);
    ed[i]->size(140 + 30 * i, 100);
    ed[i]->place_group(1);
    dock->store_panel(ed[i], FL_DOCK_RIGHT, view, 0);
  }
  dock->show_panel(ed[0]);
  int x0 = ed[0]->parent()->x();
  int w0 = ed[0]->parent()->w();
  // Showing another closes the first and opens in its place.
  dock->show_panel(ed[1]);
  EXPECT_TRUE(!ed[0]->docked());
  EXPECT_EQ(ed[1]->parent()->x(), x0);
  EXPECT_EQ(ed[1]->parent()->w(), w0);
  // Floated inside, the next one opens floated inside at the same place.
  dock->inset_panel(ed[1], 50, 60, 220, 180);
  dock->show_panel(ed[2]);
  Fl::do_widget_deletion();
  EXPECT_TRUE(!ed[1]->docked());
  EXPECT_TRUE(((Fl_Dock_Window *)ed[2]->window())->inset());
  EXPECT_EQ(ed[2]->window()->x() - dock->x(), 50);
  EXPECT_EQ(ed[2]->window()->w(), 220);
  // Docked again, the next one docks where it was.
  dock->dock_panel(ed[2]);
  Fl::do_widget_deletion();
  dock->show_panel(ed[0]);
  Fl::do_widget_deletion();
  EXPECT_TRUE(ed[0]->dock() == dock);
  EXPECT_TRUE(ed[0]->x() > view->x() + view->w());
  EXPECT_TRUE(ed[0]->x() < browser->x());
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

TEST(Tree, MoveToAnotherDock) {
  // A panel given to another dock leaves the first one altogether.
  arrange_stripes();
  Fl_Group::current(0);
  Fl_Window *w2 = new Fl_Window(400, 400);
  Fl_Dock *d2 = new Fl_Dock(0, 0, 400, 400);
  w2->end();
  Fl_Dock_Panel *a = dock->find("a");
  EXPECT_EQ(d2->add_panel(a), 1);
  EXPECT_EQ(dock->panels(), 2);
  EXPECT_EQ(d2->panels(), 1);
  EXPECT_EQ(dock->stripe(FL_DOCK_LEFT)->count(), 1);
  EXPECT_TRUE(rereads());
  delete w2;
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

TEST(Float, ReopenInMode) {
  // A panel closed from an inset or floating window opens there again.
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  Fl_Dock_Window *w = dock->inset_panel(a, 50, 60, 220, 180);
  w->position(w->x() + 10, w->y() + 5);    // the user moves it
  dock->close_panel(a);
  Fl::do_widget_deletion();
  EXPECT_EQ(a->mode(), FL_DOCK_MODE_INSET);
  dock->show_panel(a);
  EXPECT_EQ(dock->windows(), 1);
  w = dock->window_at(0);
  EXPECT_TRUE(w->inset());
  EXPECT_EQ(w->x() - dock->x(), 60);
  EXPECT_EQ(w->y() - dock->y(), 65);
  EXPECT_EQ(w->w(), 220);
  Fl_Dock_Panel *b = dock->find("b");
  dock->float_panel(b, 300, 200, 250, 150);
  dock->close_panel(b);
  Fl::do_widget_deletion();
  dock->show_panel(b);
  EXPECT_EQ(b->mode(), FL_DOCK_MODE_FLOATING);
  EXPECT_TRUE(!((Fl_Dock_Window *)b->window())->inset());
  EXPECT_EQ(b->window()->x(), 300);
  // Docking makes it docked again.
  dock->dock_panel(a);
  EXPECT_EQ(a->mode(), FL_DOCK_MODE_DOCKED);
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

TEST(Float, StoreAsInset) {
  // A panel given closed, marked to open inset, opens in an inset window.
  arrange();
  Fl_Dock_Panel *s = make_panel("s");
  dock->store_panel(s);
  s->mode(FL_DOCK_MODE_INSET);
  s->inset_rect(10, 20, 200, 150);
  dock->show_panel(s);
  EXPECT_TRUE(s->window() != win);
  EXPECT_TRUE(((Fl_Dock_Window *)s->window())->inset());
  EXPECT_EQ(s->window()->x() - dock->x(), 10);
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

TEST(Layout, OpensRoundTrip) {
  arrange();
  dock->inset_panel(dock->find("c"), 40, 30, 250, 200);
  dock->close_panel(dock->find("c"));
  Fl::do_widget_deletion();
  char saved[4096];
  dock->write_layout(saved, sizeof(saved));
  EXPECT_TRUE(strstr(saved, "closed c\nopens c inset 40 30 250 200\n") != 0);
  dock->find("c")->mode(FL_DOCK_MODE_DOCKED);
  EXPECT_EQ(dock->read_layout(saved), 0);
  EXPECT_EQ(dock->find("c")->mode(), FL_DOCK_MODE_INSET);
  EXPECT_STREQ(text(), saved);
  EXPECT_EQ(dock->read_layout("fl_dock 1\ndock\n tabs 0 a\nopens c inset 1 2 0 4\n"), -1);
  return true;
}

TEST(Layout, ClosedOpensDocked) {
  // A closed panel the text lists without an "opens" line opens docked,
  // whatever it was before.
  arrange();
  dock->inset_panel(dock->find("c"), 40, 30, 250, 200);
  dock->close_panel(dock->find("c"));
  Fl::do_widget_deletion();
  EXPECT_EQ(dock->find("c")->mode(), FL_DOCK_MODE_INSET);
  EXPECT_EQ(dock->read_layout("fl_dock 1\ndock\n split h 0.3 0.7\n  tabs 0 a b\n  center\nclosed c\n"), 0);
  EXPECT_EQ(dock->find("c")->mode(), FL_DOCK_MODE_DOCKED);
  dock->show_panel(dock->find("c"));
  EXPECT_TRUE(dock->find("c")->dock() == dock);
  return true;
}

TEST(Layout, OpensWithoutPlace) {
  // A closed panel set to open inset that has not opened yet keeps its mode.
  arrange();
  Fl_Dock_Panel *s = make_panel("s");
  dock->store_panel(s);
  s->mode(FL_DOCK_MODE_INSET);
  char saved[4096];
  dock->write_layout(saved, sizeof(saved));
  EXPECT_TRUE(strstr(saved, "opens s inset\n") != 0);
  s->mode(FL_DOCK_MODE_DOCKED);
  EXPECT_EQ(dock->read_layout(saved), 0);
  EXPECT_EQ(s->mode(), FL_DOCK_MODE_INSET);
  dock->show_panel(s);
  EXPECT_TRUE(((Fl_Dock_Window *)s->window())->inset());
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

TEST(Layout, NamesMustBeUnique) {
  // Panels without a name, or with one the dock has, are refused, so the
  // dock never writes a layout it cannot read.
  arrange();
  Fl_Dock_Panel *dup = make_panel("a");
  EXPECT_EQ(dock->add_panel(dup, FL_DOCK_RIGHT, view, 0.3), 0);
  EXPECT_EQ(dock->store_panel(dup), 0);
  EXPECT_EQ(dock->place_panel(dup), 0);
  Fl_Dock_Panel *empty = make_panel("");
  EXPECT_EQ(dock->add_panel(empty), 0);
  EXPECT_STREQ(text(), ARRANGED);
  delete dup;
  delete empty;
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

// ---- Stripes ----------------------------------------------------------------

// The standard arrangement with stripes on every side, a and b on the left.
static void arrange_stripes() {
  arrange();
  dock->stripes(FL_DOCK_SIDE_ALL);
  dock->stripe_panel(dock->find("a"), FL_DOCK_LEFT);
  dock->stripe_panel(dock->find("b"), FL_DOCK_LEFT);
}

TEST(Stripe, AreaAndButtons) {
  arrange();
  dock->stripes(FL_DOCK_SIDE_ALL);
  // Empty stripes take no space.
  EXPECT_EQ(dock->root()->x(), 0);
  EXPECT_EQ(dock->root()->w(), 800);
  dock->stripe_panel(dock->find("a"), FL_DOCK_LEFT);
  int t = Fl_Dock_Stripe::thickness();
  EXPECT_EQ(dock->root()->x(), t);
  EXPECT_EQ(dock->root()->w(), 800 - t);
  EXPECT_EQ(dock->stripe(FL_DOCK_LEFT)->count(), 1);
  // A panel has one button: moving it leaves the left stripe empty.
  dock->stripe_panel(dock->find("a"), FL_DOCK_BOTTOM, 1);
  EXPECT_EQ(dock->stripe(FL_DOCK_LEFT)->count(), 0);
  EXPECT_EQ(dock->stripe(FL_DOCK_BOTTOM)->buttons(1), 1);
  EXPECT_EQ(dock->root()->x(), 0);
  EXPECT_EQ(dock->root()->h(), 600 - t);
  dock->stripes(FL_DOCK_SIDE_ALL | FL_DOCK_STRIPE_SHOW_EMPTY * 0);
  dock->stripe_options(FL_DOCK_STRIPE_SHOW_EMPTY);
  dock->stripes(FL_DOCK_SIDE_ALL);
  EXPECT_EQ(dock->root()->x(), t);
  EXPECT_EQ(dock->root()->w(), 800 - 2 * t);
  return true;
}

TEST(Stripe, ToggleDocked) {
  arrange_stripes();
  Fl_Dock_Panel *a = dock->find("a");
  Fl_Dock_Panel *b = dock->find("b");
  int aw = a->w();
  EXPECT_TRUE(dock->panel_shown(a));
  EXPECT_EQ(dock->toggle_panel(a), 0);
  EXPECT_TRUE(!a->docked());
  EXPECT_EQ(dock->toggle_panel(a), 1);
  EXPECT_TRUE(a->docked());
  EXPECT_EQ(a->w(), aw);
  // In one group, showing one hides the other.
  ((Fl_Dock_Tabs *)b->parent())->value(b);
  EXPECT_TRUE(dock->panel_shown(b));
  dock->toggle_panel(a);
  dock->toggle_panel(a);
  EXPECT_TRUE(!dock->panel_shown(b));
  dock->stripe_options(FL_DOCK_STRIPE_SHARED);
  dock->toggle_panel(b);
  EXPECT_TRUE(dock->panel_shown(a));
  EXPECT_TRUE(dock->panel_shown(b));
  return true;
}

TEST(Stripe, SlideAndPin) {
  arrange_stripes();
  Fl_Dock_Panel *a = dock->find("a");
  EXPECT_EQ(dock->slide_panel(a), 1);
  EXPECT_EQ(a->mode(), FL_DOCK_MODE_SLIDING);
  EXPECT_TRUE(!a->docked());
  EXPECT_TRUE(normal(dock->root()));
  EXPECT_TRUE(strstr(text(), "tabs 0 a") == 0);
  // Shown, it slides out beside its stripe, at its size, over the tree.
  EXPECT_EQ(dock->toggle_panel(a), 1);
  Fl_Dock_Window *s = dock->sliding();
  EXPECT_TRUE(s != 0);
  EXPECT_TRUE(s->sliding());
  EXPECT_TRUE(a->window() == s);
  EXPECT_EQ(s->x(), Fl_Dock_Stripe::thickness());
  EXPECT_EQ(s->h(), 600);
  EXPECT_TRUE(dock->panel_shown(a));
  // Hidden, it goes back to the stripe, still sliding.
  EXPECT_EQ(dock->toggle_panel(a), 0);
  Fl::do_widget_deletion();
  EXPECT_TRUE(dock->sliding() == 0);
  EXPECT_EQ(a->mode(), FL_DOCK_MODE_SLIDING);
  // Pinned, it goes back where it was.
  EXPECT_EQ(dock->pin_panel(a), 1);
  EXPECT_TRUE(a->docked());
  // The stripe took its width from the center.
  EXPECT_STREQ(text(),
    "fl_dock 1\n"
    "dock\n"
    " split h 0.258442 0.741558\n"
    "  split v 0.5 0.5\n"
    "   tabs 0 a\n"
    "   tabs 1 b c\n"
    "  center\n"
    "stripe left a b\n"
    "size 776 600\n");
  return true;
}

TEST(Stripe, SlideWithoutStripe) {
  // A panel unpinned with no button gets one on its nearest side.
  arrange();
  dock->stripes(FL_DOCK_SIDE_ALL);
  Fl_Dock_Panel *a = dock->find("a");
  dock->slide_panel(a);
  EXPECT_EQ(a->stripe_side(), FL_DOCK_LEFT);
  return true;
}

TEST(Stripe, LayoutRoundTrip) {
  arrange_stripes();
  dock->stripe_panel(dock->find("c"), FL_DOCK_RIGHT, 1);
  dock->slide_panel(dock->find("b"));
  dock->find("b")->slide_size(180);
  char saved[4096];
  dock->write_layout(saved, sizeof(saved));
  EXPECT_TRUE(strstr(saved, "stripe left a b\n") != 0);
  EXPECT_TRUE(strstr(saved, "stripe right | c\n") != 0);
  EXPECT_TRUE(strstr(saved, "sliding b 180\n") != 0);
  // Undo it all, then restore.
  dock->pin_panel(dock->find("b"));
  dock->unstripe_panel(dock->find("a"));
  dock->stripe_panel(dock->find("c"), FL_DOCK_TOP);
  EXPECT_EQ(dock->read_layout(saved), 0);
  EXPECT_STREQ(text(), saved);
  EXPECT_EQ(dock->find("b")->mode(), FL_DOCK_MODE_SLIDING);
  EXPECT_EQ(dock->read_layout("fl_dock 1\ndock\n center\nstripe middle a\n"), -1);
  return true;
}

TEST(Stripe, HideAllAndBack) {
  arrange_stripes();
  char before[4096];
  dock->write_layout(before, sizeof(before));
  EXPECT_EQ(dock->hide_all(), 1);
  EXPECT_TRUE(dock->root() == view);
  EXPECT_EQ(dock->hidden_all(), 1);
  EXPECT_EQ(dock->hide_all(), 0);
  Fl::do_widget_deletion();
  EXPECT_STREQ(text(), before);
  return true;
}

TEST(Stripe, MaximizeAndBack) {
  arrange();
  char before[4096];
  dock->write_layout(before, sizeof(before));
  Fl_Dock_Panel *b = dock->find("b");
  EXPECT_EQ(dock->maximize_panel(b), 1);
  EXPECT_TRUE(dock->root() == b->parent());
  EXPECT_EQ(b->parent()->w(), 800);
  EXPECT_TRUE(!view->visible());
  EXPECT_EQ(dock->maximize_panel(b), 0);
  Fl::do_widget_deletion();
  EXPECT_STREQ(text(), before);
  EXPECT_TRUE(view->visible());
  return true;
}

TEST(Stripe, DropTabOnStripe) {
  arrange_stripes();
  Fl_Dock_Panel *c = dock->find("c");
  Fl_Dock_Target t;
  Fl_Dock_Stripe *st = dock->stripe(FL_DOCK_LEFT);
  // The far end of the left stripe is its second group.
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, st->x() + 5, 590, (Fl_Dock_Tabs *)c->parent(), c, t), 1);
  EXPECT_EQ(t.where, FL_DOCK_STRIPE_TARGET);
  EXPECT_EQ(t.group, 1);
  EXPECT_EQ(Fl_Dock_Drag::drop((Fl_Dock_Tabs *)c->parent(), c, t), 1);
  EXPECT_EQ(c->stripe_side(), FL_DOCK_LEFT);
  EXPECT_EQ(c->stripe_group(), 1);
  EXPECT_EQ(c->mode(), FL_DOCK_MODE_SLIDING);
  // A button dropped in the tree pins its panel there.
  EXPECT_EQ(Fl_Dock_Drag::hit(dock, view->x() + view->w() - 20, 300, 0, c, t), 1);
  EXPECT_EQ(t.where, FL_DOCK_RIGHT);
  EXPECT_EQ(Fl_Dock_Drag::drop(0, c, t), 1);
  EXPECT_TRUE(c->docked());
  EXPECT_EQ(c->mode(), FL_DOCK_MODE_DOCKED);
  EXPECT_EQ(c->stripe_side(), FL_DOCK_LEFT);
  return true;
}

static int key_toggled = 0;
TEST(Stripe, Shortcut) {
  arrange_stripes();
  Fl_Dock_Panel *a = dock->find("a");
  a->dock_shortcut(FL_ALT + '1');
  Fl::e_keysym = '1';
  Fl::e_state = FL_ALT;
  static char one[2] = "1";
  Fl::e_text = one;
  Fl::e_length = 1;
  key_toggled = dock->handle(FL_SHORTCUT);
  EXPECT_EQ(key_toggled, 1);
  EXPECT_TRUE(!a->docked());
  Fl::e_state = 0;
  return true;
}

TEST(Stripe, PanelMenu) {
  arrange_stripes();
  Fl_Group::current(0);
  Fl_Menu_Bar *bar = new Fl_Menu_Bar(0, 0, 100, 20);
  dock->panel_menu(bar, "&View/Panels");
  const Fl_Menu_Item *ia = bar->find_item("&View/Panels/a");
  EXPECT_TRUE(ia != 0);
  EXPECT_TRUE(ia->value() != 0);
  bar->picked(ia);
  EXPECT_TRUE(!dock->find("a")->docked());
  EXPECT_TRUE(bar->find_item("&View/Panels/a")->value() == 0);
  delete bar;
  dock->panel_menu(0, 0);
  return true;
}

TEST(Stripe, NamedLayouts) {
  arrange_stripes();
  Fl_Preferences prefs(Fl_Preferences::MEMORY, "fltk.org", "unittest_dock");
  char before[4096];
  dock->write_layout(before, sizeof(before));
  EXPECT_EQ(dock->save_layout(prefs, "work"), 1);
  dock->close_panel(dock->find("a"));
  EXPECT_EQ(dock->load_layout(prefs, "work"), 0);
  EXPECT_STREQ(text(), before);
  EXPECT_EQ(dock->load_layout(prefs, "nothing"), -1);
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

// ---- Removing and deleting ----------------------------------------------------

TEST(Remove, RemovePanel) {
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  EXPECT_EQ(dock->remove_panel(a), 1);
  EXPECT_TRUE(a->parent() == 0);
  EXPECT_TRUE(dock->find("a") == 0);
  EXPECT_EQ(dock->panels(), 2);
  EXPECT_TRUE(normal(dock->root()));
  EXPECT_TRUE(rereads());
  EXPECT_EQ(dock->remove_panel(a), 0);
  // It is the caller's again, to give to the dock or delete.
  dock->add_panel(a, FL_DOCK_RIGHT, view, 0.3);
  EXPECT_TRUE(a->docked());
  return true;
}

TEST(Remove, DeleteDocked) {
  arrange();
  delete dock->find("a");
  Fl::do_widget_deletion();
  EXPECT_EQ(dock->panels(), 2);
  EXPECT_TRUE(normal(dock->root()));
  EXPECT_TRUE(rereads());
  delete dock->find("c");
  EXPECT_TRUE(dock->find("b")->visible());
  EXPECT_TRUE(rereads());
  return true;
}

TEST(Remove, DeleteClosedStripedPlaced) {
  arrange_stripes();
  Fl_Group::current(0);
  Fl_Menu_Bar *bar = new Fl_Menu_Bar(0, 0, 100, 20);
  dock->panel_menu(bar, "View/Panels");
  Fl_Dock_Panel *a = dock->find("a");
  Fl_Dock_Panel *e = make_panel("e");
  a->place_group(1);
  e->place_group(1);
  dock->store_panel(e, FL_DOCK_RIGHT, view, 0.3);
  dock->close_panel(a);
  delete a;
  EXPECT_TRUE(bar->find_item("View/Panels/a") == 0);
  EXPECT_EQ(dock->stripe(FL_DOCK_LEFT)->count(), 1);
  dock->show_panel(e);
  dock->toggle_panel(dock->find("b"));
  EXPECT_TRUE(strstr(text(), " a") == 0);
  EXPECT_TRUE(rereads());
  dock->panel_menu(0, 0);
  delete bar;
  return true;
}

TEST(Remove, DeleteSliding) {
  arrange_stripes();
  Fl_Dock_Panel *a = dock->find("a");
  dock->slide_panel(a);
  dock->toggle_panel(a);
  EXPECT_TRUE(dock->sliding() != 0);
  delete a;
  Fl::do_widget_deletion();
  EXPECT_TRUE(dock->sliding() == 0);
  EXPECT_TRUE(rereads());
  return true;
}

TEST(Remove, DeleteFloating) {
  arrange();
  Fl_Dock_Panel *a = dock->find("a");
  dock->float_panel(a, 100, 100, 300, 200);
  dock->stripes(FL_DOCK_SIDE_ALL);
  dock->stripe_panel(a, FL_DOCK_LEFT);
  delete a;
  Fl::do_widget_deletion();
  EXPECT_EQ(dock->windows(), 0);
  EXPECT_EQ(dock->stripe(FL_DOCK_LEFT)->count(), 0);
  EXPECT_TRUE(rereads());
  // Deleting a floating window deletes its panels, which leave the dock.
  Fl_Dock_Panel *b = dock->find("b");
  dock->stripe_panel(b, FL_DOCK_LEFT);
  Fl_Dock_Window *w = dock->float_panel(b, 100, 100, 300, 200);
  delete w;
  EXPECT_EQ(dock->windows(), 0);
  EXPECT_TRUE(dock->find("b") == 0);
  EXPECT_EQ(dock->stripe(FL_DOCK_LEFT)->count(), 0);
  EXPECT_TRUE(rereads());
  return true;
}

TEST(Remove, DeleteInsetWithDock) {
  arrange();
  dock->float_panel(dock->find("a"), 10, 10, 200, 200);
  dock->inset_panel(dock->find("b"), 10, 10, 200, 200);
  delete win;
  win = 0;
  Fl::do_widget_deletion();
  setup(1);
  return true;
}

TEST(Remove, Center) {
  arrange();
  // center(0) gives the center back.
  dock->center(0);
  EXPECT_TRUE(dock->center() == 0);
  EXPECT_TRUE(view->parent() == 0);
  EXPECT_TRUE(normal(dock->root()));
  EXPECT_TRUE(strstr(text(), "center") == 0);
  EXPECT_TRUE(rereads());
  // A second center replaces the first.
  dock->center(view);
  Fl_Group::current(0);
  Fl_Box *v2 = new Fl_Box(0, 0, 10, 10, "v2");
  dock->center(v2);
  EXPECT_TRUE(view->parent() == 0);
  EXPECT_TRUE(dock->center() == v2);
  const char *first = strstr(text(), "center");
  EXPECT_TRUE(first != 0 && strstr(first + 1, "center") == 0);
  EXPECT_TRUE(rereads());
  delete view;
  // Deleting the center takes it out.
  delete v2;
  EXPECT_TRUE(dock->center() == 0);
  EXPECT_TRUE(strstr(text(), "center") == 0);
  EXPECT_TRUE(rereads());
  EXPECT_TRUE(normal(dock->root()));
  view = 0;
  return true;
}

TEST(Remove, CenterReplacedInPlace) {
  // A new center takes the old one's place and size; the tree is as it was,
  // with the center between panels.
  arrange();
  dock->add_panel(make_panel("d"), FL_DOCK_RIGHT, view, 0.2);
  char *before = strdup(text());
  int X = view->x();
  int Y = view->y();
  int W = view->w();
  int H = view->h();
  Fl_Group::current(0);
  Fl_Box *v2 = new Fl_Box(0, 0, 10, 10, "v2");
  dock->center(v2);
  EXPECT_TRUE(view->parent() == 0);
  EXPECT_TRUE(!view->visible());
  EXPECT_STREQ(text(), before);
  EXPECT_EQ(v2->x(), X);
  EXPECT_EQ(v2->y(), Y);
  EXPECT_EQ(v2->w(), W);
  EXPECT_EQ(v2->h(), H);
  // And back.
  dock->center(view);
  EXPECT_TRUE(view->visible());
  EXPECT_TRUE(v2->parent() == 0);
  EXPECT_STREQ(text(), before);
  EXPECT_EQ(view->w(), W);
  free(before);
  delete v2;
  return true;
}

TEST(Remove, CenterAsRoot) {
  setup(1);
  delete view;
  view = 0;
  EXPECT_TRUE(dock->root() == 0);
  EXPECT_TRUE(dock->center() == 0);
  dock->add_panel(make_panel("a"));
  EXPECT_TRUE(rereads());
  return true;
}

// ---- Event dispatch ------------------------------------------------------------

static Fl_Window *dialog = 0;
static void delete_dialog_cb(Fl_Widget *, void *) {
  delete dialog;
  dialog = 0;
}

TEST(Dispatch, FocusDeletedByCallback) {
  // A shortcut whose callback deletes the window holding the keyboard focus,
  // as dialogs are closed; the dock's event dispatch outlives the focus.
  setup(1);
  Fl_Group::current(0);
  Fl_Button *b = new Fl_Button(0, 0, 10, 10);
  win->add(b);
  b->shortcut('q');
  b->callback(delete_dialog_cb);
  dialog = new Fl_Window(200, 100);
  Fl_Input *in = new Fl_Input(50, 10, 100, 25);
  dialog->end();
  Fl::focus(in);
  Fl::e_keysym = 'q';
  static char q[2] = "q";
  Fl::e_text = q;
  Fl::e_length = 1;
  EXPECT_EQ(Fl::handle(FL_SHORTCUT, win), 1);
  EXPECT_TRUE(dialog == 0);
  return true;
}

static Fl_Event_Dispatch app_prev = 0;
static int app_events = 0;
static int app_dispatch(int event, Fl_Window *w) {
  app_events++;
  if (app_prev) return app_prev(event, w);
  return Fl::handle_(event, w);
}

TEST(Dispatch, InstalledWhileDocksExist) {
  // The docks' dispatch is set while a dock exists, and an application's
  // own, set after and chaining to it, keeps it working.
  setup(1);
  EXPECT_TRUE(Fl::event_dispatch() == Fl_Dock::dispatch);
  delete win;
  win = 0;
  Fl::do_widget_deletion();
  EXPECT_TRUE(Fl::event_dispatch() == 0);
  arrange_stripes();
  EXPECT_TRUE(Fl::event_dispatch() == Fl_Dock::dispatch);
  app_prev = Fl::event_dispatch();
  Fl::event_dispatch(app_dispatch);
  Fl_Dock_Panel *a = dock->find("a");
  dock->slide_panel(a);
  dock->toggle_panel(a);
  Fl_Group::current(0);
  Fl_Window *other = new Fl_Window(100, 50);
  other->end();
  Fl::e_x = Fl::e_y = 5;
  Fl::handle(FL_PUSH, other);
  Fl::handle(FL_RELEASE, other);
  EXPECT_TRUE(app_events > 0);
  EXPECT_TRUE(dock->sliding() == 0);
  delete other;
  // With the application's in front, the last dock leaves it there.
  delete win;
  win = 0;
  Fl::do_widget_deletion();
  EXPECT_TRUE(Fl::event_dispatch() == app_dispatch);
  Fl::event_dispatch(app_prev);
  app_prev = 0;
  setup(1);
  return true;
}

TEST(Dispatch, SlideStaysForMenus) {
  // A press in a menu or tooltip window, as a choice in the sliding panel
  // opens, leaves the slide out; a press elsewhere slides it away.
  arrange_stripes();
  Fl_Dock_Panel *a = dock->find("a");
  dock->slide_panel(a);
  dock->toggle_panel(a);
  EXPECT_TRUE(dock->sliding() != 0);
  Fl_Group::current(0);
  Fl_Menu_Window *menu = new Fl_Menu_Window(100, 50);
  menu->end();
  menu->set_menu_window();
  Fl_Menu_Window *tip = new Fl_Menu_Window(100, 50);
  tip->end();
  tip->set_tooltip_window();
  Fl_Window *other = new Fl_Window(100, 50);
  other->end();
  Fl::e_x = Fl::e_y = 5;
  Fl::handle(FL_PUSH, menu);
  Fl::handle(FL_RELEASE, menu);
  EXPECT_TRUE(dock->sliding() != 0);
  Fl::handle(FL_PUSH, tip);
  Fl::handle(FL_RELEASE, tip);
  EXPECT_TRUE(dock->sliding() != 0);
  Fl::handle(FL_PUSH, other);
  Fl::handle(FL_RELEASE, other);
  EXPECT_TRUE(dock->sliding() == 0);
  delete menu;
  delete tip;
  delete other;
  Fl::do_widget_deletion();
  return true;
}

TEST(Dispatch, SlideKeepsClicksInIt) {
  // macOS gives a click in a subwindow to the window it is in: one over the
  // sliding window leaves it out, and one beside it slides it away.
  arrange_stripes();
  Fl_Dock_Panel *a = dock->find("a");
  dock->slide_panel(a);
  dock->toggle_panel(a);
  Fl_Dock_Window *s = dock->sliding();
  EXPECT_TRUE(s != 0);
  Fl::e_x = s->x() + s->w() / 2;
  Fl::e_y = s->y() + s->h() / 2;
  Fl::handle(FL_PUSH, win);
  Fl::handle(FL_RELEASE, win);
  EXPECT_TRUE(dock->sliding() == s);
  Fl::e_x = s->x() + s->w() + 10;
  Fl::handle(FL_PUSH, win);
  Fl::handle(FL_RELEASE, win);
  EXPECT_TRUE(dock->sliding() == 0);
  Fl::do_widget_deletion();
  return true;
}

TEST(Dispatch, EscapeSlidesAway) {
  // Escape, which reaches the dock only if nothing used it, slides the panel away.
  arrange_stripes();
  Fl_Dock_Panel *a = dock->find("a");
  dock->slide_panel(a);
  dock->toggle_panel(a);
  EXPECT_TRUE(dock->sliding() != 0);
  Fl::e_keysym = 'x';
  EXPECT_EQ(dock->handle(FL_SHORTCUT), 0);
  EXPECT_TRUE(dock->sliding() != 0);
  Fl::e_keysym = FL_Escape;
  EXPECT_EQ(dock->handle(FL_SHORTCUT), 1);
  EXPECT_TRUE(dock->sliding() == 0);
  Fl::e_keysym = 0;
  Fl::do_widget_deletion();
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

TEST(Callback, CallsDoNotNotify) {
  arrange_stripes();
  dock->callback(count_cb);
  calls = 0;
  Fl_Dock_Panel *a = dock->find("a");
  dock->toggle_panel(a);
  dock->toggle_panel(a);
  dock->slide_panel(a);
  dock->pin_panel(a);
  dock->close_panel(a);
  dock->show_panel(a);
  dock->float_panel(a, 10, 10, 200, 200);
  dock->dock_panel(a);
  dock->maximize_panel(a);
  dock->maximize_panel(a);
  Fl::do_widget_deletion();
  EXPECT_EQ(calls, 0);
  dock->callback((Fl_Callback *)0);
  return true;
}

TEST(Callback, UserActionsNotify) {
  arrange_stripes();
  dock->callback(count_cb);
  calls = 0;
  Fl_Dock_Panel *a = dock->find("a");
  // A click on the stripe button hides a.
  Fl_Dock_Stripe *st = dock->stripe(FL_DOCK_LEFT);
  int bx, by, bw, bh;
  st->button_box(0, 0, bx, by, bw, bh);
  mouse(st, FL_PUSH, bx + 2, by + 2);
  mouse(st, FL_RELEASE, bx + 2, by + 2);
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(last_reason, FL_REASON_CLOSED);
  EXPECT_TRUE(last_panel == a);
  EXPECT_TRUE(!a->docked());
  // Its shortcut shows it again.
  a->dock_shortcut(FL_ALT + '1');
  Fl::e_keysym = '1';
  Fl::e_state = FL_ALT;
  static char one[2] = "1";
  Fl::e_text = one;
  Fl::e_length = 1;
  EXPECT_EQ(dock->handle(FL_SHORTCUT), 1);
  Fl::e_state = 0;
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(last_reason, FL_REASON_OPENED);
  // With FL_WHEN_NEVER only the changed flag tells.
  dock->when(FL_WHEN_NEVER);
  dock->clear_changed();
  mouse(st, FL_PUSH, bx + 2, by + 2);
  mouse(st, FL_RELEASE, bx + 2, by + 2);
  EXPECT_EQ(calls, 2);
  EXPECT_TRUE(dock->changed() != 0);
  dock->when(FL_WHEN_RELEASE);
  dock->callback((Fl_Callback *)0);
  return true;
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
