//
// Export macro for the Fl_Dock extension to the Fast Light Tool Kit (FLTK).
//
// Copyright 2026 by Rob McDonald and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//

#ifndef Fl_Dock_Export_H
#define Fl_Dock_Export_H

/*
  FL_DOCK_EXPORT marks the classes of the Fl_Dock library, which is built
  apart from FLTK and so cannot use FL_EXPORT: when an application links
  FLTK's shared library, FL_EXPORT declares classes imported from FLTK's
  DLL.  Built as a static library, the default, FL_DOCK_EXPORT is empty.
  Built as a shared one, FL_DOCK_DLL is defined for the library and for the
  code that uses it, and FL_DOCK_LIBRARY for the library alone.
*/
#if defined(FL_DOCK_DLL)
#  if defined(_WIN32)
#    if defined(FL_DOCK_LIBRARY)
#      define FL_DOCK_EXPORT __declspec(dllexport)
#    else
#      define FL_DOCK_EXPORT __declspec(dllimport)
#    endif
#  elif defined(__GNUC__) && __GNUC__ >= 4
#    define FL_DOCK_EXPORT __attribute__((visibility("default")))
#  else
#    define FL_DOCK_EXPORT
#  endif
#else
#  define FL_DOCK_EXPORT
#endif

/** \name Version Numbers

  The version of the Fl_Dock headers.  Fl_Dock::api_version() gives the
  version of the library an application is linked with.
*/
/**@{*/

/** The major release version of these Fl_Dock headers. */
#define FL_DOCK_MAJOR_VERSION 0

/** The minor release version of these Fl_Dock headers. */
#define FL_DOCK_MINOR_VERSION 1

/** The patch version of these Fl_Dock headers. */
#define FL_DOCK_PATCH_VERSION 0

/**
  The version of these Fl_Dock headers as one int, major * 10000 + minor *
  100 + patch, as FL_API_VERSION is for FLTK.
*/
#define FL_DOCK_API_VERSION (FL_DOCK_MAJOR_VERSION * 10000 + FL_DOCK_MINOR_VERSION * 100 + FL_DOCK_PATCH_VERSION)

/**@}*/

#endif // !Fl_Dock_Export_H
