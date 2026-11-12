package com.miqu.android.recitation.ui.search

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.recitation.R
import com.miqu.android.recitation.databinding.ItemSearchSuggestionBinding
import com.miqu.android.recitation.model.SearchSuggestion

class SearchSuggestionAdapter(
    private val onItemClick: (SearchSuggestion) -> Unit
) : ListAdapter<SearchSuggestion, SearchSuggestionAdapter.SuggestionViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SuggestionViewHolder {
        val binding = ItemSearchSuggestionBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return SuggestionViewHolder(binding)
    }

    override fun onBindViewHolder(holder: SuggestionViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    inner class SuggestionViewHolder(private val binding: ItemSearchSuggestionBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(item: SearchSuggestion) {
            binding.root.setOnClickListener { onItemClick(item) }

            when (item) {
                is SearchSuggestion.SurahItem -> {
                    val s = item.surah
                    binding.imageSuggestionIcon.setImageResource(R.drawable.ic_quran)
                    binding.textSuggestionTitle.text = "${s.id}. ${s.transliteration} (${s.name})"
                    binding.textSuggestionBadge.text = "Surah"
                    val typeCap = s.type.replaceFirstChar { if (it.isLowerCase()) it.titlecase() else it.toString() }
                    val meaning = if (s.english.isNotEmpty()) " • ${s.english}" else ""
                    binding.textSuggestionSubtitle.text = "$typeCap • ${s.totalVerses} Verses$meaning"
                }

                is SearchSuggestion.AyahJumpItem -> {
                    binding.imageSuggestionIcon.setImageResource(R.drawable.ic_book)
                    binding.textSuggestionTitle.text = "${item.surahName} : ${item.ayahNumber}"
                    binding.textSuggestionBadge.text = "Ayah Jump"
                    binding.textSuggestionSubtitle.text = "Jump directly to Ayah ${item.ayahNumber} of ${item.totalVerses} in Surah #${item.surahId}"
                }

                is SearchSuggestion.VerseTextItem -> {
                    val v = item.verse
                    binding.imageSuggestionIcon.setImageResource(R.drawable.ic_book)
                    binding.textSuggestionTitle.text = "${item.surahName} : ${v.verseNumber}"
                    binding.textSuggestionBadge.text = if (item.translationMarker.isNotEmpty()) item.translationMarker else "Verse"
                    val markerPrefix = if (item.translationMarker.isNotEmpty()) "[${item.translationMarker}] " else ""
                    val snippet = item.matchedSnippet.ifEmpty {
                        when {
                            v.english.isNotEmpty() -> v.english
                            v.englishY.isNotEmpty() -> v.englishY
                            v.bengali.isNotEmpty() -> v.bengali
                            v.urdu.isNotEmpty() -> v.urdu
                            v.indonesian.isNotEmpty() -> v.indonesian
                            else -> v.arabic
                        }
                    }
                    binding.textSuggestionSubtitle.text = "$markerPrefix$snippet"
                }

                is SearchSuggestion.RootItem -> {
                    val r = item.rootEntry
                    binding.imageSuggestionIcon.setImageResource(R.drawable.ic_tree)
                    binding.textSuggestionTitle.text = r.root
                    binding.textSuggestionBadge.text = "Root"
                    binding.textSuggestionSubtitle.text = r.definition
                }
            }
        }
    }

    companion object DiffCallback : DiffUtil.ItemCallback<SearchSuggestion>() {
        override fun areItemsTheSame(oldItem: SearchSuggestion, newItem: SearchSuggestion): Boolean {
            return when {
                oldItem is SearchSuggestion.SurahItem && newItem is SearchSuggestion.SurahItem ->
                    oldItem.surah.id == newItem.surah.id
                oldItem is SearchSuggestion.AyahJumpItem && newItem is SearchSuggestion.AyahJumpItem ->
                    oldItem.surahId == newItem.surahId && oldItem.ayahNumber == newItem.ayahNumber
                oldItem is SearchSuggestion.VerseTextItem && newItem is SearchSuggestion.VerseTextItem ->
                    oldItem.verse.id == newItem.verse.id && oldItem.translationMarker == newItem.translationMarker
                oldItem is SearchSuggestion.RootItem && newItem is SearchSuggestion.RootItem ->
                    oldItem.rootEntry.root == newItem.rootEntry.root
                else -> false
            }
        }

        override fun areContentsTheSame(oldItem: SearchSuggestion, newItem: SearchSuggestion): Boolean {
            return oldItem == newItem
        }
    }
}
