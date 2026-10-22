"""Hosted CPU check for the native TextFormat property binding used by font fitting.

Production setter/getter bodies and the real property-name table are compiled.
--old-setter restores the inherited rejecting virtual used before recovery; a
finite font-fit consumer then demonstrates the missing size progress safely.
This isolates the binding contract, not text rendering or full AS execution.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re

from fxgs_common import Tree, definition, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--old-setter', action='store_true')
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
    tree = Tree()
    source_path = 'src/SDKs/EATech/include/Apt/AptTextFormat.cpp'
    header_path = 'src/SDKs/EATech/include/Apt/AptTextFormat.h'
    table_path = 'src/SDKs/EATech/Apt/AptTextFormatMembersIndex.cpp'
    source = tree.read(source_path)
    header = tree.read(header_path)
    header_code = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/', '', header)
    assert re.search(r'virtual\s+bool\s+objectMemberSet\s*\(', header_code), 'Production class must override the rejecting base setter'
    setter = definition(source, 'bool AptTextFormat::objectMemberSet(')
    getter = definition(source, 'AptValue* AptTextFormat::objectMemberLookup(')
    bodies = getter + '\n' + ('' if args.old_setter else setter)
    numeric = compile_and_run(Path(__file__).with_name('AptTextFormatBinding.cpp'),
        'apt_textformat_binding_bodies.inc', bodies, 'AptTextFormatBinding',
        extra_flags='/DAPT_TEXTFORMAT_OLD_SETTER=1' if args.old_setter else '',
        extra_sources=[Path(__file__).resolve().parents[1] / table_path],
        output_dir=args.output_dir)
    if args.output_dir:
        paths = [source_path, header_path, table_path,
                 'src/SDKs/EATech/Apt/AptTextFormatMembersIndex.h']
        binding = {'coverage': 'Virtual property write/read, full setter cases, and bounded size-progress consumer; no renderer or AS VM claim',
                   'control': 'old inherited rejecting setter' if args.old_setter else None,
                   'sources': [{'path': p, 'lf_sha256': hashlib.sha256(tree.read(p).encode()).hexdigest()} for p in paths]}
        (args.output_dir / 'source-bindings.json').write_text(json.dumps(binding, indent=2) + '\n', encoding='utf-8')
    if numeric is None:
        return 1
    checks, failures = numeric
    print(f'run_apt_textformat_binding: {checks - failures}/{checks} pass ({failures} fail)')
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
