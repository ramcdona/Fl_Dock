#
# CMake build for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
#
# Copyright 2026 by Rob McDonald and others.
#
# This library is free software. Distribution and use rights are outlined in
# the file "COPYING" which should have been included with this file. If this
# file is missing or damaged, see the license at:
#
#     https://www.fltk.org/COPYING.php
#

cmake_minimum_required(VERSION 3.15)

# The version is kept in one place, FL/Fl_Dock_Export.H.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/FL/Fl_Dock_Export.H fl_dock_version_lines
  REGEX "^#define FL_DOCK_(MAJOR|MINOR|PATCH)_VERSION ")
foreach(part MAJOR MINOR PATCH)
  string(REGEX REPLACE ".*#define FL_DOCK_${part}_VERSION ([0-9]+).*" "\\1" fl_dock_${part} "${fl_dock_version_lines}")
endforeach()
project(Fl_Dock VERSION ${fl_dock_MAJOR}.${fl_dock_MINOR}.${fl_dock_PATCH} LANGUAGES C CXX)

option(FL_DOCK_BUILD_TEST "Build the test programs" ON)

# FLTK's package accepts only its own minor version, so check the lowest one here.
find_package(FLTK CONFIG REQUIRED)
if(FLTK_VERSION VERSION_LESS 1.4)
  message(FATAL_ERROR "Fl_Dock needs FLTK 1.4 or later, found ${FLTK_VERSION}")
endif()

# A shared Fl_Dock uses FLTK's shared libraries, so an application has one FLTK.
set(FL_DOCK_FLTK fltk::fltk)
set(FL_DOCK_FLTK_GL fltk::gl)
if(BUILD_SHARED_LIBS AND TARGET fltk::fltk-shared)
  set(FL_DOCK_FLTK fltk::fltk-shared)
  set(FL_DOCK_FLTK_GL fltk::gl-shared)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(FL_DOCK_HEADERS
  FL/Fl_Dock.H
  FL/Fl_Dock_Export.H
  FL/Fl_Dock_Panel.H
  FL/Fl_Dock_Split.H
  FL/Fl_Dock_Stripe.H
  FL/Fl_Dock_Tabs.H
  FL/Fl_Dock_Window.H
)

# Static unless BUILD_SHARED_LIBS is set.
add_library(fltk_dock
  src/Fl_Dock.cxx
  src/Fl_Dock_Drag.cxx
  src/Fl_Dock_Layout.cxx
  src/Fl_Dock_Panel.cxx
  src/Fl_Dock_Split.cxx
  src/Fl_Dock_Stripe.cxx
  src/Fl_Dock_Stripes.cxx
  src/Fl_Dock_Tabs.cxx
  src/Fl_Dock_Window.cxx
  ${FL_DOCK_HEADERS}
)
add_library(fltk::dock ALIAS fltk_dock)
target_include_directories(fltk_dock PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(fltk_dock PUBLIC ${FL_DOCK_FLTK})
set_target_properties(fltk_dock PROPERTIES EXPORT_NAME dock)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(fltk_dock PUBLIC FL_DOCK_DLL PRIVATE FL_DOCK_LIBRARY)
  set_target_properties(fltk_dock PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
  )
endif()
if(NOT MSVC)
  target_compile_options(fltk_dock PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

install(TARGETS fltk_dock EXPORT Fl_DockTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES ${FL_DOCK_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/FL)
install(EXPORT Fl_DockTargets NAMESPACE fltk:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Fl_Dock)
# The build tree holds a package too, so find_package() works from either.
export(EXPORT Fl_DockTargets NAMESPACE fltk:: FILE ${CMAKE_CURRENT_BINARY_DIR}/Fl_DockTargets.cmake)

configure_package_config_file(cmake/Fl_DockConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/Fl_DockConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Fl_Dock
)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/Fl_DockConfigVersion.cmake
  COMPATIBILITY SameMinorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/Fl_DockConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/Fl_DockConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Fl_Dock
)

if(FL_DOCK_BUILD_TEST)
  enable_testing()
  add_subdirectory(test)
endif()
