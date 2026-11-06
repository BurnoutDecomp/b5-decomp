import idaapi
import ida_bytes
import ida_funcs
import ida_name
import ida_nalt
import idautils
import idc
import ida_kernwin

def emit(value):
    print(value, flush=True)

emit('input=' + ida_nalt.get_input_file_path())
name='off_820DC32C'
ea=idc.get_name_ea_simple(name)
emit('%s=0x%08X' % (name, ea))
if ea != idc.BADADDR:
    emit('vtable xrefs:')
    for x in idautils.XrefsTo(ea, 0):
        emit('  from=0x%08X type=%s fn=%s' % (x.frm, x.type, idc.get_func_name(x.frm)))
    emit('vtable entries:')
    for i in range(16):
        slot=ea+4*i
        if not ida_bytes.is_loaded(slot):
            emit('  +%02X unreadable' % (4*i))
            break
        target=ida_bytes.get_dword(slot)
        fname=idc.get_func_name(target)
        f=ida_funcs.get_func(target)
        fend=f.end_ea if f else target+16
        emit('  +%02X -> 0x%08X name=%s func=%s' % (4*i, target, idc.get_name(target), fname))
        if f:
            emit('    func_range=0x%08X..0x%08X size=%d' % (f.start_ea, f.end_ea, f.end_ea-f.start_ea))
            for insn in idautils.Heads(target, min(fend, target+24)):
                emit('    0x%08X %s' % (insn, idc.generate_disasm_line(insn, 0)))
            refs=list(idautils.XrefsTo(target, 0))
            emit('    xrefs_to=%s' % ','.join('0x%08X:%s' % (x.frm, x.type) for x in refs[:20]))
        else:
            emit('    bytes=' + ida_bytes.get_bytes(target, 16).hex())

emit('named Widget functions:')
for start in idautils.Functions():
    fname=idc.get_func_name(start)
    if 'Widget' in fname:
        f=ida_funcs.get_func(start)
        emit('  0x%08X %s size=%d' % (start, fname, f.end_ea-f.start_ea if f else -1))

ida_kernwin.qexit(0)
