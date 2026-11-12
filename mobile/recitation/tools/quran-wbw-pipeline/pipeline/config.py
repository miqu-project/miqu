from pathlib import Path
import os

PROJECT_ROOT = Path(__file__).resolve().parent.parent

# Data Directories
DATA_DIR = PROJECT_ROOT / "data"
INPUT_DIR = DATA_DIR / "input"
ASSETS_DIR = DATA_DIR / "assets"
OUTPUT_DIR = DATA_DIR / "output"

# Studio Directories
STUDIO_DIR = PROJECT_ROOT / "studio"
STUDIO_PAGES_DIR = STUDIO_DIR / "pages"
STUDIO_MODIFIED_DIR = STUDIO_DIR / "modified"
STUDIO_CUTS_PATH = STUDIO_DIR / "cuts.json"

# Core PDF Paths
RAW_INPUT_PDF = INPUT_DIR / "QuranMajeed-15Lines-PakistaniPrint.pdf"
TRIMMED_PDF = OUTPUT_DIR / "01_trimmed.pdf"
CROPPED_PDF = OUTPUT_DIR / "02_cropped.pdf"
LINES_MARKED_PDF = OUTPUT_DIR / "03_lines_marked.pdf"
HEADERS_NORMALIZED_PDF = OUTPUT_DIR / "04_headers_normalized.pdf"
FINAL_MARKED_PDF = OUTPUT_DIR / "05_final_marked.pdf"

# Assets
DB_PATH = ASSETS_DIR / "quran_lines.db"
MAP_MD_PATH = ASSETS_DIR / "surah_bismillah_map.md"
P1_ASSET = ASSETS_DIR / "p001_fatihah_custom.png"
P2_ASSET = ASSETS_DIR / "p002_baqarah_custom.png"

# Processing Parameters
DEFAULT_DPI = 200
DEFAULT_QUALITY = 92
DEFAULT_WORKERS = min(4, os.cpu_count() or 4)
FONT_PATH = "/usr/share/fonts/liberation/LiberationSerif-Regular.ttf"

# Ensure essential output directories exist
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
STUDIO_DIR.mkdir(parents=True, exist_ok=True)
