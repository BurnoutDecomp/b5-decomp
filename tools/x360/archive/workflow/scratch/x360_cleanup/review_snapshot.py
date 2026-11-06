from pathlib import Path
import json
import re
import subprocess
import strip_host as s

for name in s.source_files():
    path = s.ROOT / name
    data = path.read_bytes()
    spans = []
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type in {"function_definition", "declaration", "field_declaration"}:
            if "LogParkedPlatformLeaf" in s.declarator_names(node, data):
                spans.append(s.erase_span(node, data, True))
        if node.type == "identifier" and s.text_of(node, data) == "LogParkedPlatformLeaf":
            statement = s.containing(node, "expression_statement")
            if statement: spans.append(s.erase_span(statement, data))
    if spans: data = s.apply_spans(data, spans)
    text = data.decode("utf-8-sig").replace("\r\n", "\n")
    text = re.sub(r'(?m)(#\s*include\s*[^\n]*?[>\"])[ \t]+(?=#\s*include)', r'\1\n', text)
    if spans or text.replace("\n", "\r\n").encode("utf-8") != data:
        path.write_bytes((text.rstrip() + "\n").replace("\n", "\r\n").encode("utf-8"))

diff = subprocess.check_output(["git", "diff", "--no-ext-diff", "--no-color"], cwd=s.ROOT)
(s.OUT / "experiment.diff").write_bytes(diff)
packet = f"""# Review packet: experimental Xbox 360 branch

Repository: {s.ROOT}
Branch: codex/experimental-x360-byte-match
Base: 209fb3b94f7266506dff4ffd569e8553f1b3af93
Complete current diff: {s.OUT / 'experiment.diff'}

User request: create an experimental byte-matching branch, remove PC-specific
implementations and hooks from shared code, retain src/pc as an unused reference,
and leave main/dev untouched. This is source cleanup, with missing console code
and a non-building/non-booting state accepted. No new PC fallbacks or toolchain
framework are requested. ARTIST is the authority for the 32-bit big-endian target.

Review the current source and diff read-only. Look for remaining active desktop
dependencies, erroneous removal of native code/branches, accidental syntax or
preprocessor damage, new fabricated implementations, and PC build membership.
Check the native camera/timing/endian/Apt restorations against their existing
source evidence. Missing Xbox implementations alone are expected in this branch;
do not require a runnable Xbox build. The XDK is not installed in this checkout.

Use one review session; do not spawn more agents. Make no edits, builds, status
changes, Git ref changes, or external posts. Report concrete material findings
with paths and lines, and finish with exactly VERDICT: pass or VERDICT: fail.
"""
(s.OUT / "REVIEW_PACKET.md").write_text(packet, encoding="utf-8")
print("Review snapshot:", s.OUT / "REVIEW_PACKET.md")
