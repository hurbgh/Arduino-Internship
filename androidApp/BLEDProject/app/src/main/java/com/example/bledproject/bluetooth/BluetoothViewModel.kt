package com.example.bledproject.bluetooth

import android.app.Application
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.util.Log
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.lifecycle.AndroidViewModel
import com.example.bledproject.data.UserStore
import java.io.File
import java.io.FileWriter
import java.util.*

class BluetoothViewModel(
	app: Application,
	private val bluetoothAdapter: BluetoothAdapter,
	private val userStore: UserStore,
	private val messageHandler: (String) -> Unit
) : AndroidViewModel(app) {

	val context = app.applicationContext
	val scanning = mutableStateOf(false)
	val connected = mutableStateOf(false)
	val connectedDevice = mutableStateOf("")
	val devices = mutableStateListOf<BluetoothDevice>()
	val receivedData = mutableStateOf("Waiting for data...")
	val isRecording = mutableStateOf(false)

	private var bluetoothGatt: BluetoothGatt? = null
	private var targetCharacteristic: BluetoothGattCharacteristic? = null
	private var csvFileWriter: FileWriter? = null

	fun startScan() {
		devices.clear()
		scanning.value = true
		bluetoothAdapter.bluetoothLeScanner.startScan(scanCallback)
	}

	fun stopScan() {
		scanning.value = false
		bluetoothAdapter.bluetoothLeScanner.stopScan(scanCallback)
	}

	private val scanCallback = object : ScanCallback() {
		override fun onScanResult(callbackType: Int, result: ScanResult) {
			val device = result.device
			if (!devices.contains(device)) {
				devices.add(device)
			}
		}
	}

	fun connectToDevice(device: BluetoothDevice) {
		bluetoothGatt?.disconnect()
		bluetoothGatt?.close()
		bluetoothGatt = null

		connectedDevice.value = device.name ?: device.address
		bluetoothGatt = device.connectGatt(context, false, gattCallback)
	}

	fun disconnect() {
		bluetoothGatt?.disconnect()
		bluetoothGatt?.close()
		bluetoothGatt = null
		connected.value = false
		connectedDevice.value = ""
	}

	fun writeCharacteristic(data: String) {
		targetCharacteristic?.let {
			it.value = data.toByteArray(Charsets.UTF_8)
			bluetoothGatt?.writeCharacteristic(it)
		}
	}

	fun startRecording() {
		val fileName = "ble_data_${System.currentTimeMillis()}.csv"
		val file = File(context.getExternalFilesDir(null), fileName)
		try {
			csvFileWriter = FileWriter(file)
			csvFileWriter?.append("Timestamp,Sensor,Port,Value\n")
			isRecording.value = true
			Log.i("CSV", "Recording started: ${file.absolutePath}")
		} catch (e: Exception) {
			Log.e("CSV", "Failed to start recording", e)
		}
	}

	fun stopRecording() {
		try {
			csvFileWriter?.flush()
			csvFileWriter?.close()
			Log.i("CSV", "Recording stopped and file saved")
		} catch (e: Exception) {
			Log.e("CSV", "Failed to stop recording", e)
		} finally {
			csvFileWriter = null
			isRecording.value = false
		}
	}

	private val gattCallback = object : BluetoothGattCallback() {
		override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
			if (newState == BluetoothProfile.STATE_CONNECTED) {
				connected.value = true
				gatt.discoverServices()
			} else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
				connected.value = false
			}
		}

		override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
			val serviceUUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b") // Example
			val characteristicUUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8") // Example

			val service = gatt.getService(serviceUUID)
			val characteristic = service?.getCharacteristic(characteristicUUID)

			if (characteristic != null) {
				targetCharacteristic = characteristic
				gatt.setCharacteristicNotification(characteristic, true)
				val descriptor = characteristic.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
				descriptor?.let {
					it.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
					gatt.writeDescriptor(it)
				}
			}
		}

		override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
			val message = String(characteristic.value, Charsets.UTF_8)
			Log.i("Message handler", message)
			receivedData.value = message
			messageHandler(message)

			if (isRecording.value) {
				val timestamp = System.currentTimeMillis()
				try {
					csvFileWriter?.append("$timestamp,$message\n")
				} catch (e: Exception) {
					Log.e("CSV", "Failed to write data", e)
				}
			}
		}
	}
}
