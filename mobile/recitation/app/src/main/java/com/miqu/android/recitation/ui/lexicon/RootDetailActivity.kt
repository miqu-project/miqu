package com.miqu.android.recitation.ui.lexicon

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
import com.miqu.android.recitation.databinding.ActivityRootDetailBinding
import com.miqu.android.recitation.ui.reader.ReaderActivity
import kotlin.concurrent.thread

class RootDetailActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_ROOT = "extra_root"
        const val EXTRA_DEFINITION = "extra_definition"
        private const val PAGE_SIZE = 40
    }

    private lateinit var binding: ActivityRootDetailBinding
    private lateinit var adapter: RootOccurrencesAdapter
    private lateinit var rootsDbHelper: RootsDatabaseHelper
    private lateinit var surahRepo: SurahRepository

    private var currentOffset = 0
    private var totalCount = 0
    private var isLoading = false
    private var hasMore = true

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        binding = ActivityRootDetailBinding.inflate(layoutInflater)
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

        val rootText = intent.getStringExtra(EXTRA_ROOT) ?: ""
        var definition = intent.getStringExtra(EXTRA_DEFINITION) ?: ""
        if (definition.isEmpty()) {
            definition = com.miqu.android.recitation.data.LexiconRepository(this).getRootEntry(rootText)?.definition ?: ""
        }
        val fullDefinition = definition.ifEmpty { "Root form: $rootText" }
        binding.toolbar.title = "Root: \u200E$rootText"

        rootsDbHelper = RootsDatabaseHelper.getInstance(this)
        surahRepo = SurahRepository(this)

        adapter = RootOccurrencesAdapter(
            headerRoot = rootText,
            headerDefinition = fullDefinition
        ) { wordRoot ->
            val surah = surahRepo.getSurahById(wordRoot.surah)
            val intent = Intent(this, ReaderActivity::class.java).apply {
                putExtra(ReaderActivity.EXTRA_SURAH_ID, wordRoot.surah)
                putExtra(ReaderActivity.EXTRA_SURAH_NAME, surah?.name ?: "")
                putExtra(ReaderActivity.EXTRA_SURAH_TRANSLITERATION, surah?.transliteration ?: "Surah ${wordRoot.surah}")
                putExtra(ReaderActivity.EXTRA_TOTAL_VERSES, surah?.totalVerses ?: 0)
                putExtra(ReaderActivity.EXTRA_TARGET_VERSE, wordRoot.verse)
            }
            startActivity(intent)
        }

        val layoutManager = LinearLayoutManager(this)
        binding.recyclerViewOccurrences.layoutManager = layoutManager
        binding.recyclerViewOccurrences.adapter = adapter

        binding.recyclerViewOccurrences.addOnScrollListener(object : RecyclerView.OnScrollListener() {
            override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
                super.onScrolled(recyclerView, dx, dy)
                if (dy <= 0) return
                val visibleItemCount = layoutManager.childCount
                val totalItemCount = layoutManager.itemCount
                val firstVisibleItemPosition = layoutManager.findFirstVisibleItemPosition()

                if (!isLoading && hasMore) {
                    if (visibleItemCount + firstVisibleItemPosition >= totalItemCount - 8) {
                        loadNextPage(rootText)
                    }
                }
            }
        })

        loadNextPage(rootText)
    }

    private fun loadNextPage(rootText: String) {
        if (isLoading || !hasMore) return
        isLoading = true
        binding.progressLoadingMore.visibility = if (currentOffset > 0) android.view.View.VISIBLE else android.view.View.GONE

        thread(start = true, name = "root-occurrences-page-$currentOffset") {
            if (currentOffset == 0) {
                totalCount = rootsDbHelper.getRootOccurrenceCount(rootText)
            }
            val page = rootsDbHelper.getOccurrencesForRoot(rootText, limit = PAGE_SIZE, offset = currentOffset)

            runOnUiThread {
                if (isFinishing || isDestroyed) return@runOnUiThread

                if (currentOffset == 0) {
                    adapter.updateTotalCount(totalCount)
                    adapter.submitList(page)
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
