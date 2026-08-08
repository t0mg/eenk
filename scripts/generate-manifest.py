#!/usr/bin/env python3
"""
scripts/generate-manifest.py

Generates a manifest.json file describing all firmware binaries for a release.
The manifest is consumed by the Web Serial flasher to select the correct
binary set for each hardware target and flashing mode.

Usage:
    python scripts/generate-manifest.py \\
        --release v1.0.0 \\
        --base-url https://github.com/t0mg/eenk/releases/download/v1.0.0 \\
        --artifacts-dir firmware-artifacts/ \\
        --output docs/dist/manifest.json

Arguments:
    --release       Git tag / release name (e.g. v1.0.0)
    --base-url      Base URL where release assets are hosted
    --artifacts-dir Directory containing downloaded GitHub Actions artifacts
    --output        Output path for manifest.json (default: manifest.json)
"""

import argparse
import json
import os
import sys
from datetime import datetime, timezone


# ── Partition offsets (must match partitions.csv / partitions_x4pro.csv) ──────

X4_OFFSETS = {
    # Offset 0x0 used for merged factory binary (bootloader at 0x0)
    'factory_merged': '0x0',
    'app0':           '0x10000',
    'app1_updater':   '0x710000',
}

X4PRO_OFFSETS = {
    'factory_merged': '0x0',
    'app0':           '0x10000',
    # No separate updater: A/B OTA handled from app0
}


def make_url(base_url: str, filename: str) -> str:
    return f"{base_url.rstrip('/')}/{filename}"


def find_artifact(artifacts_dir: str, *candidates: str) -> str | None:
    """Search artifacts_dir subdirectories for the first matching filename."""
    for root, _, files in os.walk(artifacts_dir):
        for candidate in candidates:
            if candidate in files:
                return os.path.join(root, candidate)
    return None


def check_artifact(artifacts_dir: str, filename: str, label: str) -> bool:
    path = find_artifact(artifacts_dir, filename)
    if path:
        size_kb = os.path.getsize(path) / 1024
        print(f"  ✓ {label}: {filename} ({size_kb:.0f} KB)")
        return True
    else:
        print(f"  ✗ {label}: {filename} NOT FOUND", file=sys.stderr)
        return False


def build_manifest(release: str, base_url: str, artifacts_dir: str) -> dict:
    ok = True

    # Expected artifact filenames (produced by PlatformIO + merge_firmware.py)
    X4_FACTORY   = 'x3x4-firmware-factory.bin'   # renamed by CI from esp32c3 build
    X4_APP       = 'x3x4-firmware.bin'
    X4PRO_FACTORY = 'x4pro-firmware-factory.bin'
    X4PRO_APP    = 'x4pro-firmware.bin'

    print("Checking artifacts:")
    ok &= check_artifact(artifacts_dir, X4_FACTORY,    'X3/X4 factory image')
    ok &= check_artifact(artifacts_dir, X4_APP,        'X3/X4 app binary')
    ok &= check_artifact(artifacts_dir, X4PRO_FACTORY, 'X4 Pro factory image')
    ok &= check_artifact(artifacts_dir, X4PRO_APP,     'X4 Pro app binary')

    if not ok:
        print("\nERROR: One or more artifacts are missing. Aborting.", file=sys.stderr)
        sys.exit(1)

    x4_factory_entry = {
        "chip": "esp32c3",
        "description": "Full erase and install (X3 / X4). Writes bootloader, partition table, and application.",
        "binaries": [
            {"offset": X4_OFFSETS['factory_merged'], "url": make_url(base_url, X4_FACTORY)}
        ]
    }

    x4_update_entry = {
        "chip": "esp32c3",
        "description": "App update only. Preserves NVS user settings.",
        "binaries": [
            {"offset": X4_OFFSETS['app0'], "url": make_url(base_url, X4_APP)}
        ]
    }

    x4pro_factory_entry = {
        "chip": "esp32s3",
        "description": "Full erase and install (X4 Pro). Writes bootloader, partition table, and application.",
        "binaries": [
            {"offset": X4PRO_OFFSETS['factory_merged'], "url": make_url(base_url, X4PRO_FACTORY)}
        ]
    }

    x4pro_update_entry = {
        "chip": "esp32s3",
        "description": "App update only (X4 Pro). Preserves NVS user settings.",
        "binaries": [
            {"offset": X4PRO_OFFSETS['app0'], "url": make_url(base_url, X4PRO_APP)}
        ]
    }

    crosspoint_entry = {
        "description": "CrossPoint / Stock Restore. Binaries sourced externally — see flasher UI.",
        "binaries": []
    }

    manifest = {
        "schema_version": 1,
        "generated": datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
        "release": release,
        "targets": {
            "X3": {
                "factory":    x4_factory_entry,
                "update":     x4_update_entry,
                "crosspoint": crosspoint_entry
            },
            "X4": {
                "factory":    x4_factory_entry,
                "update":     x4_update_entry,
                "crosspoint": crosspoint_entry
            },
            "X4Pro": {
                "factory":    x4pro_factory_entry,
                "update":     x4pro_update_entry,
                "crosspoint": crosspoint_entry
            }
        }
    }

    return manifest


def main():
    parser = argparse.ArgumentParser(description='Generate eenk firmware manifest.json')
    parser.add_argument('--release',       required=True,  help='Release tag (e.g. v1.0.0)')
    parser.add_argument('--base-url',      required=True,  help='Base URL for release assets')
    parser.add_argument('--artifacts-dir', required=True,  help='Directory with downloaded CI artifacts')
    parser.add_argument('--output',        default='manifest.json', help='Output path for manifest.json')
    args = parser.parse_args()

    if not os.path.isdir(args.artifacts_dir):
        print(f"ERROR: artifacts-dir does not exist: {args.artifacts_dir}", file=sys.stderr)
        sys.exit(1)

    print(f"Generating manifest for release {args.release}…")
    manifest = build_manifest(args.release, args.base_url, args.artifacts_dir)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)
        f.write('\n')

    print(f"\n✅ Manifest written to: {args.output}")
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
