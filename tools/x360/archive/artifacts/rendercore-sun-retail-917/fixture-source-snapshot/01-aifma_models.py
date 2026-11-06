"""Generate b5-decomp/tests/FxGateAiFusedData.h: the console's result for each AI fused site, computed with
EXACT rational arithmetic (one rounding for fmadds / fnmsubs / vmaddfp, ROUNDING_RULE 3; each single op
rounded, rule 4), on inputs chosen so that most rows differ from the twice-rounded spelling.

Every row stores its inputs and the console output as f32 bit patterns, plus a flag that says whether the
twice-rounded spelling (the pre-fix PC body) gives a different bit pattern.
"""
from fractions import Fraction
import math
import random
import struct
import sys

import numpy as np



def bits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def val(u):
    return struct.unpack("<f", struct.pack("<I", u))[0]


def f32(x):
    return float(np.float32(x))


TWO = Fraction(2)


def rn_f32(q):
    """Round a nonzero Fraction to the nearest f32, ties to even (normal, subnormal, overflow)."""
    neg = q < 0
    a = -q if neg else q
    e = a.numerator.bit_length() - a.denominator.bit_length()
    while TWO ** e > a:
        e -= 1
    while TWO ** (e + 1) <= a:
        e += 1
    scale = 149 if e < -126 else 23 - e
    m = a * TWO ** scale
    n = m.numerator // m.denominator
    r = m - n
    if r > Fraction(1, 2) or (r == Fraction(1, 2) and (n & 1)):
        n += 1
    v = Fraction(n) / TWO ** scale
    if v >= TWO ** 128:
        return -math.inf if neg else math.inf
    fv = float(v)
    assert bits(fv) == bits(float(np.float32(fv))), "not an f32"
    return -fv if neg else fv


def fma(a, c, b):
    """IEEE fused multiply-add a*c + b with one rounding to f32."""
    if math.isnan(a) or math.isnan(b) or math.isnan(c):
        return math.nan
    if math.isinf(a) or math.isinf(c):
        if a == 0 or c == 0:
            return math.nan
        sign = math.copysign(1.0, a) * math.copysign(1.0, c)
        if math.isinf(b) and math.copysign(1.0, b) != sign:
            return math.nan
        return sign * math.inf
    if math.isinf(b):
        return b
    q = Fraction(a) * Fraction(c) + Fraction(b)
    if q == 0:
        product_negative = math.copysign(1.0, a) * math.copysign(1.0, c) < 0
        if (a == 0 or c == 0) and b == 0:
            negative = product_negative and math.copysign(1.0, b) < 0
        else:
            negative = False
        return -0.0 if negative else 0.0
    return rn_f32(q)


def op(fn, *args):
    with np.errstate(all="ignore"):
        return float(fn(*[np.float32(x) for x in args]))


def add(a, b):
    return op(lambda x, y: x + y, a, b)


def sub(a, b):
    return op(lambda x, y: x - y, a, b)


def mul(a, b):
    return op(lambda x, y: x * y, a, b)


def div(a, b):
    return op(lambda x, y: x / y, a, b)


def fnmsubs(a, c, b):
    d = fma(a, c, -b)
    return d if math.isnan(d) else -d


def same(x, y):
    if math.isnan(x) and math.isnan(y):
        return True
    return bits(x) == bits(y)


# ---- the constants, as read from the image (x360rd.py) ------------------------------------------------
LOW = val(0x3FB2B8C2)      # @0x82F30428 KF_STEER_AT_LOW_SPEED
HIGH = val(0x3F000000)     # @0x820C4168 KF_STEER_AT_HIGH_SPEED
BASE = val(0x41200000)     # @0x820C4150 KF_FAN_LOOK_AHEAD_BASE
GROWTH = val(0x41700000)   # @0x820C4238 KF_FAN_LOOK_AHEAD_GROWTH
SEVENTH = val(0x3E124925)  # @0x82013AB4 1/7
SEVEN = val(0x40E00000)    # @0x820054D0 7.0
ONE = val(0x3F800000)      # @0x82001C98 1.0
DIFF = val(0x3DCCCCCD)     # @0x820C424C KF_SPEED_DIFFERENCE_MULTIPLIER
SPREAD = val(0x40000000)   # @0x820C41F4 KF_STARTING_LINE_SPREAD
WIDTH = val(0x41000000)    # @0x820C41E0 KF_STARTING_LINE_CAR_WIDTH
BRAKE = val(0x3F400000)    # @0x820C41FC KF_AI_MAX_BRAKING_SPEED_PROPORTION


# ---- per site: (console model, twice-rounded model, input maker) -----------------------------------------
def fan_console(r):
    return fma(sub(HIGH, LOW), r, LOW)


def fan_head(r):
    return add(mul(sub(HIGH, LOW), r), LOW)


def look_console(r):
    return fma(r, GROWTH, BASE)


def look_head(r):
    return add(mul(r, GROWTH), BASE)


def seg_console(frac, prev):
    return mul(fnmsubs(f32(prev), SEVENTH, frac), SEVEN)


def seg_head(frac, prev):
    return mul(sub(frac, mul(f32(prev), SEVENTH)), SEVEN)


def lerp_console(prev, nxt, s):
    return fma(sub(nxt, prev), s, prev)


def lerp_head(prev, nxt, s):
    return add(mul(sub(nxt, prev), s), prev)


def mult_console(race, target):
    return fma(sub(race, target), DIFF, ONE)


def mult_head(race, target):
    return add(mul(sub(race, target), DIFF), ONE)


def completion_console(start, end, ratio):
    return fma(sub(end, start), ratio, start)


def completion_head(start, end, ratio):
    return add(mul(sub(end, start), ratio), start)


def road_console(draw, width):
    return sub(fma(draw, SPREAD, div(WIDTH, width)), ONE)


def road_head(draw, width):
    return sub(add(mul(draw, SPREAD), div(WIDTH, width)), ONE)


def prox_console(v, mn, t):
    return fma(sub(mn, v), t, v)


def prox_head(v, mn, t):
    return add(v, mul(sub(mn, v), t))


def corner_console(inp, scaled, ramp):
    return fma(sub(scaled, inp), ramp, inp)


def corner_head(inp, scaled, ramp):
    return add(inp, mul(sub(scaled, inp), ramp))


def dist_console(x, y, z):
    return fma(x, x, mul(z, z))


def dist_head(x, y, z):
    return add(mul(x, x), mul(z, z))


