from pathlib import Path
import collections
import json
import re

import strip_host as s

identity = json.loads((s.ROOT.parent / "progress/identity.json").read_text(encoding="utf-8"))
identities = set(identity)
if "functions" in identity:
    identities = set(identity["functions"])
host_api = set()
for name in s.FILES:
    if not name.startswith("src/pc/") or Path(name).suffix not in s.SOURCE_SUFFIXES:
        continue
    data = (s.ROOT / name).read_bytes()
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type != "function_definition": continue
        scopes = []
        parent = node.parent
        while parent:
            if parent.type in {"namespace_definition", "class_specifier", "struct_specifier"}:
                n = parent.child_by_field_name("name")
                if n: scopes.append(s.text_of(n, data))
            parent = parent.parent
        for declared in s.declarator_names(node, data):
            qualified = "::".join(list(reversed(scopes)) + [declared])
            if qualified.startswith("renderengine::renderengine::"):
                qualified = qualified[len("renderengine::"):]
            if "::" in qualified and qualified not in identities:
                host_api.add(qualified)

hits = []
for name in s.source_files():
    data = (s.ROOT / name).read_bytes()
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type not in {"call_expression", "qualified_identifier"}: continue
        function = node.child_by_field_name("function") if node.type == "call_expression" else node
        if not function: continue
        value = re.sub(r"\s+", "", s.text_of(function, data))
        if value in host_api:
            hits.append({"file": name, "line": node.start_point.row + 1, "api": value})
(s.OUT / "host_api_inventory.json").write_text(json.dumps({"apis": sorted(host_api), "hits": hits}, indent=2), encoding="utf-8")
counts = collections.Counter(h["api"] for h in hits)
print("Host-only qualified API definitions:", len(host_api), "shared references:", len(hits))
for n, count in counts.most_common(70): print(count, n)
