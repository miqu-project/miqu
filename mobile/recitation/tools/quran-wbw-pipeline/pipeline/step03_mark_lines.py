#!/usr/bin/env python3
"""
Step 3: Horizontal Verse Line Marking
-------------------------------------
Detects the 15 text line intervals on each page via horizontal morphological projection
and draws crisp red divider lines across the entire page width.
"""

import sys
import os
import time
import shutil
import argparse
from pathlib import Path
from itertools import groupby
from operator import itemgetter

import numpy as np
import cv2
import img2pdf
import gi
gi.require_version('Poppler', '0.18')
from gi.repository import Poppler, GLib
import cairo

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))
from pipeline.config import CROPPED_PDF, LINES_MARKED_PDF


def mark_lines(input_path: Path = None, output_path: Path = None) -> bool:
    input_path = Path(input_path or CROPPED_PDF)
    output_path = Path(output_path or LINES_MARKED_PDF)

    if not input_path.exists():
        print(f"[ERROR] Input PDF not found: {input_path}")
        return False

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temp_dir = Path('/tmp/quran_marked_pages')
    if temp_dir.exists():
        shutil.rmtree(temp_dir)
    temp_dir.mkdir(parents=True, exist_ok=True)

    print(f"[*] Marking horizontal verse lines in: {input_path.name}...")
    uri = GLib.filename_to_uri(str(input_path.resolve()), None)
    doc = Poppler.Document.new_from_file(uri, None)
    num_pages = doc.get_n_pages()
    print(f"    Total pages to process: {num_pages}")

    t_start = time.time()
    saved_images = []

    for p in range(num_pages):
        page = doc.get_page(p)
        w, h = page.get_size()
        scale = 150.0 / 72.0
        pw, ph = int(w * scale), int(h * scale)

        surface = cairo.ImageSurface(cairo.FORMAT_RGB24, pw, ph)
        ctx = cairo.Context(surface)
        ctx.scale(scale, scale)
        page.render(ctx)

        buf = surface.get_data()
        img = np.ndarray(shape=(ph, pw, 4), dtype=np.uint8, buffer=buf)[:, :, :3].copy()
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        _, thresh = cv2.threshold(gray, 200, 255, cv2.THRESH_BINARY_INV)

        kernel_len = pw // 3
        h_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (kernel_len, 1))
        lines_img = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, h_kernel)
        proj = np.sum(lines_img, axis=1)
        line_indices = np.where(proj > 0)[0]

        lines = []
        for k, g in groupby(enumerate(line_indices), lambda ix: ix[0] - ix[1]):
            group = list(map(itemgetter(1), g))
            lines.append(int(np.mean(group)))

        if lines:
            diffs = [lines[i+1] - lines[i] for i in range(len(lines)-1)]
            avg_spacing = np.median(diffs) if diffs else ph / 15.0

            if lines[0] > avg_spacing * 0.5:
                lines.insert(0, 4)
            if lines[-1] < ph - avg_spacing * 0.5:
                lines.append(ph - 4)

            row_density = np.sum(thresh, axis=1)
            all_lines = []
            for i in range(len(lines) - 1):
                y_curr = lines[i]
                y_next = lines[i+1]
                all_lines.append(y_curr)
                gap = y_next - y_curr
                num_missing = int(round(gap / avg_spacing)) - 1
                if num_missing > 0:
                    for m in range(1, num_missing + 1):
                        approx_y = int(y_curr + m * (gap / (num_missing + 1)))
                        window = int(avg_spacing * 0.2)
                        search_start = max(0, approx_y - window)
                        search_end = min(ph, approx_y + window)
                        sub_density = row_density[search_start:search_end]
                        min_val = np.min(sub_density)
                        zero_indices = np.where(sub_density == min_val)[0]
                        best_y = search_start + int(np.mean(zero_indices))
                        all_lines.append(best_y)

            all_lines.append(lines[-1])
            all_lines = sorted(list(set(all_lines)))

            thickness = 6 if pw > 2000 else 4
            for y in all_lines:
                y_clamped = max(thickness // 2 + 1, min(ph - thickness // 2 - 1, y))
                cv2.line(img, (0, y_clamped), (pw, y_clamped), (0, 0, 255), thickness)

        out_img_path = str(temp_dir / f"page_{p+1:04d}.jpg")
        cv2.imwrite(out_img_path, img, [int(cv2.IMWRITE_JPEG_QUALITY), 90])
        saved_images.append(out_img_path)

        if (p + 1) % 50 == 0 or (p + 1) == num_pages:
            elapsed = time.time() - t_start
            speed = (p + 1) / elapsed
            print(f"    Progress: {p+1}/{num_pages} pages ({speed:.2f} pages/s, elapsed: {elapsed:.1f}s)")

    print(f"[*] Compiling marked pages into PDF: {output_path.name}...")
    t_pdf = time.time()
    with open(str(output_path), "wb") as f:
        f.write(img2pdf.convert(saved_images))

    if temp_dir.exists():
        shutil.rmtree(temp_dir)

    out_mb = output_path.stat().st_size / 1e6
    print(f"[✓] Marked PDF saved: {output_path} ({num_pages} pages, {out_mb:.1f} MB)")
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Step 3: Mark horizontal verse divider lines in red.")
    parser.add_argument("-i", "--input", default=str(CROPPED_PDF))
    parser.add_argument("-o", "--output", default=str(LINES_MARKED_PDF))
    args = parser.parse_args()

    success = mark_lines(Path(args.input), Path(args.output))
    sys.exit(0 if success else 1)
