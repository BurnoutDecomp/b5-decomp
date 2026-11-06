"""Compare the byte source, destination and order of the ARTIST and native assignment."""
import hashlib
import json
from pathlib import Path
import runpy
import struct
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
WORKFLOW = Path("D:/Reverse/BP-Decomp_Workflow")
HELPER = HERE.parent / "r148-boot-presentation-linked/verify_linked.py"
assert hashlib.sha256(HELPER.read_bytes()).hexdigest() == "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba"
Coff = runpy.run_path(str(HELPER))["Coff"]
sys.path.insert(0, str(WORKFLOW / "tools/re"))
import x360rd

ARTIST_ENTRY = 0x821F4740
ARTIST_SIZE = 0x284
SIZE = 0x128
HOLES = {0x46, 0x47, 0xBD, 0xBE, 0xBF, 0xDA, 0xDB}
EXPECTED = [offset for offset in range(SIZE) if offset not in HOLES]


def signed(value, bits):
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def trace(body, self_assignment=False):
    """Track byte provenance and access order for the instructions emitted here."""
    destination = 0x1000
    source = destination if self_assignment else 0x3000
    gpr = {3: destination, 4: source}
    fpr = {}
    memory = {destination + offset: ("dst", offset) for offset in range(SIZE)}
    memory.update({source + offset: ("src", offset) for offset in range(SIZE)})
    loads = []
    stores = []
    pc = 0
    equal = None
    for _ in range(1000):
        word, = struct.unpack_from(">I", body, pc)
        opcode = word >> 26
        reg_d = word >> 21 & 31
        reg_a = word >> 16 & 31
        reg_b = word >> 11 & 31
        imm = word & 0xFFFF
        xo = word >> 1 & 0x3FF
        if word == 0x4E800020:
            assert pc + 4 == len(body) and gpr[3] == destination
            break
        if opcode == 14:
            gpr[reg_d] = (gpr[reg_a] if reg_a else 0) + signed(imm, 16)
        elif opcode == 31 and xo == 40:
            gpr[reg_d] = gpr[reg_b] - gpr[reg_a]
        elif opcode in (32, 34, 48) or (opcode == 31 and xo == 87):
            if opcode == 31:
                address = (gpr[reg_a] if reg_a else 0) + gpr[reg_b]
                width = 1
            else:
                address = gpr[reg_a] + signed(imm, 16)
                width = 1 if opcode == 34 else 4
            loads.extend(range(address - source, address - source + width))
            (fpr if opcode == 48 else gpr)[reg_d] = tuple(memory[address + index] for index in range(width))
        elif opcode in (36, 38, 52):
            address = gpr[reg_a] + signed(imm, 16)
            width = 1 if opcode == 38 else 4
            payload = (fpr if opcode == 52 else gpr)[reg_d]
            assert len(payload) == width
            for index, value in enumerate(payload):
                offset = address - destination + index
                assert value == ("src", offset)
                stores.append(offset)
                memory[address + index] = value
        elif opcode == 10:
            assert word >> 23 & 7 == 6 and not (word & 0x00200000)
            equal = gpr[reg_a] == imm
        elif opcode == 16:
            assert word & 0xFFFF0003 == 0x409A0000 and equal is not None
            if not equal:
                pc += signed(word & 0xFFFC, 16)
                continue
        else:
            raise ValueError(f"Unexpected PPC {word:08x} at +{pc:x}")
        pc += 4
    else:
        raise ValueError("Assignment did not return")
    assert loads == stores == EXPECTED
    for offset in HOLES:
        assert memory[destination + offset] == ("src" if self_assignment else "dst", offset)
    return {"ordered_load_and_store_bytes": len(stores), "untouched": [hex(offset) for offset in sorted(HOLES)]}


def main():
    build = json.loads((HERE / "compile-results.json").read_text())
    candidate_path = Path(build["object"])
    candidate_data = candidate_path.read_bytes()
    assert build["returncode"] == 0 and build["machine"] == 0x1F2
    assert hashlib.sha256(candidate_data).hexdigest() == build["object_sha256"]
    symbol, candidate, refs = Coff(candidate_data).function("??4CarScoreData@GameStateModuleIO@BrnGameState@@")
    assert not refs
    artist = x360rd.rd(ARTIST_ENTRY, ARTIST_SIZE)
    evidence = {
        "symbol": symbol.name, "artist_va": hex(ARTIST_ENTRY),
        "artifact_sha256": build["object_sha256"],
        "artist_instruction_bytes": len(artist), "candidate_instruction_bytes": len(candidate),
        "artist_body_sha256": hashlib.sha256(artist).hexdigest(),
        "candidate_body_sha256": hashlib.sha256(candidate).hexdigest(),
        "candidate_relocations": len(refs),
        "distinct": {"artist": trace(artist), "candidate": trace(candidate)},
        "self_assignment": {"artist": trace(artist, True), "candidate": trace(candidate, True)},
    }
    (HERE / "PPC_COPY_PROOF.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print("PASS 289 bytes from matching source offsets, in order, seven untouched bytes and r3 return")


if __name__ == "__main__":
    main()
