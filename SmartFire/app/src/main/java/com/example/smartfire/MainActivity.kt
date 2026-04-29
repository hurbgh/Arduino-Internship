package com.example.smartfire

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Intent
import android.content.pm.PackageManager
import android.media.Ringtone
import android.media.RingtoneManager
import android.os.Bundle
import android.provider.Settings
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.ActivityCompat
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import org.json.JSONObject
import com.example.smartfire.ui.theme.SmartFireTheme // adjust package if needed

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
            val scanGranted = permissions[Manifest.permission.BLUETOOTH_SCAN] ?: false
            val connectGranted = permissions[Manifest.permission.BLUETOOTH_CONNECT] ?: false

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


        setContent {
            SmartFireTheme {
                val navController = rememberNavController()
                val viewModel: BluetoothViewModel =
                    viewModel(factory = ViewModelProvider.AndroidViewModelFactory.getInstance(application))

                LaunchedEffect(Unit) {
                    viewModel.setupMqtt()
                }

                NavHost(navController = navController, startDestination = "main") {
                    composable("main") {
                        MainScreen(
                            bluetoothAdapter = bluetoothAdapter,
                            permissionLauncher = permissionLauncher,
                            navController = navController,
                            viewModel = viewModel
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
    navController: NavController,
    viewModel: BluetoothViewModel
) {
    val context = LocalContext.current

    Column(
        modifier = Modifier.fillMaxSize(),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Text(
            "SmartFire",
            fontSize = 30.sp,
            modifier = Modifier.fillMaxWidth(),
            textAlign = TextAlign.Center
        )
        Spacer(modifier = Modifier.height(20.dp))

        OutlinedButton(onClick = {
            permissionLauncher.launch(
                arrayOf(
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT
                )
            )
        }) {
            Text("Bluetooth ON", fontSize = 30.sp, fontWeight = FontWeight.Bold)
        }

        Spacer(modifier = Modifier.height(20.dp))

        OutlinedButton(onClick = {
            val intent = Intent(Settings.ACTION_BLUETOOTH_SETTINGS)
            context.startActivity(intent)
        }) {
            Text("Bluetooth Settings", fontSize = 30.sp, fontWeight = FontWeight.Bold)
        }

        Spacer(modifier = Modifier.height(20.dp))

        OutlinedButton(onClick = {
            permissionLauncher.launch(
                arrayOf(
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT
                )
            )
            navController.navigate("home")
        }) {
            Text("Display Bluetooth Paired Devices", fontSize = 30.sp, fontWeight = FontWeight.Bold)
        }

        Spacer(modifier = Modifier.height(20.dp))

        OutlinedButton(onClick = {
            viewModel.publishJson("testdevice", """{"test":"hello"}""")
        }) {
            Text("Test MQTT Publish", fontSize = 20.sp, fontWeight = FontWeight.Bold)
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
fun MqttStatusDot(isConnected: Boolean) {
    val color = if (isConnected)
        Color(0xFF34C759)
    else
        Color(0xFFFF3B30)

    Box(
        modifier = Modifier
            .size(14.dp)
            .padding(4.dp)
            .background(color, shape = CircleShape)
    )
}

@Composable
fun ConnectedScreen(viewModel: BluetoothViewModel) {
    val latestValues by viewModel.latestValues.collectAsState()
    val sentValues by viewModel.sentValues.collectAsState()
    val mqttStatus by viewModel.mqttStatus.collectAsState()
    val alarmMessage by viewModel.alarmMessage.collectAsState()

    val context = LocalContext.current

    val alarmRingtone: Ringtone? = remember {
        val uri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)
            ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)
        RingtoneManager.getRingtone(context, uri)
    }

    LaunchedEffect(alarmMessage) {
        if (alarmMessage != null) {
            alarmRingtone?.play()
        } else {
            alarmRingtone?.stop()
        }
    }

    if (alarmMessage != null) {
        AlertDialog(
            onDismissRequest = { /* force explicit acknowledgement */ },
            title = { Text("FIRE / AIR QUALITY ALERT") },
            text = { Text(alarmMessage ?: "") },
            confirmButton = {
                TextButton(onClick = { viewModel.acknowledgeAlarm() }) {
                    Text("OK")
                }
            }
        )
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.Top
    ) {

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Connected!", fontSize = 24.sp, fontWeight = FontWeight.Bold)
            MqttStatusDot(isConnected = mqttStatus)
        }

        Spacer(modifier = Modifier.height(16.dp))

        Button(onClick = { viewModel.reconnect() }) {
            Text("Reconnect")
        }

        Spacer(modifier = Modifier.height(24.dp))

        Text("Dashboard", fontSize = 20.sp, fontWeight = FontWeight.Bold)
        Spacer(modifier = Modifier.height(8.dp))
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
            elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
        ) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text("Packet ID: ${latestValues["packetId"] ?: "-"}")
                Text("ESP32 Send TS: ${latestValues["esp32_send_ts"] ?: "-"}")
                Text("Phone Received TS: ${latestValues["phone_received_ts"] ?: "-"}")
                Text("Phone Forwarded TS: ${latestValues["phone_forwarded_ts"] ?: "-"}")

                Spacer(modifier = Modifier.height(8.dp))

                Text("PM1: ${latestValues["pm1"] ?: "-"}")
                Text("PM2.5: ${latestValues["pm25"] ?: "-"}")
                Text("PM10: ${latestValues["pm10"] ?: "-"}")

                Text("CO₂: ${latestValues["co2"] ?: "-"} ppm")
                Text("CO: ${latestValues["co"] ?: "-"} ppm")

                Text("Temperature: ${latestValues["temp"] ?: "-"} °C")
                Text("Humidity: ${latestValues["humidity"] ?: "-"} %")
                Text("Pressure: ${latestValues["pressure"] ?: "-"} hPa")
                Text("Altitude: ${latestValues["altitude"] ?: "-"} m")

                Text("CRC: ${latestValues["crc"] ?: "-"}")

                Spacer(modifier = Modifier.height(8.dp))
                Text("Raw Payload:")
                Text(latestValues["payload_raw"] ?: "-", fontSize = 12.sp)
            }
        }

        Spacer(modifier = Modifier.height(24.dp))

        Text("Sent Values", fontSize = 20.sp, fontWeight = FontWeight.Bold)
        Spacer(modifier = Modifier.height(8.dp))

        LazyColumn(
            modifier = Modifier.weight(1f)
        ) {
            itemsIndexed(sentValues) { _, jsonString ->
                val obj = remember(jsonString) {
                    try {
                        JSONObject(jsonString)
                    } catch (_: Exception) {
                        JSONObject()
                    }
                }

                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
                    elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text("packetId: ${obj.optString("packetId", "-")}")
                        Text("esp32_send_ts: ${obj.optString("esp32_send_ts", "-")}")
                        Text("phone_received_ts: ${obj.optLong("phone_received_ts", 0L)}")
                        Text("phone_forwarded_ts: ${obj.optLong("phone_forwarded_ts", 0L)}")

                        Text("PM1: ${obj.optString("pm1", "-")}")
                        Text("PM2.5: ${obj.optString("pm25", "-")}")
                        Text("PM10: ${obj.optString("pm10", "-")}")
                        Text("CO₂: ${obj.optString("co2", "-")} ppm")
                        Text("CO: ${obj.optString("co", "-")} ppm")
                        Text("Temperature: ${obj.optString("temp", "-")} °C")
                        Text("Humidity: ${obj.optString("humidity", "-")} %")
                        Text("Pressure: ${obj.optString("pressure", "-")} hPa")
                        Text("Altitude: ${obj.optString("altitude", "-")} m")
                        Text("CRC: ${obj.optString("crc", "-")}")

                        Text("payload_raw:")
                        Text(obj.optString("payload_raw", "-"), fontSize = 12.sp)
                    }
                }
            }
        }
    }
}
