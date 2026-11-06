"""FX-GATE SinCos work: an emu64 CPU with the VMX128 forms the inlined XMVectorSinCos blocks use, an exact Python
model of src/SDKs/XboxMath/XMVectorSinCos.h (e13f4b71), and a window runner that locates a block's sin / cos
registers and compares them with the model over many angles."""
import math
import os
import random
import struct
import sys

import numpy as np

EMU = r"D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu"
sys.path.insert(0, EMU)
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "FX-GATE.aifma"))   # aifma_models.py
import emu64 as E   # noqa: E402
import vmxemu as V  # noqa: E402
from aifma_models import bits, val, f32, fma, mul, sub, add  # noqa: E402

M32 = 0xFFFFFFFF
STACK = 0x44000000
SELF = 0x40000000
AUX = 0x40020000


class CPU(E.CPU64):
    def exec_vector(self, a, w, mn):
        op = w >> 26
        if op == 6:
            vd = ((w >> 21) & 0x1F) | (((w >> 2) & 3) << 5)
            vb = ((w >> 11) & 0x1F) | ((w & 3) << 5)
            if (w & 0x630) == 0x210:   # vpermwi128 vD, vB, PERM (all PERM values; vmxemu's 0x730 mask misses bit 0x100)
                perm = ((w >> 16) & 0x1F) | (((w >> 6) & 7) << 5)
                B = self.vr[vb]
                self.vr[vd] = tuple(B[(perm >> (6 - 2 * i)) & 3] for i in range(4))
                return True
            if (w & 0x7F0) == 0x7F0:   # vupkd3d128 vD, vB, TYPE (NORMSHORT2 only, as FX-DIRECTOR2's camrigemu)
                typ = ((w >> 16) & 0x1F) >> 2
                if typ != 1:
                    raise NotImplementedError("vupkd3d128 type %d at %08X" % (typ, a))
                wv = self.vr[vb][3]
                hi = struct.unpack(">h", struct.pack(">H", (wv >> 16) & 0xFFFF))[0]
                lo = struct.unpack(">h", struct.pack(">H", wv & 0xFFFF))[0]
                self.setvf(vd, [np.float32(3.0 + hi * 2.0 ** -22), np.float32(3.0 + lo * 2.0 ** -22),
                                np.float32(0.0), np.float32(1.0)])
                return True
        if op == 4 and (w & 0x7FF) == 522:   # vrfin: round to nearest, ties to even
            vd = (w >> 21) & 0x1F
            vb = (w >> 11) & 0x1F
            self.setvf(vd, [np.float32(np.rint(np.float64(x))) for x in self.vf(vb)])
            return True
        return super().exec_vector(a, w, mn)


def new_cpu():
    mem = V.Mem()
    mem.add(STACK, 0x20000)
    mem.add(SELF, 0x20000)
    mem.add(AUX, 0x20000)
    c = CPU(mem)
    E.install_assert_hooks(c)
    c.setg(1, STACK + 0x10000)
    return c


def step_one(c, pc):
    w = c.fetch(pc)
    if not c.exec_vector(pc, w, "w%08X" % w):
        c.step(pc, w, (pc + 4) & M32, 0)


def window(c, start, stop, skip=()):
    pc, n = start, 0
    while pc != stop:
        w = c.fetch(pc)
        nxt = (pc + 4) & M32
        if pc in skip:
            pc = nxt
            continue
        if c.exec_vector(pc, w, "w%08X" % w):
            pc = nxt
        else:
            pc = c.step(pc, w, nxt, 0)
        n += 1
        if n > 50000:
            raise RuntimeError("runaway at %08X" % pc)


def splat(c, reg, x):
    c.setvf(reg, [np.float32(x)] * 4)


def lane_bits(c, reg, lane):
    return c.vr[reg][lane] & M32


# ---- the exact model of XboxMath::XMVectorSinCos (e13f4b71) --------------------------------------------------
TWO_PI = val(0x40C90FDB)
RECIP_TWO_PI = val(0x3E22F983)
SIN = [val(u) for u in (0xBE2AAAAB, 0x3C088889, 0xB9500D01, 0x3638EF1D, 0xB2D7322B, 0x2F309231, 0xAB573F9F,
                        0x274A963C, 0xA317A4DA, 0x1EB8DC78, 0x9A3B0DA1)]
COS = [val(u) for u in (0xBF000000, 0x3D2AAAAB, 0xBAB60B61, 0x37D00D01, 0xB493F27E, 0x310F76C8, 0xAD49CBA5,
                        0x29573F9F, 0xA53413C3, 0x20F2A15D, 0x9C8671CB)]
PAIRS = {2: (1, 1), 3: (2, 1), 4: (2, 2), 5: (3, 2), 6: (3, 3), 7: (4, 3), 8: (4, 4), 9: (5, 4), 10: (5, 5),
         11: (6, 5), 12: (6, 6), 13: (7, 6), 14: (7, 7), 15: (8, 7), 16: (8, 8), 17: (9, 8), 18: (9, 9),
         19: (10, 9), 20: (10, 10), 21: (11, 10), 22: (11, 11), 23: (12, 11)}


def vnmsub(a, c, b):
    d = fma(a, c, -b)
    return d if math.isnan(d) else -d


def xm_sincos(v):
    q = mul(v, RECIP_TWO_PI)
    n = float(np.float32(np.rint(np.float64(q)))) if not math.isnan(q) else q
    p = [None] * 24
    p[1] = vnmsub(TWO_PI, n, v)
    for k in range(2, 24):
        i, j = PAIRS[k]
        p[k] = mul(p[i], p[j])
    s, co = p[1], 1.0
    for t in range(11):
        s = fma(SIN[t], p[3 + 2 * t], s)
        co = fma(COS[t], p[2 + 2 * t], co)
    return s, co


def same(x, y):
    if math.isnan(x) and math.isnan(y):
        return True
    return bits(x) == bits(y)


def angles(n, seed, lo=-8.0, hi=8.0, extra=()):
    rng = random.Random(seed)
    out = [0.0, -0.0, math.pi / 2, -math.pi / 2, val(0x40490FDB), -val(0x40490FDB), 1.0, -1.0, 0.25, 1e-3, -1e-3]
    out += list(extra)
    while len(out) < n:
        out.append(f32(rng.uniform(lo, hi)))
    return [f32(x) for x in out]


def generic_cpu(gpr_tables, vpresets, scratch_gprs=()):
    c = new_cpu()
    for r in scratch_gprs:
        c.setg(r, AUX + 0x800)
    for r, v in gpr_tables.items():
        c.setg(r, v)
    for reg, vals in vpresets.items():
        c.setvf(reg, [np.float32(x) for x in vals])
    return c


def locate(start, stop, setup, test_angles, skip=()):
    """Run the window for each angle; return the (reg, lane) cells equal to the model's sin and cos for ALL angles."""
    sin_cells = cos_cells = None
    for a in test_angles:
        c = setup(a)
        window(c, start, stop, skip)
        s, co = xm_sincos(a)
        ss = {(r, l) for r in range(128) for l in range(4) if same(val(c.vr[r][l]), s)}
        cc = {(r, l) for r in range(128) for l in range(4) if same(val(c.vr[r][l]), co)}
        sin_cells = ss if sin_cells is None else sin_cells & ss
        cos_cells = cc if cos_cells is None else cos_cells & cc
    return sorted(sin_cells), sorted(cos_cells)


def verify(start, stop, setup, cell_sin, cell_cos, test_angles, skip=(), negate_sin=False):
    bad = 0
    for a in test_angles:
        c = setup(a)
        window(c, start, stop, skip)
        s, co = xm_sincos(a)
        es = val(c.vr[cell_sin[0]][cell_sin[1]])
        ec = val(c.vr[cell_cos[0]][cell_cos[1]])
        if not (same(es, s) and same(ec, co)):
            bad += 1
            if bad < 5:
                print("  DIFF angle %r emu %08X %08X model %08X %08X" % (a, bits(es), bits(ec), bits(s), bits(co)))
    return bad
