"""ARTIST suspension GUI wire record (82493A88) and consumer (82578510).
Extract both production ends; exercise the canonical unwrapped payload and the
consumer's existing channel-40 payload pointer. --rev dd3d0174 is the red side.
"""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report

HEADER = 'src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h'
BASE = 'src/GameShared/GameClasses/Gui/CgsGuiEvent.h'
CONSUMER = 'src/GameSource/Network/Managers/BrnNetworkStateManager_wN1_04.cpp'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rev')
    args = parser.parse_args()
    tree = Tree(args.rev)
    h = tree.read(HEADER)
    types = 'template <s32 EventTypeId>\n' + definition(tree.read(BASE), 'struct GuiEvent :') + ';\n'
    types += definition(h, 'struct GuiEventNetworkSuspension :') + ';\n'
    output = definition(h, 'void OutputGuiEvent(TEvent& lrEvent)').replace(
        'void OutputGuiEvent(', 'void StateInterface::OutputGuiEvent(')
    output = 'template <typename TEvent>\n' + output + '\n'
    signature = 'inline void StateInterface::OutputGuiEvent<GuiEventNetworkSuspension>('
    if signature in h:
        output += 'template <>\n' + definition(h, signature) + '\n'
    consumer = definition(tree.read(CONSUMER), 'case KI_GUI_EVENT_NETWORK_SUSPENSION:')
    numeric = compile_and_run(Path(__file__).with_name('FxGuiSuspension.cpp'),
        'fx_gui_suspension_types.inc', types, 'FxGuiSuspension', extra_files={
            'fx_gui_suspension_output.inc': output,
            'fx_gui_suspension_consumer.inc': consumer})
    return report('run_fx_gui_suspension', [], numeric, 20)

if __name__ == '__main__':
    sys.exit(main())
