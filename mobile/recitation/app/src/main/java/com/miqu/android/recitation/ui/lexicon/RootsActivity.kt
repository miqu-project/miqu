package com.miqu.android.recitation.ui.lexicon

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
import com.miqu.android.recitation.data.LexiconRepository
import com.miqu.android.recitation.data.RootsDatabaseHelper
import com.miqu.android.recitation.databinding.ActivityRootsBinding
import com.miqu.android.recitation.model.RootEntry
import kotlin.concurrent.thread

class RootsActivity : AppCompatActivity() {

    private lateinit var binding: ActivityRootsBinding
    private lateinit var rootsDbHelper: RootsDatabaseHelper
    private lateinit var lexiconRepo: LexiconRepository
    private lateinit var adapter: LexiconAdapter
    private var allRoots: List<RootEntry> = emptyList()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        binding = ActivityRootsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ViewCompat.setOnApplyWindowInsetsListener(binding.appBarLayout) { v, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            v.updatePadding(top = statusBars.top)
            insets
        }

        ViewCompat.setOnApplyWindowInsetsListener(binding.recyclerViewRoots) { v, insets ->
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
        lexiconRepo = LexiconRepository(this)
        adapter = LexiconAdapter { entry ->
            val intent = Intent(this, RootDetailActivity::class.java).apply {
                putExtra(RootDetailActivity.EXTRA_ROOT, entry.root)
                putExtra(RootDetailActivity.EXTRA_DEFINITION, entry.definition)
            }
            startActivity(intent)
        }

        binding.recyclerViewRoots.layoutManager = LinearLayoutManager(this)
        binding.recyclerViewRoots.adapter = adapter

        binding.searchRootsEditText.doAfterTextChanged { text ->
            filterRoots(text?.toString()?.trim() ?: "")
        }

        loadRootsData()
    }

    private fun loadRootsData() {
        thread(start = true, name = "roots-activity-loader") {
            val dbRoots = rootsDbHelper.getRootsByFrequency(limit = 2000)
            allRoots = dbRoots.map { entry ->
                val def = lexiconRepo.getRootEntry(entry.root)?.definition ?: ""
                entry.copy(definition = def)
            }
            runOnUiThread {
                if (isFinishing || isDestroyed) return@runOnUiThread
                filterRoots(binding.searchRootsEditText.text?.toString()?.trim() ?: "")
            }
        }
    }

    private fun filterRoots(query: String) {
        val filtered = if (query.isBlank()) {
            allRoots
        } else {
            allRoots.filter {
                it.root.contains(query) || it.definition.contains(query, ignoreCase = true)
            }
        }
        adapter.submitList(filtered)
        binding.textRootsEmpty.visibility = if (filtered.isEmpty()) View.VISIBLE else View.GONE
    }
}
