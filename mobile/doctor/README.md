<p align="center">
  <img src="play_store_512.png" alt="Miqu Doctor Logo" width="160" />
</p>

# Doctor 🩺

An offline clinical utility and medical reference application for Android, designed for healthcare professionals, medical students, and clinicians.

Doctor combines essential clinical calculators (Obstetrics and Rabies PEP) with an offline, Markdown-powered medical notebook and reference system.

---

## ✨ Features

### 🧮 Clinical Calculators
- **Rabies Post-Exposure Prophylaxis (PEP) Calculator**:
  - Computes complete immunization schedules for:
    - **Intradermal (ID)**: Updated Thai Red Cross 2-site regimen (Days 0, 3, 7, 28)
    - **Intramuscular (IM)**: Essen 5-dose regimen (Days 0, 3, 7, 14, 28)
    - **Re-exposure**: Abbreviated regimen (Days 0, 3) for previously immunized patients
  - **Rabies Immunoglobulin (RIG) Dosage**:
    - Human RIG (HRIG, 20 IU/kg) and Equine RIG (ERIG, 40 IU/kg) dosage computation based on patient weight.
    - Clinical administration guides and wound infiltration guidance.
- **Pregnancy & Obstetric Calculator**:
  - Calculates Estimated Due Date (EDD) via adjusted **Naegele's Rule** (customizable menstrual cycle length).
  - Ultrasound-based dating (implied LMP from gestational age at scan date).
  - Exact gestational age (weeks + days), current trimester, and pregnancy progress tracking.
  - Automated scheduling for key obstetric screening milestones:
    - First Trimester / Nuchal Translucency (NT) Scan (11w0d – 13w6d)
    - Level II Fetal Anatomy Scan (18w0d – 22w0d)
    - Gestational Diabetes (GDM) Screening (24w0d – 28w0d)
    - Group B Streptococcus (GBS) Screening (36w0d – 37w6d)

### 📓 Clinical Notes & Knowledge Base
- **Offline Markdown Engine**: Rendered using [Markwon](https://github.com/noties/Markwon) with full table formatting, strike-through, and embedded image support.
- **Preloaded Reference Guides**:
  - Emergency Drugs & Resuscitation Protocols
  - Normal Laboratory Values & Reference Ranges
  - Pregnancy Clinical Reference & Red Flags
  - WHO & National Rabies PEP Guidelines
- **Custom Notebooks & Full-Text Search**:
  - Organize clinical pearls into custom notebooks.
  - Fast search across all clinical notes and references.
  - Pin important protocols for immediate bedside access.

---

## 🛠️ Tech Stack & Architecture

- **Language**: Kotlin 2.0+
- **Minimum SDK**: Android 12 (API 31)
- **Target SDK**: Android 15 / UpsideDownCake+ (API 37)
- **UI Framework**: Modern Android Views, Material Design 3 (`com.google.android.material`), Android ViewBinding
- **Navigation**: Android Jetpack Navigation Component
- **Markdown**: Markwon 4.6.2 (Core, Tables, Strikethrough, Images)
- **Persistence**: SQLite (Local Database via Android SQLiteOpenHelper)
- **Testing**: JUnit 4, AndroidX Test Runner, Espresso

---

## 📂 Project Structure

```
doctor/
├── app/
│   ├── src/
│   │   ├── main/
│   │   │   ├── assets/notes/        # Preloaded clinical markdown references
│   │   │   ├── java/com/miqu/android/doctor/
│   │   │   │   ├── data/            # SQLite database, entities, search models
│   │   │   │   ├── model/           # Clinical calculator algorithms (Rabies, Pregnancy)
│   │   │   │   └── ui/              # UI layer (Hub, Notes, Pregnancy, Rabies)
│   │   │   └── res/                 # Layouts, vector drawables, themes, values
│   │   └── test/                    # Unit tests for clinical calculators
│   └── build.gradle.kts
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

To run unit tests:
```bash
./gradlew testDebugUnitTest
```

To install on a connected device:
```bash
./gradlew installDebug
```

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
