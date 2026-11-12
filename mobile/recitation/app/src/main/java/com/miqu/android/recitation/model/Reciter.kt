package com.miqu.android.recitation.model

data class Reciter(
    val name: String,
    val identifier: String,
    val bitrate: Int = 64,
    val isTranslation: Boolean = false
) {
    override fun toString(): String = name

    companion object {
        val ARABIC_RECITERS = listOf(
            Reciter("Mishary Rashid Alafasy", "ar.alafasy", bitrate = 64),
            Reciter("Abdul Basit (Murattal)", "ar.abdulbasitmurattal", bitrate = 64),
            Reciter("Mahmoud Khalil Al-Husary", "ar.husary", bitrate = 64),
            Reciter("Husary (Mujawwad)", "ar.husarymujawwad", bitrate = 64),
            Reciter("Muhammad Siddiq Al-Minshawi", "ar.minshawi", bitrate = 128),
            Reciter("Minshawi (Mujawwad)", "ar.minshawimujawwad", bitrate = 64),
            Reciter("Abdurrahmaan As-Sudais", "ar.abdurrahmaansudais", bitrate = 64),
            Reciter("Saood bin Ibraaheem Ash-Shuraym", "ar.saoodshuraym", bitrate = 64),
            Reciter("Abu Bakr Ash-Shaatree", "ar.shaatree", bitrate = 64),
            Reciter("Ahmed ibn Ali al-Ajamy", "ar.ahmedajamy", bitrate = 64),
            Reciter("Maher Al Muaiqly", "ar.mahermuaiqly", bitrate = 64),
            Reciter("Hani Rifai", "ar.hanirifai", bitrate = 64),
            Reciter("Ayman Sowaid", "ar.aymanswoaid", bitrate = 64),
            Reciter("Muhammad Ayyoub", "ar.muhammadayyoub", bitrate = 128)
        )

        val TRANSLATION_RECITERS = listOf(
            Reciter("Ibrahim Walk (English - Saheeh Int)", "en.walk", bitrate = 192, isTranslation = true),
            Reciter("Shamshad Ali Khan (Urdu - Jalandhri)", "ur.khan", bitrate = 64, isTranslation = true)
        )

        const val DEFAULT_ARABIC_RECITER_ID = "ar.alafasy"
        const val DEFAULT_TRANSLATION_RECITER_ID = "en.walk"

        fun getArabicReciter(id: String): Reciter {
            return ARABIC_RECITERS.find { it.identifier == id } ?: ARABIC_RECITERS[0]
        }

        fun getTranslationReciter(id: String): Reciter {
            return TRANSLATION_RECITERS.find { it.identifier == id } ?: TRANSLATION_RECITERS[0]
        }

        fun getReciter(id: String): Reciter {
            return ARABIC_RECITERS.find { it.identifier == id }
                ?: TRANSLATION_RECITERS.find { it.identifier == id }
                ?: ARABIC_RECITERS[0]
        }
    }
}
