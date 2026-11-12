package com.miqu.android.recitation.ui.reader

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updateLayoutParams
import androidx.core.view.updatePadding
import androidx.recyclerview.widget.LinearLayoutManager
import com.miqu.android.recitation.R
import com.miqu.android.recitation.data.QuranDatabaseHelper
import com.miqu.android.recitation.data.UserSettings
import com.miqu.android.recitation.databinding.ActivityReaderBinding
import com.miqu.android.recitation.model.Reciter
import com.miqu.android.recitation.model.Verse
import com.miqu.android.recitation.util.QuranAudioPlayer
import kotlin.concurrent.thread

class ReaderActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_SURAH_ID = "extra_surah_id"
        const val EXTRA_SURAH_NAME = "extra_surah_name"
        const val EXTRA_SURAH_TRANSLITERATION = "extra_surah_transliteration"
        const val EXTRA_TOTAL_VERSES = "extra_total_verses"
        const val EXTRA_TARGET_VERSE = "extra_target_verse"
        const val EXTRA_OVERRIDE_TRANSLATION = "extra_override_translation"
        const val EXTRA_FORCE_SHOW_TRANSLATION = "extra_force_show_translation"
        const val EXTRA_FORCE_SHOW_ARABIC = "extra_force_show_arabic"
    }

    private lateinit var binding: ActivityReaderBinding
    private lateinit var quranDbHelper: QuranDatabaseHelper
    private lateinit var userSettings: UserSettings
    private lateinit var adapter: VerseAdapter
    private lateinit var audioPlayer: QuranAudioPlayer

    private var surahId: Int = 1
    private var verses: List<Verse> = emptyList()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        binding = ActivityReaderBinding.inflate(layoutInflater)
        setContentView(binding.root)

        userSettings = UserSettings(this)
        quranDbHelper = QuranDatabaseHelper.getInstance(this)
        audioPlayer = QuranAudioPlayer(this, userSettings)

        ViewCompat.setOnApplyWindowInsetsListener(binding.readerCoordinator) { _, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            val navBars = insets.getInsets(WindowInsetsCompat.Type.navigationBars())

            binding.appBarLayout.updatePadding(top = statusBars.top)

            val baseMarginPx = (16 * resources.displayMetrics.density).toInt()
            binding.cardPlaybackBar.updateLayoutParams<androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams> {
                bottomMargin = navBars.bottom + baseMarginPx
            }

            val listBottomPadding = navBars.bottom + (100 * resources.displayMetrics.density).toInt()
            binding.recyclerViewVerses.updatePadding(bottom = listBottomPadding)

            insets
        }

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                finish()
            }
        })

        binding.toolbar.setNavigationOnClickListener {
            finish()
        }

        surahId = intent.getIntExtra(EXTRA_SURAH_ID, 1)
        val surahName = intent.getStringExtra(EXTRA_SURAH_NAME) ?: ""
        val transliteration = intent.getStringExtra(EXTRA_SURAH_TRANSLITERATION) ?: "Surah $surahId"
        val totalVerses = intent.getIntExtra(EXTRA_TOTAL_VERSES, 0)
        val targetVerse = intent.getIntExtra(EXTRA_TARGET_VERSE, -1)
        val overrideTranslation = intent.getStringExtra(EXTRA_OVERRIDE_TRANSLATION)
        val forceShowTranslation = intent.getBooleanExtra(EXTRA_FORCE_SHOW_TRANSLATION, false)
        val forceShowArabic = intent.getBooleanExtra(EXTRA_FORCE_SHOW_ARABIC, false)

        val transSubtitle = if (!overrideTranslation.isNullOrEmpty()) {
            " • ${UserSettings.getTranslationDisplayName(overrideTranslation)}"
        } else ""

        binding.toolbar.title = "$transliteration ($surahName)"
        binding.toolbar.subtitle = (if (totalVerses > 0) "$totalVerses Verses • Surah #$surahId" else "Surah #$surahId") + transSubtitle

        binding.toolbar.inflateMenu(R.menu.menu_reader)
        binding.toolbar.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                R.id.action_play_surah -> {
                    toggleSurahPlayback()
                    true
                }
                R.id.action_appearance -> {
                    val sheet = ReaderAppearanceBottomSheetFragment {
                        adapter.overrideTranslation = null
                        adapter.forceShowTranslation = false
                        adapter.forceShowArabic = false
                        binding.toolbar.subtitle = if (totalVerses > 0) "$totalVerses Verses • Surah #$surahId" else "Surah #$surahId"
                        adapter.notifyDataSetChanged()
                        updatePlaybackBarLabels()
                    }
                    sheet.show(supportFragmentManager, "AppearanceSheet")
                    true
                }
                else -> false
            }
        }

        setupAudioPlayerCallbacks()
        setupPlaybackBarControls()

        adapter = VerseAdapter(
            context = this,
            userSettings = userSettings,
            overrideTranslation = overrideTranslation,
            forceShowTranslation = forceShowTranslation,
            forceShowArabic = forceShowArabic,
            onPlayClick = { verse ->
                if (audioPlayer.currentlyPlayingVerse == verse.verseNumber && audioPlayer.isPlaying) {
                    audioPlayer.pause()
                } else if (audioPlayer.currentlyPlayingVerse == verse.verseNumber && !audioPlayer.isPlaying) {
                    audioPlayer.resume()
                } else {
                    audioPlayer.playSingleVerse(surahId, verse.verseNumber)
                }
            },
            onMorphologyClick = { verse ->
                val intent = Intent(this, MorphologyActivity::class.java).apply {
                    putExtra(MorphologyActivity.EXTRA_SURAH, verse.surahNumber)
                    putExtra(MorphologyActivity.EXTRA_VERSE, verse.verseNumber)
                }
                startActivity(intent)
            },
            onTafsirClick = { verse ->
                val intent = Intent(this, TafsirActivity::class.java).apply {
                    putExtra(TafsirActivity.EXTRA_SURAH, verse.surahNumber)
                    putExtra(TafsirActivity.EXTRA_VERSE, verse.verseNumber)
                    putExtra(TafsirActivity.EXTRA_GLOBAL_ID, verse.id)
                }
                startActivity(intent)
            }
        )

        binding.recyclerViewVerses.layoutManager = LinearLayoutManager(this)
        binding.recyclerViewVerses.adapter = adapter

        thread(start = true, name = "verses-loader") {
            verses = quranDbHelper.getVersesForSurah(surahId)
            runOnUiThread {
                adapter.submitList(verses)
                if (targetVerse > 0) {
                    val targetPos = verses.indexOfFirst { it.verseNumber == targetVerse }
                    if (targetPos != -1) {
                        binding.recyclerViewVerses.scrollToPosition(targetPos)
                    }
                }
            }
        }
    }

    private fun toggleSurahPlayback() {
        if (audioPlayer.isPlaying) {
            audioPlayer.pause()
        } else if (audioPlayer.currentlyPlayingVerse != null) {
            audioPlayer.resume()
        } else {
            val firstVisiblePos = (binding.recyclerViewVerses.layoutManager as? LinearLayoutManager)
                ?.findFirstVisibleItemPosition() ?: 0
            val startVerse = if (firstVisiblePos in verses.indices) verses[firstVisiblePos].verseNumber else 1
            audioPlayer.playFullSurah(surahId, startVerse)
        }
    }

    private fun setupAudioPlayerCallbacks() {
        audioPlayer.onVerseStarted = { verseNumber ->
            adapter.setPlaybackState(verseNumber, isPlaying = true)
            binding.cardPlaybackBar.visibility = View.VISIBLE
            updatePlaybackBarLabels(verseNumber)
            binding.btnPlaybackPlayPause.setIconResource(R.drawable.ic_pause)
            updateToolbarPlayIcon(isPlaying = true)

            // Scroll to the active playing verse
            val pos = verses.indexOfFirst { it.verseNumber == verseNumber }
            if (pos != -1) {
                binding.recyclerViewVerses.smoothScrollToPosition(pos)
            }
        }

        audioPlayer.onSegmentChanged = { _ ->
            updatePlaybackBarLabels(audioPlayer.currentlyPlayingVerse)
        }

        audioPlayer.onStateChanged = { isPlaying, isBuffering ->
            adapter.setPlaybackState(audioPlayer.currentlyPlayingVerse, isPlaying)
            binding.btnPlaybackPlayPause.setIconResource(if (isPlaying) R.drawable.ic_pause else R.drawable.ic_play_arrow)
            updateToolbarPlayIcon(isPlaying)
            if (!isPlaying && audioPlayer.currentlyPlayingVerse == null) {
                binding.cardPlaybackBar.visibility = View.GONE
            }
        }

        audioPlayer.onPlaybackCompleted = {
            adapter.setPlaybackState(null, isPlaying = false)
            binding.cardPlaybackBar.visibility = View.GONE
            updateToolbarPlayIcon(isPlaying = false)
        }

        audioPlayer.onError = { msg ->
            Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
            adapter.setPlaybackState(null, isPlaying = false)
            binding.cardPlaybackBar.visibility = View.GONE
            updateToolbarPlayIcon(isPlaying = false)
        }
    }

    private fun setupPlaybackBarControls() {
        binding.btnPlaybackPlayPause.setOnClickListener {
            if (audioPlayer.isPlaying) {
                audioPlayer.pause()
            } else {
                audioPlayer.resume()
            }
        }

        binding.btnPlaybackNext.setOnClickListener {
            audioPlayer.nextVerse()
        }

        binding.btnPlaybackPrev.setOnClickListener {
            audioPlayer.previousVerse()
        }

        binding.btnPlaybackClose.setOnClickListener {
            audioPlayer.stop()
            binding.cardPlaybackBar.visibility = View.GONE
            adapter.setPlaybackState(null, isPlaying = false)
            updateToolbarPlayIcon(isPlaying = false)
        }
    }

    private fun updatePlaybackBarLabels(verseNumber: Int? = audioPlayer.currentlyPlayingVerse) {
        verseNumber?.let { v ->
            binding.textPlaybackVerse.text = "Ayah $surahId:$v"
        }
        val arabicReciter = Reciter.getArabicReciter(userSettings.reciterIdentifier)
        val transReciter = Reciter.getTranslationReciter(userSettings.translationReciterIdentifier)
        val mode = userSettings.audioRecitationMode

        val label = when {
            mode == QuranAudioPlayer.MODE_TRANSLATION_ONLY -> transReciter.name
            mode == QuranAudioPlayer.MODE_BOTH -> {
                if (audioPlayer.isTranslationSubSegment) {
                    "${transReciter.name} (Translation)"
                } else {
                    "${arabicReciter.name} (Arabic)"
                }
            }
            else -> arabicReciter.name
        }
        binding.textPlaybackReciter.text = label
    }

    private fun updateToolbarPlayIcon(isPlaying: Boolean) {
        val playItem = binding.toolbar.menu.findItem(R.id.action_play_surah) ?: return
        playItem.setIcon(if (isPlaying) R.drawable.ic_pause else R.drawable.ic_play_arrow)
        playItem.title = if (isPlaying) "Pause" else "Play Surah"
    }

    override fun onResume() {
        super.onResume()
        if (::adapter.isInitialized) {
            adapter.notifyDataSetChanged()
        }
    }

    override fun onStop() {
        super.onStop()
        // Stop audio playback immediately when leaving the app or activity
        audioPlayer.stop()
    }

    override fun onDestroy() {
        super.onDestroy()
        audioPlayer.release()
    }
}
