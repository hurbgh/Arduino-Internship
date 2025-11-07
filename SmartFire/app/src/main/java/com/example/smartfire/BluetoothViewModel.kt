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
import java.nio.charset.StandardCharsets

class BluetoothViewModel : ViewModel() {

    private var mqttConnected = false

    private val _receivedText = MutableStateFlow("")
    val receivedText: StateFlow<String> = _receivedText
    private var lastDevice: BluetoothDevice? = null

    private var socket: BluetoothSocket? = null
    private var inputStream: InputStream? = null
    private var readerJob: Job? = null

    private lateinit var mqttClient: Mqtt3AsyncClient

    fun setupMqtt() {
        Log.d("MQTT", "Connecting to broker...")
        mqttClient = MqttClient.builder()
            .useMqttVersion3()
            .serverHost("test.mosquitto.org")
            .serverPort(1883)
            .buildAsync()
        Log.d("MQTT", "Connecting to broker...")

        mqttClient.connect().whenComplete { ack, throwable ->
            if (throwable != null) {
                mqttConnected = false
                Log.e("MQTT", "Connection failed: ${throwable.message}")
            } else {
                mqttConnected = true
                Log.d("MQTT", "MQTT connected successfully: $ack")
            }
        }

    }

    fun publishJson(topic: String, json: String) {
        Log.d("MQTT", "Publish attempt: mqttConnected=$mqttConnected, topic=$topic, json=$json")
        if (!mqttConnected) {
            Log.e("MQTT", "Publish skipped: Not connected")
            return
        }

        mqttClient.publishWith()
            .topic(topic)
            .payload(json.toByteArray(StandardCharsets.UTF_8))
            .send()
            .whenComplete { _, throwable ->
                if (throwable != null) {
                    Log.e("MQTT", "Publish failed: ${throwable.message}")
                } else {
                    Log.d("MQTT", "Published to $topic: $json")
                }
            }
    }

    private fun parseToJson(raw: String): String {
        val values = raw.split(",").map { it.trim() }
        if (values.size < 7) return "{}"

        val map = mapOf(
            "pm" to values[0],
            "co2" to values[1],
            "co" to values[2],
            "temp" to values[3],
            "humidity" to values[4],
            "airPressure" to values[5],
            "altitude" to values[6]
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
                    val raw = String(buffer, 0, bytes)
                    _receivedText.value += "\n$raw"

                    val json = parseToJson(raw)
                    publishJson("smartfire/test", json)
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
