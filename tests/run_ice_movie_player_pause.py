"""Exercise the production ICE exit blend with the simulation paused; --rev checks an older body."""
import argparse
import os
from pathlib import Path
import sys

from fxgs_common import Tree, definition, compile_and_run, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rev")
    args = parser.parse_args()
    source = Tree(args.rev).read("src/GameSource/Director/Utils/BrnICEMoviePlayer.cpp")
    update = definition(source, "void ICEMoviePlayer::Update(")
    os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
    result = compile_and_run(Path(__file__).with_name("IceMoviePlayerPause.cpp"),
                             "ice_movie_player_pause.inc", "namespace BrnDirector {\n" + update + "\n}",
                             "IceMoviePlayerPause")
    return report("run_ice_movie_player_pause", [], result, 9)


if __name__ == "__main__":
    sys.exit(main())
