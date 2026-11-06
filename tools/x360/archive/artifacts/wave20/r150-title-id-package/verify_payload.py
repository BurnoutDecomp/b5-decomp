"""Offline XEX payload comparison; keys are supplied only through process env."""
from __future__ import annotations

import hashlib
import json
import os
import re
import struct
from pathlib import Path

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

ROOT = Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
SEALED = ROOT / "r150-link/native-game.exe"
SCRATCH = ROOT / "r150-title-id-package/native-game-title-id.xex"
PE_PATH = ROOT / "r150-link/native-game.pe"
PINS = ROOT / "r150-title-id-package/TITLE_ID_PACKAGE.json"
SEALED_DUMP = ROOT / "r150-link/xex-dump.log"
SCRATCH_DUMP = ROOT / "r150-title-id-package/dump.log"
OUT = ROOT / "r150-title-id-package/payload-equivalence.json"


def be16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sdk_dump_metadata(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="replace")
    metadata = {}
    for key, pattern in {
        "image_size": r"^\s*([0-9A-F]+) image size\s*$",
        "image_base": r"^\s*([0-9A-F]+) image base address\s*$",
        "entry_point": r"^\s*([0-9A-F]+) entry point\s*$",
        "stack_size": r"^\s*([0-9A-F]+) default stack size\s*$",
    }.items():
        match = re.search(pattern, text, re.MULTILINE)
        if match:
            metadata[key] = int(match.group(1), 16)
    match = re.search(r"^\s*(NOT-COMPRESSED[^\r\n]*)$", text, re.MULTILINE)
    if match:
        metadata["format_label"] = match.group(1).strip()
    imports = []
    in_imports = False
    for line in text.splitlines():
        if line.strip() == "SYSTEM IMPORT LIBRARIES":
            in_imports = True
            continue
        if in_imports and (not line.strip() or line.strip() == "LIBRARY VERSIONS"):
            break
        if in_imports:
            imports.append(line.strip().split()[0])
    if imports:
        metadata["system_import_libraries"] = imports
    return metadata


def aes_cbc_decrypt(key: bytes, data: bytes) -> bytes:
    if len(data) % 16:
        raise ValueError("AES-CBC input is not block-aligned")
    decryptor = Cipher(algorithms.AES(key), modes.CBC(bytes(16))).decryptor()
    return decryptor.update(data) + decryptor.finalize()


def pe_info(data: bytes) -> dict | None:
    if len(data) < 64 or data[:2] != b"MZ":
        return None
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 24 > len(data) or data[pe_offset : pe_offset + 4] != b"PE\0\0":
        return None
    coff = pe_offset + 4
    section_count = struct.unpack_from("<H", data, coff + 2)[0]
    optional_size = struct.unpack_from("<H", data, coff + 16)[0]
    optional = pe_offset + 24
    if optional + optional_size > len(data) or optional_size < 64:
        return None
    magic = struct.unpack_from("<H", data, optional)[0]
    if magic not in (0x10B, 0x20B):
        return None
    image_base_offset = 28 if magic == 0x10B else 24
    image_base = struct.unpack_from("<I" if magic == 0x10B else "<Q", data, optional + image_base_offset)[0]
    size_of_image, size_of_headers = struct.unpack_from("<II", data, optional + 56)
    section_table = optional + optional_size
    if section_table + section_count * 40 > len(data):
        return None
    sections = []
    for index in range(section_count):
        offset = section_table + index * 40
        name = data[offset : offset + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, offset + 8)
        if raw_pointer + raw_size > len(data):
            return None
        sections.append(
            {
                "name": name,
                "virtual_size": virtual_size,
                "rva": virtual_address,
                "raw_size": raw_size,
                "raw_pointer": raw_pointer,
            }
        )
    return {
        "pe_header_offset": pe_offset,
        "optional_magic": magic,
        "image_base": image_base,
        "size_of_image": size_of_image,
        "size_of_headers": size_of_headers,
        "sections": sections,
    }


def decode_xex(path: Path, master_keys: dict[str, bytes]) -> dict:
    data = path.read_bytes()
    if data[:4] != b"XEX2":
        raise ValueError(f"{path.name}: bad XEX magic")
    header_size = be32(data, 8)
    security_offset = be32(data, 0x10)
    optional_count = be32(data, 0x14)
    format_offset = None
    optional_keys = []
    execution_info = None
    for index in range(optional_count):
        key, value = struct.unpack_from(">II", data, 0x18 + index * 8)
        optional_keys.append(f"{key:08x}")
        if key == 0x000003FF:
            format_offset = value
        elif key == 0x00040006:
            if value + 0x18 > len(data):
                raise ValueError(f"{path.name}: invalid execution-info offset")
            execution_info = {
                "media_id": be32(data, value),
                "version": be32(data, value + 4),
                "base_version": be32(data, value + 8),
                "title_id": be32(data, value + 12),
                "platform": data[value + 16],
                "executable_type": data[value + 17],
                "disc_number": data[value + 18],
                "disc_count": data[value + 19],
                "savegame_id": be32(data, value + 20),
            }
    if format_offset is None:
        raise ValueError(f"{path.name}: missing file-format-info header")
    info_size = be32(data, format_offset)
    encryption_type = be16(data, format_offset + 4)
    compression_type = be16(data, format_offset + 6)
    block_count = (info_size - 8) // 8
    blocks = [
        (be32(data, format_offset + 8 + index * 8), be32(data, format_offset + 12 + index * 8))
        for index in range(block_count)
    ]
    if encryption_type != 1 or compression_type != 1:
        raise ValueError(f"{path.name}: unexpected format enums {encryption_type}/{compression_type}")
    payload = data[header_size:]
    encrypted_session_key = data[security_offset + 0x150 : security_offset + 0x160]
    if len(encrypted_session_key) != 16:
        raise ValueError(f"{path.name}: invalid encrypted session-key field")
    image_flags = be32(data, security_offset + 0x10C)
    page_size = 4096 if image_flags & 0x10000000 else 65536
    page_descriptor_count = be32(data, security_offset + 0x180)
    mapped_size = 0
    for index in range(page_descriptor_count):
        descriptor = be32(data, security_offset + 0x184 + index * 0x18)
        mapped_size += (descriptor >> 4) * page_size
    selected = None
    for key_class, master_key in master_keys.items():
        session_key = aes_cbc_decrypt(master_key, encrypted_session_key)
        decryptor = Cipher(algorithms.AES(session_key), modes.CBC(bytes(16))).decryptor()
        source_offset = 0
        image = bytearray()
        for data_size, zero_size in blocks:
            if data_size % 16 or source_offset + data_size > len(payload):
                raise ValueError(f"{path.name}: invalid basic-compression block")
            image.extend(decryptor.update(payload[source_offset : source_offset + data_size]))
            source_offset += data_size
            image.extend(bytes(zero_size))
        tail = decryptor.finalize()
        if tail:
            image.extend(tail)
        if source_offset != len(payload):
            raise ValueError(f"{path.name}: basic blocks do not cover the XEX payload")
        if len(image) > mapped_size:
            raise ValueError(f"{path.name}: basic output exceeds XEX page-descriptor image")
        image.extend(bytes(mapped_size - len(image)))
        parsed_pe = pe_info(image)
        if parsed_pe:
            selected = (key_class, image, parsed_pe)
            break
    if selected is None:
        raise ValueError(f"{path.name}: decrypted image is not a valid PE")
    key_class, image, parsed_pe = selected
    return {
        "sha256": sha(data),
        "file_size": len(data),
        "header_size": header_size,
        "security_offset": security_offset,
        "image_size": be32(data, security_offset + 4),
        "load_address": be32(data, security_offset + 0x110),
        "image_flags": image_flags,
        "mapped_size_from_page_descriptors": mapped_size,
        "page_size": page_size,
        "encryption_type": encryption_type,
        "compression_type": compression_type,
        "file_format_info_size": info_size,
        "basic_blocks": [{"data_size": size, "zero_size": zero} for size, zero in blocks],
        "payload_length": len(payload),
        "plaintext_sha256": sha(image),
        "plaintext_length": len(image),
        "plaintext_pe_valid": True,
        "master_key_class": key_class,
        "optional_header_keys": optional_keys,
        "execution_info": execution_info,
        "plaintext_pe": parsed_pe,
        "plaintext": image,
    }


def main() -> None:
    raw_keys = os.environ["XEX_MASTER_KEYS"]
    master_keys = {name: bytes.fromhex(value) for name, value in (part.split("=", 1) for part in raw_keys.split(";"))}
    pins = json.loads(PINS.read_text(encoding="utf-8"))
    pe_bytes = PE_PATH.read_bytes()
    if sha(pe_bytes) != pins["input_pe_sha256"]:
        raise ValueError("linked input PE does not match TITLE_ID_PROBE pin")
    xex = {name: decode_xex(path, master_keys) for name, path in (("sealed", SEALED), ("scratch", SCRATCH))}
    if xex["sealed"]["sha256"] != pins["sealed_xex_sha256"] or xex["scratch"]["sha256"] != pins["scratch_xex_sha256"]:
        raise ValueError("XEX bytes do not match TITLE_ID_PROBE pins")
    if (xex["sealed"]["plaintext_length"], xex["sealed"]["plaintext_sha256"]) != (xex["scratch"]["plaintext_length"], xex["scratch"]["plaintext_sha256"]):
        left, right = xex["sealed"]["plaintext"], xex["scratch"]["plaintext"]
        if len(left) != len(right):
            diff_runs = None
            diff_bytes = None
        else:
            changed = []
            start = None
            for index, (a, b) in enumerate(zip(left, right)):
                if a != b and start is None:
                    start = index
                elif a == b and start is not None:
                    changed.append([start, index])
                    start = None
            if start is not None:
                changed.append([start, len(left)])
            diff_runs = changed
            diff_bytes = sum(end - start for start, end in changed)
    else:
        diff_runs, diff_bytes = [], 0
    src_pe = pe_info(pe_bytes)
    if src_pe is None:
        raise ValueError("linked source PE is invalid")
    decoded_sections = []
    for section in src_pe["sections"]:
        virtual_length = section["virtual_size"] or section["raw_size"]
        row = {
            "name": section["name"],
            "rva": section["rva"],
            "virtual_size": section["virtual_size"],
            "raw_size": section["raw_size"],
            "raw_pointer": section["raw_pointer"],
        }
        for name, record in xex.items():
            image = record["plaintext"]
            rva = section["rva"]
            length = min(virtual_length, max(0, len(image) - rva))
            row[name] = {
                "present_in_xex_mapped_image": length == virtual_length,
                "bytes_hashed": length,
                "sha256": sha(image[rva : rva + length]) if length else None,
            }
        decoded_sections.append(row)
    result = {
        "schema": "r146-xex-payload-equivalence-v1",
        "method": "In-memory XEX2 key unwrap + Basic block AES-CBC decode, following xenia-project/xenia xex_module.cc and xex2_info.h; no decoded bytes or keys persisted.",
        "pinned_input_pe": {"path": str(PE_PATH), "sha256": sha(pe_bytes), "bytes": len(pe_bytes), "pe_header": {k: v for k, v in src_pe.items() if k != "sections"}, "sections": src_pe["sections"]},
        "xex": {},
        "plaintext_payloads_equal": xex["sealed"]["plaintext_sha256"] == xex["scratch"]["plaintext_sha256"] and xex["sealed"]["plaintext_length"] == xex["scratch"]["plaintext_length"],
        "plaintext_diff_bytes": diff_bytes,
        "plaintext_diff_runs": diff_runs,
        "source_pe_headers_match_plaintext": {name: record["plaintext"][:src_pe["size_of_headers"]] == pe_bytes[:src_pe["size_of_headers"]] for name, record in xex.items()},
        "source_pe_size_of_image": src_pe["size_of_image"],
        "sdk_loader_metadata": {"sealed": sdk_dump_metadata(SEALED_DUMP), "scratch": sdk_dump_metadata(SCRATCH_DUMP)},
        "decoded_pe_sections": decoded_sections,
    }
    for name, record in xex.items():
        result["xex"][name] = {key: value for key, value in record.items() if key not in ("plaintext", "plaintext_pe")}
        result["xex"][name]["plaintext_pe"] = {key: value for key, value in record["plaintext_pe"].items() if key != "sections"}
    OUT.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    for name, record in xex.items():
        print(name, json.dumps({"plaintext_sha256": record["plaintext_sha256"], "plaintext_length": record["plaintext_length"], "pe_valid": record["plaintext_pe_valid"], "master_key_class": record["master_key_class"], "blocks": record["basic_blocks"], "image_size": record["image_size"], "mapped_size": record["mapped_size_from_page_descriptors"]}, sort_keys=True))
    print("comparison", json.dumps({"plaintext_equal": result["plaintext_payloads_equal"], "diff_bytes": diff_bytes, "diff_runs": diff_runs, "source_pe_headers_match_plaintext": result["source_pe_headers_match_plaintext"], "receipt": str(OUT)}, sort_keys=True))


if __name__ == "__main__":
    main()
