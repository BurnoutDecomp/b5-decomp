"""Inspect ARTIST mapped bytes for the traffic slot-7 codec and replay job."""
import bisect
from collections import defaultdict
import json
from pathlib import Path
import struct
import sys

sys.dont_write_bytecode = True
root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("D:/Reverse/BP-Decomp_Workflow")
sys.path.insert(0, str(root / "tools" / "re"))
import x360rd as image

EXECUTABLE_STARTS = {0x821F0000, 0x82CCB000}
LEAVES = {
    0x8264C280: "BaseSerialiser::Construct",
    0x8264C978: "TrafficEntitySerialiser::Construct",
    0x8265D288: "TrafficEntitySerialiser::WriteFrame",
    0x8265DD38: "TrafficEntitySerialiser::ReadFrame",
    0x8265F2D8: "TrafficEntitySerialiser::Read",
    0x8265F4E8: "TrafficEntitySerialiser::Write",
}
CODECS = {address for address in LEAVES if address >= 0x8265D288}
TRACKED_OFFSETS = {
    0x24C0: "traffic serialiser, module-relative low half",
    0x251B: "traffic skip byte, module-relative low half",
    0x5680: "ReplayModule serialise job",
    0x59D0: "ReplayModule serialise job active",
    0x5A80: "ReplayModule serialise stream",
}
NAMES = json.loads((root / "progress" / "identity.json").read_text(encoding="utf-8"))
names_by_address = {
    int(address, 16): name
    for name, data in NAMES.items()
    for address in data["x360_addrs"]
}


def mapped_bytes(start, end, cumulative):
    return image._m[image._base + 4 * cumulative:
                    image._base + 4 * (cumulative + end - start):4]


def direct_target(address, word, bits):
    displacement = word & ((1 << bits) - 4)
    if displacement & (1 << (bits - 1)):
        displacement -= 1 << bits
    return ((0 if word & 2 else address) + displacement) & 0xFFFFFFFF


image._init()
pdata = next(segment for segment in image._segs if segment[0] == 0x821BB000)
starts = sorted(
    address
    for address, _ in struct.iter_unpack(">II", mapped_bytes(*pdata))
    if 0x821F0000 <= address < 0x82CCFE7C
)
codec_ranges = {
    start: starts[bisect.bisect_right(starts, start)] for start in CODECS
}


def containing_symbol(address):
    start = starts[bisect.bisect_right(starts, address) - 1]
    return f"{start:08X}+{address-start:X} {names_by_address.get(start, '')}".rstrip()


hits = defaultdict(list)
for start, end, cumulative in image._segs:
    data = mapped_bytes(start, end, cumulative)
    for target, name in LEAVES.items():
        literal = target.to_bytes(4, "big")
        offset = data.find(literal)
        while offset >= 0:
            hits[f"loaded literal {name}"].append(f"{start+offset:08X}")
            offset = data.find(literal, offset + 1)

    if start not in EXECUTABLE_STARTS:
        continue
    print(f"Code scan {start:08X}..{end:08X} (exclusive)")
    recent_lis = {}
    for index, (word,) in enumerate(struct.iter_unpack(">I", data)):
        address = start + 4 * index
        opcode = word >> 26
        low = word & 0xFFFF
        reg_a = (word >> 16) & 31
        reg_d = (word >> 21) & 31
        if opcode == 15 and reg_a == 0:
            recent_lis[reg_d] = (index, low)
        if opcode in (14, 24):
            source = reg_a if opcode == 14 else reg_d
            if source in recent_lis and (opcode != 14 or source):
                lis_index, high = recent_lis[source]
                value = (high << 16) | low
                if opcode == 14 and low >= 0x8000:
                    value = (value - 0x10000) & 0xFFFFFFFF
                if index - lis_index <= 24 and value in CODECS:
                    hits["local lis plus addi/ori candidate"].append(
                        f"{address:08X} -> {value:08X} {containing_symbol(address)}"
                    )
        if opcode in (16, 18):
            target = direct_target(address, word, 16 if opcode == 16 else 26)
            if target in LEAVES:
                hits[f"direct branch {LEAVES[target]}"].append(
                    f"{address:08X} {containing_symbol(address)}"
                )
            for codec_start, codec_end in codec_ranges.items():
                if codec_start < target < codec_end and not codec_start <= address < codec_end:
                    hits["outside direct branch into codec body"].append(
                        f"{address:08X} -> {target:08X} {containing_symbol(address)}"
                    )
        if opcode in (38, 39) and low == 0x5B:
            hits["stb/stbu displacement +0x5B"].append(
                f"{address:08X} {word:08X} {containing_symbol(address)}"
            )
        if opcode in (14, 15, 24, 25, 32, 34, 36, 38, 40, 44) and low in TRACKED_OFFSETS:
            hits[f"immediate 0x{low:04X} ({TRACKED_OFFSETS[low]})"].append(
                f"{address:08X} {word:08X} {containing_symbol(address)}"
            )

for section, rows in sorted(hits.items()):
    print(f"\n{section} ({len(rows)})")
    print("\n".join(rows))
print(f"\nOutside direct branches into codec bodies: {len(hits['outside direct branch into codec body'])}")
print(f"Local lis plus addi/ori codec candidates: {len(hits['local lis plus addi/ori candidate'])}")

