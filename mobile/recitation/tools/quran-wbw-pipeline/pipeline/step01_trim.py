#!/usr/bin/env python3
"""
Step 1: Trim PDF
----------------
Removes the cover page (page 1) and the back informational pages (last 5 pages)
from the original 616-page scan to leave pages 1 through 610.
"""

import sys
import subprocess
import argparse
from pathlib import Path

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))
from pipeline.config import RAW_INPUT_PDF, TRIMMED_PDF


def trim_pdf(input_path: Path = None, output_path: Path = None) -> bool:
    input_path = Path(input_path or RAW_INPUT_PDF)
    output_path = Path(output_path or TRIMMED_PDF)

    if not input_path.exists():
        print(f"[ERROR] Raw input PDF not found: {input_path}")
        return False

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = output_path.with_suffix('.tmp.pdf')

    print(f"[*] Trimming pages 2-611 from {input_path.name}...")
    cmd = [
        'qpdf', '--empty',
        '--pages', str(input_path), '2-611', '--',
        str(temp_path)
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[ERROR] qpdf trimming failed:\n{res.stderr}")
        if temp_path.exists():
            temp_path.unlink()
        return False

    # Linearize / finalize
    lin_cmd = ['qpdf', '--linearize', str(temp_path), str(output_path)]
    subprocess.run(lin_cmd, check=True)
    if temp_path.exists():
        temp_path.unlink()

    print(f"[✓] Trimmed PDF created: {output_path} (610 pages, {output_path.stat().st_size / 1e6:.1f} MB)")
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Step 1: Strip cover and back pages to produce 610 pages.")
    parser.add_argument("-i", "--input", default=str(RAW_INPUT_PDF), help="Input PDF path")
    parser.add_argument("-o", "--output", default=str(TRIMMED_PDF), help="Output PDF path")
    args = parser.parse_args()

    success = trim_pdf(Path(args.input), Path(args.output))
    sys.exit(0 if success else 1)
