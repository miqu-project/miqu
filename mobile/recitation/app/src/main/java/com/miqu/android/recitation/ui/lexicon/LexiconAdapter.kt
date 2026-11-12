package com.miqu.android.recitation.ui.lexicon

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.recitation.databinding.ItemRootBinding
import com.miqu.android.recitation.model.RootEntry

class LexiconAdapter(
    private val onRootClick: (RootEntry) -> Unit
) : RecyclerView.Adapter<LexiconAdapter.RootViewHolder>() {

    private var roots: List<RootEntry> = emptyList()

    fun submitList(newList: List<RootEntry>) {
        roots = newList
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RootViewHolder {
        val binding = ItemRootBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return RootViewHolder(binding)
    }

    override fun onBindViewHolder(holder: RootViewHolder, position: Int) {
        holder.bind(roots[position])
    }

    override fun getItemCount(): Int = roots.size

    inner class RootViewHolder(private val binding: ItemRootBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(entry: RootEntry) {
            binding.textRootArabic.text = entry.root
            binding.textRootArabic.typeface = com.miqu.android.recitation.util.FontHelper.getArabicTypeface(binding.root.context)
            if (entry.definition.isNotBlank()) {
                binding.textRootDefinition.visibility = android.view.View.VISIBLE
                binding.textRootDefinition.text = entry.definition
            } else {
                binding.textRootDefinition.visibility = android.view.View.GONE
            }
            if (entry.occurrencesCount > 0) {
                binding.textRootOccurrences.visibility = android.view.View.VISIBLE
                binding.textRootOccurrences.text = "${entry.occurrencesCount} occurrences"
            } else {
                binding.textRootOccurrences.visibility = android.view.View.GONE
            }

            binding.root.setOnClickListener {
                onRootClick(entry)
            }
        }
    }
}
