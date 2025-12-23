package com.example.smartfire

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.io.IOException
import java.io.InputStream
import java.util.*
import org.json.JSONObject
import android.util.Log
import com.hivemq.client.mqtt.MqttClient
import com.hivemq.client.mqtt.mqtt3.Mqtt3AsyncClient

class BluetoothViewModel : ViewModel() {

    private var mqttConnected = false
    private val _receivedText = MutableStateFlow("")

    // Latest parsed values for dashboard
    private val _latestValues = MutableStateFlow<Map<String, String>>(emptyMap())
    val latestValues: StateFlow<Map<String, String>> = _latestValues

    // History of sent JSON payloads
    private val _sentValues = MutableStateFlow<List<String>>(emptyList())
    val sentValues: StateFlow<List<String>> = _sentValues

    val receivedText: StateFlow<String> = _receivedText

    private var lastDevice: BluetoothDevice? = null
    private var socket: BluetoothSocket? = null
    private var inputStream: InputStream? = null
    private var readerJob: Job? = null

    private var mqttClient: Mqtt3AsyncClient? = null

    // ✅ Setup MQTT with HiveMQ client
    fun setupMqtt() {
        mqttClient = MqttClient.builder()
            .useMqttVersion3()
            .serverHost("test.mosquitto.org")
            .serverPort(1883)
            .identifier("AndroidClient_" + System.currentTimeMillis())
            .buildAsync()

        mqttClient?.connect()?.whenComplete { _, throwable ->
            if (throwable == null) {
                mqttConnected = true
                Log.d("MQTT", "Connected to test.mosquitto.org")
                publishJson("testDevice123", "{\"msg\":\"hello from Irfan\"}")
            } else {
                mqttConnected = false
                Log.e("MQTT", "Connection failed", throwable)
            }
        }
    }

    fun isMqttConnected(): Boolean = mqttConnected

    // ✅ Publish to interlab/node/bluetooth/<deviceId>/data
    fun publishJson(deviceId: String, json: String) {
        if (!mqttConnected) {
            Log.e("MQTT", "Publish skipped: Not connected")
            return
        }

        val topic = "interlab/node/bluetooth/$deviceId/data"
        mqttClient?.publishWith()
            ?.topic(topic)
            ?.payload(json.toByteArray())
            ?.send()
        Log.d("MQTT", "Published to $topic: $json")
    }

    // ✅ Expect exactly 7 values, all strings
    private fun parseToJson(raw: String): String {
        val values = raw.split(",").map { it.trim() }
        if (values.size < 9) return "{}"

        val map = mapOf(
            "timestamp" to System.currentTimeMillis().toString(),
            "pm1.0" to values[0],
            "pm2.5" to values[1],
            "pm10" to values[2],
            "co2" to values[3],
            "co" to values[4],
            "temperature" to values[5],
            "humidity" to values[6],
            "airpressure" to values[7],
            "altitude" to values[8]
        )

        return JSONObject(map).toString()
    }


    fun connectToDevice(device: BluetoothDevice) {
        lastDevice = device
        val uuid = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
        socket = device.createRfcommSocketToServiceRecord(uuid)
        try {
            BluetoothAdapter.getDefaultAdapter().cancelDiscovery()
            socket?.connect()
            inputStream = socket?.inputStream
            Log.d("Bluetooth", "Connected to ${device.name}")
            startReading()
        } catch (e: IOException) {
            _receivedText.value += "\nConnection failed: ${e.message}"
        }
    }

    fun startReading() {
        readerJob = CoroutineScope(Dispatchers.IO).launch {
            Log.d("Bluetooth", "Started reading from input stream")
            try {
                val buffer = ByteArray(1024)
                while (true) {
                    val bytes = inputStream?.read(buffer) ?: break
                    val raw = String(buffer, 0, bytes).trim()
                    if (raw.isNotEmpty()) {
                        _receivedText.value += "\n$raw"

                        if (mqttConnected) {
                            val json = parseToJson(raw)
                            val deviceId = lastDevice?.address?.replace(":", "") ?: "unknown"
                            publishJson(deviceId, json)

                            // ✅ Update dashboard with latest parsed values
                            val obj = JSONObject(json)
                            val map = obj.keys().asSequence().associateWith { obj.getString(it) }
                            _latestValues.value = map

                            // ✅ Update history (newest first)
                            _sentValues.value = listOf(json) + _sentValues.value
                        } else {
                            Log.w("MQTT", "Skipping publish, not connected yet")
                        }
                    }
                }
            } catch (e: IOException) {
                _receivedText.value += "\nConnection lost: ${e.message}"
            }
        }
    }

    fun reconnect() {
        readerJob?.cancel()
        inputStream?.close()
        socket?.close()
        _receivedText.value += "\nAttempting to reconnect..."
        lastDevice?.let { connectToDevice(it) }
    }

    override fun onCleared() {
        super.onCleared()
        readerJob?.cancel()
        inputStream?.close()
        socket?.close()
    }
}
