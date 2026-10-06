"""Compare real road-rule records in two checksum-validated B5SV save images."""
from pathlib import Path
import argparse
import json
import struct
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "saves"))
from convert_xenia_save import read_container

# BrnProfile_SaveImage.cpp KU_CHALLENGE_DATA; the original fixed save-image
# block contains 64 ChallengePlayerScoreEntry records, each 40 bytes.
def records(path):
    image, _ = read_container(Path(path).read_bytes())
    result = []
    for slot in range(64):
        dirty, valid, time, crash, time_car, crash_car = struct.unpack_from(
            "<QQiiQQ", image, 100040 + 40 * slot)
        result.append({"slot": slot, "valid": valid, "time": time,
                       "crash": crash, "time_car": time_car,
                       "crash_car": crash_car})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before")
    parser.add_argument("after")
    parser.add_argument("--retained", action="store_true")
    parser.add_argument("--slot", type=int, default=37)
    parser.add_argument("--par-ms", type=int, default=4699)
    args = parser.parse_args()
    before, after = records(args.before), records(args.after)
    changed, lost, existing = [], [], 0
    for old, new in zip(before, after):
        for bit, field in ((1, "time"), (2, "crash")):
            old_valid, new_valid = bool(old["valid"] & bit), bool(new["valid"] & bit)
            if old_valid:
                existing += 1
                worse = new[field] > old[field] if bit == 1 else new[field] < old[field]
                if not new_valid or worse:
                    lost.append({"slot": old["slot"], "kind": field,
                                 "before": old[field], "after": new[field] if new_valid else None})
            if new_valid and (not old_valid or old[field] != new[field]):
                changed.append({"slot": new["slot"], "kind": field,
                                "before": old[field] if old_valid else None,
                                "after": new[field], "car": f'{new[field + "_car"]:016X}'})
    new_times = [row for row in changed if row["kind"] == "time"
                 and row["slot"] == args.slot and 0 < row["after"] < args.par_ms]
    passed = (existing > 0 and not lost) if args.retained else bool(new_times)
    print(json.dumps({"pass": passed, "existing_scores": existing,
                      "target_slot": args.slot, "target_par_ms": args.par_ms,
                      "new_times_below_authored_par": len(new_times),
                      "changed": changed, "lost": lost}))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
