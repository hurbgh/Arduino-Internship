package com.example.learningmqqt

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.example.learningmqqt.ui.theme.LearningMQQTTheme

class MainActivity : ComponentActivity() {
    private lateinit var mqttClient: MqttAndroidClient
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Call your MQTT connect function here
        mqttConnect(
            applicationContext,
            "broker.hivemq.com", // Replace with your broker address
            "yourUsername",
            "yourPassword"
        )
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            LearningMQQTTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    Greeting(
                        name = "Android",
                        modifier = Modifier.padding(innerPadding)
                    )
                }
            }
        }
    }
}

@Composable
fun Greeting(name: String, modifier: Modifier = Modifier) {
    Text(
        text = "Hello $name!",
        modifier = modifier
    )
}

@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    LearningMQQTTheme {
        Greeting("Android")
    }
}