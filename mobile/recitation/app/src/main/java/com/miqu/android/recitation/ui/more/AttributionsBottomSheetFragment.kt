package com.miqu.android.recitation.ui.more

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.miqu.android.recitation.databinding.BottomSheetSourcesBinding

class AttributionsBottomSheetFragment : BottomSheetDialogFragment() {

    private var _binding: BottomSheetSourcesBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = BottomSheetSourcesBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.btnSourcesClose.setOnClickListener {
            dismiss()
        }

        binding.btnLinkCorpus.setOnClickListener {
            openUrl("http://corpus.quran.com")
        }

        binding.btnLinkTanzil.setOnClickListener {
            openUrl("http://tanzil.info")
        }
    }

    private fun openUrl(url: String) {
        try {
            val intent = Intent(Intent.ACTION_VIEW, Uri.parse(url))
            startActivity(intent)
        } catch (_: Exception) {}
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    companion object {
        const val TAG = "AttributionsBottomSheet"
        fun newInstance() = AttributionsBottomSheetFragment()
    }
}
