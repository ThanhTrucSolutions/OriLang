#!/usr/bin/env python3
"""Golden output tests for OriLang beginner samples 27–29.

Runs each sample through the `ori` interpreter and compares the
actual output against the expected golden output.

Usage:
    python3 scripts/test_golden.py
    
Expected output for beginner_27_even_odd.ori:
    n=7
    2n=14
    greater than five
    less than ten
    4/2=2

Expected output for beginner_28_absolute_diff.ori:
    12-5=7
    abs=7
    9-3=6

Expected output for beginner_29_double_value.ori:
    n=6
    double=12
    double is over ten
    half=6
    half matches n
"""

import subprocess
import sys
from pathlib import Path

SAMPLES_DIR = Path(__file__).resolve().parent.parent / "samples"
ORI_BIN = "ori"  # Assumes `ori` is on PATH, or override via env

GOLDEN = {
    "beginner_27_even_odd.ori": [
        "n=7",
        "2n=14",
        "greater than five",
        "less than ten",
        "4/2=2",
    ],
    "beginner_28_absolute_diff.ori": [
        "12-5=7",
        "abs=7",
        "9-3=6",
    ],
    "beginner_29_double_value.ori": [
        "n=6",
        "double=12",
        "double is over ten",
        "half=6",
        "half matches n",
    ],
}


def run_sample(path: Path) -> str:
    result = subprocess.run(
        [ORI_BIN, str(path)],
        capture_output=True,
        text=True,
        timeout=10,
    )
    if result.returncode != 0:
        print(f"  STDOUT: {result.stdout}")
        print(f"  STDERR: {result.stderr}")
        result.check_returncode()
    return result.stdout.strip()


def main() -> int:
    failures = 0

    for sample_name, expected_lines in GOLDEN.items():
        sample_path = SAMPLES_DIR / sample_name
        if not sample_path.exists():
            print(f"❌ {sample_name}: file not found at {sample_path}")
            failures += 1
            continue

        print(f"Testing {sample_name}...", end=" ")
        actual = run_sample(sample_path)
        actual_lines = [l.strip() for l in actual.split("\n") if l.strip()]

        if actual_lines == expected_lines:
            print("✅ PASS")
        else:
            print("❌ FAIL")
            print(f"  Expected ({len(expected_lines)} lines):")
            for line in expected_lines:
                print(f"    {line}")
            print(f"  Got ({len(actual_lines)} lines):")
            for line in actual_lines:
                print(f"    {line}")
            failures += 1

    print()
    if failures:
        print(f"{failures} test(s) FAILED")
        return 1
    else:
        print("All golden output tests PASSED")
        return 0


if __name__ == "__main__":
    sys.exit(main())
