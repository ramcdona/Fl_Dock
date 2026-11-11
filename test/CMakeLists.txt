#
# Test programs for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
#
# Copyright 2026 by Rob McDonald and others.
#
# This library is free software. Distribution and use rights are outlined in
# the file "COPYING" which should have been included with this file. If this
# file is missing or damaged, see the license at:
#
#     https://www.fltk.org/COPYING.php
#

# Unit tests of the tree, the drop targets and layouts.  They show no window,
# but on X11 and Wayland measuring text needs a display.
add_executable(unittest_dock unittest_dock.cxx)
target_link_libraries(unittest_dock PRIVATE fltk::dock)
add_test(NAME unittest_dock COMMAND unittest_dock)

# OpenGL views drawing wherever the dock puts them; it needs a display.
add_executable(gl_dock gl_dock.cxx)
target_link_libraries(gl_dock PRIVATE fltk::dock ${FL_DOCK_FLTK_GL})
add_test(NAME gl_dock COMMAND gl_dock)
set_tests_properties(gl_dock PROPERTIES LABELS display TIMEOUT 30)
