#!/usr/bin/env python3
"""
Step 4: Surah Header & Bismillah Normalization
----------------------------------------------
1. Replaces Page 1 (Al-Fatihah) and Page 2 (Al-Baqarah) with standard rectangular assets.
2. For all remaining 112 Surahs across pages 3-610:
   - Clears ornate Surah heading banners and draws centered 'surah_name'.
   - Clears ornate Bismillah heading banners and draws centered 'bismillah'.
"""

import sys
import os
import io
import time
import sqlite3
import argparse
from pathlib import Path
from collections import defaultdict
from itertools import groupby
from operator import itemgetter

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import pikepdf
import img2pdf

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))
from pipeline.config import (
    LINES_MARKED_PDF, HEADERS_NORMALIZED_PDF, DB_PATH,
    P1_ASSET, P2_ASSET, FONT_PATH
)


def normalize_headers(input_path: Path = None, output_path: Path = None) -> bool:
    input_path = Path(input_path or LINES_MARKED_PDF)
    output_path = Path(output_path or HEADERS_NORMALIZED_PDF)

    if not input_path.exists():
        print(f"[ERROR] Input PDF not found: {input_path}")
        return False
    if not DB_PATH.exists():
        print(f"[ERROR] Lines database not found: {DB_PATH}")
        return False

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temp_pdf = output_path.with_suffix('.tmp.pdf')

    print(f"[*] Connecting to lines database: {DB_PATH.name}...")
    conn = sqlite3.connect(str(DB_PATH))
    cursor = conn.cursor()
    query = (
        "SELECT page_number, line_number, line_type, surah_number, surah_name "
        "FROM lines WHERE line_type IN ('surah_name', 'basmallah') "
        "AND surah_number >= 3 ORDER BY page_number, line_number;"
    )
    cursor.execute(query)
    targets = cursor.fetchall()
    conn.close()

    pages_dict = defaultdict(list)
    for p_num, l_num, l_type, s_num, s_name in targets:
        text = "surah_name" if l_type == 'surah_name' else "bismillah"
        pages_dict[p_num].append((l_num, text, l_type))

    print(f"[*] Opening PDF with pikepdf: {input_path.name}...")
    pdf = pikepdf.open(str(input_path))
    assert len(pdf.pages) == 610, f"Expected 610 pages, found {len(pdf.pages)}"

    # Part A: Replace Page 1 and Page 2
    if P1_ASSET.exists() and P2_ASSET.exists():
        print(f"[*] Replacing Page 1 and Page 2 with custom assets...")
        p1_pdf = pikepdf.open(io.BytesIO(img2pdf.convert(str(P1_ASSET))))
        p2_pdf = pikepdf.open(io.BytesIO(img2pdf.convert(str(P2_ASSET))))
        pdf.pages[0] = p1_pdf.pages[0]
        pdf.pages[1] = p2_pdf.pages[0]
    else:
        print(f"[!] Warning: P1 or P2 asset not found in {P1_ASSET.parent}, skipping P1/P2 replacement")

    # Part B: Process remaining Surah headers (Surah 3-114)
    total_pages_to_process = len(pages_dict)
    print(f"[*] Normalizing {total_pages_to_process} pages for Surahs 3-114...")

    t_start = time.time()
    processed_count = 0

    for p_num in sorted(pages_dict.keys()):
        p_idx = p_num - 1
        page = pdf.pages[p_idx]

        images = page.get_images()
        img_name = list(images.keys())[0]
        pim = pikepdf.PdfImage(images[img_name])
        pil_orig = pim.as_pil_image()
        img_cv = cv2.cvtColor(np.array(pil_orig), cv2.COLOR_RGB2BGR)
        h, w, _ = img_cv.shape

        b, g, r = img_cv[:, :, 0], img_cv[:, :, 1], img_cv[:, :, 2]
        red_mask = (r > 180) & (b < 70) & (g < 70)
        red_row_counts = np.sum(red_mask, axis=1)
        red_rows = np.where(red_row_counts > w * 0.4)[0]

        line_centers = []
        for k, g_iter in groupby(enumerate(red_rows), lambda ix: ix[0] - ix[1]):
            group = list(map(itemgetter(1), g_iter))
            line_centers.append((min(group), max(group), int(np.mean(group))))

        pil_canvas = Image.fromarray(cv2.cvtColor(img_cv, cv2.COLOR_BGR2RGB))
        draw = ImageDraw.Draw(pil_canvas)

        for l_num, text, l_type in pages_dict[p_num]:
            s_top, e_top, _ = line_centers[l_num - 1]
            s_bot, e_bot, _ = line_centers[l_num]

            clear_top = e_top + 1
            clear_bot = s_bot
            draw.rectangle([(0, clear_top), (w, clear_bot - 1)], fill=(255, 255, 255))

            slot_h = clear_bot - clear_top
            font_size = int(round(slot_h * 0.36))
            font = ImageFont.truetype(FONT_PATH, font_size)

            bbox = draw.textbbox((0, 0), text, font=font)
            tw = bbox[2] - bbox[0]
            th = bbox[3] - bbox[1]
            tx = (w - tw) // 2 - bbox[0]
            ty = clear_top + (slot_h - th) // 2 - bbox[1]

            draw.text((tx, ty), text, fill=(0, 0, 0), font=font)

        buf = io.BytesIO()
        pil_canvas.save(buf, format='JPEG', quality=92)
        single_pdf_bytes = img2pdf.convert(buf.getvalue())
        single_pdf = pikepdf.open(io.BytesIO(single_pdf_bytes))
        pdf.pages[p_idx] = single_pdf.pages[0]

        processed_count += 1
        if processed_count % 25 == 0 or processed_count == total_pages_to_process:
            elapsed = time.time() - t_start
            print(f"    Normalized {processed_count}/{total_pages_to_process} pages ({processed_count/elapsed:.1f} p/s, elapsed: {elapsed:.1f}s)")

    print(f"[*] Saving updated document to {output_path.name}...")
    pdf.save(str(temp_pdf))
    pdf.close()

    temp_pdf.replace(output_path)
    out_mb = output_path.stat().st_size / 1e6
    print(f"[✓] Normalized headers saved: {output_path} ({out_mb:.1f} MB)")
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Step 4: Standardize Surah and Bismillah header boxes.")
    parser.add_argument("-i", "--input", default=str(LINES_MARKED_PDF))
    parser.add_argument("-o", "--output", default=str(HEADERS_NORMALIZED_PDF))
    args = parser.parse_args()

    success = normalize_headers(Path(args.input), Path(args.output))
    sys.exit(0 if success else 1)
