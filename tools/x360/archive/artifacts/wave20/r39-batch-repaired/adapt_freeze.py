import hashlib
import json
import pathlib

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
original = wave / "r39-batch"
old_review = original / "REVIEW.json"
assert json.loads(old_review.read_text())["verdict"] == "fail"
srp = wave.parent / "wave39/commsrp-ack-reload-terra"
def pin(path):
    return {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

row = {
    "parent": "45c56b5cb768c56deb458bc914afb7294297bbe2",
    "head": "a507b64667ee9a5a3f93e53090c90089c3b316c5",
    "patch": pin(srp / "ack-reload.patch"),
    "compile_report": pin(srp / "native1/compile-results.json"),
    "inputs": [pin(p) for p in sorted((srp / "host-check").glob("*")) if p.is_file()],
    "scope": "One CommSRP receive ACK post-callback reload and both bounds",
}
with (here / "SRP_REPAIR_BINDING.json").open("x", encoding="utf-8") as stream:
    json.dump(row, stream, indent=2)
    stream.write("\n")
text = (original / "freeze.py").read_text()
text = text.replace("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra",
                    "C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
text = text.replace('assert git("rev-parse", "HEAD").decode().strip() == read(audio)["head"]',
                    'assert git("rev-parse", "HEAD").decode().strip() == BASE')
text = text.replace('make_candidate("ai-debug-render-world", ai)',
                    'make_candidate("ai-debug-render-world", ai, ai.with_name("ROUTE_CORNER_FIX_RECEIPT.json"))')
text = text.replace('("dirtysdk-commsrp", srp_provenance["parent"], srp_provenance["candidate"], (srp,))',
                    '("dirtysdk-commsrp", srp_provenance["parent"], read(HERE / "SRP_REPAIR_BINDING.json")["head"], (srp, HERE / "SRP_REPAIR_BINDING.json"))')
text = text.replace('    "base": BASE, "base_tree": base_tree, "projected_tree": projected,',
                    '    "base": BASE, "base_tree": base_tree, "projected_tree": projected,\n    "original_review": pin(WAVE / "r39-batch/REVIEW.json"),')
with (here / "freeze.py").open("x", encoding="utf-8") as stream:
    stream.write(text)
