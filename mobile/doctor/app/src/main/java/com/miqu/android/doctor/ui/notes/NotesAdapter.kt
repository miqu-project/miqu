package com.miqu.android.doctor.ui.notes

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.doctor.data.NoteEntity
import com.miqu.android.doctor.databinding.ItemNoteBinding

class NotesAdapter(
    private val onNoteClicked: (NoteEntity) -> Unit,
    private val onNoteLongClicked: (NoteEntity) -> Unit
) : ListAdapter<NoteEntity, NotesAdapter.NoteViewHolder>(NoteDiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): NoteViewHolder {
        val binding = ItemNoteBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return NoteViewHolder(binding)
    }

    override fun onBindViewHolder(holder: NoteViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    inner class NoteViewHolder(private val binding: ItemNoteBinding) : RecyclerView.ViewHolder(binding.root) {
        fun bind(note: NoteEntity) {
            binding.tvNoteTitle.text = note.title
            binding.chipNoteBadge.text = if (note.isBuiltIn) "Clinical Guide" else note.category
            binding.ivPinned.visibility = if (note.isPinned) View.VISIBLE else View.GONE

            binding.root.setOnClickListener {
                onNoteClicked(note)
            }
            binding.root.setOnLongClickListener {
                onNoteLongClicked(note)
                true
            }
        }
    }

    object NoteDiffCallback : DiffUtil.ItemCallback<NoteEntity>() {
        override fun areItemsTheSame(oldItem: NoteEntity, newItem: NoteEntity): Boolean = oldItem.id == newItem.id
        override fun areContentsTheSame(oldItem: NoteEntity, newItem: NoteEntity): Boolean = oldItem == newItem
    }
}
