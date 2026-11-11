//
// Window tests for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

// Checks how a dock's windows behave on the screen: a layout restored before
// the application's window is shown shows its floating windows with it, on
// the screens even if it put them off them; a panel dragged onto a floating
// window over the dock goes into that window; and closing the application's
// window closes them too, so Fl::run() returns.  It needs a display, so
// CTest labels it "display".

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Dock.H>
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock_Tabs.H>
#include <FL/Fl_Dock_Window.H>
#include <stdio.h>
#include <stdlib.h>

static Fl_Double_Window *win = 0;
static Fl_Dock *dock = 0;
static int step = 0;
static int failed = 0;

static void expect(int ok, const char *what) {
  if (ok) {
    printf("ok     %s\n", what);
  } else {
    printf("FAILED %s\n", what);
    failed++;
  }
  fflush(stdout);
}

static Fl_Dock_Panel *make_panel(const char *name) {
  Fl_Group::current(0);
  Fl_Dock_Panel *p = new Fl_Dock_Panel(0, 0, 150, 120, name);
  p->name(name);
  p->end();
  return p;
}

// 1 if win lies inside the work area of some screen.
static int on_a_screen(Fl_Window *w) {
  for (int i = 0; i < Fl::screen_count(); i++) {
    int sx, sy, sw, sh;
    Fl::screen_work_area(sx, sy, sw, sh, i);
    if (w->x() >= sx && w->y() >= sy && w->x() + w->w() <= sx + sw && w->y() + w->h() <= sy + sh) return 1;
  }
  return 0;
}

// Sends a mouse event at X, Y on the screen to w, which is in the main window.
static void mouse(Fl_Widget *w, int event, int X, int Y) {
  Fl::e_x_root = X;
  Fl::e_y_root = Y;
  Fl::e_x = X - win->x_root();
  Fl::e_y = Y - win->y_root();
  Fl::e_keysym = FL_Button + FL_LEFT_MOUSE;
  Fl::e_state = FL_BUTTON1;
  if (event == FL_RELEASE) Fl::e_state = 0;
  w->handle(event);
}

static void script(void *) {
  switch (step++) {
    case 0: {
      Fl_Dock_Window *f = (Fl_Dock_Window *)dock->find("f")->window();
      expect(f && f != win && f->shown() && f->visible(), "a floating window restored before show is shown");
      expect(dock->find("f")->visible_r(), "its panel is visible");
      Fl_Window *g = dock->find("g")->window();
      expect(g && g->shown() && on_a_screen(g), "a floating window restored off the screens is put on one");
      // g fills its screen; docked, it is out of the way.
      dock->dock_panel(dock->find("g"));
      // f over the middle of the main window.
      f->resize(win->x_root() + 150, win->y_root() + 100, 200, 160);
      break;
    }
    case 1: {
      // A tab dragged from the main window to the middle of f, which lies
      // over the main window, goes into f.
      Fl_Dock_Panel *c = dock->find("c");
      Fl_Dock_Tabs *t = (Fl_Dock_Tabs *)c->parent();
      Fl_Window *f = dock->find("f")->window();
      int sx = win->x_root() + t->x() + 10;
      int sy = win->y_root() + t->y() + 5;
      int fx = f->x_root() + f->w() / 2;
      int fy = f->y_root() + f->h() / 2;
      mouse(t, FL_PUSH, sx, sy);
      mouse(t, FL_DRAG, sx + 20, sy + 20);
      mouse(t, FL_DRAG, fx, fy);
      mouse(t, FL_RELEASE, fx, fy);
      expect(c->window() == f, "a panel dropped on a floating window over the dock goes into it");
      break;
    }
    case 2:
      win->hide();
      return;
    default:
      break;
  }
  Fl::repeat_timeout(0.5, script);
}

static void give_up(void *) {
  printf("FAILED Fl::run() did not return\n");
  exit(1);
}

int main(int argc, char **argv) {
  win = new Fl_Double_Window(500, 400, "window_dock");
  dock = new Fl_Dock(0, 0, 500, 400);
  win->end();
  win->resizable(dock);
  Fl_Group::current(0);
  dock->center(new Fl_Box(0, 0, 10, 10, "center"));
  dock->add_panel(make_panel("c"));
  dock->add_panel(make_panel("f"));
  dock->add_panel(make_panel("g"));
  dock->add_panel(make_panel("i"));
  // The usual start: restore the saved layout, then show the window.
  int r = dock->read_layout("fl_dock 1\ndock\n split h 0.3 0.7\n  tabs 0 c\n  center\n"
                            "window 60 60 200 160\n tabs 0 f\n"
                            "window -1000000 -1000000 1000000 1000000\n tabs 0 g\n"
                            "inset 20 20 200 150\n tabs 0 i\n");
  expect(r == 0, "the layout is read");
  win->show(argc, argv);
  Fl::add_timeout(1.0, script);
  Fl::add_timeout(10.0, give_up);
  Fl::run();
  expect(step == 3 && Fl::first_window() == 0, "closing the window closes the floating windows");
  printf("%d failed\n", failed);
  return failed > 0;
}
