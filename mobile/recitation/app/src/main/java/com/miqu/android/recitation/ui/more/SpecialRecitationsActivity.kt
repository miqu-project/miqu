package com.miqu.android.recitation.ui.more

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.recyclerview.widget.LinearLayoutManager
import com.miqu.android.recitation.data.VideoRepository
import com.miqu.android.recitation.databinding.ActivitySpecialRecitationsBinding

class SpecialRecitationsActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySpecialRecitationsBinding
    private lateinit var videoRepository: VideoRepository
    private lateinit var adapter: VideoAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        binding = ActivitySpecialRecitationsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ViewCompat.setOnApplyWindowInsetsListener(binding.appBarLayout) { v, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            v.updatePadding(top = statusBars.top)
            insets
        }

        ViewCompat.setOnApplyWindowInsetsListener(binding.recyclerViewVideos) { v, insets ->
            val navBars = insets.getInsets(WindowInsetsCompat.Type.navigationBars())
            v.updatePadding(bottom = navBars.bottom + 24)
            insets
        }

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                finish()
            }
        })

        binding.toolbar.setNavigationOnClickListener {
            finish()
        }

        videoRepository = VideoRepository(this)
        adapter = VideoAdapter { video ->
            val webIntent = Intent(
                Intent.ACTION_VIEW,
                Uri.parse("https://www.youtube.com/watch?v=${video.id}")
            )
            startActivity(webIntent)
        }

        binding.recyclerViewVideos.layoutManager = LinearLayoutManager(this)
        binding.recyclerViewVideos.adapter = adapter
        adapter.submitList(videoRepository.getVideos())
    }
}
