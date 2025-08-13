package com.example.bledproject.bluetooth

import android.Manifest
import android.content.pm.PackageManager
import android.widget.Toast
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.core.app.ActivityCompat

@Composable
fun TestBluetoothScreen(bluetoothViewModel: BluetoothViewModel) {

	// Show toast when connected/disconnected
	LaunchedEffect(key1 = bluetoothViewModel.connected.value) {
		if (bluetoothViewModel.connected.value) {
			Toast.makeText(
				bluetoothViewModel.context,
				"Connected to ${bluetoothViewModel.connectedDevice.value}",
				Toast.LENGTH_SHORT
			).show()
		} else {
			Toast.makeText(
				bluetoothViewModel.context,
				"Disconnected",
				Toast.LENGTH_SHORT
			).show()
		}
	}

	Column(modifier = Modifier.fillMaxSize()) {
		Row(modifier = Modifier.fillMaxWidth()) {
			Button(
				onClick = {
					if (bluetoothViewModel.scanning.value) {
						bluetoothViewModel.stopScan()
					} else {
						bluetoothViewModel.startScan()
					}
				}
			) {
				Text(if (bluetoothViewModel.scanning.value) "Stop Scan" else "Start Scan")
			}

			if (bluetoothViewModel.connected.value) {
				Button(
					modifier = Modifier.fillMaxWidth(),
					onClick = {
						bluetoothViewModel.disconnect()
					}
				) {
					Text("Disconnect")
				}
			}
		}

		LazyColumn(modifier = Modifier.weight(1f)) {
			bluetoothViewModel.devices.forEach { device ->
				item {
					Row(modifier = Modifier.fillMaxWidth()) {
						if (ActivityCompat.checkSelfPermission(
								bluetoothViewModel.context,
								Manifest.permission.BLUETOOTH_CONNECT
							) != PackageManager.PERMISSION_GRANTED
						) {
							return@item
						}
						Text(device.name ?: "Unnamed device")
						Text(
							modifier = Modifier.weight(1f),
							text = device.address
						)
						Button(onClick = {
							bluetoothViewModel.connectToDevice(device)
						}) {
							Text("Connect")
						}
					}
				}
			}
		}

		if (bluetoothViewModel.connected.value) {
			Row(modifier = Modifier.fillMaxWidth()) {
				Text("Connected to: ")
				Text(text = bluetoothViewModel.connectedDevice.value)
			}

			Row(modifier = Modifier.fillMaxWidth()) {
				Text("Read Characteristic: ")
				Text(text = bluetoothViewModel.receivedData.value)
				Button(onClick = {
					bluetoothViewModel.writeCharacteristic("Test send")
				}) {
					Text("Write")
				}
			}

			// 🎙️ Start/Stop Recording Button
			Row(modifier = Modifier.fillMaxWidth()) {
				val isRecording = bluetoothViewModel.isRecording.value
				Button(onClick = {
					if (isRecording) {
						bluetoothViewModel.stopRecording()
						Toast.makeText(
							bluetoothViewModel.context,
							"Recording stopped",
							Toast.LENGTH_SHORT
						).show()
					} else {
						bluetoothViewModel.startRecording()
						Toast.makeText(
							bluetoothViewModel.context,
							"Recording started",
							Toast.LENGTH_SHORT
						).show()
					}
				}) {
					Text(if (isRecording) "Stop Recording" else "Start Recording")
				}
			}
		}
	}
}
