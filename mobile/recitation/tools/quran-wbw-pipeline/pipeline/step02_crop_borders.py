#!/usr/bin/env python3
"""
Step 2: Border Cropping & Micro-Deskewing
-----------------------------------------
1. Detects outer border rectangle on each page using adaptive contour analysis.
2. Perspective-warps content to a flat rectangle.
3. Micro-deskew pass: corrects minute tilt (< 3°) using horizontal text divider lines.
4. Cleans thin border remnants hugging outer margins.
5. Repacks all pages into a new PDF using img2pdf.
"""

import sys
import os
import argparse
import glob
import subprocess
import tempfile
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor

import cv2
import numpy as np
from PIL import Image
import img2pdf

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))
from pipeline.config import TRIMMED_PDF, CROPPED_PDF, DEFAULT_DPI, DEFAULT_QUALITY, DEFAULT_WORKERS
def longest_run(arr):
    """Longest contiguous run of non-zero values in a 1-D array."""
    best, cur = 0, 0
    for v in arr:
        if v > 0:
            cur += 1
            best = max(best, cur)
        else:
            cur = 0
    return best


def scan_from_edges(binary, h, w):
    """
    Scan inward from each edge strictly within outer 35% margin zones.
    Bridges small breaks in lines using morphological closing.
    """
    max_x = int(w * 0.35)
    min_x_right = int(w * 0.65)
    max_y = int(h * 0.35)
    min_y_bot = int(h * 0.65)

    kernel_v = cv2.getStructuringElement(cv2.MORPH_RECT, (1, 15))
    closed_v = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, kernel_v)

    kernel_h = cv2.getStructuringElement(cv2.MORPH_RECT, (15, 1))
    closed_h = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, kernel_h)

    for thresh in [0.40, 0.30, 0.20, 0.15]:
        min_h = thresh * h
        min_w = thresh * w

        left_x  = next((x for x in range(0, max_x) if longest_run(closed_v[:, x]) >= min_h), None)
        right_x = next((x for x in range(w - 1, min_x_right, -1) if longest_run(closed_v[:, x]) >= min_h), None)
        top_y   = next((y for y in range(0, max_y) if longest_run(closed_h[y, :]) >= min_w), None)
        bot_y   = next((y for y in range(h - 1, min_y_bot, -1) if longest_run(closed_h[y, :]) >= min_w), None)

        if None not in [left_x, right_x, top_y, bot_y]:
            if (right_x - left_x) >= 0.50 * w and (bot_y - top_y) >= 0.50 * h:
                return left_x, right_x, top_y, bot_y, thresh

    return None, None, None, None, None


def fit_hline(gray, y_center, band=25, dark=200):
    """Fit y = slope*x + intercept through dark pixels in a horizontal band."""
    y0 = max(0, y_center - band)
    y1 = min(gray.shape[0] - 1, y_center + band)
    pts = np.column_stack(np.where(gray[y0:y1+1, :] < dark))
    if len(pts) < 20:
        return 0.0, float(y_center)
    c = np.polyfit(pts[:, 1].astype(float), (pts[:, 0] + y0).astype(float), 1)
    return float(c[0]), float(c[1])


def fit_vline(gray, x_center, band=25, dark=200):
    """Fit x = slope*y + intercept through dark pixels in a vertical band."""
    x0 = max(0, x_center - band)
    x1 = min(gray.shape[1] - 1, x_center + band)
    pts = np.column_stack(np.where(gray[:, x0:x1+1] < dark))
    if len(pts) < 20:
        return 0.0, float(x_center)
    c = np.polyfit(pts[:, 0].astype(float), (pts[:, 1] + x0).astype(float), 1)
    return float(c[0]), float(c[1])


def intersect(hs, hb, vs, vb):
    """Intersect y = hs*x + hb  with  x = vs*y + vb. Returns (x, y) or None."""
    d = 1.0 - hs * vs
    if abs(d) < 1e-9:
        return None
    y = (hs * vb + hb) / d
    x = vs * y + vb
    return (float(x), float(y))


def detect_border_corners(gray):
    """
    Detect the 4 corners of the outer border rectangle.
    Returns (corners np.float32 (4,2), skew_deg, used_threshold) or (None, 0, None).
    """
    h, w = gray.shape
    binary = cv2.threshold(gray, 200, 255, cv2.THRESH_BINARY_INV)[1]

    left_x, right_x, top_y, bot_y, used_thresh = scan_from_edges(binary, h, w)

    if None in [left_x, right_x, top_y, bot_y]:
        return None, 0.0, None

    top_hs,  top_hb   = fit_hline(gray, top_y)
    bot_hs,  bot_hb   = fit_hline(gray, bot_y)
    left_vs, left_vb  = fit_vline(gray, left_x)
    right_vs, right_vb = fit_vline(gray, right_x)

    tl = intersect(top_hs, top_hb, left_vs, left_vb)
    tr = intersect(top_hs, top_hb, right_vs, right_vb)
    br = intersect(bot_hs, bot_hb, right_vs, right_vb)
    bl = intersect(bot_hs, bot_hb, left_vs, left_vb)

    if None in [tl, tr, br, bl]:
        return None, 0.0, used_thresh

    tol = max(h, w) * 0.10
    for pt in [tl, tr, br, bl]:
        if pt[0] < -tol or pt[0] > w + tol or pt[1] < -tol or pt[1] > h + tol:
            return None, 0.0, used_thresh

    box_w = tr[0] - tl[0]
    box_h = bl[1] - tl[1]
    if box_w < 0.50 * w or box_h < 0.50 * h:
        return None, 0.0, used_thresh

    corners = np.array([tl, tr, br, bl], dtype=np.float32)
    skew = float(np.degrees(np.arctan2(tr[1] - tl[1], tr[0] - tl[0])))
    return corners, skew, used_thresh


# ---------------------------------------------------------------------------
# Fine-tuning: micro-deskew using internal line dividers
# ---------------------------------------------------------------------------

def micro_deskew_dividers(warped_img):
    """
    Detects internal horizontal line dividers across the 15 lines.
    Calculates residual tilt and rotates slightly to make them perfectly horizontal.
    """
    h, w = warped_img.shape[:2]
    gray = cv2.cvtColor(warped_img, cv2.COLOR_BGR2GRAY)
    binary = cv2.threshold(gray, 200, 255, cv2.THRESH_BINARY_INV)[1]

    # Exclude outer 5% margin to prevent border edge lines from interfering
    margin_x = int(w * 0.05)
    margin_y = int(h * 0.05)
    inner_bin = binary[margin_y:h - margin_y, margin_x:w - margin_x]

    # Horizontal filter to extract divider lines
    kernel_len = int(w * 0.35)
    h_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (kernel_len, 1))
    h_lines = cv2.morphologyEx(inner_bin, cv2.MORPH_OPEN, h_kernel)

    # Detect lines with fine angular resolution (1/7200 rad)
    lines = cv2.HoughLinesP(h_lines, 1, np.pi / 7200, threshold=100,
                             minLineLength=int(w * 0.35), maxLineGap=40)

    angles = []
    if lines is not None:
        for l in lines:
            x1, y1, x2, y2 = l.flatten()
            dx = x2 - x1
            dy = y2 - y1
            ang = np.degrees(np.arctan2(dy, dx))
            if abs(ang) < 3.0:  # should be very close to horizontal
                angles.append((ang, np.sqrt(dx * dx + dy * dy)))

    if len(angles) >= 5:
        weights = [a[1] for a in angles]
        deg_vals = [a[0] for a in angles]
        tilt_deg = float(np.average(deg_vals, weights=weights))

        if abs(tilt_deg) > 0.02:
            center = (w / 2.0, h / 2.0)
            M = cv2.getRotationMatrix2D(center, tilt_deg, 1.0)
            deskewed = cv2.warpAffine(warped_img, M, (w, h), flags=cv2.INTER_LANCZOS4,
                                      borderMode=cv2.BORDER_CONSTANT, borderValue=(255, 255, 255))
            return deskewed, tilt_deg

    return warped_img, 0.0


def clean_edge_slivers(img, max_margin_px=14):
    """
    Safely in-paints thin vertical border remnants hugging the left and right outer boundaries
    without any cropping or risk to text content.
    """
    cleaned = img.copy()
    h, w = cleaned.shape[:2]
    gray = cv2.cvtColor(cleaned, cv2.COLOR_BGR2GRAY)

    # Left edge cleaning
    left_cut_x = 0
    for x in range(max_margin_px):
        col = gray[:, x]
        if col.mean() < 215 or np.sum(col < 150) > 0.15 * h:
            left_cut_x = x + 1
        else:
            break

    if left_cut_x > 0:
        cleaned[:, :left_cut_x] = 255

    # Right edge cleaning
    right_cut_x = 0
    for x in range(max_margin_px):
        col = gray[:, w - 1 - x]
        if col.mean() < 215 or np.sum(col < 150) > 0.15 * h:
            right_cut_x = x + 1
        else:
            break

    if right_cut_x > 0:
        cleaned[:, w - right_cut_x:] = 255

    return cleaned, left_cut_x, right_cut_x


# ---------------------------------------------------------------------------
# Per-page processing
# ---------------------------------------------------------------------------

def process_page(ppm_path, page_num, debug=False, debug_dir=None):
    """
    Process a single rasterised page.
    Returns (PIL Image, status_str).
    """
    img_bgr = cv2.imread(ppm_path)
    if img_bgr is None:
        raise RuntimeError(f"Cannot read: {ppm_path}")

    h, w = img_bgr.shape[:2]
    gray = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)

    corners, skew, used_thresh = detect_border_corners(gray)

    if corners is not None:
        tl, tr, br, bl = corners
        out_w = int(max(np.linalg.norm(tr - tl), np.linalg.norm(br - bl)))
        out_h = int(max(np.linalg.norm(bl - tl), np.linalg.norm(br - tr)))
        out_w = max(100, min(out_w, w * 2))
        out_h = max(100, min(out_h, h * 2))

        dst = np.array([[0, 0], [out_w - 1, 0], [out_w - 1, out_h - 1], [0, out_h - 1]], dtype=np.float32)
        M = cv2.getPerspectiveTransform(corners, dst)
        warped = cv2.warpPerspective(img_bgr, M, (out_w, out_h),
                                     flags=cv2.INTER_LANCZOS4,
                                     borderMode=cv2.BORDER_CONSTANT,
                                     borderValue=(255, 255, 255))

        # Pass 2: micro-deskew using internal line dividers
        final_img, divider_tilt = micro_deskew_dividers(warped)

        # Standardize: stretch/squeeze to uniform target dimension (1748 x 2908 px at 200 DPI)
        TARGET_W, TARGET_H = 1748, 2908
        if final_img.shape[1] != TARGET_W or final_img.shape[0] != TARGET_H:
            final_img = cv2.resize(final_img, (TARGET_W, TARGET_H), interpolation=cv2.INTER_LANCZOS4)

        # Apply specific edge cut: top=20px, bottom=18px, left=18px, right=18px
        cut_top = 20
        cut_bot = 18
        cut_left = 18
        cut_right = 18
        trimmed_content = final_img[cut_top:TARGET_H - cut_bot, cut_left:TARGET_W - cut_right]

        # Rescale trimmed content back to the exact uniform page dimensions
        final_img = cv2.resize(trimmed_content, (TARGET_W, TARGET_H), interpolation=cv2.INTER_LANCZOS4)

        # Pass 3: In-paint any occasional remaining edge slivers on left/right margins to pure white
        final_img, lx_clean, rx_clean = clean_edge_slivers(final_img, max_margin_px=14)

        if debug and debug_dir:
            vis = img_bgr.copy()
            pts = corners.astype(int)
            cv2.polylines(vis, [pts.reshape(-1, 1, 2)], True, (0, 0, 255), 6)
            for pt in pts:
                cv2.circle(vis, tuple(pt), 20, (0, 255, 0), -1)
                cv2.circle(vis, tuple(pt), 22, (0, 0, 0), 3)
            cv2.imwrite(os.path.join(debug_dir, f'p{page_num:04d}_corners.jpg'), vis)
            cv2.imwrite(os.path.join(debug_dir, f'p{page_num:04d}_final.jpg'), final_img)

        result_rgb = cv2.cvtColor(final_img, cv2.COLOR_BGR2RGB)
        status = f"thresh={used_thresh:.0%} border_skew={skew:+.2f}° divider_adj={divider_tilt:+.2f}° cleaned=[L:{lx_clean},R:{rx_clean}] size={TARGET_W}x{TARGET_H}"
    else:
        # Fallback: simple fixed-percentage crop and stretch to target size
        TARGET_W, TARGET_H = 1748, 2908
        pad = 0.08
        y0, y1 = int(h * pad), int(h * (1 - pad))
        x0, x1 = int(w * pad), int(w * (1 - pad))
        cropped = img_bgr[y0:y1, x0:x1]
        resized = cv2.resize(cropped, (TARGET_W, TARGET_H), interpolation=cv2.INTER_LANCZOS4)
        result_rgb = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
        status = f"FALLBACK size={TARGET_W}x{TARGET_H}"

    return Image.fromarray(result_rgb), status


def _worker_process_page(args_tuple):
    ppm_path, page_num, debug, debug_dir, quality, img_path = args_tuple
    pil_img, status = process_page(ppm_path, page_num, debug=debug, debug_dir=debug_dir)
    pil_img.save(img_path, 'JPEG', quality=quality, optimize=True, subsampling=0)
    return page_num, status, img_path


# ---------------------------------------------------------------------------
# Main pipeline
# ---------------------------------------------------------------------------


def crop_borders(input_path: Path = None, output_path: Path = None,
                 dpi: int = DEFAULT_DPI, quality: int = DEFAULT_QUALITY,
                 workers: int = DEFAULT_WORKERS, pages: str = None,
                 debug: bool = False) -> bool:
    input_path = Path(input_path or TRIMMED_PDF)
    output_path = Path(output_path or CROPPED_PDF)

    if not input_path.exists():
        print(f"[ERROR] Input PDF not found: {input_path}")
        return False

    output_path.parent.mkdir(parents=True, exist_ok=True)

    info = subprocess.check_output(['pdfinfo', str(input_path)]).decode()
    total = int([l for l in info.splitlines() if l.startswith('Pages:')][0].split()[1])
    print(f"[*] Processing borders for: {input_path.name} ({total} pages)")

    if pages:
        ps, pe = (pages.split('-', 1) + [pages])[:2]
        page_start, page_end = int(ps), int(pe)
    else:
        page_start, page_end = 1, total

    print(f"    Range : pages {page_start}–{page_end}  DPI={dpi}  quality={quality}  workers={workers}")

    debug_dir = None
    if debug:
        debug_dir = str(output_path.parent / 'debug_crops')
        os.makedirs(debug_dir, exist_ok=True)

    fallback_pages = []

    with tempfile.TemporaryDirectory(prefix='quran_crop_') as tmpdir:
        ppm_dir = os.path.join(tmpdir, 'ppm')
        img_dir = os.path.join(tmpdir, 'imgs')
        os.makedirs(ppm_dir); os.makedirs(img_dir)

        print(f"[*] Rasterising pages {page_start}–{page_end}...")
        subprocess.run(['pdftoppm', '-r', str(dpi),
                        '-f', str(page_start), '-l', str(page_end),
                        str(input_path), os.path.join(ppm_dir, 'p')],
                       check=True, capture_output=True)

        ppm_files = sorted(glob.glob(os.path.join(ppm_dir, 'p-*.ppm')))
        n = len(ppm_files)
        print(f"[*] Rasterised {n} pages. Running border cropping...")

        tasks = []
        for i, ppm_path in enumerate(ppm_files):
            page_num = page_start + i
            img_path = os.path.join(img_dir, f'p{page_num:05d}.jpg')
            tasks.append((ppm_path, page_num, debug, debug_dir, quality, img_path))

        img_paths = [t[-1] for t in tasks]

        if workers > 1:
            with ProcessPoolExecutor(max_workers=workers) as executor:
                for idx, (p_num, status, _) in enumerate(executor.map(_worker_process_page, tasks), start=1):
                    pct = idx / n * 100
                    if idx % 20 == 0 or idx == n:
                        print(f"    [{pct:5.1f}%] page {p_num:4d}… {status}", flush=True)
                    if 'FALLBACK' in status:
                        fallback_pages.append(p_num)
        else:
            for idx, task in enumerate(tasks, start=1):
                p_num, status, _ = _worker_process_page(task)
                pct = idx / n * 100
                if idx % 20 == 0 or idx == n:
                    print(f"    [{pct:5.1f}%] page {p_num:4d}… {status}", flush=True)
                if 'FALLBACK' in status:
                    fallback_pages.append(p_num)

        print(f"[*] Packing {len(img_paths)} pages into PDF...")
        with open(str(output_path), 'wb') as f:
            f.write(img2pdf.convert(img_paths))

    in_mb  = input_path.stat().st_size / 1e6
    out_mb = output_path.stat().st_size / 1e6
    print(f"[✓] Output saved: {output_path} ({len(img_paths)} pages, {in_mb:.1f} MB → {out_mb:.1f} MB)")
    if fallback_pages:
        print(f"    [!] Fallback pages (check manually): {fallback_pages}")
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Step 2: Crop borders & micro-deskew pages.")
    parser.add_argument("-i", "--input", default=str(TRIMMED_PDF))
    parser.add_argument("-o", "--output", default=str(CROPPED_PDF))
    parser.add_argument("--dpi", type=int, default=DEFAULT_DPI)
    parser.add_argument("--quality", type=int, default=DEFAULT_QUALITY)
    parser.add_argument("--workers", type=int, default=DEFAULT_WORKERS)
    parser.add_argument("--pages", type=str, default=None)
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    success = crop_borders(Path(args.input), Path(args.output),
                           dpi=args.dpi, quality=args.quality,
                           workers=args.workers, pages=args.pages,
                           debug=args.debug)
    sys.exit(0 if success else 1)
