# Quran Word-by-Word Extraction Pipeline

> **Image Processing & Annotation Pipeline for 15-Line Quran Pages**

A modular Python image processing pipeline for extracting, deskewing, normalizing, and annotating standard 15-line Quran page scans for the `recitation` Android app.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **`pipeline/`**: Step-by-step image processing stages:
  - `step01_trim.py`: Strip non-Quran cover and end pages.
  - `step02_crop_borders.py`: Perspective detection, border crop, and deskew.
  - `step03_mark_lines.py`: Horizontal line coordinate segmentation.
  - `step04_normalize_headers.py`: Normalize Surah title boxes and Bismillah headers.
  - `step05_invert_juz_headers.py`: Standardize Juz header blocks.
  - `step06_extract_pages.py`: Export production WebP/PNG line slices.
- **`studio/`**: Local Flask/web application (`app.py`) for visual word boundary and cut verification.
- **`run_pipeline.py`**: CLI orchestrator for running complete or individual pipeline stages.

---

## 🔨 Usage

```bash
# Check status of pipeline stages:
python3 run_pipeline.py --status

# Run all steps sequentially:
python3 run_pipeline.py --all

# Start interactive review studio:
python3 studio/app.py
```
