package com.miqu.android.doctor.ui.notes

import android.graphics.Typeface
import android.text.Spannable
import android.text.SpannableString
import android.text.style.StyleSpan
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.doctor.data.SearchResultNote
import com.miqu.android.doctor.databinding.ItemSearchResultBinding

class SearchResultsAdapter(
    private val onResultClicked: (SearchResultNote) -> Unit
) : ListAdapter<SearchResultNote, SearchResultsAdapter.ViewHolder>(DiffCallback) {

    private var currentQuery: String = ""

    fun submitResults(list: List<SearchResultNote>, query: String) {
        currentQuery = query.trim()
        submitList(list)
    }

    class ViewHolder(val binding: ItemSearchResultBinding) : RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val binding = ItemSearchResultBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return ViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val item = getItem(position)

        // Highlight matched query in title if found
        holder.binding.tvSearchNoteTitle.text = highlightText(item.note.title, currentQuery)
        holder.binding.chipNotebookName.text = item.notebookName

        // Highlight matched query in snippet
        holder.binding.tvSearchSnippet.text = highlightText(item.matchSnippet, currentQuery)

        holder.itemView.setOnClickListener {
            onResultClicked(item)
        }
    }

    private fun highlightText(fullText: String, query: String): CharSequence {
        if (query.isEmpty() || !fullText.contains(query, ignoreCase = true)) {
            return fullText
        }
        val spannable = SpannableString(fullText)
        val lowerText = fullText.lowercase()
        val lowerQuery = query.lowercase()
        var start = lowerText.indexOf(lowerQuery)
        while (start >= 0) {
            val end = start + lowerQuery.length
            spannable.setSpan(
                StyleSpan(Typeface.BOLD),
                start,
                end,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
            start = lowerText.indexOf(lowerQuery, end)
        }
        return spannable
    }

    object DiffCallback : DiffUtil.ItemCallback<SearchResultNote>() {
        override fun areItemsTheSame(oldItem: SearchResultNote, newItem: SearchResultNote): Boolean {
            return oldItem.note.id == newItem.note.id
        }

        override fun areContentsTheSame(oldItem: SearchResultNote, newItem: SearchResultNote): Boolean {
            return oldItem == newItem
        }
    }
}
