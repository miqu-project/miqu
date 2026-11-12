package com.miqu.android.recitation.ui.more

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.recitation.databinding.ItemVideoBinding
import com.miqu.android.recitation.model.VideoEntry

class VideoAdapter(
    private val onVideoClick: (VideoEntry) -> Unit
) : RecyclerView.Adapter<VideoAdapter.VideoViewHolder>() {

    private var videos: List<VideoEntry> = emptyList()

    fun submitList(newList: List<VideoEntry>) {
        videos = newList
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VideoViewHolder {
        val binding = ItemVideoBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return VideoViewHolder(binding)
    }

    override fun onBindViewHolder(holder: VideoViewHolder, position: Int) {
        holder.bind(videos[position])
    }

    override fun getItemCount(): Int = videos.size

    inner class VideoViewHolder(private val binding: ItemVideoBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(video: VideoEntry) {
            binding.textVideoTitle.text = video.title
            binding.textVideoDesc.text = video.desc

            binding.btnWatchVideo.setOnClickListener {
                onVideoClick(video)
            }
            binding.root.setOnClickListener {
                onVideoClick(video)
            }
        }
    }
}
