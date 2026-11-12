#!/usr/bin/env python3
"""
mark_pdf_words.py
-----------------
Automated Word-by-Word Vertical Divider Line Marker for 15-Line Quran PDF.

Uses:
- Baseline Core Vertical Projection
- PAU (Part-of-Arabic-Word) count guidance from quran_lines.db
- Dynamic Programming global alignment
- Snapping to cleanest local whitespace valleys
- Royal Blue (#0078FF / BGR (255, 100, 0), thickness 4px) vertical dividers
- Multiprocessing (4 cores) to process all 610 pages in parallel
- Safe atomic PDF replacement with verification
"""

import os
import io
import time
import shutil
import sqlite3
import unicodedata
import json
from pathlib import Path
from itertools import groupby
from operator import itemgetter
from multiprocessing import Pool, cpu_count

import cv2
import numpy as np
import pikepdf
import img2pdf

PDF_PATH = '/home/anisur/projects/quran-extract/15_lines_ind_pak_hifz_cropped_marked.pdf'
BACKUP_PATH = '/home/anisur/projects/quran-extract/15_lines_ind_pak_hifz_cropped_marked_backup.pdf'
TEMP_PDF_PATH = '/home/anisur/projects/quran-extract/15_lines_ind_pak_hifz_cropped_marked_temp.pdf'
DB_PATH = '/home/anisur/projects/quran-extract/quran_lines.db'
OVERRIDES_PATH = '/home/anisur/extract_quran/cuts_override.json'
TEMP_DIR = '/tmp/quran_word_marked_pages'

WORD_COLOR = (255, 100, 0) # Royal Blue in BGR
THICKNESS = 4

NON_CONNECTING = {
    'ا', 'أ', 'إ', 'آ', 'ٱ', 'ء',
    'د', 'ذ', 'ڈ', 'ڌ', 'ڍ', 'ڎ', 'ڏ', 'ڐ',
    'ر', 'ز', 'ڑ', 'ږ', 'ڕ', 'ڙ', 'ژ',
    'و', 'ؤ', 'ۄ', 'ۅ', 'ۆ', 'ۇ', 'ۈ', 'ۉ', 'ۋ',
}

def count_paus(token):
    base_chars = [c for c in token if unicodedata.category(c) in ('Lo', 'Lm')]
    if not base_chars:
        return 1
    pau_count = 0
    curr_len = 0
    for ch in token:
        if unicodedata.category(ch) in ('Lo', 'Lm'):
            curr_len += 1
            if ch in NON_CONNECTING:
                pau_count += 1
                curr_len = 0
    if curr_len > 0:
        pau_count += 1
    return max(1, pau_count)

def get_word_weight(text):
    has_letters = any(unicodedata.category(c) in ('Lo', 'Lm') for c in text)
    if not has_letters:
        return 1.8 # pure ayah marker circle
        
    has_stop = any(ch in text for ch in ['ۙ', 'ۚ', 'ۖ', 'ۗ', 'ۘ', 'ؗ', 'ؕ', '۠', 'ۛ', '۬'])
    letters = [ch for ch in text if unicodedata.category(ch) in ('Lo', 'Lm')]
    letters_str = ''.join(letters)
    if letters_str in ('لله', 'اللّٰه', 'الله', 'للّٰه'):
        return 2.5 + (0.5 if has_stop else 0.0)
    
    weights = {'ا': 0.85, 'د': 1.25, 'ذ': 1.25, 'ر': 1.25, 'ز': 1.25, 'و': 1.25,
               'ب': 1.8, 'ت': 1.8, 'ث': 1.8, 'س': 2.5, 'ش': 2.5, 'ص': 2.4, 'ض': 2.4,
               'ط': 2.0, 'ظ': 2.0, 'ك': 1.8, 'ف': 1.8, 'ق': 1.8, 'ل': 1.4, 'م': 1.4,
               'ن': 1.4, 'ه': 1.4, 'ي': 1.4, 'ى': 1.4, 'ئ': 1.4, 'ج': 1.6, 'ح': 1.6,
               'خ': 1.6, 'ع': 1.6, 'غ': 1.6}
    base = sum(weights.get(ch, 1.4) for ch in letters)
    if letters and letters[-1] in 'بتثنىي':
        base += 0.5
    return max(1.0, float(base) + (0.5 if has_stop else 0.0))

def find_line_cuts_pau(slot, words_rtl):
    h, w = slot.shape[:2]
    gray = cv2.cvtColor(slot, cv2.COLOR_BGR2GRAY)
    _, bin_inv = cv2.threshold(gray, 200, 255, cv2.THRESH_BINARY_INV)
    bin_inv[:6, :] = 0
    bin_inv[-6:, :] = 0
    bin_inv[:, :25] = 0
    bin_inv[:, -25:] = 0
    
    K = len(words_rtl)
    if K <= 1:
        return []
    
    words_ltr = list(reversed(words_rtl))
    w_weights = [get_word_weight(wd) for wd in words_ltr]
    w_paus = [count_paus(wd) for wd in words_ltr]
    total_weight = sum(w_weights)
    
    h_proj = np.sum(bin_inv > 0, axis=1)
    if np.sum(h_proj) == 0:
        step = w / K
        return [int(round(i * step)) for i in range(1, K)]
        
    base_y = np.argmax(h_proj)
    core = bin_inv[max(0, base_y-50):min(h, base_y+35), :]
    v_core = np.sum(core > 0, axis=0)
    v_full = np.sum(bin_inv > 0, axis=0)
    
    ink_cols = np.where(v_full > 0)[0]
    if len(ink_cols) == 0:
        step = w / K
        return [int(round(i * step)) for i in range(1, K)]
        
    x_min, x_max = ink_cols[0], ink_cols[-1]
    span = x_max - x_min + 1
    
    valleys = []
    zero_core = np.where(v_core == 0)[0]
    for k, g_iter in groupby(enumerate(zero_core), lambda ix: ix[0]-ix[1]):
        group = list(map(itemgetter(1), g_iter))
        c_x = (group[0] + group[-1]) // 2
        width = len(group)
        full_val = np.min(v_full[group[0]:group[-1]+1])
        valleys.append((c_x, width, full_val))
        
    for x in range(x_min + 15, x_max - 15):
        if v_full[x] <= 15 and v_full[x] <= v_full[x-1] and v_full[x] <= v_full[x+1]:
            if not any(abs(v[0] - x) < 8 for v in valleys):
                valleys.append((x, 1, v_full[x]))
                
    valleys = [v for v in valleys if x_min + 15 < v[0] < x_max - 15]
    valleys.sort(key=lambda v: v[0])
    
    if len(valleys) < K - 1:
        step = span / K
        return [int(round(x_min + i * step)) for i in range(1, K)]
        
    cands = [x_min] + [v[0] for v in valleys] + [x_max]
    cand_info = {v[0]: v for v in valleys}
    cand_info[x_min] = (x_min, 10, 0)
    cand_info[x_max] = (x_max, 10, 0)
    M = len(cands)
    
    exp_w = [span * (wt / total_weight) for wt in w_weights]
    
    dp = {(0, 0): (0.0, -1)}
    
    for k in range(1, K + 1):
        target_w = exp_w[k - 1]
        for i in range(k, M - (K - k)):
            best = (float('inf'), -1)
            x_curr = cands[i]
            v_curr = cand_info[x_curr]
            v_width, v_ink = v_curr[1], v_curr[2]
            v_bonus = min(20, v_width) * 3.0 - v_ink * 2.0
            
            for j in range(k - 1, i):
                if (j, k - 1) in dp:
                    x_prev = cands[j]
                    seg_w = x_curr - x_prev
                    if seg_w < target_w * 0.4 or seg_w > target_w * 2.2:
                        continue
                    
                    ratio = seg_w / max(1.0, target_w)
                    w_cost = ((ratio - 1.0) ** 2) * 50.0
                    exp_valleys = max(0, w_paus[k - 1] - 1)
                    act_valleys = i - j - 1
                    pau_cost = abs(act_valleys - exp_valleys) * 15.0
                    cost = dp[(j, k - 1)][0] + w_cost + pau_cost - v_bonus
                    if cost < best[0]:
                        best = (cost, j)
            if best[1] != -1:
                dp[(i, k)] = best
                
    best_last = (float('inf'), -1)
    target_w = exp_w[K - 1]
    for j in range(K - 1, M - 1):
        if (j, K - 1) in dp:
            seg_w = cands[-1] - cands[j]
            ratio = seg_w / max(1.0, target_w)
            w_cost = ((ratio - 1.0) ** 2) * 50.0
            exp_valleys = max(0, w_paus[K - 1] - 1)
            act_valleys = (M - 1) - j - 1
            pau_cost = abs(act_valleys - exp_valleys) * 15.0
            cost = dp[(j, K - 1)][0] + w_cost + pau_cost
            if cost < best_last[0]:
                best_last = (cost, j)
                
    if best_last[1] == -1:
        step = span / K
        return [int(round(x_min + i * step)) for i in range(1, K)]
        
    cuts_idx = []
    curr_j = best_last[1]
    curr_k = K - 1
    while curr_k > 0:
        cuts_idx.append(curr_j)
        curr_j = dp[(curr_j, curr_k)][1]
        curr_k -= 1
        
    cuts_idx.reverse()
    return [cands[idx] for idx in cuts_idx]

def process_single_page(args):
    p_num, page_lines, overrides_dict, temp_dir = args
    pdf = pikepdf.open(PDF_PATH)
    page = pdf.pages[p_num - 1]
    
    img_name = list(page.images.keys())[0]
    pim = pikepdf.PdfImage(page.images[img_name])
    img = cv2.cvtColor(np.array(pim.as_pil_image()), cv2.COLOR_RGB2BGR)
    h, w, _ = img.shape
    
    b, g, r = img[:, :, 0], img[:, :, 1], img[:, :, 2]
    red_rows = np.where(np.sum((r > 180) & (b < 70) & (g < 70), axis=1) > w * 0.4)[0]
    line_bands = []
    for k, g_iter in groupby(enumerate(red_rows), lambda ix: ix[0]-ix[1]):
        group = list(map(itemgetter(1), g_iter))
        line_bands.append((min(group), max(group)))
        
    num_dividers = len(line_bands)
    
    # Process each ayah line
    for l_num, l_type, wc, words_str in page_lines:
        if l_type != 'ayah' or not words_str or wc <= 1:
            continue
            
        # Determine slot index in line_bands
        if num_dividers == 16:
            # Standard 15-line page
            s_idx = l_num
        elif num_dividers == 9:
            # Pages 1 and 2: slots correspond to lines 8 to 15
            s_idx = l_num - 7
        elif num_dividers == 11 and p_num == 610:
            # Page 610: slots correspond to lines 1 to 10
            s_idx = l_num
        else:
            # Fallback
            s_idx = l_num
            
        if s_idx < 1 or s_idx >= len(line_bands):
            continue
            
        y_top = line_bands[s_idx - 1][1] + 1
        y_bot = line_bands[s_idx][0] - 1
        if y_bot <= y_top:
            continue
            
        slot = img[y_top:y_bot+1, :]
        words_rtl = words_str.split(' | ')
        
        # Check if manual override exists
        key = f"page_{p_num:03d}_line_{l_num:02d}"
        if key in overrides_dict and overrides_dict[key].get('approved'):
            truth_1208 = overrides_dict[key]['cuts']
            v_proj = np.sum(cv2.threshold(cv2.cvtColor(slot, cv2.COLOR_BGR2GRAY), 200, 255, cv2.THRESH_BINARY_INV)[1] > 0, axis=0)
            snapped_cuts = []
            for tc in truth_1208:
                approx_x = int(round(40 + (tc - 10) / (1208 - 20) * (2680 - 40)))
                x1 = max(0, approx_x - 30)
                x2 = min(w - 1, approx_x + 30)
                sub = v_proj[x1:x2+1]
                min_v = np.min(sub)
                zeros = np.where(sub == min_v)[0] + x1
                best_x = zeros[np.argmin(np.abs(zeros - approx_x))]
                snapped_cuts.append(int(best_x))
            cuts = snapped_cuts
        else:
            cuts = find_line_cuts_pau(slot, words_rtl)
            
        # Draw vertical lines
        for cx in cuts:
            cv2.line(img, (cx, y_top), (cx, y_bot), WORD_COLOR, THICKNESS)
            
    out_img_path = os.path.join(temp_dir, f"page_{p_num:04d}.jpg")
    cv2.imwrite(out_img_path, img, [int(cv2.IMWRITE_JPEG_QUALITY), 92])
    return p_num

def main():
    print(f"Loading database {DB_PATH}...")
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("SELECT page_number, line_number, line_type, word_count, words FROM lines ORDER BY page_number, line_number")
    all_lines = c.fetchall()
    conn.close()
    
    pages_dict = {}
    for p, l, t, wc, ws in all_lines:
        pages_dict.setdefault(p, []).append((l, t, wc, ws))
        
    overrides_dict = {}
    if os.path.exists(OVERRIDES_PATH):
        try:
            with open(OVERRIDES_PATH) as f:
                overrides_dict = json.load(f)
        except Exception:
            pass
            
    if os.path.exists(TEMP_DIR):
        shutil.rmtree(TEMP_DIR)
    os.makedirs(TEMP_DIR, exist_ok=True)
    
    num_pages = 610
    tasks = [(p, pages_dict.get(p, []), overrides_dict, TEMP_DIR) for p in range(1, num_pages + 1)]
    
    workers = min(4, cpu_count())
    print(f"Starting parallel processing of {num_pages} pages across {workers} workers...")
    t_start = time.time()
    
    completed = 0
    with Pool(processes=workers) as pool:
        for p_num in pool.imap_unordered(process_single_page, tasks):
            completed += 1
            if completed % 25 == 0 or completed == num_pages:
                elapsed = time.time() - t_start
                rate = completed / elapsed
                remaining = (num_pages - completed) / max(0.01, rate)
                print(f"Progress: {completed}/{num_pages} pages ({rate:.1f} pages/s, elapsed: {elapsed:.1f}s, remaining: {remaining:.1f}s)")
                
    print(f"All {num_pages} pages marked in {time.time() - t_start:.1f}s!")
    
    # Compile images into PDF
    print(f"Compiling marked pages into temporary PDF: {TEMP_PDF_PATH}...")
    img_files = [os.path.join(TEMP_DIR, f"page_{p:04d}.jpg") for p in range(1, num_pages + 1)]
    t_pdf = time.time()
    with open(TEMP_PDF_PATH, "wb") as f:
        f.write(img2pdf.convert(img_files))
    print(f"Compiled PDF in {time.time() - t_pdf:.1f}s!")
    
    # Verify PDF
    print("Verifying temporary PDF...")
    verify = pikepdf.open(TEMP_PDF_PATH)
    assert len(verify.pages) == num_pages, f"Expected {num_pages} pages, found {len(verify.pages)}"
    verify.close()
    print(f"Verification PASSED ({num_pages} pages).")
    
    # Backup original
    if not os.path.exists(BACKUP_PATH):
        print(f"Creating backup of original PDF to {BACKUP_PATH}...")
        shutil.copyfile(PDF_PATH, BACKUP_PATH)
        print("Backup created.")
        
    # Replace
    print(f"Atomically updating {PDF_PATH}...")
    os.replace(TEMP_PDF_PATH, PDF_PATH)
    print("Clean up temp images...")
    shutil.rmtree(TEMP_DIR)
    
    total_time = time.time() - t_start
    print(f"SUCCESS! Finished entire Quran word marking in {total_time:.1f}s ({total_time/60:.2f} mins).")

if __name__ == '__main__':
    main()
