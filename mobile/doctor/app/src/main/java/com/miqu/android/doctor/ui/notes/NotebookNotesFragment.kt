package com.miqu.android.doctor.ui.notes

import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.textfield.TextInputEditText
import com.miqu.android.doctor.data.NoteEntity
import com.miqu.android.doctor.data.NotesDatabaseHelper
import com.miqu.android.doctor.databinding.FragmentNotebookNotesBinding

class NotebookNotesFragment : Fragment() {

    private var _binding: FragmentNotebookNotesBinding? = null
    private val binding get() = _binding!!

    private lateinit var dbHelper: NotesDatabaseHelper
    private lateinit var adapter: NotesAdapter

    private var notebookId: Long = NotesDatabaseHelper.NOTEBOOK_ID_GENERAL
    private var notebookName: String = "Notes"

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentNotebookNotesBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        dbHelper = NotesDatabaseHelper.getInstance(requireContext())
        notebookId = arguments?.getLong("notebookId") ?: NotesDatabaseHelper.NOTEBOOK_ID_GENERAL
        notebookName = arguments?.getString("notebookName") ?: "Notes"

        (requireActivity() as? AppCompatActivity)?.supportActionBar?.title = notebookName

        setupRecyclerView()
        setupListeners()
        loadNotes()
    }

    private fun setupRecyclerView() {
        adapter = NotesAdapter(
            onNoteClicked = { note ->
                NoteEditorActivity.startView(requireContext(), note.id)
            },
            onNoteLongClicked = { note ->
                showNoteActionDialog(note)
            }
        )

        binding.rvNotes.layoutManager = LinearLayoutManager(requireContext())
        binding.rvNotes.adapter = adapter
    }

    private fun showNoteActionDialog(note: NoteEntity) {
        val options = mutableListOf<String>()
        options.add(if (note.isPinned) "Unpin Note" else "Pin to Top")
        options.add("Move to Notebook...")
        if (!note.isBuiltIn) {
            options.add("Delete Note")
        }

        MaterialAlertDialogBuilder(requireContext())
            .setTitle(note.title)
            .setItems(options.toTypedArray()) { _, which ->
                when (which) {
                    0 -> {
                        dbHelper.togglePin(note.id, !note.isPinned)
                        loadNotes()
                    }
                    1 -> {
                        showMoveToNotebookDialog(note)
                    }
                    2 -> {
                        dbHelper.deleteNote(note.id)
                        loadNotes()
                    }
                }
            }
            .show()
    }

    private fun showMoveToNotebookDialog(note: NoteEntity) {
        val notebooks = dbHelper.getAllNotebooksWithCounts()
            .filter { it.id != notebookId && (!it.isBuiltIn || note.isBuiltIn) }
        if (notebooks.isEmpty()) {
            return
        }

        val names = notebooks.map { it.name }.toTypedArray()
        MaterialAlertDialogBuilder(requireContext())
            .setTitle("Move '${note.title}' to:")
            .setItems(names) { _, index ->
                val target = notebooks[index]
                dbHelper.moveNoteToNotebook(note.id, target.id)
                loadNotes()
            }
            .show()
    }

    private fun setupListeners() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.fabAddNote) { v, insets ->
            val navBars = insets.getInsets(WindowInsetsCompat.Type.navigationBars())
            val density = resources.displayMetrics.density
            val baseMargin = (96 * density).toInt()
            val params = v.layoutParams as ViewGroup.MarginLayoutParams
            params.bottomMargin = baseMargin + navBars.bottom
            params.rightMargin = (16 * density).toInt()
            v.layoutParams = params
            insets
        }

        binding.rvNotes.addOnScrollListener(object : RecyclerView.OnScrollListener() {
            override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
                if (dy > 4 && binding.fabAddNote.isExtended) {
                    binding.fabAddNote.shrink()
                } else if (dy < -4 && !binding.fabAddNote.isExtended) {
                    binding.fabAddNote.extend()
                }
            }
        })

        binding.fabAddNote.setOnClickListener {
            showNewNoteTitleDialog()
        }

        binding.etSearchNotes.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            override fun afterTextChanged(s: Editable?) {
                loadNotes()
            }
        })
    }

    private fun showNewNoteTitleDialog() {
        val input = TextInputEditText(requireContext()).apply {
            hint = "e.g. Ward Rounds, Protocol..."
            isSingleLine = true
        }
        val container = android.widget.FrameLayout(requireContext()).apply {
            val pad = (20 * resources.displayMetrics.density).toInt()
            setPadding(pad, (8 * resources.displayMetrics.density).toInt(), pad, 0)
            addView(input)
        }

        MaterialAlertDialogBuilder(requireContext())
            .setTitle("New Note Title")
            .setView(container)
            .setPositiveButton("Create") { _, _ ->
                val title = input.text?.toString().orEmpty().trim().ifEmpty { "Untitled Note" }
                NoteEditorActivity.startCreate(requireContext(), title, notebookId)
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun loadNotes() {
        val query = binding.etSearchNotes.text?.toString().orEmpty()
        val notes = dbHelper.getNotesInNotebook(notebookId, query)
        adapter.submitList(notes)

        if (notes.isEmpty()) {
            binding.tvEmptyNotes.visibility = View.VISIBLE
        } else {
            binding.tvEmptyNotes.visibility = View.GONE
        }
    }

    override fun onResume() {
        super.onResume()
        loadNotes()
        (requireActivity() as? AppCompatActivity)?.supportActionBar?.title = notebookName
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
