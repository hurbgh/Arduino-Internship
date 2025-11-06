package com.example.smartfire

import BluetoothViewModel
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.ActivityResultCallback
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.example.smartfire.ui.theme.SmartFireTheme



class MainActivity : ComponentActivity() {
    lateinit var bluetoothManager: BluetoothManager
    lateinit var bluetoothAdapter: BluetoothAdapter
    lateinit var takePermission: ActivityResultLauncher<String>
    lateinit var takeResultLauncher: ActivityResultLauncher<Intent>
    override fun onCreate(savedInstanceState: Bundle?) {

        super.onCreate(savedInstanceState)

        bluetoothManager=getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter=bluetoothManager.adapter
        takePermission=registerForActivityResult(ActivityResultContracts.RequestPermission()){
            if (it){
                    val intent= Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                    takeResultLauncher.launch(intent)
                }else{
                    Toast.makeText(this, "Bluetooth Denied", Toast.LENGTH_SHORT).show()

            }
        }
        takeResultLauncher=registerForActivityResult(ActivityResultContracts.StartActivityForResult(),
            ActivityResultCallback {
                result->
                if (result.resultCode== RESULT_OK) {
                    Toast.makeText(this, "Bluetooth Enabled", Toast.LENGTH_SHORT).show()
                }else
                {
                    Toast.makeText(this, "Bluetooth Denied", Toast.LENGTH_SHORT).show()
                }
                })


        enableEdgeToEdge()
        setContent {
            SmartFireTheme {
                val navController = rememberNavController()
                val viewModel: BluetoothViewModel = viewModel()

                NavHost(navController = navController, startDestination = "main") {
                    composable("main") {
                        MainScreen(
                            bluetoothAdapter = bluetoothAdapter,
                            takePermission = takePermission,
                            navController = navController
                        )
                    }
                    composable("home") {
                        HomeScreen(
                            bluetoothAdapter = bluetoothAdapter,
                            viewModel = viewModel, // ✅ Pass it here
                            onNavigateToConnected = { navController.navigate("connected") }
                        )
                    }
                    composable("connected") {
                        ConnectedScreen(viewModel = viewModel) // ✅ Pass it here too
                    }
                }

            }
        }

    }
}



@Composable
fun HomeScreen(
    bluetoothAdapter: BluetoothAdapter,
    viewModel: BluetoothViewModel,
    onNavigateToConnected: () -> Unit
) {
    val pairedDevices = bluetoothAdapter.bondedDevices.toList()

    Column(modifier = Modifier.padding(16.dp)) {
        Text("Paired Devices", fontSize = 20.sp, fontWeight = FontWeight.Bold)
        Spacer(modifier = Modifier.height(8.dp))
        pairedDevices.forEach { device ->
            Column(modifier = Modifier.padding(8.dp)) {
                Text("Name: ${device.name}")
                Text("Address: ${device.address}")

                Button(onClick = {
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

@Composable
fun MainScreen(
    bluetoothAdapter: BluetoothAdapter,
    takePermission: ActivityResultLauncher<String>,
    navController: androidx.navigation.NavController
) {
    val context = androidx.compose.ui.platform.LocalContext.current

    Column(
        modifier = Modifier.fillMaxSize(),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text("SmartFire", fontSize = 30.sp, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
        Spacer(modifier = Modifier.height(20.dp))

        OutlinedButton(onClick = {
            takePermission.launch(android.Manifest.permission.BLUETOOTH_CONNECT)
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
            navController.navigate("home")
        }) {
            Text("Display Bluetooth Paired Devices", fontSize = 30.sp, fontWeight = FontWeight.Bold)
        }
    }
}



