package com.miqu.android.recitation.ui.reader

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.color.MaterialColors
import com.miqu.android.recitation.R
import com.miqu.android.recitation.data.UserSettings
import com.miqu.android.recitation.databinding.ItemVerseBinding
import com.miqu.android.recitation.model.Verse

class VerseAdapter(
    private val context: Context,
    private val userSettings: UserSettings,
    var overrideTranslation: String? = null,
    var forceShowTranslation: Boolean = false,
    var forceShowArabic: Boolean = false,
    private val onPlayClick: (Verse) -> Unit,
    private val onMorphologyClick: (Verse) -> Unit,
    private val onTafsirClick: (Verse) -> Unit
) : RecyclerView.Adapter<VerseAdapter.VerseViewHolder>() {

    private var verses: List<Verse> = emptyList()
    var playingVerseNumber: Int? = null
        private set
    var isAudioPlaying: Boolean = false
        private set

    fun submitList(newList: List<Verse>) {
        verses = newList
        notifyDataSetChanged()
    }

    fun setPlaybackState(verseNumber: Int?, isPlaying: Boolean) {
        val oldVerse = playingVerseNumber
        playingVerseNumber = verseNumber
        isAudioPlaying = isPlaying

        oldVerse?.let { v ->
            val index = verses.indexOfFirst { it.verseNumber == v }
            if (index != -1) notifyItemChanged(index)
        }
        verseNumber?.let { v ->
            val index = verses.indexOfFirst { it.verseNumber == v }
            if (index != -1) notifyItemChanged(index)
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VerseViewHolder {
        val binding = ItemVerseBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return VerseViewHolder(binding)
    }

    override fun onBindViewHolder(holder: VerseViewHolder, position: Int) {
        holder.bind(verses[position])
    }

    override fun getItemCount(): Int = verses.size

    inner class VerseViewHolder(private val binding: ItemVerseBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(verse: Verse) {
            val isCurrentPlaying = (verse.verseNumber == playingVerseNumber)

            val primaryColor = MaterialColors.getColor(binding.root, com.google.android.material.R.attr.colorPrimary)
            val defaultStrokeColor = MaterialColors.getColor(binding.root, com.google.android.material.R.attr.colorOutlineVariant)
            val defaultBgColor = MaterialColors.getColor(binding.root, com.google.android.material.R.attr.colorSurfaceContainerLow)
            val activeBgColor = MaterialColors.getColor(binding.root, com.google.android.material.R.attr.colorSurfaceContainerHigh)

            if (isCurrentPlaying) {
                binding.root.strokeColor = primaryColor
                binding.root.strokeWidth = 2
                binding.root.setCardBackgroundColor(activeBgColor)
            } else {
                binding.root.strokeColor = defaultStrokeColor
                binding.root.strokeWidth = 1
                binding.root.setCardBackgroundColor(defaultBgColor)
            }

            binding.textVerseKey.text = "${verse.surahNumber}:${verse.verseNumber}"

            // Text visibility controls
            val showArabic = forceShowArabic || userSettings.showArabic
            val showTranslation = forceShowTranslation || userSettings.showTranslation

            binding.textArabic.visibility = if (showArabic) View.VISIBLE else View.GONE
            binding.textTranslation.visibility = if (showTranslation) View.VISIBLE else View.GONE
            binding.dividerVerse.visibility = if (showArabic && showTranslation) View.VISIBLE else View.GONE

            if (showArabic) {
                binding.textArabic.text = verse.arabic
                binding.textArabic.textSize = userSettings.arabicFontSize
                binding.textArabic.typeface = com.miqu.android.recitation.util.FontHelper.getArabicTypeface(context)
                binding.textArabic.textAlignment = View.TEXT_ALIGNMENT_VIEW_END
                binding.textArabic.gravity = android.view.Gravity.END or android.view.Gravity.RIGHT
            }

            val activeTranslation = overrideTranslation ?: userSettings.translation
            val translationText = verse.getTranslation(activeTranslation)
            if (showTranslation) {
                binding.textTranslation.text = translationText
                binding.textTranslation.textSize = userSettings.translationFontSize
                binding.textTranslation.typeface = com.miqu.android.recitation.util.FontHelper.getTranslationTypeface(context, activeTranslation, userSettings)
            }

            // Play / Pause Icon
            if (isCurrentPlaying && isAudioPlaying) {
                binding.btnPlayVerse.setIconResource(R.drawable.ic_pause)
                binding.btnPlayVerse.contentDescription = context.getString(R.string.nav_media)
            } else {
                binding.btnPlayVerse.setIconResource(R.drawable.ic_play_arrow)
                binding.btnPlayVerse.contentDescription = context.getString(R.string.play)
            }

            binding.btnPlayVerse.setOnClickListener {
                onPlayClick(verse)
            }

            binding.btnCopyVerse.setOnClickListener {
                val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                val clip = ClipData.newPlainText(
                    "Ayah ${verse.surahNumber}:${verse.verseNumber}",
                    "${verse.arabic}\n\n$translationText\n(${verse.surahNumber}:${verse.verseNumber})"
                )
                clipboard.setPrimaryClip(clip)
                Toast.makeText(context, context.getString(R.string.verse_copied), Toast.LENGTH_SHORT).show()
            }

            binding.btnMorphology.setOnClickListener {
                onMorphologyClick(verse)
            }

            binding.btnTafsir.setOnClickListener {
                onTafsirClick(verse)
            }
        }
    }
}
