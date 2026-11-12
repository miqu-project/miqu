package com.miqu.android.recitation.ui.reader

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.recitation.databinding.ItemWordMorphologyBinding
import com.miqu.android.recitation.model.CorpusSegment
import com.miqu.android.recitation.model.WordRoot

class MorphologyWordAdapter(
    private val onRootClick: (String) -> Unit,
    private val onWordClick: (WordRoot) -> Unit
) : RecyclerView.Adapter<MorphologyWordAdapter.WordViewHolder>() {

    data class WordWithGrammar(
        val wordRoot: WordRoot,
        val segments: List<CorpusSegment>
    )

    private var items: List<WordWithGrammar> = emptyList()

    fun submitList(newList: List<WordWithGrammar>) {
        items = newList
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): WordViewHolder {
        val binding =
            ItemWordMorphologyBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return WordViewHolder(binding)
    }

    override fun onBindViewHolder(holder: WordViewHolder, position: Int) {
        holder.bind(items[position])
    }

    override fun getItemCount(): Int = items.size

    inner class WordViewHolder(private val binding: ItemWordMorphologyBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(item: WordWithGrammar) {
            val word = item.wordRoot
            binding.textWordArabic.text = word.arabic
            binding.textWordArabic.typeface = com.miqu.android.recitation.util.FontHelper.getArabicTypeface(binding.root.context)
            binding.textWordEnglish.text = word.english

            if (word.root.isNotBlank()) {
                binding.btnViewRoot.visibility = View.VISIBLE
                binding.btnViewRoot.text = "Root: ${word.root}"
                binding.btnViewRoot.setOnClickListener {
                    onRootClick(word.root)
                }
            } else {
                binding.btnViewRoot.visibility = View.GONE
            }

            if (item.segments.isNotEmpty()) {
                val grammarText = item.segments.joinToString("\n") { seg ->
                    "• ${seg.formatDisplay()}"
                }
                binding.textWordGrammar.text = grammarText
                binding.textWordGrammar.visibility = View.VISIBLE
            } else {
                binding.textWordGrammar.visibility = View.GONE
            }

            binding.root.setOnClickListener {
                onWordClick(word)
            }
        }
    }
}
