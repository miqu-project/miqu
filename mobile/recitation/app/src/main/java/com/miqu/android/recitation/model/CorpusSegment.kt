package com.miqu.android.recitation.model

data class CorpusSegment(
    val word: Int,
    val segment: Int,
    val form: String,
    val tag: String,
    val tagDesc: String,
    val features: String
) {
    fun formatDisplay(): String {
        val posLabel = if (tagDesc.contains("(") && tagDesc.contains(")")) {
            val ar = tagDesc.substringBefore("(").trim()
            val en = tagDesc.substringAfter("(").substringBefore(")").trim()
            "$en ($ar)"
        } else {
            tagDesc
        }

        val parts = features.split('|')
        val gram = mutableListOf<String>()
        var lemma: String? = null
        for (p in parts) {
            when {
                p == "GEN" -> gram.add("Genitive (مجرور)")
                p == "NOM" -> gram.add("Nominative (مرفوع)")
                p == "ACC" -> gram.add("Accusative (منصوب)")
                p == "MS" -> gram.add("Masc. Sing.")
                p == "FS" -> gram.add("Fem. Sing.")
                p == "MP" -> gram.add("Masc. Plural")
                p == "FP" -> gram.add("Fem. Plural")
                p == "MD" -> gram.add("Masc. Dual")
                p == "FD" -> gram.add("Fem. Dual")
                p == "IMPF" -> gram.add("Imperfect (مضارع)")
                p == "PERF" -> gram.add("Perfect (ماض)")
                p == "IMPV" -> gram.add("Imperative (أمر)")
                p == "PASS" -> gram.add("Passive (مجهول)")
                p.startsWith("LEM:") -> lemma = p.removePrefix("LEM:")
            }
        }

        val sb = StringBuilder("• ").append(posLabel)
        if (gram.isNotEmpty()) {
            sb.append(" — ").append(gram.joinToString(" • "))
        }
        if (!lemma.isNullOrBlank()) {
            sb.append(" [Lemma: ").append(lemma).append("]")
        }
        return sb.toString()
    }
}
