package com.miqu.android.recitation.ui.lexicon

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.recitation.databinding.ItemRootDetailHeaderBinding
import com.miqu.android.recitation.databinding.ItemRootOccurrenceBinding
import com.miqu.android.recitation.model.WordRoot

class RootOccurrencesAdapter(
    private val headerRoot: String,
    private val headerDefinition: String,
    private val onOccurrenceClick: (WordRoot) -> Unit
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    companion object {
        private const val VIEW_TYPE_HEADER = 0
        private const val VIEW_TYPE_OCCURRENCE = 1
    }

    private var totalCount: Int = 0
    private val occurrences = mutableListOf<WordRoot>()

    fun updateTotalCount(count: Int) {
        totalCount = count
        notifyItemChanged(0)
    }

    fun submitList(newList: List<WordRoot>) {
        occurrences.clear()
        occurrences.addAll(newList)
        notifyDataSetChanged()
    }

    fun appendList(additionalList: List<WordRoot>) {
        if (additionalList.isEmpty()) return
        val startPos = 1 + occurrences.size
        occurrences.addAll(additionalList)
        notifyItemRangeInserted(startPos, additionalList.size)
    }

    override fun getItemCount(): Int = 1 + occurrences.size

    override fun getItemViewType(position: Int): Int {
        return if (position == 0) VIEW_TYPE_HEADER else VIEW_TYPE_OCCURRENCE
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return if (viewType == VIEW_TYPE_HEADER) {
            val binding = ItemRootDetailHeaderBinding.inflate(inflater, parent, false)
            HeaderViewHolder(binding)
        } else {
            val binding = ItemRootOccurrenceBinding.inflate(inflater, parent, false)
            OccurrenceViewHolder(binding)
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        if (holder is HeaderViewHolder) {
            holder.bind()
        } else if (holder is OccurrenceViewHolder) {
            holder.bind(occurrences[position - 1])
        }
    }

    inner class HeaderViewHolder(private val binding: ItemRootDetailHeaderBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind() {
            binding.textRootArabicLarge.text = headerRoot
            binding.textRootArabicLarge.typeface =
                com.miqu.android.recitation.util.FontHelper.getArabicTypeface(binding.root.context)
            binding.textRootFullDefinition.text = headerDefinition

            if (totalCount > 0) {
                binding.textRootTotalOccurrences.text = "$totalCount Occurrences"
            } else {
                binding.textRootTotalOccurrences.text = "Occurrences"
            }
        }
    }

    inner class OccurrenceViewHolder(private val binding: ItemRootOccurrenceBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(wordRoot: WordRoot) {
            binding.textOccurrenceSurahVerse.text = "Ayah ${wordRoot.surah}:${wordRoot.verse}"
            binding.textOccurrenceArabic.text = wordRoot.arabic
            binding.textOccurrenceArabic.typeface =
                com.miqu.android.recitation.util.FontHelper.getArabicTypeface(binding.root.context)
            binding.textOccurrenceEnglish.text = wordRoot.english

            binding.root.setOnClickListener {
                onOccurrenceClick(wordRoot)
            }
        }
    }
}

