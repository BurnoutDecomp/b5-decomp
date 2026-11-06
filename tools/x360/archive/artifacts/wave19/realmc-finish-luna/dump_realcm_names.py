import ida_auto, ida_funcs, ida_lines, ida_name, ida_typeinf, idautils, idc, pathlib
ida_auto.auto_wait()
out = pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave19\realmc-finish-luna\ida-name-scan.txt')
with out.open('w', encoding='utf-8') as f:
    f.write('### named items containing Realmc/Message/Processor\n')
    for ea, name in idautils.Names():
        if any(term.lower() in name.lower() for term in ('Realmc', 'Message', 'Processor')):
            f.write(f'{ea:08X} {name} {idc.get_type(ea)}\n')
    f.write('### strings containing Realmc/Message/Processor\n')
    for s in idautils.Strings():
        value = str(s)
        if any(term.lower() in value.lower() for term in ('Realmc', 'Message', 'Processor')):
            f.write(f'{int(s.ea):08X} {value}\n')
    f.write('### local type names\n')
    try:
        til = ida_typeinf.get_idati()
        qty = ida_typeinf.get_ordinal_qty(til)
        f.write(f'ordinals={qty}\n')
        for ordinal in range(1, qty + 1):
            name = ida_typeinf.get_numbered_type_name(til, ordinal)
            if name and any(term.lower() in name.lower() for term in ('Realmc', 'Message', 'Processor')):
                f.write(f'{ordinal} {name}\n')
    except Exception as exc:
        f.write(f'type-enumeration-error={exc}\n')
