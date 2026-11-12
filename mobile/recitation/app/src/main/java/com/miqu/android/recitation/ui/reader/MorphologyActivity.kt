package com.miqu.android.recitation.ui.reader

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.recyclerview.widget.LinearLayoutManager
import com.miqu.android.recitation.data.LexiconRepository
import com.miqu.android.recitation.data.RootsDatabaseHelper
import com.miqu.android.recitation.data.SurahRepository
import com.miqu.android.recitation.databinding.ActivityMorphologyBinding
import com.miqu.android.recitation.ui.lexicon.RootDetailActivity
import kotlin.concurrent.thread

class MorphologyActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_SURAH = "extra_surah"
        const val EXTRA_VERSE = "extra_verse"
    }

    private lateinit var binding: ActivityMorphologyBinding
    private lateinit var adapter: MorphologyWordAdapter
    private lateinit var rootsDbHelper: RootsDatabaseHelper
    private lateinit var lexiconRepo: LexiconRepository
    private lateinit var surahRepo: SurahRepository

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        binding = ActivityMorphologyBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ViewCompat.setOnApplyWindowInsetsListener(binding.appBarLayout) { v, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            v.updatePadding(top = statusBars.top)
            insets
        }

        ViewCompat.setOnApplyWindowInsetsListener(binding.textMorphologyAttribution) { v, insets ->
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
        val verse = intent.getIntExtra(EXTRA_VERSE, 1)

        surahRepo = SurahRepository(this)
        val surahItem = surahRepo.getSurahById(surah)
        binding.toolbar.title = "Word Morphology"
        binding.toolbar.subtitle = "${surahItem?.transliteration ?: "Surah $surah"} • Ayah $verse"

        binding.textMorphologyAttribution.setOnClickListener {
            try {
                val intent = Intent(Intent.ACTION_VIEW, Uri.parse("http://corpus.quran.com"))
                startActivity(intent)
            } catch (_: Exception) {}
        }

        rootsDbHelper = RootsDatabaseHelper.getInstance(this)
        lexiconRepo = LexiconRepository(this)

        adapter = MorphologyWordAdapter(
            onRootClick = { root ->
                val entry = lexiconRepo.getRootEntry(root)
                val intent = Intent(this, RootDetailActivity::class.java).apply {
                    putExtra(RootDetailActivity.EXTRA_ROOT, root)
                    putExtra(RootDetailActivity.EXTRA_DEFINITION, entry?.definition ?: "")
                }
                startActivity(intent)
            },
            onWordClick = { word ->
                val intent = Intent(this, WordDetailActivity::class.java).apply {
                    putExtra(WordDetailActivity.EXTRA_ARABIC_WORD, word.arabic)
                    putExtra(WordDetailActivity.EXTRA_CURRENT_MEANING, word.english)
                    putExtra(WordDetailActivity.EXTRA_ROOT, word.root)
                }
                startActivity(intent)
            }
        )

        binding.recyclerViewWords.layoutManager = LinearLayoutManager(this)
        binding.recyclerViewWords.adapter = adapter

        binding.progressBar.visibility = View.VISIBLE
        thread(start = true, name = "verse-words-loader") {
            val words = rootsDbHelper.getWordsForVerse(surah, verse)
            val segmentsMap = rootsDbHelper.getCorpusSegmentsForVerse(surah, verse)
            val combined = words.map { word ->
                MorphologyWordAdapter.WordWithGrammar(
                    wordRoot = word,
                    segments = segmentsMap[word.position] ?: emptyList()
                )
            }
            runOnUiThread {
                binding.progressBar.visibility = View.GONE
                adapter.submitList(combined)
            }
        }
    }
}
