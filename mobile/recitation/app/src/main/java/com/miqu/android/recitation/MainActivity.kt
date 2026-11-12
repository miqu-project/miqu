package com.miqu.android.recitation

import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.core.widget.doAfterTextChanged
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.search.SearchView
import com.miqu.android.recitation.data.UnifiedSearchRepository
import com.miqu.android.recitation.databinding.ActivityMainBinding
import com.miqu.android.recitation.model.SearchSuggestion
import com.miqu.android.recitation.ui.learn.LearnFragment
import com.miqu.android.recitation.ui.lexicon.RootDetailActivity
import com.miqu.android.recitation.ui.more.MoreFragment
import com.miqu.android.recitation.ui.reader.ReaderActivity
import com.miqu.android.recitation.ui.search.SearchSuggestionAdapter
import com.miqu.android.recitation.ui.surah.SurahListFragment
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var searchRepo: UnifiedSearchRepository
    private lateinit var searchAdapter: SearchSuggestionAdapter

    private val searchHandler = Handler(Looper.getMainLooper())
    private var searchRunnable: Runnable? = null

    private val surahListFragment = SurahListFragment()
    private val learnFragment = LearnFragment()
    private val moreFragment = MoreFragment()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        searchRepo = UnifiedSearchRepository(this)

        ViewCompat.setOnApplyWindowInsetsListener(binding.appBarLayout) { v, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            v.updatePadding(top = statusBars.top)
            insets
        }

        ViewCompat.setOnApplyWindowInsetsListener(binding.bottomNavigation) { v, insets ->
            val navBars = insets.getInsets(WindowInsetsCompat.Type.navigationBars())
            v.updatePadding(bottom = navBars.bottom)
            insets
        }

        setupSearch()

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (binding.searchView.isShowing) {
                    binding.searchView.hide()
                } else {
                    isEnabled = false
                    onBackPressedDispatcher.onBackPressed()
                }
            }
        })

        if (savedInstanceState == null) {
            switchFragment(surahListFragment)
        }

        binding.bottomNavigation.setOnItemSelectedListener { item ->
            when (item.itemId) {
                R.id.nav_surahs -> {
                    switchFragment(surahListFragment)
                    true
                }
                R.id.nav_learn -> {
                    switchFragment(learnFragment)
                    true
                }
                R.id.nav_more -> {
                    switchFragment(moreFragment)
                    true
                }
                else -> false
            }
        }

        handleSearchIntent(intent)
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleSearchIntent(intent)
    }

    private fun handleSearchIntent(intent: Intent?) {
        val query = intent?.getStringExtra("search_query") ?: return
        binding.searchBar.post {
            binding.searchView.show()
            binding.searchView.setText(query)
            binding.searchView.editText.setSelection(query.length)
        }
    }

    private fun setupSearch() {
        binding.searchView.setupWithSearchBar(binding.searchBar)

        searchAdapter = SearchSuggestionAdapter { suggestion ->
            handleSuggestionClick(suggestion)
        }

        binding.recyclerSearchResults.layoutManager = LinearLayoutManager(this)
        binding.recyclerSearchResults.adapter = searchAdapter

        binding.searchView.editText.doAfterTextChanged { text ->
            val q = text?.toString() ?: ""
            searchRunnable?.let { searchHandler.removeCallbacks(it) }
            val runnable = Runnable {
                executeSearch(q)
            }
            searchRunnable = runnable
            searchHandler.postDelayed(runnable, 200)
        }

        binding.searchView.addTransitionListener { _, _, newState ->
            if (newState == SearchView.TransitionState.SHOWN) {
                executeSearch(binding.searchView.text.toString())
            }
        }
    }

    private fun executeSearch(query: String) {
        val q = query.trim()
        if (q.isNotEmpty()) {
            binding.progressSearch.visibility = View.VISIBLE
        }
        thread(start = true, name = "unified-search") {
            val results = searchRepo.search(q)
            runOnUiThread {
                binding.progressSearch.visibility = View.GONE
                searchAdapter.submitList(results)
                binding.textNoResults.visibility =
                    if (results.isEmpty() && q.isNotEmpty()) View.VISIBLE else View.GONE
            }
        }
    }

    private fun handleSuggestionClick(item: SearchSuggestion) {
        when (item) {
            is SearchSuggestion.SurahItem -> {
                val intent = Intent(this, ReaderActivity::class.java).apply {
                    putExtra(ReaderActivity.EXTRA_SURAH_ID, item.surah.id)
                    putExtra(ReaderActivity.EXTRA_SURAH_NAME, item.surah.name)
                    putExtra(ReaderActivity.EXTRA_SURAH_TRANSLITERATION, item.surah.transliteration)
                    putExtra(ReaderActivity.EXTRA_TOTAL_VERSES, item.surah.totalVerses)
                }
                binding.searchView.hide()
                startActivity(intent)
            }

            is SearchSuggestion.AyahJumpItem -> {
                val intent = Intent(this, ReaderActivity::class.java).apply {
                    putExtra(ReaderActivity.EXTRA_SURAH_ID, item.surahId)
                    putExtra(ReaderActivity.EXTRA_SURAH_NAME, item.surahName)
                    putExtra(ReaderActivity.EXTRA_SURAH_TRANSLITERATION, item.surahName)
                    putExtra(ReaderActivity.EXTRA_TOTAL_VERSES, item.totalVerses)
                    putExtra(ReaderActivity.EXTRA_TARGET_VERSE, item.ayahNumber)
                }
                binding.searchView.hide()
                startActivity(intent)
            }

            is SearchSuggestion.VerseTextItem -> {
                val intent = Intent(this, ReaderActivity::class.java).apply {
                    putExtra(ReaderActivity.EXTRA_SURAH_ID, item.verse.surahNumber)
                    putExtra(ReaderActivity.EXTRA_SURAH_NAME, item.surahName)
                    putExtra(ReaderActivity.EXTRA_SURAH_TRANSLITERATION, item.surahName)
                    putExtra(ReaderActivity.EXTRA_TARGET_VERSE, item.verse.verseNumber)
                    if (item.translationKey != null) {
                        putExtra(ReaderActivity.EXTRA_OVERRIDE_TRANSLATION, item.translationKey)
                        putExtra(ReaderActivity.EXTRA_FORCE_SHOW_TRANSLATION, true)
                    } else if (item.translationMarker == "Arabic") {
                        putExtra(ReaderActivity.EXTRA_FORCE_SHOW_ARABIC, true)
                    }
                }
                binding.searchView.hide()
                startActivity(intent)
            }

            is SearchSuggestion.RootItem -> {
                val intent = Intent(this, RootDetailActivity::class.java).apply {
                    putExtra(RootDetailActivity.EXTRA_ROOT, item.rootEntry.root)
                    putExtra(RootDetailActivity.EXTRA_DEFINITION, item.rootEntry.definition)
                }
                binding.searchView.hide()
                startActivity(intent)
            }
        }
    }

    private fun switchFragment(fragment: Fragment) {
        supportFragmentManager.beginTransaction()
            .replace(R.id.nav_host_fragment, fragment)
            .commit()
    }
}
