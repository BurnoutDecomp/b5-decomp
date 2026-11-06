from pathlib import Path
import hashlib
import json
import re
import struct


out = Path(__file__).resolve().parent
gate = json.loads((out / "NATIVE_GATE_RESULT.json").read_text())
needles = {
    "CgsAptAux": ["?ConstructApt@AptAux@CgsGui", "?LoadAnimation@AptCallbackFile@CgsGui"],
    "AptLoader": ["?AptCompleteAnimationAsyncLoad@@", "?CompleteLoad@AptLoader", "?Update@AptLoader",
                  "?notify@AptLoader", "?findFile@AptLoader", "?IsLoaded@AptLoader", "?Load@AptLoader",
                  "?AllImportsAvailable@AptLoader", "?CancelPreloadedAnimation@AptLoader"],
    "AptLinker": ["?Notify@AptLinker", "?GlobalNotificationFunction@@", "?isFileImported@AptLinker",
                  "?Load@AptLinker", "?PushBack@PendingFileVector@AptLinker",
                  "??1PendingFileVector@AptLinker", "?Swap@PendingFileVector@AptLinker"],
    "AptFile": ["?isFileImported@AptFile"],
    "AptCharacterAnimationInst": ["??0AptCharacterAnimationInst@@", "??1AptCharacterAnimationInst@@",
                                  "?MakeCharacterAnimationInst@@"],
    "AptCharacterAnimation": ["?IncCharacterList@AptCharacterAnimation"],
    "AptLinkerThingy": ["??0AptLinkerThingy@@", "?ScalarDeletingDestructor@AptLinkerThingy",
                        "?MakeLinkerThingy@@"],
    "AptSharedPtr": ["?Dispose@?$AptSharedPtr", "?VectorDeletingDestructor@?$AptSharedPtr"],
    "AptSavedInputCheckpoints": ["?updateState@AptSavedInputCheckpoints", "?Checkpoint@AptSavedInputCheckpoints"],
    "StringAsVectorPolicy": ["??$ReAlloc@U?$AptSharedPtr"],
}


def symbols(data):
    assert struct.unpack_from("<H", data)[0] == 0x01F2
    start, count = struct.unpack_from("<II", data, 8)
    strings = start + count * 18
    result = {}
    index = 0
    while index < count:
        entry = start + index * 18
        name = data[entry:entry + 8]
        if any(name[:4]):
            name = name.rstrip(b"\0")
        else:
            offset = strings + struct.unpack_from("<I", name, 4)[0]
            name = data[offset:data.index(0, offset)]
        section = struct.unpack_from("<h", data, entry + 12)[0]
        result[name.decode("ascii", errors="replace")] = section
        index += 1 + data[entry + 17]
    return result


def main():
    artifact = out / "NATIVE_ABI_EXCERPTS.txt"
    manifest = {}
    with artifact.open("x", encoding="utf-8", newline="\n") as output:
        for row in gate["rows"]:
            stem = Path(row["source"]).stem
            if stem not in needles:
                continue
            assert row["passed"], stem
            obj = Path(row["object"])
            asm = Path(row["assembly"])
            table = symbols(obj.read_bytes())
            lines = asm.read_text(encoding="utf-8", errors="replace").splitlines()
            starts = {i: line.split()[0] for i, line in enumerate(lines) if re.search(r"^\S+\s+PROC\s", line)}
            symbols_of_interest = ("AptSharedPtr", "GlobalNotificationFunction", "updateState@", "PendingFileVector")
            record = {"assembly": str(asm), "object": str(obj), "proof": {}, "coff": {}}
            for name, section in table.items():
                if any(needle in name for needle in symbols_of_interest + tuple(needles[stem])):
                    record["coff"][name] = section
            for index, name in starts.items():
                if not any(name.startswith(prefix) for prefix in needles[stem]):
                    continue
                end = next(i for i in range(index + 1, len(lines)) if lines[i].split()[:2] == [name, "ENDP"])
                record["proof"][name] = [index + 1, end + 1]
                assert table[name] > 0, name
                output.write(f"\n{asm.name} {index + 1}-{end + 1} {name}\n")
                for i in range(index, end + 1):
                    if re.match(r"^\s+[0-9a-f]{5}\s+[0-9a-f]{8}\s", lines[i]):
                        output.write(f"{i + 1:6}  {lines[i].strip()}\n")
            manifest[stem] = record
    assert len(manifest) == len(needles)
    report = {"format": "PowerPC COFF 0x01F2, symbol value here is section number",
              "excerpt_sha256": hashlib.sha256(artifact.read_bytes()).hexdigest(), "translation_units": manifest}
    with (out / "NATIVE_ABI_INDEX.json").open("x", encoding="utf-8", newline="\n") as output:
        json.dump(report, output, indent=2)
        output.write("\n")


if __name__ == "__main__":
    main()
