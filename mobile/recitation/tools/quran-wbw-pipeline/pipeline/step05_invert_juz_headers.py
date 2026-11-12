#!/usr/bin/env python3
"""
Step 5: Juz Beginning Negative Header Inversion & Typography Enhancement
-----------------------------------------------------------------------
Inverts the 29 white-on-black negative Juz headers (pages 22, 42, 62, 82, etc.)
to dark-text-on-white, applies a gamma curve to darken the ink core, and thickens
strokes with morphological erosion to match normal body lines.
"""

import sys
import os
import io
import time
import argparse
from pathlib import Path
from itertools import groupby
from operator import itemgetter

import cv2
import numpy as np
import pikepdf
import img2pdf

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))
from pipeline.config import HEADERS_NORMALIZED_PDF, FINAL_MARKED_PDF

JUZ_PAGES = [
    22, 42, 62, 82, 102, 122, 142, 162, 182, 202,
    222, 242, 262, 282, 302, 322, 342, 362, 382, 402,
    422, 442, 462, 482, 502, 522, 542, 562, 586
]


def enhance_text(inv_box):
    """Darken stroke core and thicken text strokes by ~1-2px using morphological erosion."""
    gray = cv2.cvtColor(inv_box, cv2.COLOR_BGR2GRAY)
    
    # 1. Gamma curve to darken midtones and ink core
    gamma_val = 2.2
    lut = np.array([min(255, int(255.0 * (i / 255.0) ** gamma_val)) if i < 225 else 255 for i in range(256)], dtype=np.uint8)
    darkened = cv2.LUT(gray, lut)
    
    # 2. Morphological erosion with 2x2 kernel to add ~1-2px thickness to calligraphy
    kernel2 = np.ones((2, 2), np.uint8)
    thickened = cv2.erode(darkened, kernel2, iterations=1)
    
    # 3. Clean pure white background
    thickened[thickened >= 215] = 255
    res = cv2.cvtColor(thickened, cv2.COLOR_GRAY2BGR)
    
    # 4. Clear outer 3 border pixels to ensure 100% seamless blend with surrounding white slot
    res[:3, :] = [255, 255, 255]
    res[-3:, :] = [255, 255, 255]
    res[:, :3] = [255, 255, 255]
    res[:, -3:] = [255, 255, 255]
    return res


def process_page(img_cv, p_num):
    h, w, _ = img_cv.shape

    # 1. Detect red divider lines
    b, g, r = img_cv[:, :, 0], img_cv[:, :, 1], img_cv[:, :, 2]
    red_mask = (r > 180) & (b < 70) & (g < 70)
    red_rows = np.where(np.sum(red_mask, axis=1) > w * 0.4)[0]
    raw_lines = []
    for k, g_iter in groupby(enumerate(red_rows), lambda ix: ix[0]-ix[1]):
        group = list(map(itemgetter(1), g_iter))
        raw_lines.append((min(group), max(group)))

    # Use grid spacing to pick the 16 divider lines robustly
    step = (raw_lines[-1][0] - raw_lines[0][0]) / 15.0
    lines = []
    for i in range(16):
        target_y = raw_lines[0][0] + i * step
        closest = min(raw_lines, key=lambda l: abs(l[0] - target_y))
        lines.append(closest)
    lines = sorted(list(set(lines)), key=lambda l: l[0])
    assert len(lines) == 16, f"Page {p_num}: expected 16 red divider lines, got {len(lines)}"

    # 2. Determine target line slot: Line 3 for Surah openings, Line 1 for all others
    s_idx = 3 if p_num in [282, 322, 342, 502, 542, 562, 586] else 1
    y_top = lines[s_idx - 1][1] + 1
    y_bot = lines[s_idx][0]

    slot = img_cv[y_top:y_bot, :].copy()
    gray = cv2.cvtColor(slot, cv2.COLOR_BGR2GRAY)

    # 3. Detect solid black banner bounding box
    is_dark = (gray < 40)
    dark_rows = np.where(np.mean(is_dark, axis=1) > 0.50)[0]
    r_runs = []
    for k, g_iter in groupby(enumerate(dark_rows), lambda ix: ix[0]-ix[1]):
        group = list(map(itemgetter(1), g_iter))
        r_runs.append((group[0], group[-1], len(group)))
    r_runs.sort(key=lambda x: x[2], reverse=True)
    r_top, r_bot, r_len = r_runs[0]

    banner_slice = gray[r_top:r_bot+1, :]
    col_dark = np.mean(banner_slice < 40, axis=0)
    valid_cols = np.where(col_dark[30:2705] > 0.70)[0] + 30
    c_left, c_right = valid_cols[0], valid_cols[-1]

    # Inset slightly inside banner
    r_top_in = r_top + 3
    r_bot_in = r_bot - 3
    c_left_in = c_left + 4
    c_right_in = c_right - 4

    # 4. Canvas with pure white slot
    res = img_cv.copy()
    res[y_top:y_bot, :] = 255

    # 5. Extract, invert, darken & thicken
    box = slot[r_top_in:r_bot_in+1, c_left_in:c_right_in+1]
    inv_box = 255 - box
    enhanced_box = enhance_text(inv_box)

    res[y_top+r_top_in : y_top+r_bot_in+1, c_left_in:c_right_in+1] = enhanced_box
    return res


def invert_juz_headers(input_path: Path = None, output_path: Path = None) -> bool:
    input_path = Path(input_path or HEADERS_NORMALIZED_PDF)
    output_path = Path(output_path or FINAL_MARKED_PDF)

    if not input_path.exists():
        print(f"[ERROR] Input PDF not found: {input_path}")
        return False

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temp_pdf = output_path.with_suffix('.tmp.pdf')

    print(f"[*] Inverting {len(JUZ_PAGES)} negative Juz headers in {input_path.name}...")
    pdf = pikepdf.open(str(input_path))
    assert len(pdf.pages) == 610, f"Expected 610 pages, found {len(pdf.pages)}"

    t_start = time.time()
    for idx, p_num in enumerate(JUZ_PAGES, 1):
        p_idx = p_num - 1
        page = pdf.pages[p_idx]
        img_name = list(page.get_images().keys())[0]
        pim = pikepdf.PdfImage(page.get_images()[img_name])
        img_orig = cv2.cvtColor(np.array(pim.as_pil_image()), cv2.COLOR_RGB2BGR)

        res = process_page(img_orig, p_num)

        _, enc = cv2.imencode('.jpg', res, [int(cv2.IMWRITE_JPEG_QUALITY), 95])
        single_pdf = pikepdf.open(io.BytesIO(img2pdf.convert(enc.tobytes())))
        pdf.pages[p_idx] = single_pdf.pages[0]
        print(f"    [{idx:2d}/{len(JUZ_PAGES)}] Inverted Juz beginning: Page {p_num}")

    print(f"[*] Saving updated document to {output_path.name}...")
    pdf.save(str(temp_pdf))
    pdf.close()

    temp_pdf.replace(output_path)
    out_mb = output_path.stat().st_size / 1e6
    print(f"[✓] Final marked PDF saved: {output_path} ({out_mb:.1f} MB)")
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Step 5: Invert negative Juz beginnings.")
    parser.add_argument("-i", "--input", default=str(HEADERS_NORMALIZED_PDF))
    parser.add_argument("-o", "--output", default=str(FINAL_MARKED_PDF))
    args = parser.parse_args()

    success = invert_juz_headers(Path(args.input), Path(args.output))
    sys.exit(0 if success else 1)
