package com.miqu.android.recitation.ui.learn

import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.core.widget.doAfterTextChanged
import androidx.recyclerview.widget.LinearLayoutManager
import com.miqu.android.recitation.data.RootsDatabaseHelper
import com.miqu.android.recitation.databinding.ActivityWordsBinding
import com.miqu.android.recitation.model.QuranWord
import com.miqu.android.recitation.ui.reader.WordDetailActivity
import kotlin.concurrent.thread

class WordsActivity : AppCompatActivity() {

    private lateinit var binding: ActivityWordsBinding
    private lateinit var rootsDbHelper: RootsDatabaseHelper
    private lateinit var adapter: WordsAdapter
    private var allWords: List<QuranWord> = emptyList()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        binding = ActivityWordsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ViewCompat.setOnApplyWindowInsetsListener(binding.appBarLayout) { v, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            v.updatePadding(top = statusBars.top)
            insets
        }

        ViewCompat.setOnApplyWindowInsetsListener(binding.recyclerViewWords) { v, insets ->
            val navBars = insets.getInsets(WindowInsetsCompat.Type.navigationBars())
            val ime = insets.getInsets(WindowInsetsCompat.Type.ime())
            val bottomPadding = maxOf(navBars.bottom, ime.bottom) + 24
            v.updatePadding(bottom = bottomPadding)
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

        rootsDbHelper = RootsDatabaseHelper.getInstance(this)
        adapter = WordsAdapter { word ->
            val intent = Intent(this, WordDetailActivity::class.java).apply {
                putExtra(WordDetailActivity.EXTRA_ARABIC_WORD, word.arabic)
                putExtra(WordDetailActivity.EXTRA_ROOT, word.root)
            }
            startActivity(intent)
        }

        binding.recyclerViewWords.layoutManager = LinearLayoutManager(this)
        binding.recyclerViewWords.adapter = adapter

        binding.searchWordsEditText.doAfterTextChanged { text ->
            filterWords(text?.toString()?.trim() ?: "")
        }

        loadWordsData()
    }

    private fun loadWordsData() {
        thread(start = true, name = "words-activity-loader") {
            allWords = rootsDbHelper.getUniqueWords(limit = 400)
            runOnUiThread {
                if (isFinishing || isDestroyed) return@runOnUiThread
                filterWords(binding.searchWordsEditText.text?.toString()?.trim() ?: "")
            }
        }
    }

    private fun filterWords(query: String) {
        if (query.isBlank()) {
            adapter.submitList(allWords)
            binding.textWordsEmpty.visibility = if (allWords.isEmpty()) View.VISIBLE else View.GONE
        } else {
            thread(start = true, name = "words-activity-search") {
                val results = rootsDbHelper.getUniqueWords(query, limit = 200)
                runOnUiThread {
                    if (isFinishing || isDestroyed) return@runOnUiThread
                    adapter.submitList(results)
                    binding.textWordsEmpty.visibility = if (results.isEmpty()) View.VISIBLE else View.GONE
                }
            }
        }
    }
}
