package com.miqu.android.recitation.ui.reader

import android.content.Intent
import android.os.Bundle
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.recitation.data.RootsDatabaseHelper
import com.miqu.android.recitation.data.SurahRepository
import com.miqu.android.recitation.databinding.ActivityWordDetailBinding
import kotlin.concurrent.thread

class WordDetailActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_ARABIC_WORD = "extra_arabic_word"
        const val EXTRA_CURRENT_MEANING = "extra_current_meaning"
        const val EXTRA_ROOT = "extra_root"
        private const val PAGE_SIZE = 40
    }

    private lateinit var binding: ActivityWordDetailBinding
    private lateinit var adapter: ExactWordOccurrencesAdapter
    private lateinit var rootsDbHelper: RootsDatabaseHelper
    private lateinit var surahRepo: SurahRepository

    private var currentOffset = 0
    private var totalCount = 0
    private var isLoading = false
    private var hasMore = true
    private var wordRoot: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        binding = ActivityWordDetailBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ViewCompat.setOnApplyWindowInsetsListener(binding.appBarLayout) { v, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            v.updatePadding(top = statusBars.top)
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

        val arabicWord = intent.getStringExtra(EXTRA_ARABIC_WORD) ?: ""
        wordRoot = intent.getStringExtra(EXTRA_ROOT)?.trim()

        binding.textExactWordLarge.text = arabicWord
        binding.textExactWordLarge.typeface = com.miqu.android.recitation.util.FontHelper.getArabicTypeface(this)
        binding.toolbar.title = "Word: $arabicWord"

        rootsDbHelper = RootsDatabaseHelper.getInstance(this)
        surahRepo = SurahRepository(this)

        adapter = ExactWordOccurrencesAdapter(surahRepo) { occurrence ->
            val surah = surahRepo.getSurahById(occurrence.surah)
            val intent = Intent(this, ReaderActivity::class.java).apply {
                putExtra(ReaderActivity.EXTRA_SURAH_ID, occurrence.surah)
                putExtra(ReaderActivity.EXTRA_SURAH_NAME, surah?.name ?: "")
                putExtra(
                    ReaderActivity.EXTRA_SURAH_TRANSLITERATION,
                    surah?.transliteration ?: "Surah ${occurrence.surah}"
                )
                putExtra(ReaderActivity.EXTRA_TOTAL_VERSES, surah?.totalVerses ?: 0)
                putExtra(ReaderActivity.EXTRA_TARGET_VERSE, occurrence.verse)
            }
            startActivity(intent)
        }

        val layoutManager = LinearLayoutManager(this)
        binding.recyclerViewWordOccurrences.layoutManager = layoutManager
        binding.recyclerViewWordOccurrences.adapter = adapter

        binding.recyclerViewWordOccurrences.addOnScrollListener(object : RecyclerView.OnScrollListener() {
            override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
                super.onScrolled(recyclerView, dx, dy)
                if (dy <= 0) return
                val visibleItemCount = layoutManager.childCount
                val totalItemCount = layoutManager.itemCount
                val firstVisibleItemPosition = layoutManager.findFirstVisibleItemPosition()

                if (!isLoading && hasMore) {
                    if (visibleItemCount + firstVisibleItemPosition >= totalItemCount - 8) {
                        loadNextPage(arabicWord)
                    }
                }
            }
        })

        loadNextPage(arabicWord)
    }

    private fun loadNextPage(arabicWord: String) {
        if (isLoading || !hasMore) return
        isLoading = true
        binding.progressLoadingMore.visibility = if (currentOffset > 0) android.view.View.VISIBLE else android.view.View.GONE

        thread(start = true, name = "word-occurrences-page-$currentOffset") {
            if (currentOffset == 0) {
                totalCount = rootsDbHelper.getExactWordOccurrenceCount(arabicWord)
            }
            val page = rootsDbHelper.getExactWordOccurrences(arabicWord, limit = PAGE_SIZE, offset = currentOffset)

            if (wordRoot.isNullOrBlank()) {
                wordRoot = page.firstOrNull { it.root.isNotBlank() }?.root?.trim()
            }

            runOnUiThread {
                if (isFinishing || isDestroyed) return@runOnUiThread

                if (currentOffset == 0) {
                    binding.textExactWordCount.text = "$totalCount Occurrences"
                    adapter.submitList(page)

                    if (!wordRoot.isNullOrBlank()) {
                        binding.cardWordRootLink.visibility = android.view.View.VISIBLE
                        binding.textWordRootLabel.text = "Root: $wordRoot  ›"
                        binding.cardWordRootLink.setOnClickListener {
                            val rootIntent = Intent(this, com.miqu.android.recitation.ui.lexicon.RootDetailActivity::class.java).apply {
                                putExtra(com.miqu.android.recitation.ui.lexicon.RootDetailActivity.EXTRA_ROOT, wordRoot)
                                val def = com.miqu.android.recitation.data.LexiconRepository(this@WordDetailActivity).getRootEntry(wordRoot!!)?.definition ?: ""
                                putExtra(com.miqu.android.recitation.ui.lexicon.RootDetailActivity.EXTRA_DEFINITION, def)
                            }
                            startActivity(rootIntent)
                        }
                    } else {
                        binding.cardWordRootLink.visibility = android.view.View.GONE
                    }
                } else {
                    adapter.appendList(page)
                }

                currentOffset += page.size
                hasMore = page.size >= PAGE_SIZE && currentOffset < totalCount
                isLoading = false
                binding.progressLoadingMore.visibility = android.view.View.GONE
            }
        }
    }
}
