"""Compile and exercise the production Stack header's GetLength assertion contract."""
from pathlib import Path
import hashlib
import os

import fxgs_common


os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
REPO = fxgs_common.REPO
WORKFLOW = next(
    (candidate for candidate in (REPO, *REPO.parents)
     if (candidate / "tools/build/msvc_flags.txt").is_file()
     and (candidate / "b5-decomp").is_dir()),
    None,
)
if WORKFLOW is None:
    print("SETUP: could not find workflow root with canonical MSVC flags/providers")
    raise SystemExit(2)
fxgs_common.WORKFLOW = WORKFLOW

relative_headers = (
    "src/GameShared/GameClasses/Containers/CgsStack.h",
    "src/GameShared/GameClasses/Core/CgsAssert.h",
    "src/types.hpp",
)
shadow = {}
header_hash = None
for relative in relative_headers:
    header_path = REPO / relative
    if not header_path.is_file():
        print(f"SETUP: required production header is missing: {header_path}")
        raise SystemExit(2)
    contents = header_path.read_text(encoding="utf-8-sig")
    shadow[relative] = contents
    if relative.endswith("/CgsStack.h"):
        header_hash = hashlib.sha256(header_path.read_bytes()).hexdigest()

if "s32 GetLength() const" not in shadow[relative_headers[0]]:
    print("SETUP: production CgsStack.h lacks the expected GetLength definition")
    raise SystemExit(2)

print(f"PRODUCTION_HEADER: {REPO / relative_headers[0]}")
print(f"PRODUCTION_HEADER_SHA256: {header_hash}")
result = fxgs_common.compile_and_run(
    Path(__file__).with_name("CgsStackGetLength.cpp"),
    "CgsStackGetLengthUnused.inc",
    "",
    "CgsStackGetLength",
    shadow=shadow,
)
raise SystemExit(
    fxgs_common.report(
        "run_cgs_stack_getlength",
        [("canonical MSVC flags/providers and current-checkout headers supplied", True)],
        result,
        12,
    )
)
