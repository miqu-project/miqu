package com.miqu.android.recitation.ui.reader

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.recitation.data.TafsirRepository
import com.miqu.android.recitation.data.UserSettings
import com.miqu.android.recitation.databinding.ItemTafsirPageBinding
import com.miqu.android.recitation.model.Verse
import com.miqu.android.recitation.util.FontHelper
import io.noties.markwon.Markwon
import kotlin.concurrent.thread

class TafsirPagerAdapter(
    private val context: Context,
    private val verses: List<Verse>,
    private val tafsirRepo: TafsirRepository,
    private val markwon: Markwon,
    private val userSettings: UserSettings,
    var currentTafsirFile: String
) : RecyclerView.Adapter<TafsirPagerAdapter.TafsirPageViewHolder>() {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TafsirPageViewHolder {
        val binding = ItemTafsirPageBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return TafsirPageViewHolder(binding)
    }

    override fun onBindViewHolder(holder: TafsirPageViewHolder, position: Int) {
        holder.bind(verses[position])
    }

    override fun getItemCount(): Int = verses.size

    inner class TafsirPageViewHolder(
        private val binding: ItemTafsirPageBinding
    ) : RecyclerView.ViewHolder(binding.root) {

        fun bind(verse: Verse) {
            binding.textVerseKey.text = "Ayah ${verse.surahNumber}:${verse.verseNumber}"
            binding.textArabicVerse.text = verse.arabic
            binding.textArabicVerse.typeface = FontHelper.getArabicTypeface(context)

            val translationLang = userSettings.translation
            binding.textTranslation.text = verse.getTranslation(translationLang)

            val bookFile = currentTafsirFile
            binding.progressBar.visibility = View.VISIBLE
            binding.textTafsirContent.text = "Loading exegesis..."

            thread(start = true, name = "tafsir-page-loader-${verse.id}") {
                val commentary = tafsirRepo.getTafsirForVerse(bookFile, verse.id)
                binding.root.post {
                    if (currentTafsirFile == bookFile) {
                        binding.progressBar.visibility = View.GONE
                        markwon.setMarkdown(binding.textTafsirContent, commentary)
                    }
                }
            }
        }
    }
}
