package com.example.smartfire

import android.Manifest
import android.content.pm.PackageManager
import androidx.compose.foundation.layout.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.sp
import androidx.core.app.ActivityCompat
import androidx.compose.ui.platform.LocalContext
import androidx.navigation.NavController
import BluetoothViewModel
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.example.smartfire.ui.theme.SmartFireTheme

class MainActivity : ComponentActivity() {
    lateinit var bluetoothManager: BluetoothManager
    lateinit var bluetoothAdapter: BluetoothAdapter
    lateinit var permissionLauncher: ActivityResultLauncher<Array<String>>
    lateinit var takeResultLauncher: ActivityResultLauncher<Intent>

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        bluetoothManager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        permissionLauncher = registerForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()
        ) { permissions ->
            val scanGranted = permissions[android.Manifest.permission.BLUETOOTH_SCAN] ?: false
            val connectGranted = permissions[android.Manifest.permission.BLUETOOTH_CONNECT] ?: false

            if (scanGranted && connectGranted) {
                val intent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                takeResultLauncher.launch(intent)
            } else {
                Toast.makeText(this, "Bluetooth permissions denied", Toast.LENGTH_SHORT).show()
            }
        }

        takeResultLauncher = registerForActivityResult(
            ActivityResultContracts.StartActivityForResult()
        ) { result ->
            if (result.resultCode == RESULT_OK) {
                Toast.makeText(this, "Bluetooth Enabled", Toast.LENGTH_SHORT).show()
            } else {
                Toast.makeText(this, "Bluetooth Denied", Toast.LENGTH_SHORT).show()
            }
        }

        enableEdgeToEdge()
        setContent {
            SmartFireTheme {
                val navController = rememberNavController()
                val viewModel: BluetoothViewModel = viewModel()

                NavHost(navController = navController, startDestination = "main") {
                    composable("main") {
                        MainScreen(
                            bluetoothAdapter = bluetoothAdapter,
                            permissionLauncher = permissionLauncher,
                            navController = navController
                        )
                    }
                    composable("home") {
                        HomeScreen(
                            bluetoothAdapter = bluetoothAdapter,
                            viewModel = viewModel,
                            onNavigateToConnected = { navController.navigate("connected") }
                        )
                    }
                    composable("connected") {
                        ConnectedScreen(viewModel = viewModel)
                    }
                }
            }
        }
    }
}

@Composable
fun MainScreen(
    bluetoothAdapter: BluetoothAdapter,
    permissionLauncher: ActivityResultLauncher<Array<String>>,
    navController: androidx.navigation.NavController
) {
    val context = androidx.compose.ui.platform.LocalContext.current

    Column(
        modifier = Modifier.fillMaxSize(),
        horizontalAlignment = androidx.compose.ui.Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text("SmartFire", fontSize = 30.sp, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
        Spacer(modifier = Modifier.height(20.dp))

        OutlinedButton(onClick = {
            permissionLauncher.launch(arrayOf(
                android.Manifest.permission.BLUETOOTH_SCAN,
                android.Manifest.permission.BLUETOOTH_CONNECT
            ))
        }) {
            Text("Bluetooth ON", fontSize = 30.sp, fontWeight = FontWeight.Bold)
        }

        Spacer(modifier = Modifier.height(20.dp))

        OutlinedButton(onClick = {
            val intent = Intent(android.provider.Settings.ACTION_BLUETOOTH_SETTINGS)
            context.startActivity(intent)
        }) {
            Text("Bluetooth OFF", fontSize = 30.sp, fontWeight = FontWeight.Bold)
        }

        Spacer(modifier = Modifier.height(20.dp))

        OutlinedButton(onClick = {
            permissionLauncher.launch(arrayOf(
                android.Manifest.permission.BLUETOOTH_SCAN,
                android.Manifest.permission.BLUETOOTH_CONNECT
            ))
            navController.navigate("home")
        }) {
            Text("Display Bluetooth Paired Devices", fontSize = 30.sp, fontWeight = FontWeight.Bold)
        }
    }
}

@Composable
fun HomeScreen(
    bluetoothAdapter: BluetoothAdapter,
    viewModel: BluetoothViewModel,
    onNavigateToConnected: () -> Unit
) {
    val context = LocalContext.current
    val pairedDevices = bluetoothAdapter.bondedDevices.toList()

    val hasScanPermission = ActivityCompat.checkSelfPermission(
        context,
        Manifest.permission.BLUETOOTH_SCAN
    ) == PackageManager.PERMISSION_GRANTED

    if (!hasScanPermission) {
        Toast.makeText(context, "Missing BLUETOOTH_SCAN permission", Toast.LENGTH_LONG).show()
        return
    }

    Column(modifier = Modifier.padding(16.dp)) {
        Text("Paired Devices", fontSize = 20.sp, fontWeight = FontWeight.Bold)
        Spacer(modifier = Modifier.height(8.dp))
        pairedDevices.forEach { device ->
            Column(modifier = Modifier.padding(8.dp)) {
                Text("Name: ${device.name}")
                Text("Address: ${device.address}")
                Button(onClick = {
                    bluetoothAdapter.cancelDiscovery()
                    viewModel.connectToDevice(device)
                    onNavigateToConnected()
                }) {
                    Text("Connect")
                }
            }
        }
    }
}


@Composable
fun ConnectedScreen(viewModel: BluetoothViewModel) {
    val receivedText by viewModel.receivedText.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.Top,
        horizontalAlignment = Alignment.Start
    ) {
        Text("Connected!", fontSize = 24.sp, fontWeight = FontWeight.Bold)
        Spacer(modifier = Modifier.height(16.dp))
        Text(receivedText, fontSize = 16.sp)
        Spacer(modifier = Modifier.height(16.dp))
        Button(onClick = { viewModel.reconnect() }) {
            Text("Reconnect")
        }
    }
}
