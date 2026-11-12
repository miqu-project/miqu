# Quran Extract & Line/Word Annotation Pipeline

High-performance, modular pipeline for extracting, deskewing, normalizing, and annotating the standard 15-line Pakistani/Indo-Pak Quran print (`QuranMajeed-15Lines-PakistaniPrint.pdf`).

---

## Repository Structure

```
quran-extract/
├── data/
│   ├── input/
│   │   └── QuranMajeed-15Lines-PakistaniPrint.pdf   # Original 616-page scan
│   ├── assets/
│   │   ├── p001_fatihah_custom.png                  # Clean Page 1 header box asset
│   │   ├── p002_baqarah_custom.png                  # Clean Page 2 header box asset
│   │   ├── surah_bismillah_map.md                   # 114 Surahs / 113 Bismillahs mapping table
│   │   └── quran_lines.db                           # SQLite database with line slots
│   └── output/
│       ├── 01_trimmed.pdf                           # 610-page trimmed PDF (pages 1-610)
│       ├── 02_cropped.pdf                           # Border-cropped & deskewed PDF
│       ├── 03_lines_marked.pdf                      # Horizontal verse lines marked in red
│       ├── 04_headers_normalized.pdf                # Surah names & Bismillahs standardized
│       └── 05_final_marked.pdf                      # Juz beginnings inverted & enhanced
├── pipeline/
│   ├── config.py                                    # Central path & parameter configuration
│   ├── step01_trim.py                               # Strip cover page & back pages
│   ├── step02_crop_borders.py                       # Perspective quad detection & deskew
│   ├── step03_mark_lines.py                         # Horizontal red verse line marking
│   ├── step04_normalize_headers.py                  # Standardize P1, P2 & all 112 surah/bismillah lines
│   ├── step05_invert_juz_headers.py                 # Invert & thicken 29 white-on-black Juz headers
│   └── step06_extract_pages.py                      # Export clean page PNGs for studio
├── studio/                                          # Interactive token review web app
│   ├── app.py                                       # Localhost web server (default port 8080)
│   ├── pages/                                       # 610 clean extracted PNGs
│   ├── modified/                                    # User-annotated and modified page PNGs
│   └── cuts.json                                    # Verified word cuts storage
├── run_pipeline.py                                  # Master CLI runner (run all or specific steps)
└── README.md                                        # Documentation
```

---

## Quick Start

### 1. Check Pipeline Status
```bash
python3 run_pipeline.py --status
```

### 2. Run the End-to-End Pipeline
```bash
# Run all steps sequentially (forces regeneration of all outputs):
python3 run_pipeline.py --all

# Run pipeline, skipping steps whose output files already exist:
python3 run_pipeline.py --resume
```

### 3. Run Individual Steps
```bash
python3 run_pipeline.py --step 1    # Trim cover & end pages
python3 run_pipeline.py --step 2    # Border cropping & micro-deskewing
python3 run_pipeline.py --step 3    # Mark horizontal verse lines in red
python3 run_pipeline.py --step 4    # Normalize Surah & Bismillah headers
python3 run_pipeline.py --step 5    # Invert 29 negative Juz headers
python3 run_pipeline.py --step 6    # Export clean page PNGs to studio/pages/
```
Or run any step script directly:
```bash
python3 pipeline/step02_crop_borders.py --dpi 200 --quality 92 --workers 4
```

---

## Interactive Word Review Studio

Launch the local web studio to view pages, inspect lines, and annotate vertical word cuts interactively:

```bash
python3 studio/app.py --port 8080 --page 3
```
Then open `http://localhost:8080` in your web browser.

- **Click & Drag**: Add, move, or remove vertical word cut lines.
- **Save**: Saves verified cut coordinates into `studio/cuts.json` and exports rendered annotated images to `studio/modified/`.
- **Keyboard Shortcuts**: Left/Right arrows to navigate between pages.
