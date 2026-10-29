#!/usr/bin/env python3
"""Drives the game window on an X display (Xvfb, no window manager).

    xkeys.py DISPLAY ACTION...

ACTION is one of:
    KEY          tap a key (X keysym name: s, space, BackSpace, Down, q, w, F11...)
    KEY:MS       hold it MS milliseconds
    wait:MS      wait MS milliseconds
    resize:WxH   move every top-level window to 0,0 and resize it to WxH

Game keys: s = X (menu), space = A, BackSpace = B, q / w = L / R, arrows.
"""
import ctypes
import sys
import time

x = ctypes.CDLL("libX11.so.6")
xtst = ctypes.CDLL("libXtst.so.6")
x.XOpenDisplay.restype = ctypes.c_void_p
x.XDefaultRootWindow.restype = ctypes.c_ulong
x.XStringToKeysym.restype = ctypes.c_ulong
x.XKeysymToKeycode.restype = ctypes.c_ubyte


def key(display, name, hold_ms):
    keysym = x.XStringToKeysym(name.encode())
    if not keysym:
        sys.exit(f"Unknown key: {name}")
    code = x.XKeysymToKeycode(display, ctypes.c_ulong(keysym))
    xtst.XTestFakeKeyEvent(display, code, True, 0)
    x.XFlush(display)
    time.sleep(hold_ms / 1000)
    xtst.XTestFakeKeyEvent(display, code, False, 0)
    x.XFlush(display)


def resize(display, width, height):
    root = ctypes.c_ulong(x.XDefaultRootWindow(display))
    parent, children = ctypes.c_ulong(), ctypes.POINTER(ctypes.c_ulong)()
    count = ctypes.c_uint()
    x.XQueryTree(display, root, ctypes.byref(ctypes.c_ulong()), ctypes.byref(parent),
                 ctypes.byref(children), ctypes.byref(count))
    for i in range(count.value):
        x.XMoveResizeWindow(display, ctypes.c_ulong(children[i]), 0, 0, width, height)
    x.XSync(display, 0)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    display = ctypes.c_void_p(x.XOpenDisplay(sys.argv[1].encode()))
    if not display.value:
        sys.exit(f"Cannot open display {sys.argv[1]}")
    for action in sys.argv[2:]:
        name, _, value = action.partition(":")
        if name == "wait":
            time.sleep(int(value) / 1000)
        elif name == "resize":
            width, height = value.split("x")
            resize(display, int(width), int(height))
        else:
            key(display, name, int(value) if value else 60)


if __name__ == "__main__":
    main()
