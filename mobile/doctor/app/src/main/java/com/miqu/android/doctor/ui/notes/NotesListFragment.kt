package com.miqu.android.doctor.ui.notes

import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.os.bundleOf
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.textfield.TextInputEditText
import com.miqu.android.doctor.R
import com.miqu.android.doctor.data.NotebookEntity
import com.miqu.android.doctor.data.NotesDatabaseHelper
import com.miqu.android.doctor.databinding.FragmentNotesListBinding

class NotesListFragment : Fragment() {

    private var _binding: FragmentNotesListBinding? = null
    private val binding get() = _binding!!

    private lateinit var dbHelper: NotesDatabaseHelper
    private lateinit var notebooksAdapter: NotebooksAdapter
    private lateinit var searchResultsAdapter: SearchResultsAdapter

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentNotesListBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        dbHelper = NotesDatabaseHelper.getInstance(requireContext())
        setupRecyclerViews()
        setupListeners()
        loadNotebooks()
    }

    private fun setupRecyclerViews() {
        notebooksAdapter = NotebooksAdapter(
            onNotebookClicked = { notebook ->
                val bundle = bundleOf(
                    "notebookId" to notebook.id,
                    "notebookName" to notebook.name
                )
                findNavController().navigate(R.id.action_hub_to_notebook_notes, bundle)
            },
            onNotebookLongClicked = { notebook ->
                showNotebookOptionsDialog(notebook)
            }
        )
        binding.rvNotebooks.layoutManager = LinearLayoutManager(requireContext())
        binding.rvNotebooks.adapter = notebooksAdapter

        searchResultsAdapter = SearchResultsAdapter(
            onResultClicked = { searchResult ->
                NoteEditorActivity.startView(requireContext(), searchResult.note.id)
            }
        )
        binding.rvSearchResults.layoutManager = LinearLayoutManager(requireContext())
        binding.rvSearchResults.adapter = searchResultsAdapter
    }

    private fun showNotebookOptionsDialog(notebook: NotebookEntity) {
        if (notebook.isBuiltIn) {
            Toast.makeText(requireContext(), "Clinical Guides notebook is permanent", Toast.LENGTH_SHORT).show()
            return
        }

        val options = arrayOf("Rename Notebook", "Delete Notebook")
        MaterialAlertDialogBuilder(requireContext())
            .setTitle(notebook.name)
            .setItems(options) { _, which ->
                when (which) {
                    0 -> showRenameNotebookDialog(notebook)
                    1 -> showDeleteNotebookDialog(notebook)
                }
            }
            .show()
    }

    private fun showRenameNotebookDialog(notebook: NotebookEntity) {
        val input = TextInputEditText(requireContext()).apply {
            hint = "Notebook Name"
            setText(notebook.name)
            isSingleLine = true
            setSelection(text?.length ?: 0)
        }
        val container = FrameLayout(requireContext()).apply {
            val pad = (20 * resources.displayMetrics.density).toInt()
            setPadding(pad, (8 * resources.displayMetrics.density).toInt(), pad, 0)
            addView(input)
        }

        MaterialAlertDialogBuilder(requireContext())
            .setTitle("Rename Notebook")
            .setView(container)
            .setPositiveButton("Save") { _, _ ->
                val newName = input.text?.toString().orEmpty().trim()
                if (newName.isNotEmpty() && newName != notebook.name) {
                    dbHelper.renameNotebook(notebook.id, newName)
                    loadNotebooks()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showDeleteNotebookDialog(notebook: NotebookEntity) {
        MaterialAlertDialogBuilder(requireContext())
            .setTitle("Delete '${notebook.name}'")
            .setMessage("What should happen to the notes in this notebook?")
            .setPositiveButton("Move to General") { _, _ ->
                dbHelper.deleteNotebook(notebook.id, moveToGeneral = true)
                loadNotebooks()
                Toast.makeText(requireContext(), "Notebook deleted, notes moved to General", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("Delete Notes Too") { _, _ ->
                dbHelper.deleteNotebook(notebook.id, moveToGeneral = false)
                loadNotebooks()
                Toast.makeText(requireContext(), "Notebook and notes deleted", Toast.LENGTH_SHORT).show()
            }
            .setNeutralButton("Cancel", null)
            .show()
    }

    private fun setupListeners() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.fabAddNotebook) { v, insets ->
            val navBars = insets.getInsets(WindowInsetsCompat.Type.navigationBars())
            val density = resources.displayMetrics.density
            val baseMargin = (96 * density).toInt()
            val params = v.layoutParams as ViewGroup.MarginLayoutParams
            params.bottomMargin = baseMargin + navBars.bottom
            params.rightMargin = (16 * density).toInt()
            v.layoutParams = params
            insets
        }

        binding.rvNotebooks.addOnScrollListener(object : RecyclerView.OnScrollListener() {
            override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
                if (dy > 4 && binding.fabAddNotebook.isExtended) {
                    binding.fabAddNotebook.shrink()
                } else if (dy < -4 && !binding.fabAddNotebook.isExtended) {
                    binding.fabAddNotebook.extend()
                }
            }
        })

        binding.fabAddNotebook.setOnClickListener {
            showCreateNotebookDialog()
        }

        binding.etSearchNotes.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            override fun afterTextChanged(s: Editable?) {
                handleSearch(s?.toString().orEmpty())
            }
        })
    }

    private fun handleSearch(query: String) {
        val trimmed = query.trim()
        if (trimmed.isEmpty()) {
            binding.rvNotebooks.visibility = View.VISIBLE
            binding.rvSearchResults.visibility = View.GONE
            binding.tvEmptySearch.visibility = View.GONE
            binding.fabAddNotebook.show()
            loadNotebooks()
        } else {
            binding.rvNotebooks.visibility = View.GONE
            binding.rvSearchResults.visibility = View.VISIBLE
            binding.fabAddNotebook.hide()

            val results = dbHelper.smartSearch(trimmed)
            searchResultsAdapter.submitResults(results, trimmed)

            if (results.isEmpty()) {
                binding.tvEmptySearch.text = "No notes found matching '$trimmed'"
                binding.tvEmptySearch.visibility = View.VISIBLE
            } else {
                binding.tvEmptySearch.visibility = View.GONE
            }
        }
    }

    private fun showCreateNotebookDialog() {
        val input = TextInputEditText(requireContext()).apply {
            hint = "e.g. Pediatrics, ICU, Surgery..."
            isSingleLine = true
        }
        val container = FrameLayout(requireContext()).apply {
            val pad = (20 * resources.displayMetrics.density).toInt()
            setPadding(pad, (8 * resources.displayMetrics.density).toInt(), pad, 0)
            addView(input)
        }

        MaterialAlertDialogBuilder(requireContext())
            .setTitle("New Notebook")
            .setView(container)
            .setPositiveButton("Create") { _, _ ->
                val name = input.text?.toString().orEmpty().trim()
                if (name.isNotEmpty()) {
                    dbHelper.insertNotebook(name)
                    loadNotebooks()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun loadNotebooks() {
        val notebooks = dbHelper.getAllNotebooksWithCounts()
        notebooksAdapter.submitList(notebooks)
    }

    override fun onResume() {
        super.onResume()
        val currentQuery = binding.etSearchNotes.text?.toString().orEmpty()
        if (currentQuery.isBlank()) {
            loadNotebooks()
        } else {
            handleSearch(currentQuery)
        }
        (requireActivity() as? AppCompatActivity)?.supportActionBar?.title = "Clinical Notes"
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
