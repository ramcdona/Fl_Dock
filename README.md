# Fl_Dock

Docking panels for [FLTK](https://www.fltk.org) 1.4 and later: an application's panels can be arranged side by
side, stacked as tabs, floated in windows of their own or slid out from stripes along the window's edges, by
dragging them, and the arrangement saved and restored.

Fl_Dock is written for every platform FLTK supports, Wayland included, and follows FLTK's conventions and license.
So far its tests have been run on macOS only; Windows, X11 and Wayland still need them run there.

## Building

Fl_Dock needs FLTK 1.4 or later and CMake 3.15 or later.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/fltk/install -DCMAKE_INSTALL_PREFIX=/path/to/install
cmake --build build
ctest --test-dir build
cmake --install build
```

This builds the static library `fltk_dock`, the test programs in `test/`, and installs the library, its headers
in `FL/` and a CMake package, so another project can use `find_package(Fl_Dock)` and link `fltk::dock`.

## Documentation

The manual is made with Doxygen, as FLTK's is:

```sh
cmake -S . -B build -DFL_DOCK_BUILD_HTML_DOCS=ON
cmake --build build --target html
```

It is then in `build/documentation/html/index.html`: building and a first program, how Fl_Dock works, and every
class and function.  `test/dock.cxx` is a complete program that shows every feature.

## License

Fl_Dock is distributed under the FLTK license; see [COPYING](COPYING).
