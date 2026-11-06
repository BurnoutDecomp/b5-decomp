import hashlib
import json
import re
import subprocess
from pathlib import Path


root = Path(__file__).resolve().parent
base = "ef3f9b543b9b2482e11d5c77914da04595d2b884"
source = Path("src/GameSource/Gui/BrnGuiProfileHost.h")
fields = {
    "alignof(ProfileHost)": (0x8, 0x10),
    "sizeof(ProfileHost)": (0x414C0, 0x414D0),
    "ProfileHost.mProfileManager": (0x8, 0x10),
    "sizeof(ProfileManager)": (0x414B8, 0x414B8),
    "ProfileManager.mSaveLoadSystem": (0x1068, 0x1068),
    "ProfileManager.mStoredData": (0x12C0, 0x12C0),
    "sizeof(GuiModule)": (0x18DCC0, 0x18DCD0),
    "GuiModule.mProfileHost": (0xA66D0, 0xA66D0),
    "GuiModule.mSystemUserProfile": (0xE7B90, 0xE7BA0),
    "GuiModule.mbPerformingInvite": (0x18D828, 0x18D838),
    "GuiModule.mbInviteRequiresLogin": (0x18D829, 0x18D839),
    "BrnGameModule.mGuiModule": (0x6EAA20, 0x6EAA20),
    "BrnGameModule.mEffectsModule": (0x878700, 0x878700),
    "alignof(EffectsModule)": (0x80, 0x80),
    "BrnGameModule.mMainFlowStateMachine": (0x9A0664, 0x9A0664),
    "BrnGameModule.mbRequestStreamingWait": (0x9A0625, 0x9A0625),
    "BrnGameModule.mbWorldStreamingRequestedStall": (0x9A062B, 0x9A062B),
    "BrnGameModule.miGuiFsmStage": (0x9A0644, 0x9A0644),
    "BrnGameModule.mbGuiPhaseComplete": (0x9A0648, 0x9A0648),
    "BrnGameModule.mbGuiPreAccept": (0x9A0649, 0x9A0649),
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def procedures(path):
    text = path.read_text(errors="replace")
    return dict(re.findall(r"^(\S+) PROC NEAR[^\n]*\n(.*?)^\1 ENDP", text, re.M | re.S))


receipt = {"base": base, "header_sha256": sha256(source), "runtime": "not run", "native": {}}
values = {}
for index, variant in enumerate(("baseline", "candidate")):
    directory = root / variant
    report_path = directory / "compile-results.json"
    report = json.loads(report_path.read_text())
    assert report["passed"] == 3 and report["failed"] == 0
    assert all(item["machine"] == 0x1F2 for item in report["results"])
    probe_asm = directory / "layout_probe.asm"
    probe_obj = directory / "layout_probe.obj"
    assert probe_obj.read_bytes()[:2] == b"\xf2\x01"
    layout = re.search(r"^gR140Layout (DD\s+[0-9A-F]+H\n(?:\s+DD\s+[0-9A-F]+H\n)+)",
                       probe_asm.read_text(), re.M | re.I)
    emitted = [int(value, 16) for value in re.findall(r"DD\s+([0-9A-F]+)H", layout[1], re.I)]
    assert emitted == [pair[index] for pair in fields.values()], (variant, emitted)
    values[variant] = dict(zip(fields, emitted))
    receipt["native"][variant] = {
        "compile_report_sha256": sha256(report_path),
        "layout_probe_coff_sha256": sha256(probe_obj),
        "object_sha256": {item["source"]: sha256(Path(item["object"])) for item in report["results"]},
    }

old = procedures(root / "baseline/BrnGameModule-f766a093.asm")
new = procedures(root / "candidate/BrnGameModule-f766a093.asm")
assert old.keys() == new.keys()
changed = [name for name in old if old[name] != new[name]]
assert set(changed) == {f"??{op}{name}@BrnGui@@QAA@XZ" for op in (0, 1)
                        for name in ("ProfileHost", "GuiModule")}, changed
receipt["changed_BrnGameModule_assembly_procedures"] = changed
receipt["unchanged_BrnGameModule_assembly_procedures"] = len(old) - len(changed)
receipt["layout_offsets"] = {field: {variant: hex(v[field]) for variant, v in values.items()} for field in fields}
for variant, layout in values.items():
    gui_end = layout["BrnGameModule.mGuiModule"] + layout["sizeof(GuiModule)"]
    effects_alignment = layout["alignof(EffectsModule)"]
    assert (gui_end + effects_alignment - 1) & -effects_alignment == layout["BrnGameModule.mEffectsModule"]
    save_load = (layout["GuiModule.mProfileHost"] + layout["ProfileHost.mProfileManager"]
                 + layout["ProfileManager.mSaveLoadSystem"])
    receipt["native"][variant]["GuiModule_to_EffectsModule_gap"] = hex(layout["BrnGameModule.mEffectsModule"] - gui_end)
    receipt["native"][variant]["GuiModule_save_load_system"] = hex(save_load)
    receipt["native"][variant]["BrnGameModule_save_load_system"] = hex(layout["BrnGameModule.mGuiModule"] + save_load)

receipt["artist"] = {
    "ProfileHost.mProfileManager": "827E5CE8/5CFC/5D08, 824ED0E8, 82517C60, 82517CCC, 82525FA4",
    "ProfileHost extent, GuiModule.mSystemUserProfile": "827E5CE8 and 8251877C (difference 0x414D0)",
    "GuiModule invite flags": "825113F8..8251141C",
    "GuiModule sizeof": "8251807C..825180A4 (Gui Module Size)",
    "GuiModule save/load system": "8252A098..8252A0A0 (+0xA7748)",
    "BrnGameModule GUI/effects boundary": "827E5FBC..827E5FD0 (+0x6EAA20/+0x878700)",
    "BrnGameModule save/load FSM": "827E6030 (+0x9A0664)",
    "BrnGameModule stream wait": "823EE3EC (+0x9A0625), 823E8D8C (+0x9A062B)",
    "BrnGameModule pending GUI": "823DCA2C (+0x9A0644), 823DCB2C (+0x9A0648)",
}
head = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
if head != base:
    assert subprocess.check_output(["git", "rev-parse", "HEAD^"], text=True).strip() == base
    changed_paths = subprocess.check_output(
        ["git", "diff-tree", "--no-commit-id", "--name-only", "-r", head], text=True
    ).splitlines()
    assert changed_paths == [source.as_posix()]
    receipt["candidate_commit"] = head
(root / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n")
print(f"Verified both native layouts and {len(old) - len(changed)} unchanged BrnGameModule procedures")
