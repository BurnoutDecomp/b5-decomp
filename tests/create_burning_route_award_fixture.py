"""Make an isolated save with the first Burning Route reward awaiting its reveal.

Seed: tools/tests/fixtures/DISCO_collect_base.sav. This prepares a saved state;
it does not simulate winning a race. No installed profile is overwritten.
Usage: python b5-decomp/tests/create_burning_route_award_fixture.py --output scratch/first_burning_route_pending.sav
"""
import argparse
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--seed", type=Path, default=ROOT / "tools/tests/fixtures/DISCO_collect_base.sav")
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()

def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value

def car_id(name):
    value = 0
    for char in name.upper().ljust(12):
        value = value * 40 + " -/0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_".index(char)
    return value

data = bytearray(args.seed.read_bytes())
magic, version, image_size, mugshot_size, saved_hash, reserved = struct.unpack_from("<6I", data)
if (magic, version, image_size, reserved) != (0x42355356, 1, 262144, 0):
    raise ValueError("Unsupported seed container")
if len(data) != 312 + image_size + mugshot_size or checksum(memoryview(data)[312:]) != saved_hash:
    raise ValueError("Seed size/checksum mismatch")
image = memoryview(data)[312:312 + image_size]
# BrnProfile_SaveImage.cpp: count604, CarData[512]624; count608, LiveryData[512]12912.
# XUSM1B1 is the pattern variant parented on PUSMC01 in VEHICLES/VEHICLELIST.BUNDLE.
award = car_id("XUSM1B1")
for count_offset, base, kind in ((604, 624, "car"), (608, 12912, "livery")):
    count = struct.unpack_from("<i", image, count_offset)[0]
    if not 0 < count <= 512:
        raise ValueError("Invalid seed garage count")
    index = next((i for i in range(count) if struct.unpack_from("<Q", image, base + i*24)[0] == award), count)
    if index == count:
        if count == 512:
            raise ValueError("Seed garage is full")
        struct.pack_into("<i", image, count_offset, count + 1)
    if kind == "car":
        struct.pack_into("<QBBBBfiI", image, base + index*24, award, 255, 255, 0, 0, 0.85, 1, 0)
    else:
        struct.pack_into("<QQfI", image, base + index*24, award, award, 0.0, 0)
struct.pack_into("<I", data, 16, checksum(memoryview(data)[312:]))
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open("xb") as output:
    output.write(data)
print("Prepared pending XUSM1B1 award:", args.output.resolve())
