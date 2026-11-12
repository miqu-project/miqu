# Mobile Applications (Miqu Ecosystem)

> **Native Android Mobile Companion Applications for the Miqu Ecosystem**

This directory hosts standalone and companion Android mobile applications developed within the Miqu project, built in Kotlin with modern Jetpack Compose.

Website & Documentation: [**miqu-project.github.io**](https://miqu-project.github.io)

---

## 🔍 What is What

| Application | Directory | Description |
| :--- | :--- | :--- |
| **`doctor`** | [`doctor/`](doctor/) | Offline clinical utility, obstetrics and rabies PEP calculators, and medical reference notes. |
| **`recitation`** | [`recitation/`](recitation/) | Feature-rich Quran study companion, word-by-word morphology, audio recitations, and semantic vector search. |

---

## 🔨 Building

Each mobile application is an independent Gradle project.

To build an application, navigate to its directory and run the Gradle wrapper:

```bash
# Build Doctor APK
cd mobile/doctor
./gradlew assembleDebug

# Build Recitation APK
cd mobile/recitation
./gradlew assembleDebug
```
