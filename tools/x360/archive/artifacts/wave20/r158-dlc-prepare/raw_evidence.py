import json
import sys
from pathlib import Path

sys.path.insert(0, r'D:\Reverse\BP-Decomp_Workflow\tools\re')
import x360rd
from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN

sites = {
    0x8266AF5C: ('mr', 'r31, r3'),
    0x8266AFD0: ('stw', 'r27, 0x620(r31)'),
    0x8266AFE0: ('bl', '0x82b42ef8'),
    0x8266AFE8: ('beq', 'cr6, 0x8266be28'),
    0x8266B020: ('bl', '0x821f14f8'),
    0x8266B070: ('lis', 'r10, -0x7df6'),
    0x8266B0BC: ('bl', '0x82927f60'),
    0x8266B9B4: ('stw', 'r27, 0x624(r31)'),
    0x8266B9C4: ('li', 'r3, 0'),
    0x8266BA0C: ('bl', '0x828f2300'),
    0x8266BA20: ('bl', '0x828de3e8'),
    0x8266BA34: ('bl', '0x828de338'),
    0x8266BA3C: ('bl', '0x82661160'),
    0x8266BA4C: ('bl', '0x82662e18'),
    0x8266BA90: ('bl', '0x828f23d8'),
    0x8266BAE8: ('bl', '0x828f24c0'),
    0x8266BB48: ('bl', '0x82c0ce88'),
    0x8266BBE4: ('cmplwi', 'cr6, r11, 1'),
    0x8266BC10: ('bl', '0x82661628'),
    0x8266BC34: ('bl', '0x826616a0'),
    0x8266BCA4: ('cmplw', 'cr6, r27, r11'),
    0x8266BCE4: ('stw', 'r10, 0x40(r11)'),
    0x8266BDDC: ('bge', 'cr6, 0x8266bdf4'),
    0x8266BDE4: ('li', 'r3, 0'),
    0x8266BDF8: ('li', 'r3, 1'),
    0x82669DE8: ('addi', 'r11, r11, 0x26a8'),
    0x82669DEC: ('stb', 'r10, 0x60(r1)'),
    0x82669DF0: ('stw', 'r11, 0x64(r1)'),
}

dis = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
rows = []
for address, expected in sites.items():
    raw = x360rd.rd(address, 4)
    insn = next(dis.disasm(raw, address))
    observed = (insn.mnemonic, insn.op_str)
    rows.append({'address': f'{address:08X}', 'bytes': raw.hex(),
                 'instruction': ' '.join(observed), 'expected': ' '.join(expected),
                 'pass': observed == expected})

prefix = x360rd.rd(0x820A290C, 10)
pattern = x360rd.rd(0x820A2918, len(b'p_hdd:/dlc%d/DATA\0'))
manager_file = x360rd.rd(0x820A070C, 64).split(b'\0', 1)[0]
string_file = x360rd.rd(0x82001DA4, 72).split(b'\0', 1)[0]
mask_message = x360rd.rd(0x820A2844, 48).split(b'\0', 1)[0]
thread_name = x360rd.rd(0x820A26A8, 32).split(b'\0', 1)[0]
checks = {'disc_prefix': prefix == b'p_hdd:/d/\0',
          'hdd_pattern': pattern == b'p_hdd:/dlc%d/DATA\0',
          'assert_paths': (manager_file == br'..\..\..\GameSource\Resource/BrnDLCManager.cpp'
                           and string_file == br'..\..\..\GameShared\GameClasses\Core/CgsStringUtils.h'),
          'mask_message': mask_message == b'invlaid package mask in version.txt',
          'thread_name': thread_name == b'DLCNotifyThread',
          'instructions': all(row['pass'] for row in rows)}
out = {'raw_image': x360rd.RAW or x360rd.FLAT,
       'disc_prefix_hex': prefix.hex(), 'hdd_pattern_hex': pattern.hex(),
       'manager_assert_path': manager_file.decode('ascii'),
       'string_assert_path': string_file.decode('ascii'),
       'mask_message': mask_message.decode('ascii'),
       'thread_name': thread_name.decode('ascii'),
       'checks': checks, 'instructions': rows}
Path(__file__).with_name('raw-evidence.json').write_text(json.dumps(out, indent=2) + '\n', encoding='utf-8')
print(json.dumps(checks, indent=2))
if not all(checks.values()):
    raise SystemExit(1)
