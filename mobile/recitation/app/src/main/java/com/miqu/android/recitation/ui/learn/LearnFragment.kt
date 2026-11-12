package com.miqu.android.recitation.ui.learn

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.miqu.android.recitation.databinding.FragmentLearnBinding
import com.miqu.android.recitation.ui.lexicon.RootsActivity

class LearnFragment : Fragment() {

    private var _binding: FragmentLearnBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentLearnBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.cardLearnRoots.setOnClickListener {
            val intent = Intent(requireContext(), RootsActivity::class.java)
            startActivity(intent)
        }

        binding.cardLearnWords.setOnClickListener {
            val intent = Intent(requireContext(), WordsActivity::class.java)
            startActivity(intent)
        }

        binding.cardLearnGrammar.setOnClickListener {
            val intent = Intent(requireContext(), GrammarActivity::class.java)
            startActivity(intent)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}

