package com.miqu.android.recitation.ui.more

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.miqu.android.recitation.databinding.FragmentMoreBinding
import com.miqu.android.recitation.ui.settings.SettingsActivity

class MoreFragment : Fragment() {

    private var _binding: FragmentMoreBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentMoreBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.cardMoreSettings.setOnClickListener {
            val intent = Intent(requireContext(), SettingsActivity::class.java)
            startActivity(intent)
        }

        binding.cardMoreSpecialRecitations.setOnClickListener {
            val intent = Intent(requireContext(), SpecialRecitationsActivity::class.java)
            startActivity(intent)
        }

        binding.cardMoreTestActivity.setOnClickListener {
            val intent = Intent(requireContext(), TestActivty::class.java)
            startActivity(intent)
        }

        binding.cardMoreSources.setOnClickListener {
            AttributionsBottomSheetFragment.newInstance()
                .show(childFragmentManager, AttributionsBottomSheetFragment.TAG)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
