from __future__ import annotations

import argparse
import sys
import zipfile
from pathlib import Path


def choose_path(candidates: list[Path], description: str) -> Path:
    if not candidates:
        raise FileNotFoundError(f"Could not find {description} in the build tree.")

    return sorted(candidates, key=lambda path: (len(path.parts), str(path)))[0]


def find_plugin_bundle(build_dir: Path) -> Path:
    candidates = [path for path in build_dir.rglob("AI Synth.vst3") if path.is_dir()]
    return choose_path(candidates, "the VST3 bundle")


def find_standalone(build_dir: Path, runner_os: str) -> Path:
    if runner_os == "Windows":
        candidates = [path for path in build_dir.rglob("AI Synth.exe") if path.is_file()]
        return choose_path(candidates, "the Windows standalone executable")

    if runner_os == "macOS":
        candidates = [path for path in build_dir.rglob("AI Synth.app") if path.is_dir()]
        return choose_path(candidates, "the macOS standalone app")

    candidates = [
        path
        for path in build_dir.rglob("AI Synth")
        if path.is_file() and "Standalone" in path.parts
    ]

    if not candidates:
        candidates = [path for path in build_dir.rglob("AI Synth") if path.is_file()]

    return choose_path(candidates, "the Linux standalone binary")


def zip_path(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        destination.unlink()

    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        if source.is_dir():
            root = source.parent
            for file_path in sorted(source.rglob("*")):
                if file_path.is_dir():
                    continue
                archive.write(file_path, file_path.relative_to(root))
        else:
            archive.write(source, source.name)


def main() -> int:
    parser = argparse.ArgumentParser(description="Package AI Synth build outputs for GitHub releases.")
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--dist-dir", required=True, type=Path)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--runner-os", required=True)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    dist_dir = args.dist_dir.resolve()

    plugin_bundle = find_plugin_bundle(build_dir)
    standalone = find_standalone(build_dir, args.runner_os)

    plugin_zip = dist_dir / f"AI-Synth-{args.platform}-VST3.zip"
    standalone_zip = dist_dir / f"AI-Synth-{args.platform}-Standalone.zip"

    zip_path(plugin_bundle, plugin_zip)
    zip_path(standalone, standalone_zip)

    print(f"Packaged plugin: {plugin_zip}")
    print(f"Packaged standalone: {standalone_zip}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
