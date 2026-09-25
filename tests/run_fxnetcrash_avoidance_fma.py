"""crash parity FX-NETCRASH (REVIEW_G item 4, 2026-09-24): the traffic avoidance pipeline's FUSED
multiply-adds.

be6c3ead lowered every vmaddfp / vmaddfp128 of the avoidance pipeline as `a * b + c` -- two roundings
(MSVC /fp:precise never contracts). The console rounds each of them ONCE:
  0x8272C40C  `vmaddfp128 v4, v127, v3, v4`  feeler [4] = Dir * sin60 + (Right * cos60)
  0x8272C424  `vmaddfp128 v9, v127, v7, v9`  feeler [3] = Dir * sin75 + (Right * cos75)
              (vmaddfp128 vD = vA * vB + vD; the addend vD was rounded by its own vmulfp128 at
              0x8272C3C8 / 0x8272C3FC, the Dir * sin product is not)
  0x82719B5C  `vmaddfp v0, v12, v0, v11` (raw fields: vD = vA * vC + vB) passing score =
              (ImpactTimeMax - t) * ScoreFactor + (MaxDistance - vminfp(|space|, MaxDistance))
  0x8273D33C  `vmaddfp v0, v12, v0, v13` steering blend = (avoid - target) * mfSimTimeStep + target
The fix is std::fma lane by lane at those four sites. The remaining vnmsubfp / vmaddfp of the
pipeline are Newton steps on a vrefp / vrsqrtefp estimate (0x82719AC4/AC8, 0x82708E30/E34,
0x82708E50/E54, 0x82708E60/E64, 0x8272C73C/740); they stay inside the tree's exact-reciprocal
convention and are not exercised here.

The oracle is independent of the reconstruction: every expected value is computed HERE with exact
rational arithmetic and one round-to-nearest-even per console instruction (vmulfp128 / vsubfp round
once, vmaddfp rounds the exact a*b + c once, vrsqrtefp is exact-then-rounded as in the campaign's
emulator), on inputs searched (fixed seed) so that the fused and the twice-rounded result differ --
a double-rounding counterexample per site. The inputs keep every other step exact (dyadic dots,
power-of-two reciprocals, a passing space whose console x * rsqrt(x) equals the PC's sqrt), so a
mismatch can only come from the multiply-add under test.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_avoidance_fma.py [--rev <b5 rev>]
"""
from decimal import Decimal, getcontext
from fractions import Fraction
from pathlib import Path
import argparse
import math
import random
import re
import struct
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
FIXTURE = "AvoidFixture"

CONSTANTS = ["KF_AVOIDANCE_MIN_RISK", "KF_AVOIDANCE_MIN_TARGET_DIST", "KF_AVOIDANCE_SNAP_DOT",
             "KI_AVOID_DIAG_CAP", "KI_AVOID_QUIET_DIAG_CAP", "giAvoidDiagLines", "giAvoidQuietDiagLines"]
HELPERS = ["inline VecFloat SplatDrive(f32 lfValue)",
           "inline f32 AvoidVmxMax(f32 lfA, f32 lfB)",
           "inline f32 AvoidVmxMin(f32 lfA, f32 lfB)"]
FREE_BODIES = ["void Convert3DVectorTo2D(Vector3 l3DVector, Vector2& l2DVector)"]
BODIES = [
    "VecFloat TrafficEntityModule::GetAvoidPassImpactTimeMax() const",
    "VecFloat TrafficEntityModule::GetAvoidPassImpactTimeScoreFactor() const",
    "VecFloat TrafficEntityModule::GetAvoidPassMaxDistance() const",
    "VecFloat TrafficEntityModule::GetAvoidPassHeightSkip() const",
    "VecFloat TrafficEntityModule::Avoidance_CalculateDistancePosVelToOrigin(",
    "VecFloat TrafficEntityModule::Avoidance_CalculatePassingScore(",
    "void TrafficEntityModule::Avoidance_CalculateFeelers(",
    "void TrafficEntityModule::CalculateAndSetSteeringUsingAvoidance(",
]

# Construct's seeds (0x82740690): kfVehicle_AvoidancePassingFactor_Constants = {flt_820BA8DC 4.0,
# flt_820BA5E4 10.0, flt_820BA5E4 10.0, flt_820BA5F4 3.0}; the feeler angles are 75 / 60 degrees.
IMPACT_TIME_MAX, SCORE_FACTOR, MAX_DISTANCE = 4.0, 10.0, 10.0


# ---- f32 arithmetic, exact -------------------------------------------------------------------------
def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def bits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def rn(value):
    """A Fraction rounded to the nearest f32 (ties to even); normal range only."""
    if value == 0:
        return 0.0
    sign = -1 if value < 0 else 1
    a = abs(value)
    e = a.numerator.bit_length() - a.denominator.bit_length()
    if Fraction(2) ** e > a:
        e -= 1
    assert -126 <= e <= 127, "outside the normal f32 range"
    quantum = Fraction(2) ** (e - 23)
    m = a / quantum
    n = m.numerator // m.denominator
    rest = m - n
    if rest > Fraction(1, 2) or (rest == Fraction(1, 2) and n & 1):
        n += 1
    return sign * float(n * quantum)


F = Fraction


def mul(a, b):            # vmulfp128
    return rn(F(a) * F(b))


def sub(a, b):            # vsubfp / vsubfp128
    return rn(F(a) - F(b))


def madd(a, b, c):        # vmaddfp: a * b + c, ONE rounding
    return rn(F(a) * F(b) + F(c))


def madd_twice(a, b, c):  # the pre-fix lowering: the product rounded, then the sum
    return rn(F(mul(a, b)) + F(c))


def rsqrte(x):            # vrsqrtefp, exact-then-rounded (the campaign emulator's model)
    getcontext().prec = 60
    exact = F(x)
    root = (Decimal(exact.numerator) / Decimal(exact.denominator)).sqrt()
    return rn(F(Decimal(1) / root))


def console_sqrt(x):
    """Avoidance_CalculateDistancePosVelToOrigin's tail 0x82708E40..0x82708E6C: two Newton steps
    on vrsqrtefp (vnmsubfp e = 1 - x*(r*r), vmaddfp r = (r*0.5)*e + r), then x * r."""
    r = rsqrte(x)
    for _ in range(2):
        e = rn(1 - F(x) * F(mul(r, r)))
        r = rn(F(mul(r, 0.5)) * F(e) + F(r))
    return mul(x, r)


# ---- the counterexamples ---------------------------------------------------------------------------
def rand_f32(rng, lo, hi):
    return f32(rng.uniform(lo, hi))


def feeler_cases(rng, cos_sin):
    """[1]..[4] for Dir / Right lanes where every + lane's fused result differs from two roundings."""
    cases = []
    while len(cases) < 3:
        d = [rand_f32(rng, -1.0, 1.0) for _ in range(3)]
        r = [rand_f32(rng, -1.0, 1.0) for _ in range(3)]
        feelers = []
        differs = True
        for c, s in cos_sin:
            minus = [sub(mul(d[i], s), mul(r[i], c)) for i in range(3)]
            feelers.append(minus)
        for c, s in cos_sin:
            plus = [madd(d[i], s, mul(r[i], c)) for i in range(3)]
            old = [madd_twice(d[i], s, mul(r[i], c)) for i in range(3)]
            differs = differs and all(plus[i] != old[i] for i in range(3))
            feelers.append(plus)
        if differs:
            cases.append((d, r, feelers))
    return cases


def passing_cases(rng):
    """A = origin moving (0, 0, v), B = (a, 0, 0) standing: t = a^2 / v^2 and the XZ miss is |a|,
    both exact; the score's multiply-add is the only inexact step that differs."""
    cases = []
    while len(cases) < 3:
        v = 2.0 ** rng.randint(-1, 3)
        a = rn(F(rng.randint(2049, 4095)) * F(2) ** rng.randint(-12, -8))     # 12 significant bits
        if not (0 < a < min(2 * v, 9.99)):
            continue
        t = rn(F(a) * F(a) * F(rn(F(1) / (F(v) * F(v)))))
        if t > IMPACT_TIME_MAX or F(t) != F(a) * F(a) / (F(v) * F(v)):
            continue
        x = rn(F(a) * F(a))
        if F(x) != F(a) * F(a) or console_sqrt(x) != a:
            continue
        passing = sub(MAX_DISTANCE, min(a, MAX_DISTANCE))
        lead = sub(IMPACT_TIME_MAX, t)
        fused = madd(lead, SCORE_FACTOR, passing)
        twice = madd_twice(lead, SCORE_FACTOR, passing)
        if fused != twice:
            cases.append((a, v, fused, twice))
    return cases


def blend_cases(rng, step):
    cases = []
    while len(cases) < 3:
        avoid = [rand_f32(rng, 0.5, 1.0), rand_f32(rng, -0.2, 0.2), rand_f32(rng, -0.3, 0.3)]
        target = [rand_f32(rng, -0.3, 0.3), rand_f32(rng, -0.2, 0.2), rand_f32(rng, 0.5, 1.0)]
        dot = sum(F(avoid[i]) * F(target[i]) for i in range(3))
        if dot >= F(1, 2):
            continue
        delta = [sub(avoid[i], target[i]) for i in range(3)]
        fused = [madd(delta[i], step, target[i]) for i in range(3)]
        twice = [madd_twice(delta[i], step, target[i]) for i in range(3)]
        if all(fused[i] != twice[i] for i in range(3)):
            cases.append((avoid, target, fused))
    return cases


def u32s(values):
    return ", ".join(f"0x{bits(v):08X}u" for v in values)


def cases_inc():
    rng = random.Random(0xB5FEE1)
    c75, s75 = f32(math.cos(math.radians(75))), f32(math.sin(math.radians(75)))
    c60, s60 = f32(0.5), f32(math.sin(math.radians(60)))
    step = f32(1.0 / 30.0)
    lines = ["// generated by run_fxnetcrash_avoidance_fma.py -- exact rational model, one rounding per",
             "// console instruction; every case is a fused-vs-twice-rounded counterexample",
             f"static const u32 KU_COS_SIN[2][2] = {{ {{ {u32s([c75, s75])} }}, {{ {u32s([c60, s60])} }} }};",
             f"static const u32 KU_STEP = 0x{bits(step):08X}u;"]
    feelers = feeler_cases(rng, [(c75, s75), (c60, s60)])
    lines.append("struct FeelerCase { u32 dir[3]; u32 right[3]; u32 feeler[4][3]; };")
    lines.append("static const FeelerCase KA_FEELER_CASES[] = {")
    for d, r, fs in feelers:
        lines.append("    { { " + u32s(d) + " }, { " + u32s(r) + " }, { "
                     + ", ".join("{ " + u32s(f) + " }" for f in fs) + " } },")
    lines.append("};")
    passing = passing_cases(rng)
    lines.append("struct PassingCase { u32 a; u32 v; u32 fused; u32 twice; };")
    lines.append("static const PassingCase KA_PASSING_CASES[] = {")
    for a, v, fused, twice in passing:
        lines.append(f"    {{ {u32s([a, v, fused, twice])} }},")
    lines.append("};")
    blends = blend_cases(rng, step)
    lines.append("struct BlendCase { u32 avoid[3]; u32 target[3]; u32 fused[3]; };")
    lines.append("static const BlendCase KA_BLEND_CASES[] = {")
    for avoid, target, fused in blends:
        lines.append("    { { " + u32s(avoid) + " }, { " + u32s(target) + " }, { " + u32s(fused) + " } },")
    lines.append("};")
    return "\n".join(lines) + "\n"


def constant_line(source, name):
    match = re.search(r"^[ \t]*(?:const[ \t]+)?[\w:]+[ \t]+" + re.escape(name) + r"[ \t]*=[^;]*;", source, re.M)
    if match is None:
        raise ValueError("constant absent: " + name)
    return match.group(0).strip()


NUMERIC_CHECKS = 3 + 3 + 3 + 1   # feeler cases, passing cases, blend cases, the blend gate witness


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        parts = ["namespace BrnTraffic {", "namespace {"]
        parts += [constant_line(module, name) for name in CONSTANTS]
        parts += [definition(module, helper) for helper in HELPERS]
        parts.append("}")
        parts += [definition(module, body) for body in FREE_BODIES]
        parts += [definition(module, body).replace("TrafficEntityModule::", FIXTURE + "::", 1) for body in BODIES]
        parts.append("}")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    cases = cases_inc()
    return compile_and_run(Path(__file__).with_name("FxNetcrashAvoidanceFma.cpp"), "fma_bodies.inc",
                           "\n".join(parts), "FxNetcrashAvoidanceFma", extra_sources=[STRSTREAM_CPP],
                           extra_files={"fma_cases.inc": cases})


def wiring(tree):
    module = tree.read(MODULE_CPP)
    feelers = definition(module, "void TrafficEntityModule::Avoidance_CalculateFeelers(") \
        if "void TrafficEntityModule::Avoidance_CalculateFeelers(" in module else ""
    score = definition(module, "VecFloat TrafficEntityModule::Avoidance_CalculatePassingScore(") \
        if "VecFloat TrafficEntityModule::Avoidance_CalculatePassingScore(" in module else ""
    blend = definition(module, "void TrafficEntityModule::CalculateAndSetSteeringUsingAvoidance(") \
        if "void TrafficEntityModule::CalculateAndSetSteeringUsingAvoidance(" in module else ""
    return [
        ("feelers [3] / [4]: the + side is std::fma per lane (vmaddfp128 0x8272C424 / 0x8272C40C)",
         len(re.findall(r"std::fma\(\s*lDirection\.[xyzw]\s*,\s*lfSin\s*,", feelers)) == 4),
        ("passing score: one std::fma (vmaddfp 0x82719B5C)", "std::fma(" in score),
        ("steering blend: std::fma per lane (vmaddfp 0x8273D33C)",
         len(re.findall(r"std::fma\(\s*lDelta\.[xyzw]\s*,\s*mfSimTimeStep\s*,", blend)) == 4),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxnetcrash_avoidance_fma", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
