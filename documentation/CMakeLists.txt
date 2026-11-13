#
# The Fl_Dock manual, made with Doxygen.
#
# Copyright 2026 by Rob McDonald and others.
#
# This library is free software. Distribution and use rights are outlined in
# the file "COPYING" which should have been included with this file. If this
# file is missing or damaged, see the license at:
#
#     https://www.fltk.org/COPYING.php
#

find_package(Doxygen)
if(NOT DOXYGEN_FOUND)
  message(STATUS "Doxygen not found: the manual cannot be made")
  return()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

# The target html makes the manual in html/; it is not part of the default build.
add_custom_target(html
  COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Making the Fl_Dock manual in ${CMAKE_CURRENT_BINARY_DIR}/html"
  VERBATIM
)

if(FL_DOCK_INSTALL_HTML_DOCS)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/html
    OPTIONAL
  )
endif()
