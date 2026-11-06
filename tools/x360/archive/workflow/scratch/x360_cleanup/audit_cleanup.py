from pathlib import Path
import collections
import json
import subprocess

import strip_host as s

changed = subprocess.check_output(["git", "diff", "--name-only", "--diff-filter=M"], cwd=s.ROOT, text=True).splitlines()
proc = subprocess.Popen(["git", "cat-file", "--batch"], cwd=s.ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
issues = []
counts = []
for name in changed:
    if Path(name).suffix not in s.SOURCE_SUFFIXES:
        continue
    now = (s.ROOT / name).read_bytes()
    proc.stdin.write(("HEAD:" + name + "\n").encode())
    proc.stdin.flush()
    header = proc.stdout.readline().decode().split()
    old = proc.stdout.read(int(header[-1]))
    proc.stdout.read(1)
    def errors(data):
        return [(n.start_point.row + 1, n.type, (s.text_of(n, data).splitlines() or [""])[0][:120])
                for n in s.walk(s.PARSER.parse(data).root_node) if n.type == "ERROR" or n.is_missing]
    old_errors, now_errors = errors(old), errors(now)
    if len(now_errors) > len(old_errors):
        issues.append({"file": name, "old": len(old_errors), "new": len(now_errors), "errors": now_errors})
    old_fns = sum(n.type == "function_definition" for n in s.walk(s.PARSER.parse(old).root_node))
    now_fns = sum(n.type == "function_definition" for n in s.walk(s.PARSER.parse(now).root_node))
    if old_fns - now_fns >= 8:
        counts.append((old_fns - now_fns, old_fns, now_fns, name))
proc.stdin.close()
proc.wait()
(s.OUT / "parse_issues.json").write_text(json.dumps(issues, indent=2), encoding="utf-8")
print("Files with more parse errors:", len(issues))
for i in issues:
    print(i["file"], i["old"], "->", i["new"], i["errors"][:5])
print("Function removal counts:")
for i in sorted(counts, reverse=True)[:30]:
    print(i)
