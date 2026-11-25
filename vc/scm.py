"""Minimal Vice City CLEO SCM assembler (same encoder as BlackHoleMod_src/build.py)."""
import struct


class Scm:
    def __init__(self):
        self.b = bytearray()
        self.labels = {}
        self.fix = []

    @staticmethod
    def i(v):
        v = int(v)
        if v >= 2 ** 31:
            v -= 2 ** 32
        return b"\x01" + struct.pack("<i", v)

    @staticmethod
    def f(v):
        return b"\x06" + struct.pack("<f", v)

    @staticmethod
    def lv(n):
        return b"\x03" + struct.pack("<H", n)

    @staticmethod
    def s(text):                      # CLEO long string
        t = text.encode("ascii")
        return b"\x0e" + bytes([len(t)]) + t

    @staticmethod
    def t8(text):                     # 8-byte text label (no type byte)
        return text.encode("ascii")[:7].ljust(8, b"\0")

    def label(self, name):
        self.labels[name] = len(self.b)

    def op(self, opcode, *args):
        self.b += struct.pack("<H", opcode)
        for a in args:
            if isinstance(a, tuple) and a[0] == "@":
                self.b += b"\x01"
                self.fix.append((len(self.b), a[1]))
                self.b += b"\0\0\0\0"
            else:
                self.b += a

    def raw(self, data):
        self.b += data

    def build(self):
        for pos, name in self.fix:
            # CLEO scripts use negative offsets relative to script start
            struct.pack_into("<i", self.b, pos, -self.labels[name])
        return bytes(self.b)


def L(name):
    return ("@", name)


IF = b"\x04\x00"
