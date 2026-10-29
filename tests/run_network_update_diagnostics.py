"""Source-coupled controls for the ARTIST network log stream and update messages."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re

import fxgs_common as common

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = next(parent for parent in REPO.parents
                if (parent / "tools/build/msvc_flags.txt").is_file())
common.WORKFLOW = WORKFLOW

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--packet", type=Path,
                    default=Path(os.environ["NETWORK_DIAG_PACKET"]) if os.environ.get("NETWORK_DIAG_PACKET")
                    else WORKFLOW / "scratch/DEBUG_MENU_1011/swarm_network_send_receive_diagnostics_recovery_20261028_revision02")
parser.add_argument("--fixture-output", type=Path)
args = parser.parse_args()
PACKET = args.packet.resolve()

SOURCE_REL = Path("src/GameSource/Network/BrnNetworkPlayer.cpp")
SOURCE = REPO / SOURCE_REL
BASELINE = PACKET / "pre/src/GameSource/Network/BrnNetworkPlayer.cpp"
FIXTURE = REPO / "tests/NetworkUpdateDiagnostics.cpp"
STRSTREAM_SOURCE = REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"
CHANNEL_SOURCE = REPO / "src/GameShared/GameClasses/Development/Log/CgsLogChannelOutput.cpp"
OUTPUT = (args.fixture_output.resolve() if args.fixture_output else PACKET / "compile/fixture01")

SEND_SIGNATURE = "void BrnNetworkPlayer::HandleSendingUpdateMessage()"
RECV_SIGNATURE = "void BrnNetworkPlayer::TellNetworkCarAboutRecievedUpdateMessage(u16 lu16CurrentFrame,"
SEND_CONDITION = "if (mpNetworkModule->IsSendUpdateMessageToBeShown())"
RECV_CONDITION = "if (mpNetworkModule->IsRecvUpdateMessageToBeShown())"
NETWORK_STREAM = "CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput"
VECTOR_FORMAT = 'AppendFormat("(%f, %f, %f)"'


def definition(source, signature):
    try:
        return common.definition(source, signature)
    except (ValueError, IndexError) as error:
        raise RuntimeError(f"missing source definition: {signature}") from error


def token_sequence(text):
    token = re.compile(
        r"/\*[\s\S]*?\*/|//[^\n]*|\"(?:\\.|[^\"\\])*\"|"
        r"'(?:\\.|[^'\\])*'|[A-Za-z_]\w*|(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?|"
        r"::|->|&&|\|\||==|!=|<=|>=|\+=|-=|\*=|/=|<<|>>|\+\+|--|[^\s]"
    )
    return [m.group(0) for m in token.finditer(text)
            if not m.group(0).startswith(("//", "/*"))]


def conditional_block(body, condition):
    try:
        return common.definition(body, condition)
    except (ValueError, IndexError) as error:
        raise RuntimeError(f"missing branch: {condition}") from error


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


if not BASELINE.is_file():
    raise RuntimeError(f"missing exact preimage: {BASELINE}")
before = BASELINE.read_text(encoding="utf-8-sig")
after = SOURCE.read_text(encoding="utf-8-sig")
before_send = definition(before, SEND_SIGNATURE)
before_recv = definition(before, RECV_SIGNATURE)
after_send = definition(after, SEND_SIGNATURE)
after_recv = definition(after, RECV_SIGNATURE)
send_block = conditional_block(after_send, SEND_CONDITION)
recv_block = conditional_block(after_recv, RECV_CONDITION)

for body, condition, label in (
    (after_send, SEND_CONDITION, "send"),
    (after_recv, RECV_CONDITION, "receive"),
):
    require(body.count(condition) == 1, f"{label} flag getter must be read once")
    block = conditional_block(body, condition)
    require(NETWORK_STREAM in block, f"{label} block must use the attested network stream")
    require(VECTOR_FORMAT in block, f"{label} block must call the existing vector AppendFormat contract")
    require("gpDebugPrint" not in block, f"{label} block must not substitute gpDebugPrint")
    require('" POS=("' in block and '") FRAMES SINCE START="' in block,
            f"{label} outer literals must preserve the ARTIST parentheses")

send_without_diag = after_send.replace(send_block, "", 1)
recv_without_diag = after_recv.replace(recv_block, "", 1)
require(token_sequence(before_send) == token_sequence(send_without_diag),
        "send handler has a non-diagnostic change outside the two recovered blocks")
require(token_sequence(before_recv) == token_sequence(recv_without_diag),
        "receive handler has a non-diagnostic change outside the two recovered blocks")

send_prepare = after_send.index("mUpdateMessageSend.PrepareForSend(&lUpdateData)")
send_reset = after_send.index("mbHasResetCarTransformSinceLastUpdateSent = false")
require(after_send.index(send_block) < send_prepare < send_reset,
        "send diagnostic must precede prepare and preserve reset order")
recv_headset = after_recv.index("lpVoipManager->HandleReceivedHeadsetStatus(")
recv_driver = after_recv.index("lpVehicleDriverInputInterface->GetUpdateDriverQueue()->AddEvent(")
recv_game = after_recv.index("mpNetworkModule->GetNetworkEventQueue()->AddEvent(")
require(recv_headset < after_recv.index(recv_block) < recv_driver < recv_game,
        "receive diagnostic must remain between headset and existing queue effects")

expected_include = f"""
struct FakeNetworkModule
{{
    bool mbShowSend;
    bool mbShowReceive;
    unsigned miSendGetterCalls = 0;
    unsigned miRecvGetterCalls = 0;
    bool IsSendUpdateMessageToBeShown() {{ ++miSendGetterCalls; return mbShowSend; }}
    bool IsRecvUpdateMessageToBeShown() {{ ++miRecvGetterCalls; return mbShowReceive; }}
}};

struct SendDiagnosticContext
{{
    FakeNetworkModule* mpNetworkModule;
    s32 miPlayerID;
    const BrnNetwork::UpdateData& lUpdateData;
    s32 GetPlayerID() const {{ return miPlayerID; }}
    void Run() {{
{send_block}
    }}
}};

struct ReceiveDiagnosticContext
{{
    FakeNetworkModule* mpNetworkModule;
    s32 miPlayerID;
    const BrnNetwork::UpdateData* lpUpdateData;
    s32 GetPlayerID() const {{ return miPlayerID; }}
    void Run() {{
{recv_block}
    }}
}};
"""

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
if OUTPUT.exists() and any(OUTPUT.iterdir()):
    raise RuntimeError(f"refusing to overwrite non-empty fixture directory: {OUTPUT}")
result = common.compile_and_run(
    FIXTURE,
    "NetworkUpdateDiagnostics.inc",
    expected_include,
    "NetworkUpdateDiagnostics",
    extra_flags=f'/I"{REPO / "src"}"',
    extra_sources=(STRSTREAM_SOURCE, CHANNEL_SOURCE),
    output_dir=OUTPUT,
)
if result is None:
    raise SystemExit(1)
checks, failures = result
receipt = {
    "source_path": str(SOURCE_REL).replace("\\", "/"),
    "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
    "baseline_sha256": hashlib.sha256(BASELINE.read_bytes()).hexdigest(),
    "fixture_sha256": hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
    "production_checks": {
        "send_uses_static_network_log_channel_output": True,
        "receive_uses_static_network_log_channel_output": True,
        "both_use_original_vector_format_via_append_format": True,
        "all_other_handler_tokens_equal_preimage": True,
        "integer_format_claim_scope": "fixture covers default decimal mode only; no claim is made for non-decimal modes",
    },
    "source_coupled_fixture": {"checks": checks, "failures": failures},
    "limits": [
        "The exact extracted production branches execute against the real LogChannelOutput and StrStreamBase implementations.",
        "The fixture uses a test WriteToLog sink and does not execute either complete network handler or the game runtime.",
    ],
}
(OUTPUT / "SOURCE_COUPLED_REGRESSION.json").write_text(
    json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
print("SOURCE_WIRING=pass")
print("PRODUCTION_SIDE_EFFECTS=unchanged")
print(f"FIXTURE=pass ({checks} checks, {failures} failures)")
raise SystemExit(bool(failures))
