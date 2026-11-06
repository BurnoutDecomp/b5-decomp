import json
import pathlib
import struct
import sys

evidence = pathlib.Path(__file__).with_name("tables-native.json")
target = pathlib.Path(sys.argv[1])
data = json.loads(evidence.read_text())
codebooks = {1: 0, 2: 1, 3: 2, 5: 3, 6: 4, 7: 5, 8: 6, 9: 7, 10: 8, 11: 9, 12: 10, 13: 11, 15: 12, 16: 13, 24: 14}
with target.open("x", encoding="utf-8") as stream:
    stream.write("// ARTIST MPEG Layer III gain and Huffman tables.\n")
    stream.write("// Global gain has 256 floats at 0x82159300.\n")
    gains = struct.unpack(">256f", bytes.fromhex(data["gain"]["bytes"])[:1024])
    stream.write("static const f32 kGlobalGain[] =\n{\n")
    for start in range(0, len(gains), 4):
        stream.write("    " + ", ".join(f"{value:.9e}f" for value in gains[start:start+4]) + ",\n")
    stream.write("};\n")
    for index, source_index in codebooks.items():
        table = data["huffman"][source_index]
        words = struct.unpack(f">{table['size']//2}h", bytes.fromhex(table["bytes"]))
        stream.write(f"\n// ARTIST 0x{table['address']:08X}\n")
        stream.write(f"static s16 kHuffman{index}[] =\n{{\n")
        for start in range(0, len(words), 12):
            stream.write("    " + ", ".join(map(str, words[start:start+12])) + ",\n")
        stream.write("};\n")
