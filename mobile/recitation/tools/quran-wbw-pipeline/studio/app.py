#!/usr/bin/env python3
"""
review_studio.py - Interactive Quran Word Verification & Slicing Studio
======================================================================
Page-by-page review application for manual verification and fine-tuning
of word-by-word vertical divider marks.

Run:
    python3 review_studio.py [--port 8080] [--pdf path] [--page 3]
"""

import os
import io
import sys
import json
import time
import base64
import sqlite3
import argparse
from pathlib import Path
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
from itertools import groupby
from operator import itemgetter

import cv2
import numpy as np
import pikepdf
import unicodedata

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))
from pipeline.config import (
    STUDIO_PAGES_DIR, STUDIO_MODIFIED_DIR, STUDIO_CUTS_PATH,
    DB_PATH, FINAL_MARKED_PDF
)

PAGES_DIR = STUDIO_PAGES_DIR
MODIFIED_DIR = STUDIO_MODIFIED_DIR
APPROVED_CUTS_PATH = STUDIO_CUTS_PATH
WORD_COLOR = (255, 100, 0)
THICKNESS = 4

NON_CONNECTING = {
    'ا', 'أ', 'إ', 'آ', 'ٱ', 'ء',
    'د', 'ذ', 'ڈ', 'ڌ', 'ڍ', 'ڎ', 'ڏ', 'ڐ',
    'ر', 'ز', 'ڑ', 'ږ', 'ڕ', 'ڙ', 'ژ',
    'و', 'ؤ', 'ۄ', 'ۅ', 'ۆ', 'ۇ', 'ۈ', 'ۉ', 'ۋ',
}

_pdf_doc = None

def get_pdf():
    global _pdf_doc
    if _pdf_doc is None:
        _pdf_doc = pikepdf.open(str(PDF_PATH))
    return _pdf_doc

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
        return 1.8
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

def load_approved_cuts():
    if APPROVED_CUTS_PATH.exists():
        try:
            with open(APPROVED_CUTS_PATH, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {}
    return {}

def save_approved_cuts(data):
    with open(APPROVED_CUTS_PATH, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

def get_page_data(p_num: int):
    if p_num < 1 or p_num > 610:
        return None
        
    conn = sqlite3.connect(f"file:{DB_PATH}?mode=ro", uri=True)
    c = conn.cursor()
    c.execute(
        "SELECT line_number, line_type, surah_number, surah_name, word_count, words "
        "FROM lines WHERE page_number=? ORDER BY line_number", (p_num,)
    )
    raw_lines = c.fetchall()
    conn.close()
    
    page_path = PAGES_DIR / f"page_{p_num:03d}.png"
    if not page_path.exists():
        return None
    img = cv2.imread(str(page_path))
    if img is None:
        return None
    h, w, _ = img.shape
    
    # Red horizontal dividers
    b, g, r = img[:, :, 0], img[:, :, 1], img[:, :, 2]
    red_rows = np.where(np.sum((r > 180) & (b < 70) & (g < 70), axis=1) > w * 0.4)[0]
    line_bands = []
    for k, g_iter in groupby(enumerate(red_rows), lambda ix: ix[0]-ix[1]):
        group = list(map(itemgetter(1), g_iter))
        line_bands.append((min(group), max(group)))
        
    num_dividers = len(line_bands)
    overrides = load_approved_cuts()
    
    page_approved = True
    surah_name_display = ""
    lines_output = []
    
    for l_num, l_type, s_num, s_name, wc, words_str in raw_lines:
        if s_name and not surah_name_display:
            surah_name_display = s_name
            
        if num_dividers == 16:
            s_idx = l_num
        elif num_dividers == 9:
            s_idx = l_num - 7
        elif num_dividers == 11 and p_num == 610:
            s_idx = l_num
        else:
            s_idx = l_num
            
        slot_valid = (1 <= s_idx < len(line_bands))
        if slot_valid:
            y_top = line_bands[s_idx - 1][1] + 1
            y_bot = line_bands[s_idx][0] - 1
        else:
            y_top, y_bot = 0, 0
            
        words_rtl = words_str.split(' | ') if words_str else []
        words_ltr = list(reversed(words_rtl))
        
        cuts = []
        line_ink_profile = []
        is_line_approved = False
        img_b64 = ""
        
        if slot_valid and y_bot > y_top:
            slot = img[y_top:y_bot+1, :]
            sh, sw = slot.shape[:2]
            
            # Compress line image for transmission
            _, enc = cv2.imencode('.jpg', slot, [int(cv2.IMWRITE_JPEG_QUALITY), 88])
            img_b64 = "data:image/jpeg;base64," + base64.b64encode(enc.tobytes()).decode('ascii')
            
            gray = cv2.cvtColor(slot, cv2.COLOR_BGR2GRAY)
            _, bin_inv = cv2.threshold(gray, 200, 255, cv2.THRESH_BINARY_INV)
            bin_inv[:6, :] = 0
            bin_inv[-6:, :] = 0
            v_full = np.sum(bin_inv > 0, axis=0)
            line_ink_profile = v_full.tolist()
            
            key = f"page_{p_num:03d}_line_{l_num:02d}"
            if key in overrides:
                item = overrides[key]
                is_line_approved = item.get('approved', False)
                raw_cuts = item.get('cuts', [])
                if raw_cuts and max(raw_cuts) < 1400:
                    # Scale from 1208 space
                    scaled_cuts = []
                    for tc in raw_cuts:
                        approx_x = int(round(40 + (tc - 10) / (1208 - 20) * (2680 - 40)))
                        sub = v_full[max(0, approx_x-30):min(sw-1, approx_x+30)+1]
                        best_x = np.where(sub == np.min(sub))[0][0] + max(0, approx_x-30)
                        scaled_cuts.append(int(best_x))
                    cuts = scaled_cuts
                else:
                    cuts = raw_cuts
            else:
                if l_type == 'ayah' and wc > 1:
                    cuts = find_line_cuts_pau(slot, words_rtl)
                    is_line_approved = False
                    page_approved = False
                    
        if l_type == 'ayah' and wc > 1 and not is_line_approved:
            page_approved = False
            
        lines_output.append({
            "line_number": l_num,
            "line_type": l_type,
            "surah_name": s_name or "",
            "word_count": wc,
            "words": words_rtl,
            "words_ltr": words_ltr,
            "width": w if slot_valid else 0,
            "height": (y_bot - y_top + 1) if slot_valid else 0,
            "image_b64": img_b64,
            "cuts": cuts,
            "ink_profile": line_ink_profile,
            "approved": is_line_approved
        })
        
    return {
        "page_number": p_num,
        "total_pages": 610,
        "surah_name": surah_name_display,
        "is_saved": (MODIFIED_DIR / f"page_{p_num:03d}.png").exists(),
        "is_approved": page_approved,
        "lines": lines_output
    }


HTML_CONTENT = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Quran Word Segmentation Studio</title>
<style>
  :root {
    --bg-main: #0f172a;
    --bg-card: #1e293b;
    --bg-card-hover: #26354a;
    --border: #334155;
    --text-primary: #f8fafc;
    --text-secondary: #94a3b8;
    --accent: #38bdf8;
    --accent-hover: #0ea5e9;
    --cut-blue: #0078ff;
    --cut-green: #10b981;
    --cut-red: #ef4444;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg-main);
    color: var(--text-primary);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    user-select: none;
    overflow-x: hidden;
  }
  header {
    position: sticky;
    top: 0;
    z-index: 1000;
    background: rgba(15, 23, 42, 0.95);
    backdrop-filter: blur(8px);
    border-bottom: 1px solid var(--border);
    padding: 10px 24px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4);
  }
  .header-left, .header-center, .header-right {
    display: flex;
    align-items: center;
    gap: 12px;
  }
  .brand-title {
    font-size: 16px;
    font-weight: 700;
    color: var(--accent);
    letter-spacing: 0.5px;
  }
  .btn {
    background: var(--bg-card);
    border: 1px solid var(--border);
    color: var(--text-primary);
    padding: 6px 14px;
    border-radius: 6px;
    font-size: 13px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.15s ease;
    display: inline-flex;
    align-items: center;
    gap: 6px;
  }
  .btn:hover { background: var(--bg-card-hover); border-color: var(--text-secondary); }
  .btn-primary { background: #0284c7; border-color: #38bdf8; }
  .btn-primary:hover { background: #0369a1; }
  .btn-success { background: #059669; border-color: #34d399; }
  .btn-success:hover { background: #047857; }
  .badge {
    padding: 4px 10px;
    border-radius: 9999px;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.5px;
    text-transform: uppercase;
  }
  .badge-approved { background: rgba(16, 185, 129, 0.2); color: #34d399; border: 1px solid #059669; }
  .badge-pending { background: rgba(245, 158, 11, 0.2); color: #fbbf24; border: 1px solid #d97706; }
  .page-input {
    width: 60px;
    text-align: center;
    background: var(--bg-card);
    border: 1px solid var(--border);
    color: var(--text-primary);
    padding: 4px 8px;
    border-radius: 6px;
    font-weight: 700;
    font-size: 14px;
  }
  main {
    max-width: 1440px;
    margin: 20px auto;
    padding: 0 20px 80px 20px;
    display: flex;
    flex-direction: column;
    gap: 18px;
  }
  .line-card {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 8px;
    overflow: hidden;
    box-shadow: 0 4px 8px rgba(0,0,0,0.25);
  }
  .line-header {
    background: rgba(30, 41, 59, 0.8);
    border-bottom: 1px solid var(--border);
    padding: 8px 16px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    font-size: 12px;
    font-weight: 600;
    color: var(--text-secondary);
  }
  .line-header .title {
    color: var(--text-primary);
    font-size: 13px;
  }
  .interactive-canvas-wrap {
    position: relative;
    width: 100%;
    background: #ffffff;
    cursor: default;
    touch-action: none;
  }
  .line-img {
    width: 100%;
    display: block;
    pointer-events: none;
  }
  .cut-overlay {
    position: absolute;
    top: 0; left: 0; width: 100%; height: 100%;
    pointer-events: none;
  }
  .cut-handle {
    position: absolute;
    top: 0; bottom: 0;
    width: 10px;
    margin-left: -5px;
    cursor: ew-resize;
    pointer-events: auto;
    z-index: 10;
  }
  .cut-handle::after {
    content: '';
    position: absolute;
    top: 0; bottom: 0;
    left: 4px;
    width: 3px;
    background: var(--cut-blue);
    box-shadow: 0 0 4px rgba(0, 120, 255, 0.8);
    transition: background 0.1s ease;
  }
  .cut-handle.green::after {
    background: var(--cut-green);
    box-shadow: 0 0 5px rgba(16, 185, 129, 0.9);
  }
  .cut-handle.red::after {
    background: var(--cut-red);
    box-shadow: 0 0 6px rgba(239, 68, 68, 1);
  }
  .cut-handle:hover::after, .cut-handle.dragging::after {
    width: 5px;
    left: 3px;
  }
  .cut-handle .coord-tooltip {
    position: absolute;
    bottom: -22px;
    left: 50%;
    transform: translateX(-50%);
    background: rgba(15, 23, 42, 0.9);
    color: #ffffff;
    font-size: 10px;
    padding: 2px 5px;
    border-radius: 4px;
    pointer-events: none;
    white-space: nowrap;
    display: none;
    z-index: 20;
  }
  .cut-handle:hover .coord-tooltip, .cut-handle.dragging .coord-tooltip {
    display: block;
  }
  .chips-strip {
    display: flex;
    flex-direction: row-reverse;
    background: #0b1324;
    border-top: 1px solid var(--border);
    padding: 6px 12px;
    gap: 8px;
    overflow-x: auto;
  }
  .chip {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 4px 8px;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 2px;
    min-width: 60px;
    flex: 1;
  }
  .chip-arabic {
    font-family: "Scheherazade New", "Amiri", "Traditional Arabic", serif;
    font-size: 18px;
    direction: rtl;
    color: #e2e8f0;
  }
  .chip-meta {
    font-size: 10px;
    color: var(--text-secondary);
    font-weight: 600;
  }
  .banner-box {
    padding: 20px;
    text-align: center;
    background: #0f172a;
    color: var(--text-secondary);
    font-style: italic;
    font-size: 13px;
  }
  .floating-tools {
    position: fixed;
    bottom: 20px;
    right: 30px;
    z-index: 1000;
    display: flex;
    gap: 12px;
  }
  .hotkey-legend {
    font-size: 11px;
    color: var(--text-secondary);
    display: flex;
    gap: 12px;
    align-items: center;
  }
  .kbd {
    background: #334155;
    padding: 2px 6px;
    border-radius: 4px;
    color: #f1f5f9;
    font-family: monospace;
    font-weight: 700;
  }
</style>
</head>
<body>

<header>
  <div class="header-left">
    <span class="brand-title">QURAN SLICER STUDIO</span>
    <button class="btn" onclick="prevPage()">&#8592; Prev</button>
    <div style="display: flex; align-items: center; gap: 4px;">
      <span>Page</span>
      <input type="number" id="pageInput" class="page-input" min="1" max="610" value="3" onchange="jumpPage(this.value)">
      <span>/ 610</span>
    </div>
    <button class="btn" onclick="nextPage()">Next &#8594;</button>
    <span id="pageBadge" class="badge badge-pending">PENDING</span>
  </div>

  <div class="header-center">
    <div class="hotkey-legend">
      <span><span class="kbd">Enter</span> Approve & Next</span>
      <span><span class="kbd">&#8592; / &#8594;</span> Nudge Cut 1px</span>
      <span><span class="kbd">Double-Click</span> Auto-Snap</span>
    </div>
  </div>

  <div class="header-right">
    <button class="btn" onclick="snapAllLines()">Auto-Snap All</button>
    <button class="btn btn-success" onclick="approveAndNext()">✓ Approve & Next (Enter)</button>
  </div>
</header>

<main id="mainContainer">
  <div style="text-align: center; padding: 100px; color: var(--text-secondary);">Loading page data...</div>
</main>

<script>
let currentPage = 3;
let pageData = null;
let activeDrag = null; // { lineIndex, cutIndex, handleEl, wrapEl, imgW }

async function loadPage(p) {
  currentPage = parseInt(p);
  document.getElementById("pageInput").value = currentPage;
  const container = document.getElementById("mainContainer");
  container.innerHTML = `<div style="text-align: center; padding: 100px; color: var(--text-secondary);">Loading Page ${currentPage}...</div>`;

  try {
    const res = await fetch(`/api/page?p=${currentPage}`);
    pageData = await res.json();
    renderPage();
  } catch (err) {
    container.innerHTML = `<div style="text-align: center; padding: 100px; color: #ef4444;">Failed to load Page ${currentPage}: ${err}</div>`;
  }
}

function renderPage() {
  const container = document.getElementById("mainContainer");
  container.innerHTML = "";

  const badge = document.getElementById("pageBadge");
  if (pageData.is_approved) {
    badge.className = "badge badge-approved";
    badge.textContent = "✓ APPROVED";
  } else {
    badge.className = "badge badge-pending";
    badge.textContent = "⏳ NEEDS REVIEW";
  }

  pageData.lines.forEach((line, lIdx) => {
    const card = document.createElement("div");
    card.className = "line-card";

    const header = document.createElement("div");
    header.className = "line-header";
    header.innerHTML = `
      <span class="title">Line ${line.line_number}: ${line.surah_name || pageData.surah_name}</span>
      <span>${line.line_type.toUpperCase()} | ${line.word_count} Tokens | ${line.cuts.length} Cuts</span>
    `;
    card.appendChild(header);

    if (line.line_type === "ayah" && line.image_b64) {
      const wrap = document.createElement("div");
      wrap.className = "interactive-canvas-wrap";
      wrap.id = `wrap_${lIdx}`;

      const img = document.createElement("img");
      img.className = "line-img";
      img.src = line.image_b64;
      wrap.appendChild(img);

      const overlay = document.createElement("div");
      overlay.className = "cut-overlay";
      overlay.id = `overlay_${lIdx}`;

      line.cuts.forEach((cutX, cIdx) => {
        const handle = createCutHandle(lIdx, cIdx, cutX, line.width, line.ink_profile);
        overlay.appendChild(handle);
      });

      wrap.appendChild(overlay);
      card.appendChild(wrap);

      // Chips strip
      const chipsStrip = document.createElement("div");
      chipsStrip.className = "chips-strip";
      chipsStrip.id = `chips_${lIdx}`;
      renderChips(chipsStrip, line);
      card.appendChild(chipsStrip);

    } else {
      const banner = document.createElement("div");
      banner.className = "banner-box";
      banner.textContent = `[ ${line.line_type.toUpperCase()} - Banner Slot (No cuts needed) ]`;
      card.appendChild(banner);
    }

    container.appendChild(card);
  });
}

function createCutHandle(lIdx, cIdx, cutX, imgW, inkProfile) {
  const handle = document.createElement("div");
  handle.className = "cut-handle";
  handle.dataset.line = lIdx;
  handle.dataset.cut = cIdx;

  const pct = (cutX / imgW) * 100;
  handle.style.left = `${pct}%`;

  // Collision state
  const inkVal = inkProfile ? (inkProfile[Math.round(cutX)] || 0) : 0;
  if (inkVal === 0) {
    handle.classList.add("green");
  } else if (inkVal > 25) {
    handle.classList.add("red");
  }

  const tip = document.createElement("div");
  tip.className = "coord-tooltip";
  tip.textContent = `${Math.round(cutX)}px (Ink: ${inkVal})`;
  handle.appendChild(tip);

  // Dragging logic
  handle.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    handle.setPointerCapture(e.pointerId);
    handle.classList.add("dragging");
    const wrap = document.getElementById(`wrap_${lIdx}`);
    activeDrag = { lineIndex: lIdx, cutIndex: cIdx, handle, wrap, imgW, inkProfile };
  });

  handle.addEventListener("pointermove", (e) => {
    if (!activeDrag || activeDrag.handle !== handle) return;
    const rect = activeDrag.wrap.getBoundingClientRect();
    let relX = e.clientX - rect.left;
    relX = Math.max(10, Math.min(rect.width - 10, relX));
    const pxX = (relX / rect.width) * activeDrag.imgW;

    pageData.lines[activeDrag.lineIndex].cuts[activeDrag.cutIndex] = Math.round(pxX);
    updateHandlePos(handle, pxX, activeDrag.imgW, activeDrag.inkProfile);
    renderChips(document.getElementById(`chips_${activeDrag.lineIndex}`), pageData.lines[activeDrag.lineIndex]);
  });

  handle.addEventListener("pointerup", (e) => {
    if (activeDrag && activeDrag.handle === handle) {
      handle.releasePointerCapture(e.pointerId);
      handle.classList.remove("dragging");
      activeDrag = null;
    }
  });

  // Double click to auto-snap to nearest zero
  handle.addEventListener("dblclick", () => {
    snapCut(lIdx, cIdx);
  });

  return handle;
}

function updateHandlePos(handle, pxX, imgW, inkProfile) {
  const pct = (pxX / imgW) * 100;
  handle.style.left = `${pct}%`;
  const inkVal = inkProfile ? (inkProfile[Math.round(pxX)] || 0) : 0;
  handle.className = "cut-handle";
  if (inkVal === 0) handle.classList.add("green");
  else if (inkVal > 25) handle.classList.add("red");
  const tip = handle.querySelector(".coord-tooltip");
  if (tip) tip.textContent = `${Math.round(pxX)}px (Ink: ${inkVal})`;
}

function snapCut(lIdx, cIdx) {
  const line = pageData.lines[lIdx];
  const curX = line.cuts[cIdx];
  const prof = line.ink_profile;
  if (!prof) return;

  const w = line.width;
  const x1 = Math.max(0, curX - 35);
  const x2 = Math.min(w - 1, curX + 35);

  let bestX = curX;
  let minInk = 999999;
  for (let x = x1; x <= x2; x++) {
    const val = prof[x] || 0;
    if (val < minInk || (val === minInk && Math.abs(x - curX) < Math.abs(bestX - curX))) {
      minInk = val;
      bestX = x;
    }
  }

  line.cuts[cIdx] = bestX;
  const overlay = document.getElementById(`overlay_${lIdx}`);
  const handle = overlay.children[cIdx];
  if (handle) updateHandlePos(handle, bestX, line.width, prof);
  renderChips(document.getElementById(`chips_${lIdx}`), line);
}

function snapAllLines() {
  pageData.lines.forEach((line, lIdx) => {
    if (line.line_type === "ayah" && line.cuts) {
      line.cuts.forEach((_, cIdx) => snapCut(lIdx, cIdx));
    }
  });
}

function renderChips(container, line) {
  if (!container || !line.words) return;
  container.innerHTML = "";

  const cuts = [0, ...line.cuts.slice().sort((a,b)=>a-b), line.width];
  // In RTL order:
  // Word 1 is rightmost: cuts[len-2] to cuts[len-1]
  line.words.forEach((wd, idx) => {
    const chip = document.createElement("div");
    chip.className = "chip";
    const xR = cuts[cuts.length - 1 - idx];
    const xL = cuts[cuts.length - 2 - idx];
    const wPx = Math.round(xR - xL);

    chip.innerHTML = `
      <span class="chip-arabic">${wd}</span>
      <span class="chip-meta">#${idx + 1} (${wPx}px)</span>
    `;
    container.appendChild(chip);
  });
}

async function saveCurrentPage(approved = true) {
  if (!pageData) return;
  const payload = {
    page: currentPage,
    approved: approved,
    lines: {}
  };
  pageData.lines.forEach(l => {
    if (l.line_type === "ayah") {
      payload.lines[l.line_number] = l.cuts.map(Math.round).sort((a,b)=>a-b);
    }
  });

  await fetch("/api/save_page", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  });
}

async function approveAndNext() {
  await saveCurrentPage(true);
  if (currentPage < 610) {
    loadPage(currentPage + 1);
  } else {
    alert("Reached the last page (610)!");
  }
}

function prevPage() {
  if (currentPage > 1) loadPage(currentPage - 1);
}

function nextPage() {
  if (currentPage < 610) loadPage(currentPage + 1);
}

function jumpPage(p) {
  const val = parseInt(p);
  if (val >= 1 && val <= 610) loadPage(val);
}

// Keyboard shortcuts
window.addEventListener("keydown", (e) => {
  if (e.target.tagName === "INPUT") return;
  if (e.key === "Enter") {
    e.preventDefault();
    approveAndNext();
  } else if (e.key === "[" || (e.altKey && e.key === "ArrowLeft")) {
    e.preventDefault();
    prevPage();
  } else if (e.key === "]" || (e.altKey && e.key === "ArrowRight")) {
    e.preventDefault();
    nextPage();
  }
});

// Load initial page
window.addEventListener("DOMContentLoaded", () => {
  loadPage(currentPage);
});
</script>
</body>
</html>
"""

def save_page_modifications(p_num: int, lines_cuts: dict):
    page_path = PAGES_DIR / f"page_{p_num:03d}.png"
    if not page_path.exists():
        return False
    img = cv2.imread(str(page_path))
    if img is None:
        return False
    h, w, _ = img.shape
    
    b, g, r = img[:, :, 0], img[:, :, 1], img[:, :, 2]
    red_rows = np.where(np.sum((r > 180) & (b < 70) & (g < 70), axis=1) > w * 0.4)[0]
    line_bands = []
    for k, g_iter in groupby(enumerate(red_rows), lambda ix: ix[0]-ix[1]):
        group = list(map(itemgetter(1), g_iter))
        line_bands.append((min(group), max(group)))
        
    num_dividers = len(line_bands)
    overrides = load_approved_cuts()
    
    for l_num_str, cuts in lines_cuts.items():
        l_num = int(l_num_str)
        if num_dividers == 16:
            s_idx = l_num
        elif num_dividers == 9:
            s_idx = l_num - 7
        elif num_dividers == 11 and p_num == 610:
            s_idx = l_num
        else:
            s_idx = l_num
            
        if 1 <= s_idx < len(line_bands):
            y_top = line_bands[s_idx - 1][1] + 1
            y_bot = line_bands[s_idx][0] - 1
            valid_cuts = [int(round(c)) for c in cuts]
            for cx in valid_cuts:
                cv2.line(img, (cx, y_top), (cx, y_bot), WORD_COLOR, THICKNESS)
                
            key = f"page_{p_num:03d}_line_{l_num:02d}"
            overrides[key] = {
                "cuts": valid_cuts,
                "approved": True,
                "timestamp": int(time.time())
            }
            
    MODIFIED_DIR.mkdir(parents=True, exist_ok=True)
    out_path = MODIFIED_DIR / f"page_{p_num:03d}.png"
    cv2.imwrite(str(out_path), img, [cv2.IMWRITE_PNG_COMPRESSION, 1])
    save_approved_cuts(overrides)
    print(f"[✓] Saved modified PNG to {out_path}")
    return True

class StudioRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        qs = parse_qs(parsed.query)

        if path in ("/", "/index.html"):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(HTML_CONTENT.encode("utf-8"))

        elif path == "/api/page":
            p_num = int(qs.get("p", ["3"])[0])
            data = get_page_data(p_num)
            if data is None:
                self.send_response(404)
                self.end_headers()
                return

            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.end_headers()
            self.wfile.write(json.dumps(data).encode("utf-8"))

        elif path == "/api/stats":
            overrides = load_approved_cuts()
            approved_pages = set()
            for k, v in overrides.items():
                if v.get("approved"):
                    approved_pages.add(int(k.split("_")[1]))
            
            resp = {
                "total_pages": 610,
                "approved_pages_count": len(approved_pages),
                "approved_pages": sorted(list(approved_pages))
            }
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(resp).encode("utf-8"))

        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/save_page":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length)
            payload = json.loads(body.decode("utf-8"))

            p_num = payload["page"]
            approved = payload.get("approved", True)
            lines_cuts = payload.get("lines", {})

            success = save_page_modifications(p_num, lines_cuts)
            self.send_response(200 if success else 500)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"status": "OK", "page": p_num}).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()


def run_server(port=8080, start_page=3):
    server = HTTPServer(("0.0.0.0", port), StudioRequestHandler)
    print(f"\n=======================================================")
    print(f"  QURAN WORD VERIFICATION STUDIO RUNNING")
    print(f"  URL: http://localhost:{port}")
    print(f"  Initial Page: {start_page}")
    print(f"=======================================================\n")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server...")
        server.server_close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Quran Word Review Studio")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on (default 8080)")
    parser.add_argument("--page", type=int, default=3, help="Initial page number")
    args = parser.parse_args()

    run_server(args.port, args.page)