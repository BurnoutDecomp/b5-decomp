"""Check the production RWAC clock against wall time and exact large-counter oracles."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev", help="read the production clock from a b5 revision")
parser.add_argument("--wall-only", action="store_true", help="run only the independent native rate check")
args = parser.parse_args()

source = Tree(args.rev).read("vendor/renderware/src/rw/audio/core/System.cpp")
body = definition(source, "u32 GetCpuCycle()")

# Python unbounded integers supply the direct unit-contract oracle, independent
# of the production quotient/remainder decomposition and MSVC intrinsics.
max_counter = (1 << 63) - 1
cases = [
    (0, 10_000_000, counter) for counter in (
        0, 1, 9_999_999, 10_000_000, 861_146_324,
        861_146_325, 10_000_000_000_000_001, max_counter)
] + [
    (1, 1, counter) for counter in (0, (1 << 32) - 1, 1 << 32, max_counter)
] + [
    (2, max_counter, counter) for counter in (0, 1, max_counter // 2, max_counter - 1, max_counter)
]
oracle = "const ClockCase KAC_CLOCK_CASES[] = {\n" + "".join(
    f"    {{{clock}, {counter}ull, {(counter * 49_875_000 // frequency) & 0xFFFFFFFF}u}},\n"
    for clock, frequency, counter in cases
) + "};\n"

numeric = compile_and_run(
    Path(__file__).with_name("RwAudioCoreClockRateRegression.cpp"),
    "rw_audio_core_clock_source.inc", body, "RwAudioCoreClockRateRegression",
    extra_flags="/DRWAC_CLOCK_RATE_ONLY=1" if args.wall_only else "",
    extra_files={"rw_audio_core_clock_cases.inc": oracle})
raise SystemExit(report("run_rw_audio_core_clock_rate_regression", [], numeric,
                       1 if args.wall_only else 1 + len(cases)))
