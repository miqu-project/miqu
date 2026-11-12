<p align="center">
  <img src="play_store_512.png" alt="Miqu Recitation Logo" width="160" />
</p>

# Recitation 📖

An offline, feature-rich Quran study companion and Arabic linguistic analysis app for Android.

Recitation provides an end-to-end Quranic study platform combining a high-fidelity reader, word-by-word morphological breakdowns, classical Arabic roots (Lane's Lexicon), offline multi-language Tafsir, audio recitations, and on-device semantic search powered by ONNX neural embeddings.

---

## ✨ Features

### 📖 Quran Reader & Exegesis
- **High-Fidelity Page & Ayah View**:
  - 15-line Quran page views with optimized WebP line-by-line rendering.
  - Surah list with Meccan / Medinan categorization, revelation chronology, and verse indexing.
  - Customizable Arabic fonts, translation typography, and text sizing.
- **Word-by-Word Morphology**:
  - Detailed linguistic analysis for individual words: parts of speech, lemma, root derivations, and grammatical syntax (attributed to Quranic Arabic Corpus).
  - Exact word concordance: browse and inspect all occurrences of a specific word across the entire Quran.
- **Multi-Language Offline Tafsir**:
  - Includes offline Tafsir commentaries rendered with [Markwon](https://github.com/noties/Markwon):
    - *Tafsir Ibn Kathir* (English, Bengali, Urdu)
    - *Ma\'arif-ul-Qur\'an* (English)
    - *Tafsir Al-Jalalayn* (Indonesian)
    - *Al-Mokhtasar fi Tafsir al-Qur\'an* (Arabic / International)

### 🌿 Quranic Lexicon & Arabic Grammar
- **Roots & Lexical Concordance**:
  - Search and explore classical Arabic roots (e.g. `كتب`, `سلم`, `نور`).
  - Classical definitions and derivations based on Lane's Lexicon.
  - Complete index of every verse and grammatical form derived from each root.
- **Vocabulary & Grammar Modules**:
  - Frequency-sorted Quranic vocabulary tracker.
  - Visual grammar references covering Arabic verb patterns, case markers, and syntactic rules.

### 🧠 Semantic & Unified Search
- **On-Device Semantic Vector Search**:
  - Embedded MiniLM ONNX model (`minilm.onnx`) running locally on device for natural language and conceptual meaning search across verses.
- **Unified Fast Search**:
  - Instant search across Surahs, specific Ayah references (e.g., `2:255`), translations, and Arabic roots.

### 🎧 Audio & Media Recitations
- **Audio Reciter Playback**:
  - Integrated audio streaming and playback across different reciters with playback controls.
- **Special Recitations**:
  - Synchronized video recitations with real-time text tracking.

---

## 🛠️ Tech Stack & Architecture

- **Language**: Kotlin 2.0+
- **Minimum SDK**: Android 12 (API 31)
- **Target SDK**: Android 15 / UpsideDownCake+ (API 37)
- **UI Framework**: Material Design 3 (`com.google.android.material`), Android ViewBinding, Lottie animations
- **Navigation**: Jetpack Navigation Component
- **Machine Learning / Inference**: ONNX Runtime with quantized MiniLM embedding model for semantic search
- **Markdown & Exegesis**: Markwon 4.6.2 (Core, Tables, Strikethrough)
- **Data & Storage**: SQLite databases (`quran.db`, `roots.db`) stored uncompressed for rapid random-access reading
- **Testing**: JUnit 4, AndroidX Test Runner, Espresso

---

## 📂 Project Structure

```
recitation/
├── app/
│   ├── src/
│   │   ├── main/
│   │   │   ├── assets/
│   │   │   │   ├── databases/       # Preloaded SQLite databases (quran.db, roots.db)
│   │   │   │   ├── model/           # ONNX embedding model (minilm.onnx, vocab.txt)
│   │   │   │   ├── quran_lines/     # Pre-rendered 15-line Quran page graphics
│   │   │   │   └── tafsir/          # Offline Markdown Tafsir commentaries
│   │   │   ├── java/com/miqu/android/recitation/
│   │   │   │   ├── data/            # SQLite helpers, repositories, and user settings
│   │   │   │   ├── model/           # Surah, Verse, Word, Root, and Corpus data models
│   │   │   │   ├── ui/              # Reader, Lexicon, Learn, Surah, Media & Settings UI
│   │   │   │   └── util/            # Audio player and typography helpers
│   │   │   └── res/                 # Layouts, vector drawables, themes, strings
│   │   └── test/
│   └── build.gradle.kts
├── build_install.sh                 # Convenience build and device installation script
├── gradle/
│   └── libs.versions.toml           # Gradle Version Catalog
├── build.gradle.kts
└── settings.gradle.kts
```

---

## 🚀 Building & Running

### Prerequisites
- JDK 11 or newer (JDK 17/21 recommended)
- Android SDK with API 31+ installed (Android Studio Ladybug or command-line SDK)

### Build with Gradle
To compile the debug APK:
```bash
./gradlew assembleDebug
```

To build and install directly to an attached Android device:
```bash
./build_install.sh
# or
./gradlew installDebug
```

To execute unit tests:
```bash
./gradlew testDebugUnitTest
```

---

## 📄 License & Attributions

- Morphological analysis and syntax annotations derived from [The Quranic Arabic Corpus](https://corpus.quran.com).
- Quranic text and translations provided via [Tanzil Project](https://tanzil.net).
- Licensed under the [MIT License](LICENSE).
