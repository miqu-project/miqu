package com.miqu.android.recitation.ui.surah

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.recitation.databinding.ItemSurahBinding
import com.miqu.android.recitation.model.Surah

class SurahAdapter(
    private val onSurahClick: (Surah) -> Unit
) : RecyclerView.Adapter<SurahAdapter.SurahViewHolder>() {

    private var surahs: List<Surah> = emptyList()

    fun submitList(newList: List<Surah>) {
        surahs = newList
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SurahViewHolder {
        val binding = ItemSurahBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return SurahViewHolder(binding)
    }

    override fun onBindViewHolder(holder: SurahViewHolder, position: Int) {
        holder.bind(surahs[position])
    }

    override fun getItemCount(): Int = surahs.size

    inner class SurahViewHolder(private val binding: ItemSurahBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(surah: Surah) {
            binding.textSurahNumber.text = surah.id.toString()
            binding.textSurahTransliteration.text = surah.transliteration
            binding.textSurahMeaning.text = surah.english
            val typeStr = surah.type.replaceFirstChar { it.uppercase() }
            binding.textSurahMeta.text = "$typeStr • ${surah.totalVerses} Verses"
            binding.textSurahArabic.text = surah.name
            binding.textSurahArabic.typeface = com.miqu.android.recitation.util.FontHelper.getArabicTypeface(binding.root.context)

            binding.root.setOnClickListener {
                onSurahClick(surah)
            }
        }
    }
}
