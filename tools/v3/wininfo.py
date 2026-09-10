#!/usr/bin/env python3
"""列出 X 窗口树 (id, 几何, 标题)."""
import ctypes

x11 = ctypes.CDLL("libX11.so.6")
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
x11.XDefaultRootWindow.restype = ctypes.c_ulong
x11.XQueryTree.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
    ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong),
    ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)), ctypes.POINTER(ctypes.c_uint)]
x11.XQueryTree.restype = ctypes.c_int
x11.XGetGeometry.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
    ctypes.POINTER(ctypes.c_ulong)] + [ctypes.POINTER(ctypes.c_int)] * 2 + [ctypes.POINTER(ctypes.c_uint)] * 4
x11.XFetchName.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_char_p)]
x11.XFetchName.restype = ctypes.c_int

d = x11.XOpenDisplay(b":99")
root = x11.XDefaultRootWindow(d)

def children(w, depth=0):
    r = ctypes.c_ulong(); p = ctypes.c_ulong()
    ch = ctypes.POINTER(ctypes.c_ulong)(); n = ctypes.c_uint()
    if not x11.XQueryTree(d, w, ctypes.byref(r), ctypes.byref(p), ctypes.byref(ch), ctypes.byref(n)):
        return
    for i in range(n.value):
        wch = ch[i]
        rr = ctypes.c_ulong(); x = ctypes.c_int(); y = ctypes.c_int()
        wdt = ctypes.c_uint(); h = ctypes.c_uint(); bw = ctypes.c_uint(); dep = ctypes.c_uint()
        x11.XGetGeometry(d, wch, ctypes.byref(rr), ctypes.byref(x), ctypes.byref(y),
                         ctypes.byref(wdt), ctypes.byref(h), ctypes.byref(bw), ctypes.byref(dep))
        nm = ctypes.c_char_p()
        x11.XFetchName(d, wch, ctypes.byref(nm))
        print("  " * depth + f"0x{wch:x} @({x.value},{y.value}) {wdt.value}x{h.value} '{nm.value.decode() if nm.value else ''}'")
        children(wch, depth + 1)

children(root)
