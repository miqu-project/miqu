# Recitation 📖

> **Offline Quran Study Companion, Word-by-Word Morphology & Audio Reciter**

`recitation` is an offline Quran study platform and Arabic linguistic analysis application for Android, built in Kotlin and Material Design 3.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **High-Fidelity Quran Reader**: 15-line Quran page view with line-by-line WebP rendering, customizable Arabic fonts, and Surah indexing.
- **Word-by-Word Morphology**: Linguistic breakdown for every word: part of speech, lemma, root derivations, and grammatical syntax.
- **Classical Arabic Roots (Lane's Lexicon)**: Root-based lexical search with complete concordance and classical definitions.
- **Offline Multi-Language Tafsir**: Offline commentaries (Ibn Kathir, Ma'arif-ul-Qur'an, Jalalayn) rendered with Markwon.
- **Semantic Vector Search**: On-device natural language semantic search powered by quantized MiniLM running locally via ONNX Runtime.
- **Audio & Video Recitations**: Audio streaming with playback controls and synchronized video recitation text tracking.

---

## 🔨 Building

```bash
cd mobile/recitation
./gradlew assembleDebug
```
The output APK is generated at `app/build/outputs/apk/debug/app-debug.apk`.
