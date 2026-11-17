"""Build the opt-in save-generation menu in an isolated tracked-source copy.

No device operation or standard-firmware artifact is performed. Run after
building tools/target/debug/firmware-build with the project's offline toolchain.
"""

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
BUILD_ROOT = ROOT / "tests/build/save-generation-candidate"
BUILDER = ROOT / "tools/target/debug/firmware-build"


def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True, stdout=subprocess.DEVNULL)


def main():
    if not BUILDER.is_file():
        raise SystemExit("build the offline firmware-build tool first")
    BUILD_ROOT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="source-", dir=BUILD_ROOT) as folder:
        repo = Path(folder)
        tracked = subprocess.check_output(
            ["git", "ls-files", "-z", "Firm_Saturn"], cwd=ROOT).split(b"\0")
        for raw in tracked:
            if not raw:
                continue
            relative = Path(raw.decode())
            destination = repo / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        makefile = repo / "Firm_Saturn/Makefile"
        makefile.write_bytes(b"SAROO_SAVE_GENERATION=1\n" + makefile.read_bytes())
        run("git", "init", "-q", cwd=repo)
        run("git", "add", "Firm_Saturn", cwd=repo)
        run("git", "-c", "user.name=SAROO Build Fixture",
            "-c", "user.email=build-fixture@example.invalid", "commit", "-qm",
            "Opt-in save-generation build fixture", cwd=repo)
        output = repo / "tests/build/menu"
        result = subprocess.run(
            [str(BUILDER), "menu", "--repo", str(repo), "--out", str(output)],
            capture_output=True, text=True)
        if result.returncode:
            sys.stderr.write(result.stdout + result.stderr)
            raise SystemExit(result.returncode)
        receipt = json.loads((output / "receipt.json").read_text())
        resources = receipt["resources"]
        direct_stack = receipt["compiler_direct_stack"]
        print(json.dumps({
            "code_data_bytes": resources["code_data_bytes"],
            "bss_bytes": resources["bss_bytes"],
            "body_margin_bytes": resources["body_margin_bytes"],
            "reserved_stack_bytes": resources["reserved_stack_bytes"],
            "image_sha256": resources["image_sha256"],
            "largest_direct_frame_bytes": {
                name: max((entry["bytes"] for entry in direct_stack[name]),
                          default=0)
                for name in ("main.su", "conio.su", "game_load.su",
                             "game_save.su", "save_journal.su")
            },
        }, sort_keys=True))


if __name__ == "__main__":
    main()
