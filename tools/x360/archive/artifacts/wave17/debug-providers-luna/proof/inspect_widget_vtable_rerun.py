import ida_bytes
import ida_funcs
import ida_nalt
import idautils
import idc


def emit(value):
    print(value, flush=True)


def show_function(target):
    f=ida_funcs.get_func(target)
    if not f:
        emit('  no function at 0x%08X' % target)
        return
    emit('  function=0x%08X..0x%08X size=%d name=%s' % (f.start_ea, f.end_ea, f.end_ea-f.start_ea, idc.get_func_name(target)))
    for insn in idautils.Heads(f.start_ea, min(f.end_ea, f.start_ea+32)):
        emit('    0x%08X %s' % (insn, idc.generate_disasm_line(insn, 0)))

emit('input=' + ida_nalt.get_input_file_path())
vt=idc.get_name_ea_simple('off_820DC32C')
emit('WidgetLineGraph vtable addresspoint=0x%08X' % vt)
if vt != idc.BADADDR:
    emit('vtable slot 0:')
    update=ida_bytes.get_dword(vt)
    emit('  target=0x%08X name=%s' % (update, idc.get_name(update)))
    show_function(update)
    emit('vtable slot 1:')
    render=ida_bytes.get_dword(vt+4)
    emit('  target=0x%08X name=%s' % (render, idc.get_name(render)))
    show_function(render)

emit('all xrefs to slot0 target 0x%08X:' % update)
refs=list(idautils.XrefsTo(update, 0))
for x in refs:
    if x.type in (1,2,3,4,5,6):
        emit('  data ref from=0x%08X type=%d name=%s' % (x.frm, x.type, idc.get_name(x.frm)))
        for delta in (-8,-4,0,4,8):
            a=x.frm+delta
            if ida_bytes.is_loaded(a):
                emit('    [%+d] 0x%08X = 0x%08X name=%s' % (delta,a,ida_bytes.get_dword(a),idc.get_name(ida_bytes.get_dword(a))))
    else:
        emit('  code ref from=0x%08X type=%d fn=%s' % (x.frm, x.type, idc.get_func_name(x.frm)))

emit('all xrefs to slot1 target 0x%08X:' % render)
for x in idautils.XrefsTo(render, 0):
    emit('  ref from=0x%08X type=%d name=%s fn=%s' % (x.frm, x.type, idc.get_name(x.frm), idc.get_func_name(x.frm)))
