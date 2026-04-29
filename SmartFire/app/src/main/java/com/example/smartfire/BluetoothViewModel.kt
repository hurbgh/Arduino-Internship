package com.example.smartfire

import android.app.Application
import android.content.ContentValues
import android.content.Context
import android.provider.MediaStore
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.hivemq.client.mqtt.mqtt3.Mqtt3AsyncClient
import com.hivemq.client.mqtt.MqttClient
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.io.IOException
import java.io.InputStream
import java.util.UUID
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket

data class SensorPacket(
    val packetId: String,
    val esp32SendTs: String,
    val pm1: String,
    val pm25: String,
    val pm10: String,
    val co2: String,
    val co: String,
    val temp: String,
    val humidity: String,
    val pressure: String,
    val altitude: String,
    val crc: String
)

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

    private val _alarmMessage = MutableStateFlow<String?>(null)
    val alarmMessage: StateFlow<String?> = _alarmMessage

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
                _mqttStatus.value = false
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

            csvFileUri?.let { uri ->
                resolver.openOutputStream(uri, "wa")?.bufferedWriter().use { writer ->
                    writer?.write(
                        "packetId,esp32_send_ts,phone_received_ts,phone_forwarded_ts," +
                                "pm1,pm25,pm10,co2,co,temp,humidity,pressure,altitude,crc,payload_raw"
                    )
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

    private fun parsePayload(raw: String): SensorPacket? {
        val p = raw.split(",")
        if (p.size < 12) return null

        return SensorPacket(
            packetId = p[0].trim(),
            esp32SendTs = p[1].trim(),
            pm1 = p[2].trim(),
            pm25 = p[3].trim(),
            pm10 = p[4].trim(),
            co2 = p[5].trim(),
            co = p[6].trim(),
            temp = p[7].trim(),
            humidity = p[8].trim(),
            pressure = p[9].trim(),
            altitude = p[10].trim(),
            crc = p[11].trim()
        )
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

                    val parsed = parsePayload(raw) ?: continue

                    val phoneForwardedTs = System.currentTimeMillis()

                    val json = JSONObject().apply {
                        put("packetId", parsed.packetId)
                        put("esp32_send_ts", parsed.esp32SendTs)
                        put("phone_received_ts", phoneReceivedTs)
                        put("phone_forwarded_ts", phoneForwardedTs)
                        put("pm1", parsed.pm1)
                        put("pm25", parsed.pm25)
                        put("pm10", parsed.pm10)
                        put("co2", parsed.co2)
                        put("co", parsed.co)
                        put("temp", parsed.temp)
                        put("humidity", parsed.humidity)
                        put("pressure", parsed.pressure)
                        put("altitude", parsed.altitude)
                        put("crc", parsed.crc)
                        put("payload_raw", raw)
                    }.toString()

                    val deviceId = lastDevice?.address?.replace(":", "") ?: "unknown"

                    if (mqttConnected) publishJson(deviceId, json)

                    val csvRow = listOf(
                        parsed.packetId,
                        parsed.esp32SendTs,
                        phoneReceivedTs.toString(),
                        phoneForwardedTs.toString(),
                        parsed.pm1,
                        parsed.pm25,
                        parsed.pm10,
                        parsed.co2,
                        parsed.co,
                        parsed.temp,
                        parsed.humidity,
                        parsed.pressure,
                        parsed.altitude,
                        parsed.crc,
                        raw
                    ).joinToString(",")

                    saveCsvToDownloads(csvRow)

                    _latestValues.value = mapOf(
                        "packetId" to parsed.packetId,
                        "esp32_send_ts" to parsed.esp32SendTs,
                        "phone_received_ts" to phoneReceivedTs.toString(),
                        "phone_forwarded_ts" to phoneForwardedTs.toString(),
                        "pm1" to parsed.pm1,
                        "pm25" to parsed.pm25,
                        "pm10" to parsed.pm10,
                        "co2" to parsed.co2,
                        "co" to parsed.co,
                        "temp" to parsed.temp,
                        "humidity" to parsed.humidity,
                        "pressure" to parsed.pressure,
                        "altitude" to parsed.altitude,
                        "crc" to parsed.crc,
                        "payload_raw" to raw
                    )

                    _sentValues.value = listOf(json) + _sentValues.value

                    // Fire alarm thresholds
                    val co2Threshold = 1
                    val coThreshold = 1
                    val pm25Threshold = 1

                    val co2Val = parsed.co2.toIntOrNull() ?: 0
                    val coVal = parsed.co.toIntOrNull() ?: 0
                    val pm25Val = parsed.pm25.toIntOrNull() ?: 0

                    if (co2Val > co2Threshold || coVal > coThreshold || pm25Val > pm25Threshold) {
                        if (_alarmMessage.value == null) {
                            _alarmMessage.value =
                                "Dangerous air quality detected!\nCO₂: $co2Val ppm, CO: $coVal ppm, PM2.5: $pm25Val µg/m³"
                        }
                    }
                }
            } catch (e: IOException) {
                _receivedText.value += "\nConnection lost: ${e.message}"
            }
        }
    }

    fun acknowledgeAlarm() {
        _alarmMessage.value = null
    }

    // ---------------------------------------------------------
    // CLEANUP
    // ---------------------------------------------------------
    fun reconnect() {
        readerJob?.cancel()
        try {
            inputStream?.close()
            socket?.close()
        } catch (_: IOException) {
        }
        _receivedText.value += "\nAttempting to reconnect..."
        lastDevice?.let { connectToDevice(it) }
    }

    override fun onCleared() {
        super.onCleared()
        readerJob?.cancel()
        try {
            inputStream?.close()
            socket?.close()
        } catch (_: IOException) {
        }
    }
}
