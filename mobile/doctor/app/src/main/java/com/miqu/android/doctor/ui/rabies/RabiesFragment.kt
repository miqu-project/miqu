package com.miqu.android.doctor.ui.rabies

import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.google.android.material.datepicker.MaterialDatePicker
import com.miqu.android.doctor.R
import com.miqu.android.doctor.databinding.FragmentRabiesBinding
import com.miqu.android.doctor.databinding.ItemDoseVisitBinding
import com.miqu.android.doctor.model.DoseVisit
import com.miqu.android.doctor.model.RabiesCalculator
import com.miqu.android.doctor.model.RabiesSchedule
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

class RabiesFragment : Fragment() {

    private var _binding: FragmentRabiesBinding? = null
    private val binding get() = _binding!!

    private var selectedDay0Date: LocalDate = LocalDate.now()
    private var isIdMode: Boolean = true
    private var isPriorVaccinated: Boolean = false

    private val dateFormatter = DateTimeFormatter.ofPattern("EEE, MMM dd, yyyy", Locale.getDefault())

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentRabiesBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        setupListeners()
        updateDateButtonLabel()
        recalculate()
    }

    private fun setupListeners() {
        binding.btnPickDay0Date.setOnClickListener {
            val picker = MaterialDatePicker.Builder.datePicker()
                .setTitleText("Select Day 0 Date")
                .setSelection(selectedDay0Date.atStartOfDay(ZoneId.of("UTC")).toInstant().toEpochMilli())
                .build()

            picker.addOnPositiveButtonClickListener { millis ->
                selectedDay0Date = Instant.ofEpochMilli(millis)
                    .atZone(ZoneId.of("UTC"))
                    .toLocalDate()
                updateDateButtonLabel()
                recalculate()
            }
            picker.show(parentFragmentManager, "RABIES_DATE_PICKER")
        }

        binding.btnToday.setOnClickListener {
            selectedDay0Date = LocalDate.now()
            updateDateButtonLabel()
            recalculate()
        }

        binding.toggleRegimen.addOnButtonCheckedListener { _, checkedId, isChecked ->
            if (isChecked) {
                isIdMode = (checkedId == R.id.btnRegimenId)
                recalculate()
            }
        }

        binding.switchPriorVaccine.setOnCheckedChangeListener { _, isChecked ->
            isPriorVaccinated = isChecked
            binding.toggleRegimen.visibility = if (isChecked) View.GONE else View.VISIBLE
            binding.cardRig.visibility = if (isChecked) View.GONE else View.VISIBLE
            recalculate()
        }

        binding.etWeight.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            override fun afterTextChanged(s: Editable?) { recalculate() }
        })
    }

    private fun updateDateButtonLabel() {
        binding.btnPickDay0Date.text = "Day 0: ${selectedDay0Date.format(dateFormatter)}"
    }

    private fun recalculate() {
        val weight = binding.etWeight.text.toString().toDoubleOrNull()
        val schedule: RabiesSchedule = RabiesCalculator.calculate(selectedDay0Date, weight)

        displaySchedule(schedule)
    }

    private fun displaySchedule(schedule: RabiesSchedule) {
        val visits: List<DoseVisit>
        if (isPriorVaccinated) {
            binding.tvScheduleHeading.text = "Re-Exposure Regimen Schedule"
            binding.tvScheduleSubheading.text = "Previously fully vaccinated patients — 2 visits only (No RIG)"
            visits = schedule.reExposureVisits
        } else if (isIdMode) {
            binding.tvScheduleHeading.text = "Intradermal (ID) Regimen Schedule"
            binding.tvScheduleSubheading.text = "Updated Thai Red Cross (Days 0, 3, 7, 28) — 2 sites (0.1 mL each)"
            visits = schedule.idVisits
        } else {
            binding.tvScheduleHeading.text = "Intramuscular (IM) Regimen Schedule"
            binding.tvScheduleSubheading.text = "Essen Regimen (Days 0, 3, 7, 14, 28) — 1 standard vial in deltoid"
            visits = schedule.imVisits
        }

        binding.layoutVisitsContainer.removeAllViews()
        val inflater = LayoutInflater.from(requireContext())
        for (visit in visits) {
            val itemBinding = ItemDoseVisitBinding.inflate(inflater, binding.layoutVisitsContainer, false)
            itemBinding.tvVisitLabel.text = visit.label
            itemBinding.tvVisitDate.text = visit.date.format(dateFormatter)
            itemBinding.tvVisitDetails.text = visit.details
            binding.layoutVisitsContainer.addView(itemBinding.root)
        }

        // RIG display
        if (schedule.hrigDoseIu != null && schedule.erigDoseIu != null) {
            binding.tvHrigDose.text = "${schedule.hrigDoseIu.toInt()} IU"
            binding.tvErigDose.text = "${schedule.erigDoseIu.toInt()} IU"
        } else {
            binding.tvHrigDose.text = "--"
            binding.tvErigDose.text = "--"
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
