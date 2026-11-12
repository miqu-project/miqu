package com.miqu.android.recitation.ui.learn

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.recitation.databinding.ItemQuranWordBinding
import com.miqu.android.recitation.model.QuranWord
import com.miqu.android.recitation.util.FontHelper

class WordsAdapter(
    private val onWordClick: (QuranWord) -> Unit
) : RecyclerView.Adapter<WordsAdapter.WordViewHolder>() {

    private var words: List<QuranWord> = emptyList()

    fun submitList(newList: List<QuranWord>) {
        words = newList
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): WordViewHolder {
        val binding = ItemQuranWordBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return WordViewHolder(binding)
    }

    override fun onBindViewHolder(holder: WordViewHolder, position: Int) {
        holder.bind(words[position])
    }

    override fun getItemCount(): Int = words.size

    inner class WordViewHolder(private val binding: ItemQuranWordBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(word: QuranWord) {
            binding.textWordArabic.text = word.arabic
            binding.textWordArabic.typeface = FontHelper.getArabicTypeface(binding.root.context)

            binding.textWordCount.text = "${word.count}x"

            if (word.root.isNotBlank()) {
                binding.textWordRootBadge.visibility = View.VISIBLE
                binding.textWordRootBadge.text = "Root: ${word.root}"
            } else {
                binding.textWordRootBadge.visibility = View.GONE
            }

            binding.root.setOnClickListener {
                onWordClick(word)
            }
        }
    }
}
