"""Exercise production Apt shared-pointer and pending-vector ownership."""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, code_only


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rev')
    parser.add_argument('--growth-only', action='store_true')
    args = parser.parse_args()
    tree = Tree(args.rev)
    base = 'src/SDKs/EATech/include/Apt/'
    shared_header = code_only(tree.read(base + 'AptSharedPtr.h'))
    linker_header = code_only(tree.read(base + 'AptLinker.h'))
    types = 'template<class T> struct AptSharedPtr;\ntemplate<>\n'
    types += definition(shared_header, 'struct AptSharedPtr<AptFile>') + ';\n'
    types += 'using AptFilePtr = AptSharedPtr<AptFile>;\nstruct AptLinker {\n'
    types += definition(linker_header, 'struct PendingFileVector') + ';\n};\n'
    bodies = definition(shared_header, 'inline void AptSharedPtrDisposeDebugHook(') + '\n'
    shared = tree.read(base + 'AptSharedPtr.cpp')
    for signature in ('void AptSharedPtr<AptFile>::Dispose(',
                      'AptSharedPtr<AptFile>& AptSharedPtr<AptFile>::operator=('):
        bodies += definition(shared, signature) + '\n'
    linker = tree.read(base + 'AptLinker.cpp')
    for signature in ('void AptLinker::PendingFileVector::PushBack(',
                      'AptLinker::PendingFileVector::~PendingFileVector(',
                      'void AptLinker::PendingFileVector::Swap('):
        bodies += definition(linker, signature) + '\n'
    numeric = compile_and_run(
        Path(__file__).with_name('AptFileLifetime.cpp'),
        'apt_file_lifetime_bodies.inc', bodies, 'AptFileLifetime',
        extra_flags='/DAPT_LIFETIME_GROWTH_ONLY' if args.growth_only else '',
        extra_files={'apt_file_lifetime_types.inc': types})
    return report('run_apt_file_lifetime', [], numeric, 104 if args.growth_only else 163)


if __name__ == '__main__':
    sys.exit(main())
