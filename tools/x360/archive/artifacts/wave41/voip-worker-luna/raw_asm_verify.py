import importlib.util
import hashlib
import json
from pathlib import Path
from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN
import capstone

reader_path = Path(r'D:\Reverse\BP-Decomp_Workflow\tools\re\x360rd.py')
spec = importlib.util.spec_from_file_location('x360rd', reader_path)
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
assert reader.u32(0x82F264A8) == 4
assert [reader.u32(0x82F26488 + 8 * i) for i in range(4)] == [194, 38, 63, 61]
md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
ranges = [
    ('friends-mask', 0x82B286B0, 0x82B286DC),
    ('register-remote-talker', 0x82B28A98, 0x82B28B48),
    ('notify-payload', 0x82B28CC0, 0x82B28D08),
    ('local-voice-data-size', 0x82B28DD0, 0x82B28E70),
    ('worker-thread', 0x82B28E80, 0x82B28F18),
]
lines = ['ARTIST raw-image disassembly via calibrated x360rd.py; PPC big-endian Capstone 5.0.7',
         'sentinels: 0x82F264A8=4; 0x82F26488+8*i=[194,38,63,61]', '']
interval_hashes = []
for label, start, end in ranges:
    lines.append(f'===== {label} 0x{start:08X}-0x{end:08X} =====')
    code = reader.rd(start, end - start)
    interval_hashes.append({'label': label, 'start': f'0x{start:08X}',
                            'end': f'0x{end:08X}',
                            'sha256': hashlib.sha256(code).hexdigest()})
    for insn in md.disasm(code, start):
        lines.append(f'0x{insn.address:08X}  {insn.mnemonic:8} {insn.op_str}')
    lines.append('')
out = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave41\voip-worker-luna\raw-ppc-disassembly.txt')
out.write_text('\n'.join(lines) + '\n', encoding='utf-8')
Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave41\voip-worker-luna\raw-evidence.json').write_text(
    json.dumps({'reader': str(reader_path), 'imageData': reader.RAW or reader.FLAT,
                'capstoneVersion': capstone.__version__,
                'sentinels': {'0x82F264A8': reader.u32(0x82F264A8),
                              '0x82F26488+8*i': [reader.u32(0x82F26488 + 8 * i) for i in range(4)]},
                'intervals': interval_hashes}, indent=2) + '\n', encoding='utf-8')
print(out)
print('\n'.join(lines[:52]))
