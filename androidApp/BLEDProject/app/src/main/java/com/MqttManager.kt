package com

import android.content.Context
import android.util.Log
import org.eclipse.paho.android.service.MqttAndroidClient
import org.eclipse.paho.client.mqttv3.*
import java.util.UUID

class MqttManager(context: Context) {
    private val serverUri = "tcp://broker.hivemq.com:1883" // Public MQTT broker
    private val clientId = "BLEApp_" + UUID.randomUUID().toString()
    private val mqttClient = MqttAndroidClient(context, serverUri, clientId)

    fun connect(onConnected: () -> Unit) {
        val options = MqttConnectOptions().apply {
            isCleanSession = true
        }

        mqttClient.connect(options, null, object : IMqttActionListener {
            override fun onSuccess(asyncActionToken: IMqttToken?) {
                Log.d("MQTT", "Connected to broker")
                onConnected()
            }

            override fun onFailure(asyncActionToken: IMqttToken?, exception: Throwable?) {
                Log.e("MQTT", "Connection failed: ${exception?.message}")
            }
        })
    }

    fun publish(topic: String, message: String) {
        val mqttMessage = MqttMessage(message.toByteArray())
        mqttClient.publish(topic, mqttMessage)
    }
}
