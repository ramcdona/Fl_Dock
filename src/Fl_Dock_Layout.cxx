//
// Saving and restoring dock layouts for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#include <FL/Fl_Dock.H>
#include <FL/Fl_Dock_Panel.H>
#include <FL/Fl_Dock_Split.H>
#include <FL/Fl_Dock_Tabs.H>
#include <FL/Fl_Dock_Window.H>
#include <FL/Fl.H>
#include <FL/fl_string_functions.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  A layout is text, one node per line, indented one space per level, after a
  version line:

    fl_dock 1
    dock
     split h 0.25 0.75
      tabs 0 files
      split v 0.7 0.3
       center
       tabs 1 props log
    window 120 80 300 400
     tabs 0 notes
    inset 40 30 250 200
     tabs 0 find
    closed help find
    size 800 575

  "split h" lays its children side by side and "split v" one above another,
  each with its share; "tabs" gives the index of the tab shown, then the
  panels' names; "center" is the dock's center.  Each "window" is a floating
  window at x y w h on the screen, and each "inset" an inset window at x y w h
  in the dock.  "closed" lists the closed panels.
  "size" is the size of the dock's area, so that panels keep their sizes
  when the layout is restored in a dock of another size.
  Names are encoded so they hold no spaces.  Lines with a
  keyword this version does not know are ignored, with their children.
*/

static const int LAYOUT_VERSION = 1;

// A place kept for a panel named in a layout that the dock does not have.
struct Fl_Dock::Kept {
  char *name;
  char *ref;
  int where;
  double fraction;
};

// Appends to a buffer that may be too small, counting what it would need.
struct Layout_Writer {
  char *buf;
  int size;
  int len;
  void put(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *at = 0;
    int room = 0;
    if (buf && len < size) {
      at = buf + len;
      room = size - len;
    }
    int n = vsnprintf(at, room, fmt, ap);
    va_end(ap);
    if (n > 0) len += n;
  }
  // A name, with spaces, control characters and '%' as %XX.
  void name(const char *s) {
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
      if (*c <= ' ' || *c == '%' || *c == 127) put("%%%02X", *c);
      else put("%c", *c);
    }
  }
};

// 1 if node writes anything: a tab group with panels, the center, or a
// split holding one of those.
static int writes(Fl_Widget *node) {
  if (Fl_Dock::is_tabs(node)) return ((Fl_Group *)node)->children() > 0;
  if (Fl_Dock::is_split(node)) {
    Fl_Group *g = (Fl_Group *)node;
    for (int i = 0; i < g->children(); i++) {
      if (writes(g->child(i))) return 1;
    }
    return 0;
  }
  return 1;
}

// Writes the tree at node.  A tree not yet put back in normal form, as
// while a deleted widget is taken out, is written as if it were.
static void write_node(Layout_Writer &w, Fl_Widget *node, int depth) {
  if (Fl_Dock::is_split(node)) {
    Fl_Dock_Split *s = (Fl_Dock_Split *)node;
    int n = 0;
    int only = -1;
    double sum = 0;
    for (int i = 0; i < s->children(); i++) {
      if (!writes(s->child(i))) continue;
      n++;
      only = i;
      sum += s->weight(i);
    }
    if (n == 0) return;
    if (n == 1) {
      write_node(w, s->child(only), depth);
      return;
    }
    // The shares are written as they are unless a child is left out, so a
    // layout read back is written the same.
    if (!(sum > 0) || n == s->children()) sum = 1;
    for (int i = 0; i < depth; i++) w.put(" ");
    if (s->horizontal()) w.put("split h");
    else w.put("split v");
    for (int i = 0; i < s->children(); i++) {
      if (!writes(s->child(i))) continue;
      double wt = s->weight(i) / sum;
      if (!(wt > 0)) wt = 1e-6;
      w.put(" %.6g", wt);
    }
    w.put("\n");
    for (int i = 0; i < s->children(); i++) {
      if (writes(s->child(i))) write_node(w, s->child(i), depth + 1);
    }
    return;
  }
  if (!writes(node)) return;
  for (int i = 0; i < depth; i++) w.put(" ");
  if (Fl_Dock::is_tabs(node)) {
    Fl_Dock_Tabs *t = (Fl_Dock_Tabs *)node;
    int sel = t->find(t->value());
    if (sel >= t->panels()) sel = 0;
    w.put("tabs %d", sel);
    for (int i = 0; i < t->panels(); i++) {
      w.put(" ");
      w.name(t->panel(i)->name());
    }
    w.put("\n");
  } else {
    w.put("center\n");
  }
}

/**
  Writes the layout of this dock, its floating windows and its closed panels
  into buf, as text ending in a nul, cutting it short if size is too small.
  \return the length of the whole text, without the nul; if it is size or
    more the text was cut short, and buf needs to be that plus one long
*/
int Fl_Dock::write_layout(char *buf, int size) const {
  const Fl_Dock *o = this;
  if (owner_) o = owner_;
  Layout_Writer w;
  w.buf = buf;
  w.size = size;
  w.len = 0;
  if (buf && size > 0) buf[0] = 0;
  w.put("fl_dock %d\n", LAYOUT_VERSION);
  w.put("dock\n");
  if (o->root_) write_node(w, o->root_, 1);
  for (int i = 0; i < o->nwindows_; i++) {
    Fl_Dock_Window *win = o->windows_[i];
    if (!win->dock()->root() || !writes(win->dock()->root())) continue;
    if (win->inset()) w.put("inset %d %d %d %d\n", win->x() - o->x(), win->y() - o->y(), win->w(), win->h());
    else w.put("window %d %d %d %d\n", win->x(), win->y(), win->w(), win->h());
    write_node(w, win->dock()->root(), 1);
  }
  int nclosed = o->store_->children();
  if (nclosed > 0) {
    w.put("closed");
    for (int i = 0; i < nclosed; i++) {
      w.put(" ");
      w.name(((Fl_Dock_Panel *)o->store_->child(i))->name());
    }
    w.put("\n");
  }
  int ax, ay, aw, ah;
  o->area(ax, ay, aw, ah);
  w.put("size %d %d\n", aw, ah);
  return w.len;
}

// ---- Reading -------------------------------------------------------------

enum { NODE_SPLIT, NODE_TABS, NODE_CENTER };

struct Layout_Node {
  int kind;
  int horizontal;
  int nkids;
  Layout_Node **kids;
  double *weights;
  int selected;
  int nnames;
  char **names;
  Fl_Dock_Panel **panels;       // the panel each name resolved to, or 0
};

struct Layout_Window {
  int inset;
  int x, y, w, h;
  Layout_Node *root;
};

struct Layout_Line {
  int depth;
  int ntok;
  char **tok;
};

static void free_node(Layout_Node *n) {
  if (!n) return;
  for (int i = 0; i < n->nkids; i++) free_node(n->kids[i]);
  free(n->kids);
  free(n->weights);
  for (int i = 0; i < n->nnames; i++) free(n->names[i]);
  free(n->names);
  free(n->panels);
  free(n);
}

// Decodes %XX in place.
static void decode_name(char *s) {
  char *o = s;
  for (char *c = s; *c; c++) {
    if (c[0] == '%' && c[1] && c[2]) {
      char hex[3] = { c[1], c[2], 0 };
      *o++ = (char)strtol(hex, 0, 16);
      c += 2;
    } else {
      *o++ = *c;
    }
  }
  *o = 0;
}

// Splits text into lines of tokens with their depth.
static Layout_Line *split_lines(const char *text, int &nlines) {
  nlines = 0;
  int cap = 16;
  Layout_Line *lines = (Layout_Line *)malloc(sizeof(Layout_Line) * cap);
  const char *p = text;
  while (*p) {
    const char *e = p;
    while (*e && *e != '\n' && *e != '\r') e++;
    int depth = 0;
    const char *q = p;
    while (q < e && *q == ' ') { depth++; q++; }
    int ntok = 0;
    char **tok = 0;
    while (q < e) {
      while (q < e && (*q == ' ' || *q == '\t')) q++;
      if (q >= e) break;
      const char *t = q;
      while (q < e && *q != ' ' && *q != '\t') q++;
      tok = (char **)realloc(tok, sizeof(char *) * (ntok + 1));
      tok[ntok] = (char *)malloc(q - t + 1);
      memcpy(tok[ntok], t, q - t);
      tok[ntok][q - t] = 0;
      ntok++;
    }
    if (ntok > 0) {
      if (nlines == cap) {
        cap *= 2;
        lines = (Layout_Line *)realloc(lines, sizeof(Layout_Line) * cap);
      }
      lines[nlines].depth = depth;
      lines[nlines].ntok = ntok;
      lines[nlines].tok = tok;
      nlines++;
    }
    p = e;
    while (*p == '\n' || *p == '\r') p++;
  }
  return lines;
}

static void free_lines(Layout_Line *lines, int nlines) {
  for (int i = 0; i < nlines; i++) {
    for (int k = 0; k < lines[i].ntok; k++) free(lines[i].tok[k]);
    free(lines[i].tok);
  }
  free(lines);
}

// Reads a whole number, or fails.
static int read_int(const char *s, int &v) {
  char *end;
  long l = strtol(s, &end, 10);
  if (end == s || *end || l < -1000000 || l > 1000000) return 0;
  v = (int)l;
  return 1;
}

// Parses the node at line i, which must be at this depth; moves i past it and
// its children.  Returns 0 if it is malformed.
static Layout_Node *parse_node(Layout_Line *lines, int nlines, int &i, int depth) {
  if (i >= nlines || lines[i].depth != depth || depth > Fl_Dock::MAX_DEPTH) return 0;
  Layout_Line &l = lines[i];
  Layout_Node *n = (Layout_Node *)calloc(1, sizeof(Layout_Node));
  if (!strcmp(l.tok[0], "split")) {
    n->kind = NODE_SPLIT;
    if (l.ntok < 4 || (strcmp(l.tok[1], "h") && strcmp(l.tok[1], "v"))) { free_node(n); return 0; }
    n->horizontal = !strcmp(l.tok[1], "h");
    int nk = l.ntok - 2;
    n->weights = (double *)malloc(sizeof(double) * nk);
    n->kids = (Layout_Node **)calloc(nk, sizeof(Layout_Node *));
    double sum = 0;
    for (int k = 0; k < nk; k++) {
      char *end;
      double v = strtod(l.tok[2 + k], &end);
      if (end == l.tok[2 + k] || *end || !(v > 0) || !(v < 1e6)) { free_node(n); return 0; }
      n->weights[k] = v;
      sum += v;
    }
    // Shares that add up to 1 as written are kept as written.
    if (fabs(sum - 1) > 1e-4) {
      for (int k = 0; k < nk; k++) n->weights[k] /= sum;
    }
    i++;
    for (int k = 0; k < nk; k++) {
      Layout_Node *c = parse_node(lines, nlines, i, depth + 1);
      if (!c) { n->nkids = k; free_node(n); return 0; }
      n->kids[k] = c;
    }
    n->nkids = nk;
    return n;
  }
  if (!strcmp(l.tok[0], "tabs")) {
    n->kind = NODE_TABS;
    if (l.ntok < 3 || !read_int(l.tok[1], n->selected)) { free_node(n); return 0; }
    n->nnames = l.ntok - 2;
    n->names = (char **)malloc(sizeof(char *) * n->nnames);
    n->panels = (Fl_Dock_Panel **)calloc(n->nnames, sizeof(Fl_Dock_Panel *));
    for (int k = 0; k < n->nnames; k++) {
      n->names[k] = fl_strdup(l.tok[2 + k]);
      decode_name(n->names[k]);
    }
    i++;
    return n;
  }
  if (!strcmp(l.tok[0], "center") && l.ntok == 1) {
    n->kind = NODE_CENTER;
    i++;
    return n;
  }
  free_node(n);
  return 0;
}

// Collects the names in a tree; fails on a name seen before.
static int collect_names(Layout_Node *n, char ***names, int &count) {
  if (!n) return 1;
  for (int k = 0; k < n->nnames; k++) {
    for (int j = 0; j < count; j++) {
      if (!strcmp((*names)[j], n->names[k])) return 0;   // a name twice
    }
    *names = (char **)realloc(*names, sizeof(char *) * (count + 1));
    (*names)[count++] = n->names[k];
  }
  for (int k = 0; k < n->nkids; k++) {
    if (!collect_names(n->kids[k], names, count)) return 0;
  }
  return 1;
}

// Counts the centers in a tree.
static int count_centers(Layout_Node *n) {
  if (!n) return 0;
  int c = 0;
  if (n->kind == NODE_CENTER) c = 1;
  for (int k = 0; k < n->nkids; k++) c += count_centers(n->kids[k]);
  return c;
}

// Finds the panel for each name: one the dock has, or one the factory makes.
static void resolve(Fl_Dock *d, Layout_Node *n, Fl_Dock::Panel_Factory f, void *data) {
  if (!n) return;
  for (int k = 0; k < n->nnames; k++) {
    Fl_Dock_Panel *p = d->find(n->names[k]);
    // A made panel must be new and have the name asked for.
    if (!p && f) {
      p = f(d, n->names[k], data);
      if (p && (p->dock() || !p->name() || strcmp(p->name(), n->names[k]))) p = 0;
    }
    n->panels[k] = p;
  }
  for (int k = 0; k < n->nkids; k++) resolve(d, n->kids[k], f, data);
}

// The name of the first panel found under n, "" for the center, or 0.
static const char *first_name(Layout_Node *n) {
  if (!n) return 0;
  if (n->kind == NODE_CENTER) return "";
  for (int k = 0; k < n->nnames; k++) {
    if (n->panels[k]) return n->names[k];
  }
  for (int k = 0; k < n->nkids; k++) {
    const char *s = first_name(n->kids[k]);
    if (s) return s;
  }
  return 0;
}

/** \internal Keeps a place for every name in a parsed node that has no panel, next to one that has. */
void Fl_Dock::keep_missing(void *node, void *parent_node, int index) {
  Layout_Node *n = (Layout_Node *)node;
  Layout_Node *parent = (Layout_Node *)parent_node;
  if (!n) return;
  if (n->kind == NODE_TABS) {
    const char *here = first_name(n);
    for (int k = 0; k < n->nnames; k++) {
      if (n->panels[k]) continue;
      if (here) {
        keep_place(n->names[k], here, FL_DOCK_TAB, 0.25);
      } else if (parent) {
        // The whole group is missing: beside a neighbour in its split.
        int sib = index + 1;
        int where = FL_DOCK_TOP;
        if (parent->horizontal) where = FL_DOCK_LEFT;
        if (sib >= parent->nkids) {
          sib = index - 1;
          where = FL_DOCK_BOTTOM;
          if (parent->horizontal) where = FL_DOCK_RIGHT;
        }
        const char *ref = 0;
        if (sib >= 0) ref = first_name(parent->kids[sib]);
        keep_place(n->names[k], ref, where, parent->weights[index]);
      } else {
        keep_place(n->names[k], 0, FL_DOCK_LEFT, 0.25);
      }
    }
  }
  for (int k = 0; k < n->nkids; k++) keep_missing(n->kids[k], n, k);
}

/** \internal Builds the widgets for a parsed node; 0 if none of its panels exist. */
Fl_Widget *Fl_Dock::build_node(void *node) {
  Layout_Node *n = (Layout_Node *)node;
  if (n->kind == NODE_CENTER) return center_;
  if (n->kind == NODE_TABS) {
    Fl_Dock_Tabs *t = 0;
    Fl_Dock_Panel *sel = 0;
    for (int k = 0; k < n->nnames; k++) {
      Fl_Dock_Panel *p = n->panels[k];
      if (!p) continue;
      if (!t) t = new Fl_Dock_Tabs(x(), y(), w(), h());
      if (p->parent()) p->parent()->remove(p);
      p->fix_name();
      t->insert_panel(p, -1);
      if (k == n->selected) sel = p;
    }
    if (t && sel) t->value(sel);
    else if (t) t->value(t->panel(0));
    return t;
  }
  Fl_Dock_Split *s = new Fl_Dock_Split(x(), y(), w(), h(), n->horizontal);
  double *shares = (double *)malloc(sizeof(double) * n->nkids);
  double sum = 0;
  for (int k = 0; k < n->nkids; k++) {
    Fl_Widget *c = build_node(n->kids[k]);
    if (!c) continue;
    shares[s->children()] = n->weights[k];
    sum += n->weights[k];
    s->insert_node(c, s->children(), 0.5);
  }
  // The children that were built share the space in their saved proportions.
  if (fabs(sum - 1) < 1e-4) sum = 1;
  for (int k = 0; k < s->children(); k++) s->weight(k, shares[k] / sum);
  free(shares);
  if (s->children() == 0) {
    delete s;
    return 0;
  }
  return s;
}

/**
  Restores a layout written by write_layout().  The text is checked first
  and the dock is left as it is if anything in it is wrong.  Panels are found
  by name: those the dock has, else those the panel_factory() makes, which
  must be new panels with the names asked for.  A name
  with no panel keeps its place, so place_panel() puts the panel there when it is
  created later.  Panels the text does not mention go back where they were.
  \return 0 if the layout was restored, -1 if the text is not a layout
*/
int Fl_Dock::read_layout(const char *text) {
  if (owner_) return owner_->read_layout(text);
  if (!text) return -1;
  int nlines;
  Layout_Line *lines = split_lines(text, nlines);
  int ok = 1;
  int version = 0;
  if (nlines < 2 || lines[0].ntok != 2 || strcmp(lines[0].tok[0], "fl_dock") ||
      !read_int(lines[0].tok[1], version) || version < 1 || version > LAYOUT_VERSION) ok = 0;

  Layout_Node *dock_root = 0;
  int have_dock = 0;
  Layout_Window *wins = 0;
  int nwins = 0;
  char **closed = 0;
  int nclosed = 0;
  int saved_w = 0;
  int saved_h = 0;
  int i = 1;
  while (ok && i < nlines) {
    Layout_Line &l = lines[i];
    if (l.depth != 0) { ok = 0; break; }
    if (!strcmp(l.tok[0], "dock") && l.ntok == 1 && !have_dock) {
      have_dock = 1;
      i++;
      if (i < nlines && lines[i].depth == 1) {
        dock_root = parse_node(lines, nlines, i, 1);
        if (!dock_root) ok = 0;
      }
    } else if ((!strcmp(l.tok[0], "window") || !strcmp(l.tok[0], "inset")) && l.ntok == 5) {
      Layout_Window win;
      win.inset = !strcmp(l.tok[0], "inset");
      if (!read_int(l.tok[1], win.x) || !read_int(l.tok[2], win.y) ||
          !read_int(l.tok[3], win.w) || !read_int(l.tok[4], win.h) || win.w <= 0 || win.h <= 0) { ok = 0; break; }
      i++;
      win.root = parse_node(lines, nlines, i, 1);
      if (!win.root) { ok = 0; break; }
      wins = (Layout_Window *)realloc(wins, sizeof(Layout_Window) * (nwins + 1));
      wins[nwins++] = win;
    } else if (!strcmp(l.tok[0], "size") && l.ntok == 3) {
      if (!read_int(l.tok[1], saved_w) || !read_int(l.tok[2], saved_h) || saved_w <= 0 || saved_h <= 0) { ok = 0; break; }
      i++;
    } else if (!strcmp(l.tok[0], "closed")) {
      for (int k = 1; k < l.ntok; k++) {
        closed = (char **)realloc(closed, sizeof(char *) * (nclosed + 1));
        closed[nclosed] = fl_strdup(l.tok[k]);
        decode_name(closed[nclosed]);
        nclosed++;
      }
      i++;
    } else {
      // A keyword from a later version: skip it and its children.
      i++;
      while (i < nlines && lines[i].depth > 0) i++;
    }
  }
  if (!have_dock) ok = 0;

  // Every name once.
  // The center is only in the dock, and only if there is one.
  char **names = 0;
  int count = 0;
  if (ok) ok = collect_names(dock_root, &names, count);
  for (int k = 0; ok && k < nwins; k++) ok = collect_names(wins[k].root, &names, count);
  free(names);
  int centers = count_centers(dock_root);
  if (centers > 1 || (centers == 1 && !center_)) ok = 0;
  for (int k = 0; k < nwins; k++) {
    if (count_centers(wins[k].root) > 0) ok = 0;
  }
  free_lines(lines, nlines);

  if (!ok) {
    free_node(dock_root);
    for (int k = 0; k < nwins; k++) free_node(wins[k].root);
    free(wins);
    for (int k = 0; k < nclosed; k++) free(closed[k]);
    free(closed);
    return -1;
  }

  Fl_Group *saved = Fl_Group::current();
  Fl_Group::current(0);
  resolve(this, dock_root, factory_, factory_data_);
  for (int k = 0; k < nwins; k++) resolve(this, wins[k].root, factory_, factory_data_);

  // Note where every docked panel is, for those the text leaves out, then
  // take all the panels out of the trees.
  int np;
  Fl_Dock_Panel **all = all_panels(np);
  int *was_docked = (int *)malloc(sizeof(int) * (np + 1));
  for (int k = 0; k < np; k++) {
    was_docked[k] = all[k]->docked();
    if (was_docked[k] && all[k]->dock() == this) remember_home(all[k]);
    // The end of a split a closed panel was at is in the tree being replaced.
    else if (!was_docked[k]) all[k]->home_edge(0);
  }
  for (int k = 0; k < np; k++) {
    Fl_Dock_Panel *p = all[k];
    p->hide();
    if (p->parent()) p->parent()->remove(p);
    store_->add(p);
  }
  if (center_ && center_->parent()) center_->parent()->remove(center_);
  if (root_ && root_ != center_) {
    Fl_Widget *old = root_;
    remove(old);
    Fl::delete_widget(old);
  }
  root_ = 0;
  while (nwindows_ > 0) {
    Fl_Dock_Window *win = windows_[nwindows_ - 1];
    nwindows_--;
    win->owner_ = 0;
    win->dock()->owner_ = 0;
    win->hide();
    if (win->parent()) win->parent()->remove(win);
    Fl::delete_widget(win);
  }
  clear_kept();

  // Build the dock, and its windows.
  if (dock_root) {
    Fl_Widget *r = build_node(dock_root);
    if (r) set_root(r);
  }
  if (center_ && !center_->parent()) {
    if (!root_) set_root(center_);
    else put(center_, FL_DOCK_RIGHT, 0, 0.75);
  }
  for (int k = 0; k < nwins; k++) {
    Layout_Window &lw = wins[k];
    if (lw.inset && !window()) continue;
    int X = lw.x;
    int Y = lw.y;
    if (lw.inset) {
      X += x();
      Y += y();
    }
    Fl_Dock_Window *win = new_window(X, Y, lw.w, lw.h, lw.inset);
    Fl_Widget *r = win->dock()->build_node(lw.root);
    if (!r) {
      delete win;
      continue;
    }
    win->dock()->set_root(r);
    win->dock()->normalize();
    const char *first = first_name(lw.root);
    Fl_Dock_Panel *fp = 0;
    if (first) fp = find(first);
    if (fp) win->copy_label(fp->label());
    win->keep_inside();
  }
  normalize();
  fit_root();

  // Keep places for names with no panel.
  keep_missing(dock_root, 0, 0);
  for (int k = 0; k < nwins; k++) keep_missing(wins[k].root, 0, 0);

  // Panels that were docked and that the text leaves out go back where they were.
  for (int k = 0; k < np; k++) {
    Fl_Dock_Panel *p = all[k];
    if (p->docked() || !was_docked[k]) continue;
    int listed = 0;
    for (int c = 0; c < nclosed; c++) {
      if (!strcmp(closed[c], p->name())) listed = 1;
    }
    if (!listed) show_panel(p);
  }
  free(all);
  free(was_docked);

  // Laid out first at the size it was saved at, the tree then keeps its
  // panels' sizes as it is fitted to this dock.
  if (saved_w > 0 && root_ && keep_sizes() && center_ && center_->parent()) {
    int ax, ay, aw, ah;
    area(ax, ay, aw, ah);
    if (saved_w != aw || saved_h != ah) {
      root_->resize(ax, ay, saved_w, saved_h);
      fit_root(1);
    }
  }

  free_node(dock_root);
  for (int k = 0; k < nwins; k++) free_node(wins[k].root);
  free(wins);
  for (int k = 0; k < nclosed; k++) free(closed[k]);
  free(closed);
  Fl_Group::current(saved);
  redraw();
  return 0;
}

// ---- Kept places ---------------------------------------------------------

/** \internal Keeps a place for a panel named name that the dock does not have. */
void Fl_Dock::keep_place(const char *name, const char *ref, int where, double fraction) {
  kept_ = (Kept *)realloc(kept_, sizeof(Kept) * (nkept_ + 1));
  Kept &k = kept_[nkept_++];
  k.name = fl_strdup(name);
  k.ref = 0;
  if (ref) k.ref = fl_strdup(ref);
  k.where = where;
  k.fraction = fraction;
}

/** \internal Returns 1 and the place kept for name, or 0. */
int Fl_Dock::kept_place(const char *name, const char **ref, int *where, double *fraction) const {
  for (int i = 0; i < nkept_; i++) {
    if (!strcmp(kept_[i].name, name)) {
      *ref = kept_[i].ref;
      *where = kept_[i].where;
      *fraction = kept_[i].fraction;
      return 1;
    }
  }
  return 0;
}

/** \internal Forgets every kept place. */
void Fl_Dock::clear_kept() {
  for (int i = 0; i < nkept_; i++) {
    free(kept_[i].name);
    free(kept_[i].ref);
  }
  free(kept_);
  kept_ = 0;
  nkept_ = 0;
}
