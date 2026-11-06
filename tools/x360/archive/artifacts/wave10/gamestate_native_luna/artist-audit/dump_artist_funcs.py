import idaapi, ida_bytes, ida_funcs, ida_lines, ida_name, idc, idautils
out_path = r'D:\Reverse\BP-X360-Experimental-Artifacts\wave10\gamestate_native_luna\artist-audit\artist-functions.asm.txt'
needles = [
    'SendSetUpAllDriveThrusMessage', 'SendSetUpAllEventStartsMessage',
    'SetupParRivals', 'UpdateDriftStunts', 'UpdateBoostStunts',
    'DealWithHitProp', 'HasCrashModeEnded', 'CrashModeScoring::Update',
    'GetVehicleScoreData'
]
addresses = {0x823759D0, 0x8233F560, 0x8232CAE0, 0x8232CC18,
             0x82320DC8, 0x823129A0, 0x82320808, 0x82312AB0}
found = []
for ea in idautils.Functions():
    name = ida_name.get_name(ea) or ''
    if ea in addresses or any(n in name for n in needles):
        f = ida_funcs.get_func(ea)
        if not f:
            continue
        lines = [f'FUNCTION {ea:08X} {name} END {f.end_ea:08X}']
        for head in idautils.Heads(f.start_ea, f.end_ea):
            if ida_bytes.is_code(ida_bytes.get_full_flags(head)):
                text = idaapi.generate_disasm_line(head, 0) or ''
                lines.append(f'{head:08X}: {ida_lines.tag_remove(text)}')
        found.append('\n'.join(lines))
with open(out_path, 'w', encoding='utf-8') as f:
    f.write('\n\n'.join(found))
idaapi.qexit(0)
