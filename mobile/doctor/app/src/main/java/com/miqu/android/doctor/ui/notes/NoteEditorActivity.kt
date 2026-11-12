package com.miqu.android.doctor.ui.notes

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.ActionMode
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.miqu.android.doctor.R
import com.miqu.android.doctor.data.NoteEntity
import com.miqu.android.doctor.data.NotesDatabaseHelper
import com.miqu.android.doctor.databinding.ActivityNoteEditorBinding
import io.noties.markwon.Markwon
import io.noties.markwon.ext.strikethrough.StrikethroughPlugin
import io.noties.markwon.ext.tables.TablePlugin
import io.noties.markwon.image.ImagesPlugin

class NoteEditorActivity : AppCompatActivity() {

    private lateinit var binding: ActivityNoteEditorBinding
    private lateinit var dbHelper: NotesDatabaseHelper
    private lateinit var markwon: Markwon

    private var noteId: Long = 0L
    private var noteTitle: String = ""
    private var noteCategory: String = "General"
    private var notebookId: Long = NotesDatabaseHelper.NOTEBOOK_ID_GENERAL
    private var isPinned: Boolean = false
    private var isBuiltIn: Boolean = false
    private var initialContent: String = ""
    private var isEditMode: Boolean = false

    companion object {
        const val EXTRA_NOTE_ID = "extra_note_id"
        const val EXTRA_NOTE_TITLE = "extra_note_title"
        const val EXTRA_START_IN_EDIT = "extra_start_in_edit"
        const val EXTRA_NOTEBOOK_ID = "extra_notebook_id"

        private const val MENU_BOLD = 1001
        private const val MENU_ITALIC = 1002
        private const val MENU_H1 = 1003
        private const val MENU_H2 = 1004
        private const val MENU_STRIKE = 1005
        private const val MENU_CODE = 1006
        private const val MENU_LINK = 1007
        private const val MENU_IMAGE = 1008

        fun startView(context: Context, noteId: Long) {
            val intent = Intent(context, NoteEditorActivity::class.java).apply {
                putExtra(EXTRA_NOTE_ID, noteId)
                putExtra(EXTRA_START_IN_EDIT, false)
            }
            context.startActivity(intent)
        }

        fun startCreate(context: Context, noteTitle: String, notebookId: Long = NotesDatabaseHelper.NOTEBOOK_ID_GENERAL) {
            val intent = Intent(context, NoteEditorActivity::class.java).apply {
                putExtra(EXTRA_NOTE_TITLE, noteTitle)
                putExtra(EXTRA_NOTEBOOK_ID, notebookId)
                putExtra(EXTRA_START_IN_EDIT, true)
            }
            context.startActivity(intent)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        binding = ActivityNoteEditorBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ViewCompat.setOnApplyWindowInsetsListener(binding.appBarLayout) { v, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            v.updatePadding(top = statusBars.top)
            insets
        }

        ViewCompat.setOnApplyWindowInsetsListener(binding.etNoteContent) { v, insets ->
            val ime = insets.getInsets(WindowInsetsCompat.Type.ime())
            val navBars = insets.getInsets(WindowInsetsCompat.Type.navigationBars())
            v.updatePadding(bottom = maxOf(ime.bottom, navBars.bottom))
            insets
        }

        dbHelper = NotesDatabaseHelper.getInstance(this)
        markwon = Markwon.builder(this)
            .usePlugin(TablePlugin.create(this))
            .usePlugin(StrikethroughPlugin.create())
            .usePlugin(ImagesPlugin.create())
            .build()

        noteId = intent.getLongExtra(EXTRA_NOTE_ID, 0L)
        noteTitle = intent.getStringExtra(EXTRA_NOTE_TITLE).orEmpty()
        notebookId = intent.getLongExtra(EXTRA_NOTEBOOK_ID, NotesDatabaseHelper.NOTEBOOK_ID_GENERAL)
        val startInEdit = intent.getBooleanExtra(EXTRA_START_IN_EDIT, false)

        loadNoteData()

        isEditMode = startInEdit || noteId == 0L

        binding.toolbar.setNavigationOnClickListener {
            handleBackAction()
        }

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                handleBackAction()
            }
        })

        binding.toolbar.setOnClickListener {
            showRenameDialog()
        }

        setupToolbarMenu()
        setupTextSelectionPopup()
        applyMode()
    }

    private fun loadNoteData() {
        if (noteId != 0L) {
            val existing = dbHelper.getNoteById(noteId)
            if (existing != null) {
                notebookId = existing.notebookId
                noteTitle = existing.title
                noteCategory = existing.category
                isPinned = existing.isPinned
                isBuiltIn = existing.isBuiltIn
                initialContent = existing.content
                binding.etNoteContent.setText(existing.content)
            }
        }
        val nbName = dbHelper.getNotebookById(notebookId)?.name ?: "General"
        binding.toolbar.title = if (noteTitle.isNotEmpty()) noteTitle else "Note"
        binding.toolbar.subtitle = if (isBuiltIn) "Clinical Guides • $noteCategory" else "$nbName • $noteCategory"
    }

    private fun applyMode() {
        if (isEditMode) {
            binding.scrollPreviewArea.visibility = View.GONE
            binding.etNoteContent.visibility = View.VISIBLE
            binding.etNoteContent.requestFocus()
        } else {
            WindowCompat.getInsetsController(window, binding.etNoteContent).hide(WindowInsetsCompat.Type.ime())

            binding.etNoteContent.visibility = View.GONE
            binding.scrollPreviewArea.visibility = View.VISIBLE

            val content = binding.etNoteContent.text?.toString().orEmpty()
            val renderedMarkdown = if (noteTitle.isNotEmpty() && !content.startsWith("# ")) {
                "# $noteTitle\n\n$content"
            } else {
                content
            }
            markwon.setMarkdown(binding.tvEditorPreview, renderedMarkdown)
        }
        updateToolbarMenu()
    }

    private fun updateToolbarMenu() {
        val menu = binding.toolbar.menu
        val editItem = menu.findItem(R.id.action_edit)
        val previewItem = menu.findItem(R.id.action_preview)
        val saveItem = menu.findItem(R.id.action_save)
        val pinItem = menu.findItem(R.id.action_pin)
        val renameItem = menu.findItem(R.id.action_rename)
        val deleteItem = menu.findItem(R.id.action_delete)
        val resetItem = menu.findItem(R.id.action_reset_builtin)
        val moveItem = menu.findItem(R.id.action_move_notebook)

        renameItem?.isVisible = true

        if (isEditMode) {
            editItem?.isVisible = false
            previewItem?.isVisible = true
            saveItem?.isVisible = true
            pinItem?.isVisible = false
            deleteItem?.isVisible = false
            resetItem?.isVisible = false
            moveItem?.isVisible = false
        } else {
            editItem?.isVisible = true
            previewItem?.isVisible = false
            saveItem?.isVisible = false
            pinItem?.isVisible = true
            pinItem?.title = if (isPinned) "Unpin Note" else "Pin to Top"
            deleteItem?.isVisible = !isBuiltIn
            resetItem?.isVisible = isBuiltIn
            moveItem?.isVisible = !isBuiltIn
        }
    }

    private fun setupToolbarMenu() {
        binding.toolbar.setOnMenuItemClickListener { menuItem ->
            when (menuItem.itemId) {
                R.id.action_edit -> {
                    isEditMode = true
                    applyMode()
                    WindowCompat.getInsetsController(window, binding.etNoteContent).show(WindowInsetsCompat.Type.ime())
                    true
                }
                R.id.action_preview -> {
                    isEditMode = false
                    applyMode()
                    true
                }
                R.id.action_save -> {
                    saveNote(finishAfterSave = false)
                    isEditMode = false
                    applyMode()
                    true
                }
                R.id.action_rename -> {
                    showRenameDialog()
                    true
                }
                R.id.action_pin -> {
                    togglePin()
                    true
                }
                R.id.action_move_notebook -> {
                    showMoveNotebookDialog()
                    true
                }
                R.id.action_delete -> {
                    confirmDelete()
                    true
                }
                R.id.action_reset_builtin -> {
                    confirmReset()
                    true
                }
                else -> false
            }
        }
    }

    private fun showRenameDialog() {
        val input = com.google.android.material.textfield.TextInputEditText(this).apply {
            hint = "Note Title"
            setText(noteTitle)
            isSingleLine = true
            setSelection(text?.length ?: 0)
        }
        val container = android.widget.FrameLayout(this).apply {
            val pad = (20 * resources.displayMetrics.density).toInt()
            setPadding(pad, (8 * resources.displayMetrics.density).toInt(), pad, 0)
            addView(input)
        }

        MaterialAlertDialogBuilder(this)
            .setTitle("Rename Note")
            .setView(container)
            .setPositiveButton("Save") { _, _ ->
                val newTitle = input.text?.toString().orEmpty().trim().ifEmpty { "Untitled Note" }
                if (newTitle != noteTitle) {
                    noteTitle = newTitle
                    binding.toolbar.title = newTitle

                    if (noteId != 0L) {
                        val existing = dbHelper.getNoteById(noteId)
                        if (existing != null) {
                            dbHelper.updateNote(existing.copy(title = newTitle, updatedAt = System.currentTimeMillis()))
                        }
                    }

                    if (!isEditMode) {
                        applyMode()
                    }

                    Toast.makeText(this, "Title updated", Toast.LENGTH_SHORT).show()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showMoveNotebookDialog() {
        val notebooks = dbHelper.getAllNotebooksWithCounts().filter { it.id != notebookId }
        if (notebooks.isEmpty()) {
            Toast.makeText(this, "No other notebooks available", Toast.LENGTH_SHORT).show()
            return
        }
        val names = notebooks.map { it.name }.toTypedArray()
        MaterialAlertDialogBuilder(this)
            .setTitle("Move to Notebook")
            .setItems(names) { _, index ->
                val target = notebooks[index]
                notebookId = target.id
                if (noteId != 0L) {
                    dbHelper.moveNoteToNotebook(noteId, target.id)
                }
                loadNoteData()
                Toast.makeText(this, "Moved to ${target.name}", Toast.LENGTH_SHORT).show()
            }
            .show()
    }

    private fun togglePin() {
        if (noteId != 0L) {
            val newPin = !isPinned
            dbHelper.togglePin(noteId, newPin)
            isPinned = newPin
            updateToolbarMenu()
            Toast.makeText(this, if (newPin) "Note pinned to top" else "Note unpinned", Toast.LENGTH_SHORT).show()
        }
    }

    private fun confirmDelete() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Delete Note")
            .setMessage("Are you sure you want to delete this clinical note?")
            .setPositiveButton("Delete") { _, _ ->
                if (noteId != 0L) {
                    dbHelper.deleteNote(noteId)
                }
                finish()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun confirmReset() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Reset Clinical Guide")
            .setMessage("Restore this clinical guide to its original built-in template? Any edits will be overwritten.")
            .setPositiveButton("Reset") { _, _ ->
                if (noteId != 0L) {
                    val ok = dbHelper.resetBuiltInNote(noteId)
                    if (ok) {
                        loadNoteData()
                        applyMode()
                        Toast.makeText(this, "Restored to original guide", Toast.LENGTH_SHORT).show()
                    }
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun handleBackAction() {
        if (isEditMode) {
            saveNote(finishAfterSave = true)
        } else {
            finish()
        }
    }

    private fun saveNote(finishAfterSave: Boolean) {
        val title = noteTitle.trim().ifEmpty { "Untitled Note" }
        val content = binding.etNoteContent.text?.toString().orEmpty()

        if (noteId != 0L) {
            if (content != initialContent) {
                val existing = dbHelper.getNoteById(noteId)
                if (existing != null) {
                    val updated = existing.copy(
                        title = title,
                        content = content,
                        updatedAt = System.currentTimeMillis()
                    )
                    dbHelper.updateNote(updated)
                    initialContent = content
                    Toast.makeText(this, "Note saved", Toast.LENGTH_SHORT).show()
                }
            }
        } else {
            if (content.isNotBlank() || title.isNotBlank()) {
                val newNote = NoteEntity(
                    notebookId = notebookId,
                    title = title,
                    content = content,
                    category = "General",
                    isPinned = false,
                    isBuiltIn = false
                )
                val newId = dbHelper.insertNote(newNote)
                if (newId > 0) {
                    noteId = newId
                    initialContent = content
                    Toast.makeText(this, "Note saved", Toast.LENGTH_SHORT).show()
                }
            }
        }

        if (finishAfterSave) {
            finish()
        }
    }

    private fun setupTextSelectionPopup() {
        binding.etNoteContent.customSelectionActionModeCallback = object : ActionMode.Callback {
            override fun onCreateActionMode(mode: ActionMode, menu: Menu): Boolean {
                menu.add(Menu.NONE, MENU_BOLD, 1, "Bold")
                menu.add(Menu.NONE, MENU_ITALIC, 2, "Italic")
                menu.add(Menu.NONE, MENU_H1, 3, "H1")
                menu.add(Menu.NONE, MENU_H2, 4, "H2")
                menu.add(Menu.NONE, MENU_STRIKE, 5, "Strike")
                menu.add(Menu.NONE, MENU_CODE, 6, "Code")
                menu.add(Menu.NONE, MENU_LINK, 7, "Link")
                menu.add(Menu.NONE, MENU_IMAGE, 8, "Image")
                return true
            }

            override fun onPrepareActionMode(mode: ActionMode, menu: Menu): Boolean = true

            override fun onActionItemClicked(mode: ActionMode, item: MenuItem): Boolean {
                val start = binding.etNoteContent.selectionStart
                val end = binding.etNoteContent.selectionEnd
                if (start < 0 || end < 0 || start == end) return false

                val selectedText = binding.etNoteContent.text?.subSequence(start, end).toString()
                val replacement = when (item.itemId) {
                    MENU_BOLD -> "**$selectedText**"
                    MENU_ITALIC -> "*$selectedText*"
                    MENU_H1 -> "# $selectedText"
                    MENU_H2 -> "## $selectedText"
                    MENU_STRIKE -> "~~$selectedText~~"
                    MENU_CODE -> "`$selectedText`"
                    MENU_LINK -> "[$selectedText](url)"
                    MENU_IMAGE -> "![$selectedText](https://)"
                    else -> return false
                }

                binding.etNoteContent.text?.replace(start, end, replacement)
                val newCursor = start + replacement.length
                binding.etNoteContent.setSelection(newCursor.coerceAtMost(binding.etNoteContent.text?.length ?: 0))
                mode.finish()
                return true
            }

            override fun onDestroyActionMode(mode: ActionMode) {}
        }
    }
}
