package com.example.smartfire

import android.app.Application
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import android.content.ContentValues
import android.content.Context
import android.provider.MediaStore
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
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

class BluetoothViewModel(application: Application) : AndroidViewModel(application) {

    private val _mqttStatus = MutableStateFlow(false)
    val mqttStatus: StateFlow<Boolean> = _mqttStatus

    private val mqttUsername = "Irfan"
    private val mqttPassword = "XT30nR2d1qE9Hm"

    private var mqttClient: Mqtt3AsyncClient? = null
    private var mqttConnected = false
    private var mqttReconnectJob: Job? = null

    private var csvFileUri: android.net.Uri? = null
    private val sessionFileName = "smartfire_${System.currentTimeMillis()}.csv"

    private val appContext: Context = application.applicationContext

    private val _receivedText = MutableStateFlow("")
    private val _latestValues = MutableStateFlow<Map<String, String>>(emptyMap())
    private val _sentValues = MutableStateFlow<List<String>>(emptyList())

    val latestValues: StateFlow<Map<String, String>> = _latestValues
    val sentValues: StateFlow<List<String>> = _sentValues
    val receivedText: StateFlow<String> = _receivedText

    private var lastDevice: BluetoothDevice? = null
    private var socket: BluetoothSocket? = null
    private var inputStream: InputStream? = null
    private var readerJob: Job? = null


    // ---------------------------------------------------------
    // MQTT SETUP + AUTO RECONNECT
    // ---------------------------------------------------------
    fun setupMqtt() {
        mqttClient = MqttClient.builder()
            .useMqttVersion3()
            .serverHost("dbb1e064fb494148b791a3bbed394a13.s1.eu.hivemq.cloud")
            .serverPort(8883)
            .sslWithDefaultConfig()
            .identifier("AndroidClient_${System.currentTimeMillis()}")
            .addDisconnectedListener {
                mqttConnected = false
                Log.e("MQTT", "MQTT disconnected")
            }
            .buildAsync()

        startMqttReconnectLoop()
        connectMqtt()
    }

    private fun connectMqtt() {
        mqttClient?.connectWith()
            ?.simpleAuth()
            ?.username(mqttUsername)
            ?.password(mqttPassword.toByteArray())
            ?.applySimpleAuth()
            ?.send()
            ?.whenComplete { _, throwable ->
                if (throwable == null) {
                    mqttConnected = true
                    _mqttStatus.value = true
                    Log.d("MQTT", "Connected securely to HiveMQ Cloud")
                } else {
                    mqttConnected = false
                    _mqttStatus.value = false
                    Log.e("MQTT", "Connection failed", throwable)
                }
            }
    }

    private fun startMqttReconnectLoop() {
        mqttReconnectJob?.cancel()

        mqttReconnectJob = viewModelScope.launch(Dispatchers.IO) {
            while (isActive) {
                if (!mqttConnected) {
                    Log.d("MQTT", "Attempting reconnect...")
                    connectMqtt()
                }
                delay(3000)
            }
        }
    }


    // ---------------------------------------------------------
    // MQTT PUBLISH
    // ---------------------------------------------------------
    fun publishJson(deviceId: String, json: String) {
        if (!mqttConnected) return

        val topic = "interlab/node/bluetooth/$deviceId/data"

        mqttClient?.publishWith()
            ?.topic(topic)
            ?.payload(json.toByteArray())
            ?.send()

        Log.d("MQTT", "Published to $topic: $json")
    }


    // ---------------------------------------------------------
    // CSV WRITING
    // ---------------------------------------------------------
    private fun saveCsvToDownloads(csvLine: String) {
        val resolver = appContext.contentResolver

        if (csvFileUri == null) {
            val contentValues = ContentValues().apply {
                put(MediaStore.Downloads.DISPLAY_NAME, sessionFileName)
                put(MediaStore.Downloads.MIME_TYPE, "text/csv")
                put(MediaStore.Downloads.RELATIVE_PATH, "Download/")
            }
            csvFileUri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, contentValues)

            // Write header
            csvFileUri?.let { uri ->
                resolver.openOutputStream(uri, "wa")?.bufferedWriter().use { writer ->
                    writer?.write("packetId,esp32_send_ts,phone_received_ts,phone_forwarded_ts,payload_raw")
                    writer?.newLine()
                }
            }
        }

        csvFileUri?.let { uri ->
            resolver.openOutputStream(uri, "wa")?.bufferedWriter().use { writer ->
                writer?.write(csvLine)
                writer?.newLine()
            }
        }
    }


    // ---------------------------------------------------------
    // BLUETOOTH
    // ---------------------------------------------------------
    fun connectToDevice(device: BluetoothDevice) {
        viewModelScope.launch(Dispatchers.IO) {
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
    }

    fun startReading() {
        readerJob = viewModelScope.launch(Dispatchers.IO) {
            try {
                val buffer = ByteArray(1024)

                while (isActive) {
                    val bytes = inputStream?.read(buffer) ?: break
                    if (bytes <= 0) continue

                    val raw = String(buffer, 0, bytes).trim()
                    if (raw.isEmpty()) continue

                    val phoneReceivedTs = System.currentTimeMillis()

                    _receivedText.value += "\n$raw"

                    val (packetId, esp32SendTs) = parseEsp32Payload(raw)

                    val phoneForwardedTs = System.currentTimeMillis()

                    val json = JSONObject().apply {
                        put("packetId", packetId)
                        put("esp32_send_ts", esp32SendTs)
                        put("phone_received_ts", phoneReceivedTs)
                        put("phone_forwarded_ts", phoneForwardedTs)
                        put("payload_raw", raw)
                    }.toString()

                    val deviceId = lastDevice?.address?.replace(":", "") ?: "unknown"

                    if (mqttConnected) publishJson(deviceId, json)

                    val csvRow = jsonToCsvRow(
                        packetId,
                        esp32SendTs,
                        phoneReceivedTs,
                        phoneForwardedTs,
                        raw
                    )
                    saveCsvToDownloads(csvRow)

                    _latestValues.value = mapOf(
                        "packetId" to packetId,
                        "esp32_send_ts" to esp32SendTs,
                        "phone_received_ts" to phoneReceivedTs.toString(),
                        "phone_forwarded_ts" to phoneForwardedTs.toString(),
                        "payload_raw" to raw
                    )

                    _sentValues.value = listOf(json) + _sentValues.value
                }
            } catch (e: IOException) {
                _receivedText.value += "\nConnection lost: ${e.message}"
            }
        }
    }


    // ---------------------------------------------------------
    // HELPERS
    // ---------------------------------------------------------
    private fun parseEsp32Payload(raw: String): Pair<String, String> {
        val parts = raw.split(",")
        if (parts.size < 2) return Pair("unknown", "unknown")

        val packetId = parts[0].trim()
        val esp32SendTs = parts[1].trim()

        return Pair(packetId, esp32SendTs)
    }

    private fun jsonToCsvRow(
        packetId: String,
        esp32SendTs: String,
        phoneReceivedTs: Long,
        phoneForwardedTs: Long,
        raw: String
    ): String {
        return listOf(
            packetId,
            esp32SendTs,
            phoneReceivedTs.toString(),
            phoneForwardedTs.toString(),
            raw
        ).joinToString(",")
    }


    // ---------------------------------------------------------
    // CLEANUP
    // ---------------------------------------------------------
    fun reconnect() {
        readerJob?.cancel()
        try {
            inputStream?.close()
            socket?.close()
        } catch (_: IOException) {}
        _receivedText.value += "\nAttempting to reconnect..."
        lastDevice?.let { connectToDevice(it) }
    }

    override fun onCleared() {
        super.onCleared()
        readerJob?.cancel()
        try {
            inputStream?.close()
            socket?.close()
        } catch (_: IOException) {}
    }
}
