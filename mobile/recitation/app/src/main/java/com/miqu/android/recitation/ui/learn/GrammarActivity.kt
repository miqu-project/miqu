package com.miqu.android.recitation.ui.learn

import android.os.Bundle
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.recyclerview.widget.LinearLayoutManager
import com.miqu.android.recitation.databinding.ActivityGrammarBinding
import com.miqu.android.recitation.model.GrammarRule

class GrammarActivity : AppCompatActivity() {

    companion object {
        val GRAMMAR_RULES = listOf(
            GrammarRule("N", "Noun", "اسم", "Names of entities, places, objects, concepts, or adjectives functioning as substantives.", "كِتَاب (book), أَرْض (earth), نُور (light)"),
            GrammarRule("PN", "Proper Noun", "اسم علم", "Specific names of people, places, or divine names.", "ٱللَّه (Allah), مُحَمَّد (Muhammad), مَكَّة (Makkah)"),
            GrammarRule("V", "Verb", "فعل", "Actions occurring in the past (Perfect/ماض), present/future (Imperfect/مضارع), or command (Imperative/أمر).", "قَالَ (he said), يَعْلَمُ (he knows), ٱقْرَأْ (read!)"),
            GrammarRule("P", "Preposition", "حرف جر", "Particles that introduce nouns and place them in the genitive (مجرور) case.", "فِى (in), مِنْ (from), عَلَىٰ (upon), بِـ (with/by)"),
            GrammarRule("PRON", "Pronoun", "ضمير", "Independent or attached pronouns representing persons, duals, or plurals.", "هُوَ (He), هُمْ (they), ـنَا (our/us), ـكُمْ (your)"),
            GrammarRule("ADJ", "Adjective", "صفة", "Descriptive words following and agreeing in gender, number, and case with the noun.", "رَحِيم (Merciful), عَلِيم (All-Knowing), عَظِيم (Mighty)"),
            GrammarRule("DET", "Definite Article", "أداة تعريف", "The prefix 'Al-' (ٱلْـ) making an indefinite noun definite.", "ٱلْكِتَابُ (The Book), ٱلْحَمْدُ (The Praise)"),
            GrammarRule("CONJ", "Conjunction", "حرف عطف", "Connecting particles joining words, clauses, or sentences.", "وَ (and), فَـ (then/so), ثُمَّ (then afterwards)"),
            GrammarRule("REL", "Relative Pronoun", "اسم موصول", "Pronouns connecting a relative clause to a preceding noun.", "ٱلَّذِى (who/which - sing.), ٱلَّذِينَ (those who - pl.)"),
            GrammarRule("NEG", "Negative Particle", "حرف نفي", "Particles used to negate verbs, sentences, or nouns.", "لَا (no/not), مَا (not/what), لَمْ (did not), لَنْ (will not)"),
            GrammarRule("COND", "Conditional Particle", "أداة شرط", "Particles introducing conditional statements.", "إِنْ (if), إِذَا (when/whenever), لَوْ (if it were)"),
            GrammarRule("VOC", "Vocative Particle", "أداة نداء", "Calling particles used to address entities directly.", "يَـٰٓأَيُّهَا (O you!), يَـٰٓإِبْرَٰهِيمُ (O Abraham!)"),
            GrammarRule("NOM", "Nominative Case", "حالة الرفع (مرفوع)", "Primary case for subjects (فَاعِل), topic (مُبْتَدَأ), and predicate (خَبَر). Usually marked with Dammah (-ُ).", "ٱللَّهُ (Allah is...); قَالَ رَجُلٌ (A man said)"),
            GrammarRule("ACC", "Accusative Case", "حالة النصب (منصوب)", "Case for direct objects (مَفْعُول بِهِ), adverbs of time/place (ظَرْف), and predicates of Kaana.", "خَلَقَ ٱللَّهُ ٱلسَّمَـٰوَٰتِ (Allah created the heavens)"),
            GrammarRule("GEN", "Genitive Case", "حالة الجر (مجرور)", "Case for nouns following prepositions or in possessive construct (مُضَاف إِلَيْهِ). Usually marked with Kasrah (-ِ).", "بِسْمِ ٱللَّهِ (In the name of Allah)")
        )
    }

    private lateinit var binding: ActivityGrammarBinding
    private lateinit var adapter: GrammarAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        binding = ActivityGrammarBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ViewCompat.setOnApplyWindowInsetsListener(binding.appBarLayout) { v, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            v.updatePadding(top = statusBars.top)
            insets
        }

        ViewCompat.setOnApplyWindowInsetsListener(binding.recyclerViewGrammar) { v, insets ->
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

        adapter = GrammarAdapter(GRAMMAR_RULES)
        binding.recyclerViewGrammar.layoutManager = LinearLayoutManager(this)
        binding.recyclerViewGrammar.adapter = adapter
    }
}
