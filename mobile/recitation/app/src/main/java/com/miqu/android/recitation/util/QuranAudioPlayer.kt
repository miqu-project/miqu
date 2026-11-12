package com.miqu.android.recitation.util

import android.content.Context
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.miqu.android.recitation.data.UserSettings
import com.miqu.android.recitation.model.Reciter

class QuranAudioPlayer(
    private val context: Context,
    private val userSettings: UserSettings
) {
    companion object {
        private const val TAG = "QuranAudioPlayer"
        const val MODE_ARABIC_ONLY = 0
        const val MODE_TRANSLATION_ONLY = 1
        const val MODE_BOTH = 2

        val SURAH_TOTAL_VERSES = intArrayOf(
            7, 286, 200, 176, 120, 165, 206, 75, 129, 109, 123, 111, 43, 52, 99, 128, 111, 110,
            98, 135, 112, 78, 118, 64, 77, 227, 93, 88, 69, 60, 34, 30, 73, 54, 45, 83, 182,
            88, 75, 85, 54, 53, 89, 59, 37, 35, 38, 29, 18, 45, 60, 49, 62, 55, 78, 96, 29, 22,
            24, 13, 14, 11, 11, 18, 12, 12, 30, 52, 52, 44, 28, 28, 20, 56, 40, 31, 50, 40, 46,
            42, 29, 19, 36, 25, 22, 17, 19, 26, 30, 20, 15, 21, 11, 8, 8, 19, 5, 8, 8, 11, 11,
            8, 3, 9, 5, 4, 7, 3, 6, 3, 5, 4, 5, 6
        )

        fun getGlobalVerseIndex(surahNumber: Int, verseNumber: Int): Int {
            var sum = 0
            val validSurah = surahNumber.coerceIn(1, 114)
            for (i in 0 until validSurah - 1) {
                sum += SURAH_TOTAL_VERSES[i]
            }
            return sum + verseNumber
        }
    }

    private var mediaPlayer: MediaPlayer? = null
    private val mainHandler = Handler(Looper.getMainLooper())

    private var currentSurah = 1
    private var currentVerse = 1
    private var isPlayingContinuous = false
    private var isSubSegmentTranslation = false // for MODE_BOTH: false = playing arabic, true = playing translation
    private var isPrepared = false

    val isPlaying: Boolean
        get() = try {
            isPrepared && mediaPlayer?.isPlaying == true
        } catch (_: Exception) {
            false
        }

    val isTranslationSubSegment: Boolean
        get() = isSubSegmentTranslation

    val currentlyPlayingVerse: Int?
        get() = if (isPlaying || mediaPlayer != null) currentVerse else null

    var onVerseStarted: ((verseNumber: Int) -> Unit)? = null
    var onSegmentChanged: ((isTranslation: Boolean) -> Unit)? = null
    var onStateChanged: ((isPlaying: Boolean, isBuffering: Boolean) -> Unit)? = null
    var onPlaybackCompleted: (() -> Unit)? = null
    var onError: ((message: String) -> Unit)? = null

    fun playSingleVerse(surahNumber: Int, verseNumber: Int) {
        if (currentSurah == surahNumber && currentVerse == verseNumber && isPlaying) {
            pause()
            return
        }
        stop()
        isPlayingContinuous = false
        currentSurah = surahNumber
        currentVerse = verseNumber
        isSubSegmentTranslation = (userSettings.audioRecitationMode == MODE_TRANSLATION_ONLY)
        playCurrentVerseAudio()
    }

    fun playFullSurah(surahNumber: Int, startVerse: Int = 1) {
        stop()
        isPlayingContinuous = true
        currentSurah = surahNumber
        currentVerse = startVerse
        isSubSegmentTranslation = (userSettings.audioRecitationMode == MODE_TRANSLATION_ONLY)
        playCurrentVerseAudio()
    }

    fun pause() {
        try {
            if (isPrepared && mediaPlayer?.isPlaying == true) {
                mediaPlayer?.pause()
                mainHandler.post {
                    onStateChanged?.invoke(false, false)
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error pausing", e)
        }
    }

    fun resume() {
        try {
            if (isPrepared && mediaPlayer != null) {
                mediaPlayer?.start()
                mainHandler.post {
                    onStateChanged?.invoke(true, false)
                }
            } else {
                playCurrentVerseAudio()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error resuming", e)
            playCurrentVerseAudio()
        }
    }

    fun nextVerse() {
        val totalVerses = SURAH_TOTAL_VERSES.getOrNull(currentSurah - 1) ?: return
        if (currentVerse < totalVerses) {
            currentVerse++
            isSubSegmentTranslation = (userSettings.audioRecitationMode == MODE_TRANSLATION_ONLY)
            playCurrentVerseAudio()
        } else {
            stop()
            mainHandler.post {
                onPlaybackCompleted?.invoke()
            }
        }
    }

    fun previousVerse() {
        if (currentVerse > 1) {
            currentVerse--
            isSubSegmentTranslation = (userSettings.audioRecitationMode == MODE_TRANSLATION_ONLY)
            playCurrentVerseAudio()
        }
    }

    private fun safeReleasePlayer() {
        mediaPlayer?.let { mp ->
            try {
                mp.setOnPreparedListener(null)
                mp.setOnCompletionListener(null)
                mp.setOnErrorListener(null)
                mp.reset()
                mp.release()
            } catch (e: Exception) {
                Log.w(TAG, "Error releasing MediaPlayer", e)
            }
        }
        mediaPlayer = null
        isPrepared = false
    }

    fun stop() {
        safeReleasePlayer()
        mainHandler.post {
            onStateChanged?.invoke(false, false)
        }
    }

    fun release() {
        stop()
        onVerseStarted = null
        onSegmentChanged = null
        onStateChanged = null
        onPlaybackCompleted = null
        onError = null
    }

    private fun playCurrentVerseAudio() {
        val globalVerse = getGlobalVerseIndex(currentSurah, currentVerse)
        val mode = userSettings.audioRecitationMode

        val reciterId = when {
            mode == MODE_TRANSLATION_ONLY -> userSettings.translationReciterIdentifier
            mode == MODE_BOTH && isSubSegmentTranslation -> userSettings.translationReciterIdentifier
            else -> userSettings.reciterIdentifier
        }

        val reciter = Reciter.getReciter(reciterId)
        val bitrate = reciter.bitrate
        val url = "https://cdn.islamic.network/quran/audio/$bitrate/$reciterId/$globalVerse.mp3"
        Log.d(TAG, "Streaming audio from: $url (surah $currentSurah, verse $currentVerse, mode $mode, isSubSegmentTranslation=$isSubSegmentTranslation)")

        try {
            safeReleasePlayer()

            mainHandler.post {
                onVerseStarted?.invoke(currentVerse)
                onSegmentChanged?.invoke(isSubSegmentTranslation)
                onStateChanged?.invoke(false, true) // buffering
            }

            mediaPlayer = MediaPlayer().apply {
                setAudioAttributes(
                    AudioAttributes.Builder()
                        .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .build()
                )
                setDataSource(url)
                setOnPreparedListener { mp ->
                    isPrepared = true
                    mp.start()
                    mainHandler.post {
                        onStateChanged?.invoke(true, false) // playing
                    }
                }
                setOnCompletionListener {
                    handleVerseCompleted()
                }
                setOnErrorListener { _, what, extra ->
                    Log.e(TAG, "MediaPlayer error: what=$what extra=$extra")
                    isPrepared = false
                    mainHandler.post {
                        onError?.invoke("Audio stream unavailable (code $what)")
                        onStateChanged?.invoke(false, false)
                    }
                    true
                }
                prepareAsync()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start audio stream", e)
            mainHandler.post {
                onError?.invoke(e.localizedMessage ?: "Failed to connect to audio stream")
                onStateChanged?.invoke(false, false)
            }
        }
    }

    private fun handleVerseCompleted() {
        val mode = userSettings.audioRecitationMode

        if (mode == MODE_BOTH && !isSubSegmentTranslation) {
            // Play translation part of current verse next
            isSubSegmentTranslation = true
            playCurrentVerseAudio()
            return
        }

        // Current verse finished (both parts finished, or single mode finished)
        isSubSegmentTranslation = (mode == MODE_TRANSLATION_ONLY)
        if (isPlayingContinuous) {
            val totalVerses = SURAH_TOTAL_VERSES.getOrNull(currentSurah - 1) ?: 0
            if (currentVerse < totalVerses) {
                currentVerse++
                playCurrentVerseAudio()
            } else {
                stop()
                mainHandler.post {
                    onPlaybackCompleted?.invoke()
                }
            }
        } else {
            stop()
            mainHandler.post {
                onPlaybackCompleted?.invoke()
            }
        }
    }
}
