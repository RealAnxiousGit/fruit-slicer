#!/usr/bin/env python3
"""Keeps the game the same in both repos. shared-files.txt lists every file that must match byte for byte.

  sync-shared.py check OTHER_REPO      list files that differ; exits 1 if any do
  sync-shared.py push OTHER_REPO       copy this repo's shared files over the other repo's
  sync-shared.py pull OTHER_REPO       copy the other repo's shared files over this one's

Make the change in either repo, push it across, then build both. Anything one port does differently
belongs in that port's port/ folder and port.h, never in a shared file's copy.
"""
import filecmp
import os
import shutil
import sys

HERE = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def same(a, b):
    return os.path.exists(a) and os.path.exists(b) and filecmp.cmp(a, b, shallow=False)


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("check", "push", "pull"):
        sys.exit(__doc__)
    what, other = sys.argv[1], os.path.abspath(sys.argv[2])
    with open(os.path.join(HERE, "shared-files.txt")) as f:
        files = [line.strip() for line in f if line.strip()]
    differ = [p for p in files if not same(os.path.join(HERE, p), os.path.join(other, p))]
    if what == "check":
        for p in differ:
            print("differs:", p)
        print(f"{len(files) - len(differ)} of {len(files)} shared files match")
        sys.exit(1 if differ else 0)
    src, dst = (HERE, other) if what == "push" else (other, HERE)
    for p in differ:
        os.makedirs(os.path.dirname(os.path.join(dst, p)), exist_ok=True)
        shutil.copyfile(os.path.join(src, p), os.path.join(dst, p))
        print("copied:", p)
    print(f"{len(differ)} files updated")


if __name__ == "__main__":
    main()
