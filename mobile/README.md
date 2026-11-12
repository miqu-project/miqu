# Mobile Applications (Miqu Ecosystem)

> **Native Android mobile applications for the Miqu ecosystem.**

This directory hosts Android mobile companion and standalone applications developed within the [Miqu project](https://github.com/miqu-project).

---

## 📱 Applications

| Application | Description |
|---|---|
| [**`doctor`**](./doctor) | Healthcare & medical management mobile application. |
| [**`recitation`**](./recitation) | Audio recitation, study, and media mobile application. |

---

## 🔨 Building
Each application is an independent Gradle project.

To build an application, navigate to its directory and run the Gradle wrapper:
```bash
cd recitation
./gradlew assembleDebug
```
or for `doctor`:
```bash
cd doctor
./gradlew assembleDebug
```
