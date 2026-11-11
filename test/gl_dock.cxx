//
// OpenGL drawing test for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

// Checks that OpenGL windows draw wherever a dock puts them: as the center,
// in a panel, beside panels, floated, inset, sliding and docked again.  Each view clears to
// its own color and reads back its middle pixel; a view that is not attached
// to anything on the screen reads back black.  It needs a display, so CTest
// labels it "display"; run ctest -LE display where there is none.

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/gl.h>
#include <FL/Fl_Dock.H>
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock_Window.H>
#include <stdio.h>
#include <stdlib.h>

class Probe_View : public Fl_Gl_Window {
public:
  unsigned char red;
  unsigned char got[4];
  Probe_View(int X, int Y, int W, int H, unsigned char r) : Fl_Gl_Window(X, Y, W, H) {
    red = r;
    got[0] = got[1] = got[2] = got[3] = 0;
    end();
  }
  void draw() FL_OVERRIDE {
    glClearColor(red / 255.0f, 0.5f, 0.25f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glReadPixels(pixel_w() / 2, pixel_h() / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, got);
  }
  // True if the last draw reached the screen's buffer.
  bool drew() const { return got[0] == red && got[1] > 100; }
};

static Fl_Double_Window *win = 0;
static Fl_Dock *dock = 0;
static Probe_View *center = 0;
static Probe_View *inner = 0;
static int step = 0;
static int failed = 0;

static void check(const char *what) {
  if (center->drew() && inner->drew()) {
    printf("ok     %s\n", what);
  } else {
    printf("FAILED %s: center %d %d %d, panel %d %d %d\n", what,
           center->got[0], center->got[1], center->got[2], inner->got[0], inner->got[1], inner->got[2]);
    failed++;
  }
  fflush(stdout);
  center->got[0] = center->got[1] = 0;
  inner->got[0] = inner->got[1] = 0;
}

static void script(void *) {
  Fl_Dock_Panel *p = dock->find("gl");
  switch (step++) {
    case 0: check("center and panel at start"); dock->float_panel(p, 50, 50, 200, 200); break;
    case 1: check("panel floated"); dock->dock_panel(p); break;
    case 2: check("panel docked again"); dock->add_panel(p, FL_DOCK_TOP, center, 0.4); break;
    case 3: check("panel moved above the center"); dock->close_panel(p); dock->show_panel(p); break;
    case 4: check("panel closed and shown"); dock->inset_panel(p, 20, 20, 200, 180); break;
    case 5: check("panel in an inset window"); dock->dock_panel(p); break;
    case 6:
      check("panel docked from the inset window");
      dock->stripes(FL_DOCK_SIDE_ALL);
      dock->slide_panel(p);
      dock->toggle_panel(p);
      break;
    case 7: check("panel sliding out"); dock->pin_panel(p); break;
    case 8:
      check("panel pinned again");
      // Exit here: under CTest on macOS, Fl::run() may not return once the
      // windows are hidden.
      printf("%d failed\n", failed);
      exit(failed > 0);
  }
  center->redraw();
  inner->redraw();
  Fl::repeat_timeout(0.5, script);
}

int main(int argc, char **argv) {
  win = new Fl_Double_Window(500, 400, "gl_dock");
  dock = new Fl_Dock(0, 0, 500, 400);
  win->end();
  win->resizable(dock);
  // Made outside any group, as an application making its view first would.
  Fl_Group::current(0);
  center = new Probe_View(0, 0, 100, 100, 200);
  dock->center(center);
  Fl_Group::current(0);
  Fl_Dock_Panel *p = new Fl_Dock_Panel(0, 0, 150, 150, "GL");
  p->name("gl");
  inner = new Probe_View(0, 0, 150, 150, 100);
  p->resizable(inner);
  p->end();
  dock->add_panel(p, FL_DOCK_LEFT, 0, 0.3);
  win->show(argc, argv);
  Fl::add_timeout(1.0, script);
  return Fl::run();
}
