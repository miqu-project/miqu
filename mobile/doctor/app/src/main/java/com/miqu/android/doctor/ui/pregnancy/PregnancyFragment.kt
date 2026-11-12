package com.miqu.android.doctor.ui.pregnancy

import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.google.android.material.datepicker.MaterialDatePicker
import com.miqu.android.doctor.R
import com.miqu.android.doctor.databinding.FragmentPregnancyBinding
import com.miqu.android.doctor.databinding.ItemMilestoneBinding
import com.miqu.android.doctor.model.PregnancyCalculator
import com.miqu.android.doctor.model.PregnancyResult
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

class PregnancyFragment : Fragment() {

    private var _binding: FragmentPregnancyBinding? = null
    private val binding get() = _binding!!

    private var selectedLmpDate: LocalDate = LocalDate.now().minusWeeks(8)
    private var selectedScanDate: LocalDate = LocalDate.now()
    private var cycleLength: Int = 28
    private var isLmpMode: Boolean = true

    private val dateFormatter = DateTimeFormatter.ofPattern("MMM dd, yyyy", Locale.getDefault())
    private val rangeFormatter = DateTimeFormatter.ofPattern("MMM dd", Locale.getDefault())

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentPregnancyBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        setupListeners()
        updateDateButtonLabels()
        recalculate()
    }

    private fun setupListeners() {
        binding.toggleMethod.addOnButtonCheckedListener { _, checkedId, isChecked ->
            if (isChecked) {
                isLmpMode = (checkedId == R.id.btnMethodLmp)
                binding.layoutLmpInput.visibility = if (isLmpMode) View.VISIBLE else View.GONE
                binding.layoutUltrasoundInput.visibility = if (isLmpMode) View.GONE else View.VISIBLE
                recalculate()
            }
        }

        binding.btnPickLmpDate.setOnClickListener {
            showDatePicker(selectedLmpDate) { pickedDate ->
                selectedLmpDate = pickedDate
                updateDateButtonLabels()
                recalculate()
            }
        }

        binding.btnPickScanDate.setOnClickListener {
            showDatePicker(selectedScanDate) { pickedDate ->
                selectedScanDate = pickedDate
                updateDateButtonLabels()
                recalculate()
            }
        }

        binding.sliderCycleLength.addOnChangeListener { _, value, _ ->
            cycleLength = value.toInt()
            binding.labelCycleLength.text = "Average Cycle Length: $cycleLength Days"
            recalculate()
        }

        val watcher = object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            override fun afterTextChanged(s: Editable?) { recalculate() }
        }
        binding.etScanWeeks.addTextChangedListener(watcher)
        binding.etScanDays.addTextChangedListener(watcher)
    }

    private fun showDatePicker(initialDate: LocalDate, onDateSelected: (LocalDate) -> Unit) {
        val picker = MaterialDatePicker.Builder.datePicker()
            .setTitleText("Select Date")
            .setSelection(initialDate.atStartOfDay(ZoneId.of("UTC")).toInstant().toEpochMilli())
            .build()

        picker.addOnPositiveButtonClickListener { selectionMillis ->
            val date = Instant.ofEpochMilli(selectionMillis)
                .atZone(ZoneId.of("UTC"))
                .toLocalDate()
            onDateSelected(date)
        }
        picker.show(parentFragmentManager, "PREGNANCY_DATE_PICKER")
    }

    private fun updateDateButtonLabels() {
        binding.btnPickLmpDate.text = "LMP: ${selectedLmpDate.format(dateFormatter)}"
        binding.btnPickScanDate.text = "Scan: ${selectedScanDate.format(dateFormatter)}"
    }

    private fun recalculate() {
        val result: PregnancyResult = if (isLmpMode) {
            PregnancyCalculator.calculateFromLmp(selectedLmpDate, cycleLength)
        } else {
            val weeks = binding.etScanWeeks.text.toString().toIntOrNull() ?: 12
            val days = binding.etScanDays.text.toString().toIntOrNull() ?: 0
            PregnancyCalculator.calculateFromUltrasound(selectedScanDate, weeks, days)
        }
        displayResult(result)
    }

    private fun displayResult(result: PregnancyResult) {
        binding.tvEddDate.text = result.edd.format(dateFormatter)
        binding.tvGestationalAge.text = "${result.gestationalWeeks} Weeks, ${result.gestationalDays} Days"
        binding.chipTrimester.text = "Trimester ${result.trimester}"
        binding.tvProgressLabel.text = "Pregnancy Progress: ${result.progressPercent}%"
        binding.progressPregnancy.progress = result.progressPercent

        // Render milestones
        binding.layoutMilestonesContainer.removeAllViews()
        val inflater = LayoutInflater.from(requireContext())
        for (milestone in result.milestones) {
            val itemBinding = ItemMilestoneBinding.inflate(inflater, binding.layoutMilestonesContainer, false)
            itemBinding.tvMilestoneTitle.text = milestone.title
            itemBinding.tvMilestoneDates.text = "${milestone.windowStart.format(rangeFormatter)} – ${milestone.windowEnd.format(rangeFormatter)}"
            itemBinding.tvMilestoneDescription.text = milestone.description
            binding.layoutMilestonesContainer.addView(itemBinding.root)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
