//
// Demo of the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

// A window with an OpenGL view in the center and panels around it.  Drag a
// tab to move its panel: to an edge of the window, beside another group, or
// onto a tab strip.  Drag it out of the window, or use the menu at the right
// of the tab strip, to float it; that menu's Float Inside floats it in an
// inset window that stays inside this window.  The View menu closes and shows panels; the
// Layout menu saves, restores and resets the arrangement, which is also kept
// between runs.  The "GL" panel holds an OpenGL window of its own, which must
// keep drawing when it is floated and docked again.  "Properties" cannot be
// closed, "Notes" asks before it closes, and "Help" is not created until the
// View menu first asks for it: it opens where the last saved layout had it.
// The Log panel lists what the dock's callback reports.  Every panel has a
// button on a stripe at the side of the window, which shows and hides it,
// and a key, Alt with a digit (Cmd on macOS); the Slide item of a tab's menu
// unpins a panel, so that it slides out over the others from its stripe.
// The GL panel's button shows an icon rather than its title.  The Options
// menu changes how the stripes behave, and whether the panels keep their
// sizes as the window is resized.  The Shape and Color editors start closed
// and take turns in one place: View opens one, which closes the other.
// View's Swap Center puts another view in the center and takes the first
// back.

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Preferences.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_ask.H>
#include <FL/gl.h>
#include <FL/Fl_Dock.H>
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock_Window.H>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A spinning triangle, so it is plain when a GL window stops drawing.
class Spin_View : public Fl_Gl_Window {
  double angle_;
  float r_, g_, b_;
  static void tick(void *data) {
    Spin_View *v = (Spin_View *)data;
    v->angle_ += 2;
    if (v->visible_r()) v->redraw();
    Fl::repeat_timeout(1.0 / 30, tick, data);
  }
public:
  Spin_View(int X, int Y, int W, int H, float r, float g, float b)
    : Fl_Gl_Window(X, Y, W, H) {
    angle_ = 0;
    r_ = r;
    g_ = g;
    b_ = b;
    end();
    Fl::add_timeout(1.0 / 30, tick, this);
  }
  ~Spin_View() FL_OVERRIDE { Fl::remove_timeout(tick, this); }
  void draw() FL_OVERRIDE {
    if (!valid()) {
      glViewport(0, 0, pixel_w(), pixel_h());
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      int hh = h();
      if (hh < 1) hh = 1;
      double a = (double)w() / hh;
      glOrtho(-a, a, -1, 1, -1, 1);
      glMatrixMode(GL_MODELVIEW);
    }
    glClearColor(0.15f, 0.15f, 0.18f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    glRotated(angle_, 0, 0, 1);
    glBegin(GL_TRIANGLES);
    glColor3f(r_, g_, b_);
    glVertex2d(0, 0.8);
    glColor3f(g_, b_, r_);
    glVertex2d(-0.7, -0.5);
    glColor3f(b_, r_, g_);
    glVertex2d(0.7, -0.5);
    glEnd();
  }
};

// A panel that asks before it closes.
class Notes_Panel : public Fl_Dock_Panel {
public:
  Notes_Panel(int X, int Y, int W, int H) : Fl_Dock_Panel(X, Y, W, H, "Notes") { name("notes"); }
  int can_close() FL_OVERRIDE {
    return fl_choice("Close the notes?", "Keep", "Close", 0) == 1;
  }
};

static Fl_Double_Window *main_window = 0;
static Fl_Dock *dock = 0;
static Fl_Menu_Bar *menu = 0;
static char *default_layout = 0;

static const char *panel_names[] = { "files", "props", "log", "notes", "gl", "help", "shape", "color", 0 };
static Fl_Text_Buffer *log_buffer = 0;

static Fl_Dock_Panel *make_panel(const char *name) {
  Fl_Group::current(0);
  Fl_Dock_Panel *p = 0;
  if (!strcmp(name, "files")) {
    p = new Fl_Dock_Panel(0, 0, 200, 300, "Files");
    Fl_Hold_Browser *b = new Fl_Hold_Browser(0, 0, 200, 300);
    const char *items[] = { "README.txt", "main.cxx", "dock.cxx", "notes.txt", "image.png", "CMakeLists.txt", 0 };
    for (int i = 0; items[i]; i++) b->add(items[i]);
    p->resizable(b);
  } else if (!strcmp(name, "props")) {
    p = new Fl_Dock_Panel(0, 0, 200, 300, "Properties");
    new Fl_Input(70, 10, 120, 24, "Name");
    Fl_Value_Slider *s = new Fl_Value_Slider(70, 44, 120, 24, "Size");
    s->type(FL_HOR_NICE_SLIDER);
    s->align(FL_ALIGN_LEFT);
    s->bounds(1, 100);
    s->value(30);
    new Fl_Check_Button(70, 78, 120, 24, "Visible");
    p->dock_flags(FL_DOCK_MOVABLE | FL_DOCK_FLOATABLE);
    Fl_Box *rest = new Fl_Box(0, 110, 200, 190);
    p->resizable(rest);
  } else if (!strcmp(name, "log")) {
    p = new Fl_Dock_Panel(0, 0, 400, 150, "Log");
    Fl_Text_Display *t = new Fl_Text_Display(0, 0, 400, 150);
    log_buffer = new Fl_Text_Buffer();
    log_buffer->text("Drag a tab to move its panel.\nDrag it out of the window to float it.\n");
    t->buffer(log_buffer);
    p->resizable(t);
  } else if (!strcmp(name, "notes")) {
    p = new Notes_Panel(0, 0, 200, 150);
    Fl_Text_Display *t = new Fl_Text_Display(0, 0, 200, 150);
    Fl_Text_Buffer *buf = new Fl_Text_Buffer();
    buf->text("This panel asks before it closes.\n");
    t->buffer(buf);
    p->resizable(t);
  } else if (!strcmp(name, "help")) {
    p = new Fl_Dock_Panel(0, 0, 200, 150, "Help");
    Fl_Text_Display *t = new Fl_Text_Display(0, 0, 200, 150);
    Fl_Text_Buffer *buf = new Fl_Text_Buffer();
    buf->text("Created when first shown.\nMove it, save the layout, quit,\nand open it again from View:\nit returns here.\n");
    t->buffer(buf);
    p->resizable(t);
  } else if (!strcmp(name, "shape") || !strcmp(name, "color")) {
    // Editors for different things, which take turns in one place.
    const char *title = "Shape";
    if (!strcmp(name, "color")) title = "Color";
    p = new Fl_Dock_Panel(0, 0, 200, 150, title);
    Fl_Box *b = new Fl_Box(0, 0, 200, 150);
    if (!strcmp(name, "color")) b->label("Edits the color.\nOpening Shape closes it.");
    else b->label("Edits the shape.\nOpening Color closes it.");
    p->resizable(b);
    p->place_group(1);
  } else if (!strcmp(name, "gl")) {
    p = new Fl_Dock_Panel(0, 0, 200, 200, "GL");
    Spin_View *v = new Spin_View(0, 0, 200, 200, 0.9f, 0.6f, 0.2f);
    p->resizable(v);
  } else {
    return 0;
  }
  p->name(name);
  p->end();
  return p;
}

static int panel_key(int digit);

// The GL panel's stripe icon: a triangle in the colors of its view.
static Fl_RGB_Image *gl_icon() {
  static uchar pixels[16 * 16 * 4];
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      uchar *px = pixels + 4 * (16 * y + x);
      int inside = (2 * abs(x - 8) <= y);
      px[0] = 230;
      px[1] = 150;
      px[2] = 50;
      px[3] = 0;
      if (inside) px[3] = 255;
    }
  }
  static Fl_RGB_Image icon(pixels, 16, 16, 4);
  return &icon;
}

static Fl_Dock_Panel *factory(Fl_Dock *d, const char *name, void *) {
  Fl_Dock_Panel *p = make_panel(name);
  for (int i = 0; p && panel_names[i]; i++) {
    if (!strcmp(panel_names[i], name)) p->dock_shortcut(panel_key(i + 1));
  }
  return p;
}

// The key that shows and hides a panel: Cmd and a digit on macOS, where
// Option and a digit types a character, and Alt and a digit elsewhere.
static int panel_key(int digit) {
#ifdef __APPLE__
  return FL_META + '0' + digit;
#else
  return FL_ALT + '0' + digit;
#endif
}

// Gives a panel its stripe button and key.
static void stripe(Fl_Dock_Panel *p, Fl_Dock_Where side, int group) {
  for (int i = 0; panel_names[i]; i++) {
    if (!strcmp(panel_names[i], p->name())) p->dock_shortcut(panel_key(i + 1));
  }
  if (!strcmp(p->name(), "gl")) p->dock_icon(gl_icon());
  dock->stripe_panel(p, side, group);
}

// Opens an editor; the one of its place group that was open closes.
static void edit_cb(Fl_Widget *, void *data) {
  Fl_Dock_Panel *p = dock->find((const char *)data);
  if (p) dock->show_panel(p);
}

static void keep_sizes_cb(Fl_Widget *w, void *) {
  dock->keep_sizes(((Fl_Menu_ *)w)->mvalue()->value() != 0);
}

// Puts the other view in the center; the dock gives back the one it had.
static Fl_Widget *other_center = 0;
static void swap_center_cb(Fl_Widget *, void *) {
  Fl_Widget *next = other_center;
  other_center = dock->center();
  dock->center(next);
}

static void help_cb(Fl_Widget *, void *) {
  Fl_Dock_Panel *p = dock->find("help");
  if (!p) {
    // Made the first time it is asked for, where the saved layout had it.
    p = make_panel("help");
    dock->place_panel(p, FL_DOCK_RIGHT, 0, 0.25);
    stripe(p, FL_DOCK_RIGHT, 1);
    dock->panel_menu(menu, "&View/Panels");
  } else {
    dock->show_panel(p);
  }
}

static void hide_all_cb(Fl_Widget *, void *) { dock->hide_all(); }

// Maximizes the panel with the keyboard focus, or puts everything back.
static void maximize_cb(Fl_Widget *, void *) {
  Fl_Dock_Panel *p = 0;
  for (Fl_Widget *w = Fl::focus(); w && !p; w = w->parent()) {
    if (Fl_Dock::is_panel(w)) p = (Fl_Dock_Panel *)w;
  }
  if (!p && !dock->hidden_all()) {
    fl_message("Click in a panel first.");
    return;
  }
  dock->maximize_panel(p);
}

static void option_cb(Fl_Widget *w, void *data) {
  Fl_Menu_ *m = (Fl_Menu_ *)w;
  int flag = (int)(fl_intptr_t)data;
  int o = dock->stripe_options();
  if (m->mvalue()->value()) o |= flag;
  else o &= ~flag;
  dock->stripe_options(o);
  dock->stripes(dock->stripes());
}

static void side_cb(Fl_Widget *w, void *data) {
  Fl_Menu_ *m = (Fl_Menu_ *)w;
  int side = (int)(fl_intptr_t)data;
  int s = dock->stripes();
  if (m->mvalue()->value()) s |= side;
  else s &= ~side;
  dock->stripes(s);
}

static void named_cb(Fl_Widget *, void *data) {
  const char *what = (const char *)data;
  Fl_Preferences prefs(Fl_Preferences::USER_L, "fltk.org", "dock");
  if (what[0] == 's') dock->save_layout(prefs, what + 1);
  else if (dock->load_layout(prefs, what + 1) != 0) fl_alert("There is no layout %s yet.", what + 1);
}

static void save_layout() {
  int n = dock->write_layout(0, 0);
  char *buf = (char *)malloc(n + 1);
  dock->write_layout(buf, n + 1);
  Fl_Preferences prefs(Fl_Preferences::USER_L, "fltk.org", "dock");
  prefs.set("layout", buf);
  free(buf);
}

static void save_cb(Fl_Widget *, void *) { save_layout(); }

static void restore_cb(Fl_Widget *, void *) {
  Fl_Preferences prefs(Fl_Preferences::USER_L, "fltk.org", "dock");
  char *text = 0;
  prefs.get("layout", text, "");
  if (dock->read_layout(text) != 0) fl_alert("The saved layout could not be restored.");
  free(text);
}

static void reset_cb(Fl_Widget *, void *) {
  dock->read_layout(default_layout);
}

static void print_cb(Fl_Widget *, void *) {
  int n = dock->write_layout(0, 0);
  char *buf = (char *)malloc(n + 1);
  dock->write_layout(buf, n + 1);
  fputs(buf, stdout);
  free(buf);
}

static void dock_cb(Fl_Widget *, void *) {
  const char *what = "changed";
  Fl_Callback_Reason r = Fl::callback_reason();
  if (r == FL_REASON_DRAGGED) what = "moved";
  else if (r == FL_REASON_CLOSED) what = "closed";
  else if (r == FL_REASON_OPENED) what = "opened";
  else if (r == FL_REASON_SELECTED) what = "selected";
  else if (r == FL_REASON_GOT_FOCUS) what = "has the keyboard";
  Fl_Dock_Panel *p = dock->callback_panel();
  if (log_buffer && p) {
    char line[128];
    snprintf(line, sizeof(line), "%s %s\n", p->label(), what);
    log_buffer->append(line);
  }
}

// Closing the main window closes the dock's floating windows too.
static void quit_cb(Fl_Widget *, void *) {
  save_layout();
  main_window->hide();
}

int main(int argc, char **argv) {
  Fl::use_high_res_GL(1);
  main_window = new Fl_Double_Window(1000, 700, "Fl_Dock demo");
  menu = new Fl_Menu_Bar(0, 0, 1000, 25);
  menu->add("&File/&Quit", FL_COMMAND + 'q', quit_cb);
  menu->add("&View/Panels", 0, 0, 0, FL_SUBMENU);
  menu->add("&View/&Help", 0, help_cb);
  menu->add("&View/Hide &All", FL_COMMAND + FL_SHIFT + 'h', hide_all_cb);
  menu->add("&View/&Maximize Focused", FL_COMMAND + FL_SHIFT + 'm', maximize_cb, 0, FL_MENU_DIVIDER);
  menu->add("&View/Edit &Shape", 0, edit_cb, (void *)"shape");
  menu->add("&View/Edit &Color", 0, edit_cb, (void *)"color", FL_MENU_DIVIDER);
  menu->add("&View/S&wap Center", 0, swap_center_cb);
  menu->add("&Options/Click Gives &Focus First", 0, option_cb, (void *)(fl_intptr_t)FL_DOCK_STRIPE_FOCUS_FIRST, FL_MENU_TOGGLE);
  menu->add("&Options/Groups Show &Together", 0, option_cb, (void *)(fl_intptr_t)FL_DOCK_STRIPE_SHARED, FL_MENU_TOGGLE);
  menu->add("&Options/Show &Empty Stripes", 0, option_cb, (void *)(fl_intptr_t)FL_DOCK_STRIPE_SHOW_EMPTY, FL_MENU_TOGGLE | FL_MENU_DIVIDER);
  menu->add("&Options/Stripe on &Left", 0, side_cb, (void *)(fl_intptr_t)FL_DOCK_SIDE_LEFT, FL_MENU_TOGGLE | FL_MENU_VALUE);
  menu->add("&Options/Stripe on &Right", 0, side_cb, (void *)(fl_intptr_t)FL_DOCK_SIDE_RIGHT, FL_MENU_TOGGLE | FL_MENU_VALUE);
  menu->add("&Options/Stripe on &Top", 0, side_cb, (void *)(fl_intptr_t)FL_DOCK_SIDE_TOP, FL_MENU_TOGGLE | FL_MENU_VALUE);
  menu->add("&Options/Stripe on &Bottom", 0, side_cb, (void *)(fl_intptr_t)FL_DOCK_SIDE_BOTTOM, FL_MENU_TOGGLE | FL_MENU_VALUE | FL_MENU_DIVIDER);
  menu->add("&Options/&Keep Panel Sizes", 0, keep_sizes_cb, 0, FL_MENU_TOGGLE | FL_MENU_VALUE);
  menu->add("&Layout/&Save", 0, save_cb);
  menu->add("&Layout/&Restore", 0, restore_cb);
  menu->add("&Layout/Rese&t", 0, reset_cb);
  menu->add("&Layout/&Print", 0, print_cb, 0, FL_MENU_DIVIDER);
  menu->add("&Layout/Save as &A", 0, named_cb, (void *)"sA");
  menu->add("&Layout/Use A", 0, named_cb, (void *)"lA");
  menu->add("&Layout/Save as &B", 0, named_cb, (void *)"sB");
  menu->add("&Layout/Use B", 0, named_cb, (void *)"lB");
  dock = new Fl_Dock(0, 25, 1000, 675);
  main_window->end();
  main_window->resizable(dock);
  main_window->callback(quit_cb);

  Fl_Group::current(0);
  dock->center(new Spin_View(0, 0, 400, 400, 0.2f, 0.6f, 0.9f));
  dock->add_panel(make_panel("files"), FL_DOCK_LEFT, 0, 0.2);
  dock->add_panel(make_panel("props"), FL_DOCK_BOTTOM, dock->find("files"), 0.5);
  dock->add_panel(make_panel("notes"), FL_DOCK_TAB, dock->find("props"));
  dock->add_panel(make_panel("log"), FL_DOCK_BOTTOM, dock->center(), 0.25);
  dock->add_panel(make_panel("gl"), FL_DOCK_RIGHT, dock->center(), 0.25);
  // Stripes on every side, with a button and a key for each panel.
  dock->stripes(FL_DOCK_SIDE_ALL);
  stripe(dock->find("files"), FL_DOCK_LEFT, 0);
  stripe(dock->find("props"), FL_DOCK_LEFT, 1);
  stripe(dock->find("notes"), FL_DOCK_LEFT, 1);
  stripe(dock->find("log"), FL_DOCK_BOTTOM, 0);
  stripe(dock->find("gl"), FL_DOCK_RIGHT, 0);
  // The editors start closed, and open below the GL panel.
  dock->store_panel(make_panel("shape"), FL_DOCK_BOTTOM, dock->find("gl"), 0.5);
  dock->store_panel(make_panel("color"), FL_DOCK_BOTTOM, dock->find("gl"), 0.5);
  stripe(dock->find("shape"), FL_DOCK_RIGHT, 1);
  stripe(dock->find("color"), FL_DOCK_RIGHT, 1);
  Fl_Group::current(0);
  Fl_Box *second = new Fl_Box(FL_FLAT_BOX, 0, 0, 400, 400, "The second center view");
  second->color(FL_DARK3);
  second->labelcolor(FL_WHITE);
  other_center = second;
  dock->callback(dock_cb);
  dock->panel_factory(factory);
  dock->panel_menu(menu, "&View/Panels");

  int n = dock->write_layout(0, 0);
  default_layout = (char *)malloc(n + 1);
  dock->write_layout(default_layout, n + 1);

  // The arrangement from the last run, if there is one; its floating
  // windows show with the main window.
  Fl_Preferences prefs(Fl_Preferences::USER_L, "fltk.org", "dock");
  char *text = 0;
  prefs.get("layout", text, "");
  if (text[0]) dock->read_layout(text);
  free(text);

  main_window->show(argc, argv);

  int r = Fl::run();
  free(default_layout);
  return r;
}
