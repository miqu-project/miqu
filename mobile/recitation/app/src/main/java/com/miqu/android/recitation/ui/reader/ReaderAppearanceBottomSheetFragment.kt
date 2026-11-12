package com.miqu.android.recitation.ui.reader

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.miqu.android.recitation.data.UserSettings
import com.miqu.android.recitation.databinding.BottomSheetReaderAppearanceBinding
import com.miqu.android.recitation.model.Reciter
import com.miqu.android.recitation.ui.settings.SettingsActivity
import com.miqu.android.recitation.util.FontHelper

class ReaderAppearanceBottomSheetFragment(
    private val onAppearanceChanged: () -> Unit
) : BottomSheetDialogFragment() {

    private var _binding: BottomSheetReaderAppearanceBinding? = null
    private val binding get() = _binding!!

    private lateinit var settings: UserSettings

    private val translationOptions = listOf(
        "English (Sahih International)" to UserSettings.TRANS_ENGLISH,
        "Bengali (মুহিউদ্দীন খান / ইসলামিক ফাউন্ডেশন)" to UserSettings.TRANS_BENGALI,
        "Urdu (احمد علی)" to UserSettings.TRANS_URDU,
        "Indonesian (Bahasa Indonesia)" to UserSettings.TRANS_INDONESIAN,
        "Assamese (অসমীয়া)" to UserSettings.TRANS_ASSAMESE,
        "English (Abdullah Yusuf Ali)" to UserSettings.TRANS_YUSUF_ALI
    )

    private val audioModes = listOf(
        "Arabic Only" to 0,
        "Translation Only" to 1,
        "Both (Arabic + Translation)" to 2
    )

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = BottomSheetReaderAppearanceBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        settings = UserSettings(requireContext())

        // 1. Text display switches
        binding.switchShowArabic.isChecked = settings.showArabic
        binding.switchShowArabic.setOnCheckedChangeListener { _, isChecked ->
            // Prevent turning off both
            if (!isChecked && !settings.showTranslation) {
                binding.switchShowArabic.isChecked = true
                return@setOnCheckedChangeListener
            }
            settings.showArabic = isChecked
            onAppearanceChanged()
        }

        binding.switchShowTranslation.isChecked = settings.showTranslation
        binding.switchShowTranslation.setOnCheckedChangeListener { _, isChecked ->
            // Prevent turning off both
            if (!isChecked && !settings.showArabic) {
                binding.switchShowTranslation.isChecked = true
                return@setOnCheckedChangeListener
            }
            settings.showTranslation = isChecked
            onAppearanceChanged()
        }

        // 2. Audio Reciter Dropdown
        val arabicReciters = Reciter.ARABIC_RECITERS
        val reciterNames = arabicReciters.map { it.name }
        val reciterAdapter = ArrayAdapter(requireContext(), android.R.layout.simple_dropdown_item_1line, reciterNames)
        binding.dropdownReciter.setAdapter(reciterAdapter)
        val currentReciterIndex = arabicReciters.indexOfFirst { it.identifier == settings.reciterIdentifier }.coerceAtLeast(0)
        binding.dropdownReciter.setText(reciterNames[currentReciterIndex], false)
        binding.dropdownReciter.setOnItemClickListener { _, _, position, _ ->
            settings.reciterIdentifier = arabicReciters[position].identifier
            onAppearanceChanged()
        }

        // 3. Audio Mode Dropdown
        val modeNames = audioModes.map { it.first }
        val modeAdapter = ArrayAdapter(requireContext(), android.R.layout.simple_dropdown_item_1line, modeNames)
        binding.dropdownAudioMode.setAdapter(modeAdapter)
        val currentModeIndex = audioModes.indexOfFirst { it.second == settings.audioRecitationMode }.coerceAtLeast(0)
        binding.dropdownAudioMode.setText(modeNames[currentModeIndex], false)
        updateTranslationReciterVisibility(settings.audioRecitationMode)

        binding.dropdownAudioMode.setOnItemClickListener { _, _, position, _ ->
            val mode = audioModes[position].second
            settings.audioRecitationMode = mode
            updateTranslationReciterVisibility(mode)
            onAppearanceChanged()
        }

        // 4. Translation Reciter Dropdown
        val transReciters = Reciter.TRANSLATION_RECITERS
        val transReciterNames = transReciters.map { it.name }
        val transReciterAdapter = ArrayAdapter(requireContext(), android.R.layout.simple_dropdown_item_1line, transReciterNames)
        binding.dropdownTranslationReciter.setAdapter(transReciterAdapter)
        val currentTransReciterIndex = transReciters.indexOfFirst { it.identifier == settings.translationReciterIdentifier }.coerceAtLeast(0)
        binding.dropdownTranslationReciter.setText(transReciterNames[currentTransReciterIndex], false)
        binding.dropdownTranslationReciter.setOnItemClickListener { _, _, position, _ ->
            settings.translationReciterIdentifier = transReciters[position].identifier
            onAppearanceChanged()
        }

        // 5. Font size slider
        binding.sliderFontSize.value = settings.arabicFontSize
        binding.textFontSizeLabel.text = "Arabic Font Size (${settings.arabicFontSize.toInt()}sp)"
        binding.sliderFontSize.addOnChangeListener { _, value, _ ->
            settings.arabicFontSize = value
            binding.textFontSizeLabel.text = "Arabic Font Size (${value.toInt()}sp)"
            onAppearanceChanged()
        }

        // 6. Translation font size slider
        binding.sliderTranslationFontSize.value = settings.translationFontSize
        binding.textTranslationFontSizeLabel.text = "Translation Font Size (${settings.translationFontSize.toInt()}sp)"
        binding.sliderTranslationFontSize.addOnChangeListener { _, value, _ ->
            settings.translationFontSize = value
            binding.textTranslationFontSizeLabel.text = "Translation Font Size (${value.toInt()}sp)"
            onAppearanceChanged()
        }

        // 7. Translation dropdown
        val transDisplayList = translationOptions.map { it.first }
        val transAdapter = ArrayAdapter(requireContext(), android.R.layout.simple_dropdown_item_1line, transDisplayList)
        binding.dropdownTranslation.setAdapter(transAdapter)
        val currentTransIndex = translationOptions.indexOfFirst { it.second == settings.translation }.coerceAtLeast(0)
        binding.dropdownTranslation.setText(transDisplayList[currentTransIndex], false)
        binding.dropdownTranslation.setOnItemClickListener { _, _, position, _ ->
            settings.translation = translationOptions[position].second
            setupTranslationFontDropdown()
            onAppearanceChanged()
        }

        // 8. Translation Font dropdown
        setupTranslationFontDropdown()

        // 9. More settings button
        binding.btnAllSettings.setOnClickListener {
            dismiss()
            val intent = Intent(requireContext(), SettingsActivity::class.java)
            startActivity(intent)
        }
    }

    private fun updateTranslationReciterVisibility(mode: Int) {
        binding.layoutTranslationReciter.visibility = if (mode == 1 || mode == 2) View.VISIBLE else View.GONE
    }

    private fun setupTranslationFontDropdown() {
        val fontList = FontHelper.getFontsForLanguage(settings.translation)
        val displayList = fontList.map { it.second }
        val adapter = ArrayAdapter(requireContext(), android.R.layout.simple_dropdown_item_1line, displayList)
        binding.dropdownTranslationFont.setAdapter(adapter)

        val currentSelectedKey = FontHelper.getSelectedFontForLanguage(settings, settings.translation)
        val selectedIndex = fontList.indexOfFirst { it.first == currentSelectedKey }.coerceAtLeast(0)
        binding.dropdownTranslationFont.setText(displayList[selectedIndex], false)

        binding.dropdownTranslationFont.setOnItemClickListener { _, _, position, _ ->
            FontHelper.setSelectedFontForLanguage(settings, settings.translation, fontList[position].first)
            onAppearanceChanged()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
