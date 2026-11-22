"""Smoke-test the extracted EAI-only host and its isolated settings."""
import argparse
import json
from pathlib import Path
import subprocess
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("--zip", type=Path, required=True)
parser.add_argument("--work", type=Path, required=True)
args = parser.parse_args()
work = args.work.resolve()
work.mkdir(parents=True)
package = work / "package"
with zipfile.ZipFile(args.zip) as archive:
    for item in archive.infolist():
        if not (package / item.filename).resolve().is_relative_to(package):
            raise ValueError("Package path escapes staging")
    archive.extractall(package)

host = package / "native/windows-x64/EAI.OfflineHost.exe"
profile = work / "profile"


def request(request_id, command, **fields):
    return json.dumps({"request_id": request_id, "body": {"command": command, **fields}})


def run(*commands):
    completed = subprocess.run(
        [str(host), "--profile", str(profile)],
        input="\n".join((*commands, "stop", "")),
        encoding="utf-8",
        capture_output=True,
        check=True,
        timeout=15,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    messages = [json.loads(line) for line in completed.stdout.splitlines()]
    assert messages[0] == {
        "kind": "ready", "module_id": "local.endfield.interaction", "game_runtime": False
    }
    assert "Handshake source assets: loaded" in completed.stderr
    return {message["request_id"]: message for message in messages if message["kind"] == "reply"}


responses = run(
    "{malformed",
    request("workflow", "workflow_get"),
    request("runtime", "connect"),
    request("save", "whitelist_set", disabled=["chr_0006_wolfgd"], save=True),
)
assert responses[""]["result"] != 0
assert responses["workflow"]["result"] == 0
assert responses["runtime"]["result"] == 2 and "runtime" in responses["runtime"]["body"]["detail"]
assert responses["save"]["result"] == 0
settings = profile / "EAI-Offline/BetterEndfield/Interaction/overlay-settings.json"
assert json.loads(settings.read_text(encoding="utf-8"))["disabled_operators"] == ["chr_0006_wolfgd"]
reloaded = run(request("settings", "paired_get"))["settings"]["body"]["operator_roster"]
assert not next(actor for actor in reloaded if actor["id"] == "chr_0006_wolfgd")["enabled"]
assert not (host.parent / "Interaction.Diagnostics.log").exists()
print("PASS: bundled EAI-only host, JSON replies, isolated settings, runtime boundary, and clean stop")
