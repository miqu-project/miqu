package com.miqu.android.recitation.ui.reader

import android.os.Bundle
import android.widget.ArrayAdapter
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.viewpager2.widget.ViewPager2
import com.miqu.android.recitation.data.QuranDatabaseHelper
import com.miqu.android.recitation.data.SurahRepository
import com.miqu.android.recitation.data.TafsirRepository
import com.miqu.android.recitation.data.UserSettings
import com.miqu.android.recitation.databinding.ActivityTafsirBinding
import com.miqu.android.recitation.model.Verse
import io.noties.markwon.Markwon
import kotlin.concurrent.thread

class TafsirActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_SURAH = "extra_surah"
        const val EXTRA_VERSE = "extra_verse"
        const val EXTRA_GLOBAL_ID = "extra_global_id"
    }

    private lateinit var binding: ActivityTafsirBinding
    private lateinit var tafsirRepo: TafsirRepository
    private lateinit var userSettings: UserSettings
    private lateinit var surahRepo: SurahRepository
    private lateinit var quranDbHelper: QuranDatabaseHelper
    private lateinit var markwon: Markwon
    private lateinit var pagerAdapter: TafsirPagerAdapter

    private var verses: List<Verse> = emptyList()
    private var selectedTafsirFile: String = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        binding = ActivityTafsirBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ViewCompat.setOnApplyWindowInsetsListener(binding.appBarLayout) { v, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            v.updatePadding(top = statusBars.top)
            insets
        }

        ViewCompat.setOnApplyWindowInsetsListener(binding.viewPagerTafsir) { v, insets ->
            val navBars = insets.getInsets(WindowInsetsCompat.Type.navigationBars())
            v.updatePadding(bottom = navBars.bottom)
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

        val surah = intent.getIntExtra(EXTRA_SURAH, 1)
        val targetVerse = intent.getIntExtra(EXTRA_VERSE, 1)

        surahRepo = SurahRepository(this)
        val surahItem = surahRepo.getSurahById(surah)
        val surahName = surahItem?.transliteration ?: "Surah $surah"
        binding.toolbar.title = "Tafsir"
        binding.toolbar.subtitle = "$surahName • Ayah $targetVerse"

        tafsirRepo = TafsirRepository(this)
        userSettings = UserSettings(this)
        quranDbHelper = QuranDatabaseHelper.getInstance(this)
        markwon = Markwon.create(this)
        selectedTafsirFile = userSettings.tafsir

        val tafsirBooks = TafsirRepository.AVAILABLE_TAFSIRS
        val displayList = tafsirBooks.map { it.displayName }
        val dropdownAdapter = ArrayAdapter(this, android.R.layout.simple_dropdown_item_1line, displayList)
        binding.dropdownTafsirSelector.setAdapter(dropdownAdapter)

        val currentIndex = tafsirBooks.indexOfFirst { it.fileName == selectedTafsirFile }.coerceAtLeast(0)
        binding.dropdownTafsirSelector.setText(displayList[currentIndex], false)

        binding.dropdownTafsirSelector.setOnItemClickListener { _, _, position, _ ->
            selectedTafsirFile = tafsirBooks[position].fileName
            userSettings.tafsir = selectedTafsirFile
            if (::pagerAdapter.isInitialized) {
                pagerAdapter.currentTafsirFile = selectedTafsirFile
                pagerAdapter.notifyDataSetChanged()
            }
        }

        thread(start = true, name = "tafsir-verses-loader") {
            verses = quranDbHelper.getVersesForSurah(surah)
            runOnUiThread {
                if (isFinishing || isDestroyed) return@runOnUiThread

                pagerAdapter = TafsirPagerAdapter(
                    context = this,
                    verses = verses,
                    tafsirRepo = tafsirRepo,
                    markwon = markwon,
                    userSettings = userSettings,
                    currentTafsirFile = selectedTafsirFile
                )

                binding.viewPagerTafsir.adapter = pagerAdapter
                binding.viewPagerTafsir.offscreenPageLimit = 1

                val initialPos = verses.indexOfFirst { it.verseNumber == targetVerse }.coerceAtLeast(0)
                binding.viewPagerTafsir.setCurrentItem(initialPos, false)

                if (verses.isNotEmpty()) {
                    val activeVerse = verses[initialPos]
                    binding.toolbar.subtitle = "$surahName • Ayah ${activeVerse.verseNumber} of ${verses.size}"
                }

                binding.viewPagerTafsir.registerOnPageChangeCallback(object : ViewPager2.OnPageChangeCallback() {
                    override fun onPageSelected(position: Int) {
                        super.onPageSelected(position)
                        if (position in verses.indices) {
                            val verse = verses[position]
                            binding.toolbar.subtitle = "$surahName • Ayah ${verse.verseNumber} of ${verses.size}"
                        }
                    }
                })
            }
        }
    }
}
