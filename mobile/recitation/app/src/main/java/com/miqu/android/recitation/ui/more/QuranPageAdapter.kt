package com.miqu.android.recitation.ui.more

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.PorterDuff
import android.os.Handler
import android.os.Looper
import android.util.LruCache
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.color.MaterialColors
import com.miqu.android.recitation.databinding.ItemTestPageBinding
import java.io.File
import java.util.concurrent.Executors

class QuranPageAdapter(
    private val context: Context,
    private val totalPages: Int = 610
) : RecyclerView.Adapter<QuranPageAdapter.PageViewHolder>() {

    private val mainHandler = Handler(Looper.getMainLooper())
    private val executor = Executors.newFixedThreadPool(4)

    var targetFrameWidth: Int = 0
    var targetFrameHeight: Int = 0

    // Cache line bitmaps in memory using ~1/8 of available heap
    private val cacheSize = ((Runtime.getRuntime().maxMemory() / 1024) / 8).toInt().coerceAtLeast(16 * 1024)
    private val bitmapCache = object : LruCache<String, Bitmap>(cacheSize) {
        override fun sizeOf(key: String, bitmap: Bitmap): Int {
            return (bitmap.byteCount / 1024).coerceAtLeast(1)
        }
    }

    override fun getItemCount(): Int = totalPages

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): PageViewHolder {
        val binding = ItemTestPageBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return PageViewHolder(binding)
    }

    override fun onBindViewHolder(holder: PageViewHolder, position: Int) {
        val pageNum = position + 1
        holder.bind(pageNum)
    }

    override fun onViewRecycled(holder: PageViewHolder) {
        super.onViewRecycled(holder)
        holder.cancelLoading()
    }

    inner class PageViewHolder(val binding: ItemTestPageBinding) : RecyclerView.ViewHolder(binding.root) {

        private var currentBoundPage: Int = -1

        private val lineImageViews = listOf(
            binding.imageLine01,
            binding.imageLine02,
            binding.imageLine03,
            binding.imageLine04,
            binding.imageLine05,
            binding.imageLine06,
            binding.imageLine07,
            binding.imageLine08,
            binding.imageLine09,
            binding.imageLine10,
            binding.imageLine11,
            binding.imageLine12,
            binding.imageLine13,
            binding.imageLine14,
            binding.imageLine15
        )

        private val dividerViews = listOf(
            binding.divider01,
            binding.divider02,
            binding.divider03,
            binding.divider04,
            binding.divider05,
            binding.divider06,
            binding.divider07,
            binding.divider08,
            binding.divider09,
            binding.divider10,
            binding.divider11,
            binding.divider12,
            binding.divider13,
            binding.divider14
        )

        fun cancelLoading() {
            currentBoundPage = -1
            lineImageViews.forEach { it.setImageBitmap(null) }
        }

        fun bind(pageNum: Int) {
            currentBoundPage = pageNum

            if (targetFrameWidth > 0 && targetFrameHeight > 0) {
                val params = binding.cardPageFrame.layoutParams as FrameLayout.LayoutParams
                if (params.width != targetFrameWidth || params.height != targetFrameHeight) {
                    params.width = targetFrameWidth
                    params.height = targetFrameHeight
                    params.gravity = Gravity.CENTER
                    binding.cardPageFrame.layoutParams = params
                }
            }

            lineImageViews.forEach { it.setImageBitmap(null) }

            val tintColor = MaterialColors.getColor(
                binding.root,
                com.google.android.material.R.attr.colorOnSurface
            )

            // Determine slot layout:
            // 7 lines (pages 1 & 2): bottom 7 rows (indices 8..14)
            // 10 lines (page 610): top 10 rows (indices 0..9)
            // 14 or 15 lines: top rows (indices 0..13 or 0..14)
            val (startRowIndex, expectedLines) = when (pageNum) {
                1, 2 -> 8 to 7
                610 -> 0 to 10
                else -> 0 to 15
            }

            // Set dividers: visible only between rows that contain lines
            dividerViews.forEachIndexed { index, divider ->
                val isBetweenActiveLines = index >= startRowIndex && index < (startRowIndex + expectedLines - 1)
                divider.visibility = if (isBetweenActiveLines) View.VISIBLE else View.INVISIBLE
            }

            val expectedPage = pageNum
            executor.execute {
                val bitmaps = mutableMapOf<Int, Bitmap>()
                for (lineIdx in 1..expectedLines) {
                    val bm = loadLineBitmap(context, expectedPage, lineIdx)
                    if (bm != null) {
                        bitmaps[lineIdx] = bm
                    }
                }

                mainHandler.post {
                    if (currentBoundPage != expectedPage) return@post

                    for (lineIdx in 1..expectedLines) {
                        val rowIndex = startRowIndex + (lineIdx - 1)
                        if (rowIndex in lineImageViews.indices) {
                            val iv = lineImageViews[rowIndex]
                            val bm = bitmaps[lineIdx]
                            iv.setImageBitmap(bm)
                            if (bm != null) {
                                iv.setColorFilter(tintColor, PorterDuff.Mode.SRC_IN)
                            }
                        }
                    }
                }
            }
        }
    }

    private fun loadLineBitmap(context: Context, pageNum: Int, lineNum: Int): Bitmap? {
        val pageStr = String.format("page_%03d", pageNum)
        val baseName = String.format("line_%02d", lineNum)
        val cacheKey = "$pageStr/$baseName"

        bitmapCache.get(cacheKey)?.let { return it }

        // Try .webp first, then fallback to .png
        val extensions = listOf("webp", "png")

        for (ext in extensions) {
            val fileName = "$baseName.$ext"
            val assetPath = "quran_lines/$pageStr/$fileName"

            // 1. Try explicit path (development/test):
            val explicitFile = File("/home/anisur/projects/miqu-android-apps/recitation/app/src/main/assets/$assetPath")
            if (explicitFile.exists() && explicitFile.canRead()) {
                try {
                    val bm = BitmapFactory.decodeFile(explicitFile.absolutePath)
                    if (bm != null) {
                        bitmapCache.put(cacheKey, bm)
                        return bm
                    }
                } catch (_: Exception) {}
            }

            // 2. Try packaged assets:
            try {
                context.assets.open(assetPath).use { stream ->
                    val bm = BitmapFactory.decodeStream(stream)
                    if (bm != null) {
                        bitmapCache.put(cacheKey, bm)
                        return bm
                    }
                }
            } catch (_: Exception) {}
        }

        return null
    }
}
