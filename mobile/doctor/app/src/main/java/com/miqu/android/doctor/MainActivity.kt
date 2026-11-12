package com.miqu.android.doctor

import android.os.Bundle
import android.view.View
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.setupWithNavController
import com.miqu.android.doctor.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Handle edge-to-edge system insets
        ViewCompat.setOnApplyWindowInsetsListener(binding.coordinator) { _, insets ->
            val statusBars = insets.getInsets(WindowInsetsCompat.Type.statusBars())
            val navBars = insets.getInsets(WindowInsetsCompat.Type.navigationBars())
            binding.appBarLayout.updatePadding(top = statusBars.top)
            binding.bottomNav.updatePadding(bottom = navBars.bottom)
            insets
        }

        // Set up Jetpack Navigation with Toolbar and BottomNavigationView
        val navHostFragment = supportFragmentManager.findFragmentById(R.id.navHostFragment) as NavHostFragment
        val navController = navHostFragment.navController
        val topLevelDestinations = setOf(R.id.nav_calculators, R.id.nav_notes)
        val appBarConfiguration = AppBarConfiguration(topLevelDestinations)

        binding.topToolbar.setupWithNavController(navController, appBarConfiguration)
        binding.bottomNav.setupWithNavController(navController)

        // Top toolbar menu
        binding.topToolbar.inflateMenu(R.menu.menu_main)
        binding.topToolbar.setOnMenuItemClickListener { menuItem ->
            if (menuItem.itemId == R.id.action_about) {
                showAboutDialog()
                true
            } else {
                false
            }
        }

        // Show/hide bottom nav based on destination
        navController.addOnDestinationChangedListener { _, destination, _ ->
            if (destination.id in topLevelDestinations) {
                binding.bottomNav.visibility = View.VISIBLE
            } else {
                binding.bottomNav.visibility = View.GONE
            }
        }
    }

    private fun showAboutDialog() {
        val dialogView = layoutInflater.inflate(R.layout.dialog_about, null)
        com.google.android.material.dialog.MaterialAlertDialogBuilder(this)
            .setView(dialogView)
            .setPositiveButton("Close", null)
            .show()
    }
}