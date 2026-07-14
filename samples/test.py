#!/bin/env python3

# Rudimentary test wrapper for SDXI samples

import os
import random
import re
import stat
import subprocess

DEVICE_PATTERN = re.compile(r"^sdxi(\d+)(?:v(\d+))?$")
DEV_DIR = "/dev"

TESTS = [
        ("./context", "create a context"),
        ("./nop",     "submit one DMA NOP descriptor"),
        ("./uadd",    "atomic add"),
        ("./write_imm",    "write immediate data via descriptor"),
        ("./memcopy", "memory copy"),
        ("./repcopy", "repeated memory copy"),
        ("./crc", "CRC Mode1 operations"),
        ("./cxt_intr",    "context interrupt delivery"),
        ("./opgroups","opgroup discovery"),
]

# Tests that need two SDXI devices and use a different CLI than `-dev`.
# For each entry the wrapper picks a random pair of distinct devices.
CROSS_DEV_TESTS = [
        ("./xcopy",   "cross-device memory copy"),
]

# How many random (src, dst) pairs to exercise per cross-device test when
# more than two SDXI devices are present. Override via the
# SDXI_TEST_XCOPY_PAIRS environment variable; the wrapper caps the count at
# the number of unordered device pairs available so we never re-use a pair
# in a single run.
DEFAULT_CROSS_DEV_PAIRS = 1

def find_sdxi_devices():
    devices = []

    try:
        with os.scandir(DEV_DIR) as entries:
            for entry in entries:
                match = DEVICE_PATTERN.match(entry.name)
                if not match:
                    continue

                full_path = os.path.join(DEV_DIR, entry.name)
                try:
                    mode = entry.stat(follow_symlinks=False).st_mode
                except OSError:
                    continue

                if not stat.S_ISCHR(mode):
                    continue

                pf = int(match.group(1))
                vf = int(match.group(2)) if match.group(2) is not None else -1
                devices.append((pf, vf, full_path))
    except FileNotFoundError:
        return []

    devices.sort(key=lambda item: (item[0], item[1]))
    return [item[2] for item in devices]


def _cross_dev_pair_count(num_devices):
    """Return how many distinct (src, dst) pairs to try."""
    try:
        requested = int(os.environ.get("SDXI_TEST_XCOPY_PAIRS",
                                       DEFAULT_CROSS_DEV_PAIRS))
    except ValueError:
        requested = DEFAULT_CROSS_DEV_PAIRS
    requested = max(requested, 1)
    # n*(n-1)/2 unordered pairs; cap to avoid duplicates.
    max_pairs = num_devices * (num_devices - 1) // 2
    return min(requested, max_pairs)


def _pick_random_pairs(devices, count):
    """Pick `count` distinct unordered (src, dst) device pairs."""
    seen = set()
    pairs = []
    # Bound the loop so a degenerate RNG cannot spin forever.
    attempts = 0
    while len(pairs) < count and attempts < count * 20:
        attempts += 1
        src, dst = random.sample(devices, 2)
        key = tuple(sorted((src, dst)))
        if key in seen:
            continue
        seen.add(key)
        pairs.append((src, dst))
    return pairs


def main():
    devices = find_sdxi_devices()
    test_index = 1
    test_count = len(TESTS) * len(devices) if devices else len(TESTS)
    cross_pairs = []
    if len(devices) >= 2:
        pair_count = _cross_dev_pair_count(len(devices))
        cross_pairs = _pick_random_pairs(devices, pair_count)
        test_count += len(CROSS_DEV_TESTS) * len(cross_pairs)

    if devices:
        print("Found SDXI devices: %s" % ", ".join(devices))
    else:
        print("No /dev/sdxi* devices found, using round-robin selection")

    print("%d..%d" % (test_index, test_count))
    run_list = devices if devices else [None]

    for dev in run_list:
        for pair in TESTS:
            try:
                if dev:
                    result = subprocess.run([pair[0], "-dev", dev],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                else:
                    result = subprocess.run([pair[0]],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            except FileNotFoundError:
                print("Please run this script in the samples directory after a successful build")
                return

            if result.returncode != 0:
                result_string = "not ok"
            else:
                result_string = "ok"

            description = pair[1] if not dev else "%s - %s" % (dev, pair[1])
            print("%s %d - %s" % (result_string, test_index, description))
            test_index +=1

    # Cross-device tests need two distinct SDXI devices. Randomize the (src,
    # dst) selection so repeated CI runs cover different device pairs over
    # time; SDXI_TEST_XCOPY_PAIRS controls how many pairs to exercise per
    # invocation.
    for src, dst in cross_pairs:
        for prog, desc in CROSS_DEV_TESTS:
            try:
                result = subprocess.run(
                        [prog, "--src-dev", src, "--dst-dev", dst],
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            except FileNotFoundError:
                print("Please run this script in the samples directory after a successful build")
                return

            result_string = "ok" if result.returncode == 0 else "not ok"
            description = "%s -> %s - %s" % (src, dst, desc)
            print("%s %d - %s" % (result_string, test_index, description))
            test_index += 1


if __name__ == "__main__":
    main()
