#!/usr/bin/env python3
"""
run_pipeline.py - Master Pipeline Runner
========================================
Executes the end-to-end Quran PDF extraction and processing pipeline:

  Step 1: Trim cover & end pages (616 -> 610 pages)
  Step 2: Crop borders & perspective micro-deskew (all pages)
  Step 3: Mark 15 horizontal verse lines in red
  Step 4: Normalize Surah & Bismillah headers (P1/P2 + Surahs 3-114)
  Step 5: Invert & enhance 29 white-on-black Juz beginning lines
  Step 6: Export clean page PNGs to studio/pages/

Usage:
  python3 run_pipeline.py --all           # Run all steps sequentially
  python3 run_pipeline.py --resume        # Run pipeline, skipping steps whose output exists
  python3 run_pipeline.py --step <1-6>    # Run a single specific step
  python3 run_pipeline.py --status        # Show current pipeline file status
"""

import sys
import time
import argparse
from pathlib import Path

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(PROJECT_ROOT))

from pipeline.config import (
    RAW_INPUT_PDF, TRIMMED_PDF, CROPPED_PDF, LINES_MARKED_PDF,
    HEADERS_NORMALIZED_PDF, FINAL_MARKED_PDF, STUDIO_PAGES_DIR,
    DB_PATH, P1_ASSET, P2_ASSET
)

from pipeline.step01_trim import trim_pdf
from pipeline.step02_crop_borders import crop_borders
from pipeline.step03_mark_lines import mark_lines
from pipeline.step04_normalize_headers import normalize_headers
from pipeline.step05_invert_juz_headers import invert_juz_headers
from pipeline.step06_extract_pages import extract_pages


STEPS = [
    (1, "Trim PDF (pages 2-611)", TRIMMED_PDF, trim_pdf),
    (2, "Crop Borders & Deskew", CROPPED_PDF, crop_borders),
    (3, "Mark Horizontal Lines", LINES_MARKED_PDF, mark_lines),
    (4, "Normalize Surah/Bismillah Headers", HEADERS_NORMALIZED_PDF, normalize_headers),
    (5, "Invert Negative Juz Beginning Headers", FINAL_MARKED_PDF, invert_juz_headers),
    (6, "Export Clean Page PNGs for Studio", STUDIO_PAGES_DIR, extract_pages)
]


def print_status():
    print("\n=================== PIPELINE STATUS ===================")
    print(f"Raw Input : {'[EXISTS]' if RAW_INPUT_PDF.exists() else '[MISSING]'} {RAW_INPUT_PDF.name}")
    print(f"Database  : {'[EXISTS]' if DB_PATH.exists() else '[MISSING]'} {DB_PATH.name}")
    print(f"P1 Asset  : {'[EXISTS]' if P1_ASSET.exists() else '[MISSING]'} {P1_ASSET.name}")
    print(f"P2 Asset  : {'[EXISTS]' if P2_ASSET.exists() else '[MISSING]'} {P2_ASSET.name}")
    print("-------------------------------------------------------")
    for num, name, path, _ in STEPS:
        if isinstance(path, Path) and path.is_dir():
            count = len(list(path.glob('*.png'))) if path.exists() else 0
            state = f"[EXISTS: {count} PNGs]" if count > 0 else "[MISSING]"
            print(f"Step {num} ({name[:26]:<26}): {state} {path.name}/")
        else:
            exists = path.exists()
            size = f"({path.stat().st_size / 1e6:.1f} MB)" if exists else ""
            state = "[EXISTS] " if exists else "[MISSING]"
            print(f"Step {num} ({name[:26]:<26}): {state} {path.name} {size}")
    print("=======================================================\n")


def run_single_step(step_num: int) -> bool:
    match = [s for s in STEPS if s[0] == step_num]
    if not match:
        print(f"[ERROR] Invalid step number: {step_num}. Must be 1 to 6.")
        return False
    _, name, _, fn = match[0]
    print(f"\n>>> RUNNING STEP {step_num}: {name}")
    t0 = time.time()
    ok = fn()
    print(f">>> STEP {step_num} {'COMPLETED' if ok else 'FAILED'} in {time.time() - t0:.1f}s\n")
    return ok


def run_pipeline(resume: bool = False, force: bool = False):
    print("\n================ STARTING FULL PIPELINE ================")
    t_start = time.time()

    for num, name, out_path, fn in STEPS:
        already_done = False
        if resume and not force:
            if isinstance(out_path, Path) and out_path.is_dir():
                already_done = len(list(out_path.glob('*.png'))) >= 610
            else:
                already_done = out_path.exists() and out_path.stat().st_size > 1000000

        if already_done:
            print(f"[SKIPPING] Step {num}: {name} (Output already exists: {out_path.name})")
            continue

        print(f"\n>>> STEP {num}: {name}")
        t_step = time.time()
        success = fn()
        if not success:
            print(f"[ERROR] Step {num} ({name}) failed! Halting pipeline.")
            return False
        print(f"[✓] Step {num} completed in {time.time() - t_step:.1f}s")
    print(f"\n================ PIPELINE FINISHED IN {time.time() - t_start:.1f}s ================\n")
    print_status()
    return True


def main():
    parser = argparse.ArgumentParser(description="Master runner for the Quran extraction pipeline.")
    parser.add_argument("--all", action="store_true", help="Run all pipeline steps from start to finish.")
    parser.add_argument("--resume", action="store_true", help="Run pipeline, skipping steps whose outputs already exist.")
    parser.add_argument("--step", type=int, choices=range(1, 7), help="Run a single specific step (1-6).")
    parser.add_argument("--status", action="store_true", help="Show current status of all inputs and outputs.")
    args = parser.parse_args()

    if args.status:
        print_status()
        return

    if args.step:
        ok = run_single_step(args.step)
        sys.exit(0 if ok else 1)

    if args.all or args.resume:
        ok = run_pipeline(resume=args.resume, force=args.all)
        sys.exit(0 if ok else 1)

    # If no flags provided, display status and help
    print_status()
    parser.print_help()


if __name__ == '__main__':
    main()
