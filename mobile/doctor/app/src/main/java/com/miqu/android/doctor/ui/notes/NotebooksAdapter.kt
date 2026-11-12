package com.miqu.android.doctor.ui.notes

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.doctor.R
import com.miqu.android.doctor.data.NotebookEntity
import com.miqu.android.doctor.databinding.ItemNotebookBinding

class NotebooksAdapter(
    private val onNotebookClicked: (NotebookEntity) -> Unit,
    private val onNotebookLongClicked: (NotebookEntity) -> Unit
) : ListAdapter<NotebookEntity, NotebooksAdapter.ViewHolder>(DiffCallback) {

    class ViewHolder(val binding: ItemNotebookBinding) : RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val binding = ItemNotebookBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return ViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val item = getItem(position)
        holder.binding.tvNotebookName.text = item.name
        val countText = if (item.isBuiltIn) {
            "${item.noteCount} clinical guides"
        } else {
            "${item.noteCount} ${if (item.noteCount == 1) "note" else "notes"}"
        }
        holder.binding.tvNotebookCount.text = countText

        holder.binding.ivNotebookIcon.setImageResource(
            if (item.isBuiltIn) R.drawable.ic_notes else R.drawable.ic_notes
        )

        holder.itemView.setOnClickListener {
            onNotebookClicked(item)
        }
        holder.itemView.setOnLongClickListener {
            onNotebookLongClicked(item)
            true
        }
    }

    object DiffCallback : DiffUtil.ItemCallback<NotebookEntity>() {
        override fun areItemsTheSame(oldItem: NotebookEntity, newItem: NotebookEntity): Boolean {
            return oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: NotebookEntity, newItem: NotebookEntity): Boolean {
            return oldItem == newItem
        }
    }
}
