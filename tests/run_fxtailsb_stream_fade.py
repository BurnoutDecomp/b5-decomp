"""FX-TAILS-B (crash parity 2026-09-24, item 3): StreamingEffect::Detach @0x826EEA68 -- a stopped stream's
fade-out.

ARTIST: the send gain of every fading frame is GetOutput(1 - f, E_ONE_MINUS_EQPWR) * mfGainPreFade, the
curve inlined at 0x826EEB38..0x826EEBC8 (the CgsSoundUtils.cpp:161 assert, `fmsubs x*511 - 511`,
`fctiwz`, 1 - gafArraySinTable[..]) -- the equal-power 1 - sin(f * pi/2) -- where f = t / GetFadeOut()
(`fdivs`, no guard on the length) is clamped by `fneg ; fsel` + `fsubs ; fsel` (a NaN -> 1.0). The
pre-fade gain is read from the "Send01" send (dword_8300A6E0 = MakeHash("Send01"), CRT 0x82C61A34), and
GetFadeOut is read again for the finish test (0x826EEBD4). The PC faded LINEARLY ((1 - f) * pre), guarded
the length (`fade > 0 ? .. : 1.0` -- a negative length silenced the send), clamped with std::min/max (a
NaN position -> 0 -> full gain) and read the pre-fade gain from the stream's own send name.

Numeric: tests/FxTailsBStreamFade.cpp compiles the PRODUCTION Detach and the production Curve::GetOutput
(+ its table, constant, read helper) against a fixture effect / voice; the expected gains come from the
IMAGE's table words. --rev reads a b5 revision (the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsb_stream_fade.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report
from run_fxvoicepool_curve import optional, LAST_ELEMENT, GET_OUTPUT, TABLE, READ_HELPER, UTILS_CPP

EFFECT_CPP = "src/GameSource/Sound/Streaming/BrnStreamingEffect.cpp"
DETACH = "bool StreamingEffect::Detach()"
NUMERIC_CHECKS = 15


def case_arm(body, label):
    """The code of one `case <label>:` arm, up to the next case / default label."""
    match = re.search(r"\bcase\s+" + label + r"\s*:", body)
    if match is None:
        return ""
    rest = body[match.end():]
    following = re.search(r"\bcase\s+\w+\s*:|\bdefault\s*:", rest)
    return rest[:following.start()] if following else rest


def wiring(tree):
    body = body_or_empty(tree.read(EFFECT_CPP), DETACH)
    none_arm = case_arm(body, "E_DETACH_STATE_NONE")
    begin_arm = case_arm(body, "E_DETACH_STATE_BEGIN")
    return [
        ("case 0 reads the pre-fade gain from the \"Send01\" send (dword_8300A6E0 = MakeHash(\"Send01\"), "
         "0x826EEAD4..0x826EEAE0), not the stream's own send name",
         re.search(r"MakeHash\(\s*\"Send01\"\s*\)", none_arm) is not None and
         re.search(r"mfGainPreFade\s*=\s*mVoice\s*\.\s*GetGain\(", none_arm) is not None and
         "mCreateParams.mSendName" not in none_arm),
        ("case 1's gain is Curve::GetOutput(1 - f, E_ONE_MINUS_EQPWR) * mfGainPreFade (0x826EEB38..0x826EEBC8), "
         "not a linear (1 - f) * pre",
         re.search(r"Curve::GetOutput\(\s*1\.0f\s*-\s*lfFraction\s*,\s*CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR\s*\)"
                   r"\s*\*\s*mfGainPreFade", begin_arm) is not None and
         re.search(r"\(\s*1\.0f\s*-\s*lfFraction\s*\)\s*\*\s*mfGainPreFade", begin_arm) is None),
        ("case 1's position is the fsel clamp of t / GetFadeOut() with no guard on the length "
         "(0x826EEB18..0x826EEB34: fdivs, fneg/fsel, fsubs/fsel)",
         re.search(r"rw::math::fpu::Clamp\(\s*mfTimeThroughFade\s*/\s*lfFadeOut\s*,\s*0\.0f\s*,\s*1\.0f\s*\)",
                   begin_arm) is not None and re.search(r"lfFadeOut\s*>\s*0\.0f", begin_arm) is None),
    ]


def numeric(tree):
    try:
        detach = definition(tree.read(EFFECT_CPP), DETACH)
        utils = tree.read(UTILS_CPP)
        get_output = definition(utils, GET_OUTPUT)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    pieces = []
    constant = LAST_ELEMENT.search(code_only(utils))
    if constant:
        pieces.append(constant.group(0))
    table = optional(utils, TABLE)
    if table:
        pieces.append(table + ";")
    helper = optional(utils, READ_HELPER)
    if helper:
        pieces.append(helper)
    pieces.append(get_output)
    return compile_and_run(Path(__file__).with_name("FxTailsBStreamFade.cpp"), "fxtailsb_stream_detach.inc", detach,
                           "FxTailsBStreamFade", extra_files={"fxtailsb_curve_bodies.inc": "\n\n".join(pieces)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtailsb_stream_fade", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
