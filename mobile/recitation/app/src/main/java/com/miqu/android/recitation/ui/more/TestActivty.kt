package com.miqu.android.recitation.ui.more

import android.content.Context
import android.os.Bundle
import android.view.View
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.viewpager2.widget.ViewPager2
import com.miqu.android.recitation.databinding.ActivityTestBinding

open class TestActivty : AppCompatActivity() {

    private lateinit var binding: ActivityTestBinding
    private lateinit var pageAdapter: QuranPageAdapter

    companion object {
        const val TOTAL_PAGES = 615
        const val TARGET_ASPECT_RATIO = 500.0 / 820.0
        private const val PREFS_NAME = "test_quran_mushaf_prefs"
        private const val KEY_LAST_PAGE = "key_last_page"
        const val EXTRA_PAGE_NUMBER = "extra_page_number"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityTestBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Immersive full screen
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val controller = WindowCompat.getInsetsController(window, window.decorView)
        controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        controller.hide(WindowInsetsCompat.Type.systemBars())

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                finish()
            }
        })

        setupViewPager()

        binding.rootContainer.addOnLayoutChangeListener { _, _, _, _, _, _, _, _, _ ->
            updateAspectRatio()
        }
        binding.rootContainer.post {
            updateAspectRatio()
        }
    }

    private fun setupViewPager() {
        pageAdapter = QuranPageAdapter(this, TOTAL_PAGES)

        binding.viewPager.apply {
            adapter = pageAdapter
            // RTL for authentic Arabic / Quran book reading direction
            layoutDirection = View.LAYOUT_DIRECTION_RTL
            offscreenPageLimit = 1

            registerOnPageChangeCallback(object : ViewPager2.OnPageChangeCallback() {
                override fun onPageSelected(position: Int) {
                    super.onPageSelected(position)
                    saveLastPage(position + 1)
                }
            })
        }

        // Default to Page 4 (the test page) or last viewed page
        val initialPage = intent.getIntExtra(
            EXTRA_PAGE_NUMBER,
            getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).getInt(KEY_LAST_PAGE, 4)
        ).coerceIn(1, TOTAL_PAGES)

        binding.viewPager.setCurrentItem(initialPage - 1, false)
    }

    private fun updateAspectRatio() {
        val container = binding.rootContainer
        if (container.width <= 0 || container.height <= 0) return

        val marginPx = (8 * resources.displayMetrics.density).toInt() * 2
        val availableW = container.width - marginPx
        val availableH = container.height - marginPx

        if (availableW <= 0 || availableH <= 0) return

        val frameW: Int
        val frameH: Int

        if (availableW.toDouble() / availableH > TARGET_ASPECT_RATIO) {
            frameH = availableH
            frameW = (frameH * TARGET_ASPECT_RATIO).toInt()
        } else {
            frameW = availableW
            frameH = (frameW / TARGET_ASPECT_RATIO).toInt()
        }

        if (pageAdapter.targetFrameWidth != frameW || pageAdapter.targetFrameHeight != frameH) {
            pageAdapter.targetFrameWidth = frameW
            pageAdapter.targetFrameHeight = frameH
            pageAdapter.notifyDataSetChanged()
        }
    }

    private fun saveLastPage(pageNum: Int) {
        getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putInt(KEY_LAST_PAGE, pageNum)
            .apply()
    }
}

class TestActivity : TestActivty()
