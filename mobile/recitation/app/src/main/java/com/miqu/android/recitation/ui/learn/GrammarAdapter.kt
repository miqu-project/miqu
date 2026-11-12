package com.miqu.android.recitation.ui.learn

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.miqu.android.recitation.databinding.ItemGrammarRuleBinding
import com.miqu.android.recitation.model.GrammarRule

class GrammarAdapter(
    private val rules: List<GrammarRule>
) : RecyclerView.Adapter<GrammarAdapter.GrammarViewHolder>() {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GrammarViewHolder {
        val binding = ItemGrammarRuleBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return GrammarViewHolder(binding)
    }

    override fun onBindViewHolder(holder: GrammarViewHolder, position: Int) {
        holder.bind(rules[position])
    }

    override fun getItemCount(): Int = rules.size

    inner class GrammarViewHolder(private val binding: ItemGrammarRuleBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(rule: GrammarRule) {
            binding.textGrammarTagBadge.text = "${rule.tag} • ${rule.title}"
            binding.textGrammarArabicName.text = rule.arabicName
            binding.textGrammarDescription.text = rule.description
            binding.textGrammarExample.text = rule.example
        }
    }
}
