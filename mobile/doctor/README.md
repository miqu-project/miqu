# Doctor 🩺

> **Offline Clinical Calculator & Medical Reference Application for Android**

`doctor` is an offline medical reference and clinical utility built in Kotlin and Material Design 3 for healthcare professionals, clinicians, and medical students.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

- **Rabies PEP Calculator**: Computes complete immunization schedules for Intradermal (Thai Red Cross), Intramuscular (Essen), and re-exposure regimens, plus HRIG/ERIG weight-based dosing.
- **Obstetric & Pregnancy Calculator**: Calculates EDD via adjusted Naegele's Rule, gestational age (weeks + days), and key screening milestones (NT scan, anatomy scan, GDM, GBS).
- **Offline Medical Notebook**: Markdown-rendered clinical notebooks powered by Markwon, featuring preloaded emergency drug protocols, normal lab values, and full-text search.
- **Tech Stack**: Kotlin 2.0+, Material Design 3, ViewBinding, Navigation Component, SQLite, and Markwon.

---

## 🔨 Building

```bash
cd mobile/doctor
./gradlew assembleDebug
```
The output APK is generated at `app/build/outputs/apk/debug/app-debug.apk`.
