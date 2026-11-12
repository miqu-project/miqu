package com.miqu.android.recitation.util

import android.content.Context
import android.graphics.Typeface
import androidx.core.content.res.ResourcesCompat
import com.miqu.android.recitation.R
import com.miqu.android.recitation.data.UserSettings

object FontHelper {

    private var cachedUthman: Typeface? = null
    private var cachedRaisin: Typeface? = null
    private var cachedBangla1: Typeface? = null

    val ARABIC_FONTS = listOf(
        UserSettings.FONT_UTHMAN to "Uthman (King Fahad Hafs)"
    )

    val ENGLISH_FONTS = listOf(
        UserSettings.FONT_DEFAULT to "Default Mobile Font",
        UserSettings.FONT_RAISIN_TOPPING to "Raisin Topping"
    )

    val BENGALI_FONTS = listOf(
        UserSettings.FONT_DEFAULT to "Default Mobile Font",
        UserSettings.FONT_BANGLA1 to "Bangla 1"
    )

    fun getArabicTypeface(context: Context): Typeface? {
        if (cachedUthman == null) {
            cachedUthman = try {
                ResourcesCompat.getFont(context, R.font.uthman)
            } catch (_: Exception) {
                Typeface.DEFAULT
            }
        }
        return cachedUthman
    }

    fun getEnglishTypeface(context: Context, fontKey: String): Typeface? {
        return if (fontKey == UserSettings.FONT_RAISIN_TOPPING) {
            if (cachedRaisin == null) {
                cachedRaisin = try {
                    ResourcesCompat.getFont(context, R.font.raisin_topping)
                } catch (_: Exception) {
                    Typeface.DEFAULT
                }
            }
            cachedRaisin
        } else {
            Typeface.DEFAULT
        }
    }

    fun getBengaliTypeface(context: Context, fontKey: String): Typeface? {
        return if (fontKey == UserSettings.FONT_BANGLA1) {
            if (cachedBangla1 == null) {
                cachedBangla1 = try {
                    ResourcesCompat.getFont(context, R.font.bangla1)
                } catch (_: Exception) {
                    Typeface.DEFAULT
                }
            }
            cachedBangla1
        } else {
            Typeface.DEFAULT
        }
    }

    fun getTranslationTypeface(context: Context, userSettings: UserSettings): Typeface? {
        return getTranslationTypeface(context, userSettings.translation, userSettings)
    }

    fun getTranslationTypeface(context: Context, translationKey: String, userSettings: UserSettings): Typeface? {
        return when (translationKey) {
            UserSettings.TRANS_BENGALI -> getBengaliTypeface(context, userSettings.fontBengali)
            UserSettings.TRANS_ENGLISH, UserSettings.TRANS_YUSUF_ALI -> getEnglishTypeface(
                context,
                userSettings.fontEnglish
            )
            else -> Typeface.DEFAULT
        }
    }

    fun getFontsForLanguage(translationLang: String): List<Pair<String, String>> {
        return when (translationLang) {
            UserSettings.TRANS_ENGLISH, UserSettings.TRANS_YUSUF_ALI -> ENGLISH_FONTS
            UserSettings.TRANS_BENGALI -> BENGALI_FONTS
            else -> listOf(UserSettings.FONT_DEFAULT to "Default Mobile Font")
        }
    }

    fun getSelectedFontForLanguage(settings: UserSettings, translationLang: String): String {
        return when (translationLang) {
            UserSettings.TRANS_ENGLISH, UserSettings.TRANS_YUSUF_ALI -> settings.fontEnglish
            UserSettings.TRANS_BENGALI -> settings.fontBengali
            else -> UserSettings.FONT_DEFAULT
        }
    }

    fun setSelectedFontForLanguage(settings: UserSettings, translationLang: String, fontKey: String) {
        when (translationLang) {
            UserSettings.TRANS_ENGLISH, UserSettings.TRANS_YUSUF_ALI -> settings.fontEnglish = fontKey
            UserSettings.TRANS_BENGALI -> settings.fontBengali = fontKey
        }
    }
}
