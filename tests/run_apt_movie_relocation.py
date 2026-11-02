"""Exercise production timeline relocation and its inverse on native Apt records."""
from pathlib import Path
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, code_only


def main():
    tree = Tree()
    base = 'src/SDKs/EATech/include/Apt/'
    source = tree.read(base + 'AptMovie.cpp')
    bodies = '\n'.join(definition(source, signature) for signature in (
        'void AptMovie::resolve64(', 'void* AptMovie::unresolve('))
    handlers = code_only(tree.read(base + 'AptCharacterSpriteInstBase.h'))
    types = definition(handlers, 'struct AptClipEventHandler\n') + ';\n'
    types += definition(handlers, 'struct AptClipEventHandlerList\n') + ';\n'
    numeric = compile_and_run(
        Path(__file__).with_name('AptMovieRelocation.cpp'),
        'apt_movie_relocation_bodies.inc', bodies, 'AptMovieRelocation',
        extra_files={'apt_movie_relocation_types.inc': types})
    return report('run_apt_movie_relocation', [], numeric, 73)


if __name__ == '__main__':
    sys.exit(main())
