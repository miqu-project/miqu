#!/usr/bin/env python3
"""
Step 6: Export Clean Page PNGs for Interactive Studio
------------------------------------------------------
Extracts all 610 marked pages from the final PDF as high-resolution PNGs
into the studio/pages/ directory for live inspection and word marking.
"""

import sys
import os
import time
import argparse
from pathlib import Path
from multiprocessing import Pool
import cv2
import numpy as np
import pikepdf

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))
from pipeline.config import FINAL_MARKED_PDF, STUDIO_PAGES_DIR, DEFAULT_WORKERS

_worker_pdf = None
_worker_pdf_path = None
_worker_out_dir = None


def init_worker(pdf_path, out_dir):
    global _worker_pdf, _worker_pdf_path, _worker_out_dir
    _worker_pdf_path = pdf_path
    _worker_out_dir = out_dir
    _worker_pdf = pikepdf.open(str(_worker_pdf_path))


def extract_page(p_num: int) -> int:
    global _worker_pdf, _worker_out_dir
    out_path = Path(_worker_out_dir) / f"page_{p_num:03d}.png"
    if out_path.exists() and out_path.stat().st_size > 100000:
        return p_num

    page = _worker_pdf.pages[p_num - 1]
    images = page.get_images()
    img_name = list(images.keys())[0]
    pim = pikepdf.PdfImage(images[img_name])
    img = cv2.cvtColor(np.array(pim.as_pil_image()), cv2.COLOR_RGB2BGR)
    cv2.imwrite(str(out_path), img, [cv2.IMWRITE_PNG_COMPRESSION, 1])
    return p_num


def extract_pages(input_path: Path = None, output_dir: Path = None,
                  workers: int = DEFAULT_WORKERS) -> bool:
    input_path = Path(input_path or FINAL_MARKED_PDF)
    output_dir = Path(output_dir or STUDIO_PAGES_DIR)

    if not input_path.exists():
        print(f"[ERROR] Input PDF not found: {input_path}")
        return False

    output_dir.mkdir(parents=True, exist_ok=True)
    pdf = pikepdf.open(str(input_path))
    num_pages = len(pdf.pages)
    pdf.close()

    print(f"[*] Extracting {num_pages} clean pages from {input_path.name} to {output_dir.name}/ using {workers} workers...")
    t0 = time.time()
    completed = 0

    with Pool(processes=workers, initializer=init_worker, initargs=(input_path, output_dir)) as pool:
        for p_num in pool.imap_unordered(extract_page, range(1, num_pages + 1)):
            completed += 1
            if completed % 50 == 0 or completed == num_pages:
                elapsed = time.time() - t0
                rate = completed / max(0.01, elapsed)
                print(f"    Progress: {completed}/{num_pages} pages ({rate:.1f} p/s, elapsed: {elapsed:.1f}s)")

    total_time = time.time() - t0
    print(f"[✓] All {num_pages} pages extracted in {total_time:.1f}s ({total_time/60:.2f} mins) to {output_dir}")
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Step 6: Export page PNGs to studio/pages/")
    parser.add_argument("-i", "--input", default=str(FINAL_MARKED_PDF))
    parser.add_argument("-o", "--output", default=str(STUDIO_PAGES_DIR))
    parser.add_argument("--workers", type=int, default=DEFAULT_WORKERS)
    args = parser.parse_args()

    success = extract_pages(Path(args.input), Path(args.output), args.workers)
    sys.exit(0 if success else 1)
