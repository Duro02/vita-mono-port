#!/usr/bin/env python3
"""向指定 X 窗口发送按键. 用法: key.py WIN_ID_HEX KEYSYM_HEX"""
import ctypes, sys, time

x11 = ctypes.CDLL("libX11.so.6")
P = ctypes.c_void_p; U = ctypes.c_ulong; I = ctypes.c_int
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]; x11.XOpenDisplay.restype = P
x11.XKeysymToKeycode.argtypes = [P, U]; x11.XKeysymToKeycode.restype = ctypes.c_ubyte
x11.XSendEvent.argtypes = [P, U, I, ctypes.c_long, ctypes.c_void_p]
x11.XSendEvent.restype = I
x11.XFlush.argtypes = [P]
x11.XSetInputFocus.argtypes = [P, U, I, U]

win = int(sys.argv[1], 16)
sym = int(sys.argv[2], 16)
d = x11.XOpenDisplay(b":99")
kc = x11.XKeysymToKeycode(d, sym)
print(f"keysym {hex(sym)} -> keycode {kc}")
x11.XSetInputFocus(d, win, 1, 0)
time.sleep(0.2)
for etype in (2, 3):  # KeyPress, KeyRelease
    ev = ctypes.create_string_buffer(192)
    import struct
    struct.pack_into("i", ev, 0, etype)
    struct.pack_into("i", ev, 16, 1)
    struct.pack_into("P", ev, 24, d)
    struct.pack_into("Q", ev, 32, win)
    struct.pack_into("Q", ev, 40, win)
    struct.pack_into("i", ev, 56, 0)
    struct.pack_into("ii", ev, 64, 0, 0)
    struct.pack_into("I", ev, 80, 0)
    struct.pack_into("I", ev, 84, kc)
    struct.pack_into("i", ev, 88, 1)
    x11.XSendEvent(d, win, 1, 0, ev)
    x11.XFlush(d)
    time.sleep(0.2)
print("sent", flush=True)
