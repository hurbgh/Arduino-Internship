import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.io.IOException
import java.io.InputStream
import java.util.*

class BluetoothViewModel : ViewModel() {
    private val _receivedText = MutableStateFlow("")
    val receivedText: StateFlow<String> = _receivedText
    private var lastDevice: BluetoothDevice? = null

    private var socket: BluetoothSocket? = null
    private var inputStream: InputStream? = null
    private var readerJob: Job? = null

    fun connectToDevice(device: BluetoothDevice) {
        lastDevice=device
        val uuid = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB") // Standard SPP UUID
        socket = device.createRfcommSocketToServiceRecord(uuid)
        try {
            BluetoothAdapter.getDefaultAdapter().cancelDiscovery()
            socket?.connect()
            inputStream = socket?.inputStream
            startReading()
        } catch (e: IOException) {
            _receivedText.value += "\nConnection failed: ${e.message}"
        }
    }

    private fun startReading() {
        readerJob = viewModelScope.launch(Dispatchers.IO) {
            val buffer = ByteArray(1024)
            while (true) {
                try {
                    val bytes = inputStream?.read(buffer) ?: break
                    val text = String(buffer, 0, bytes)
                    _receivedText.value += text
                } catch (e: IOException) {
                    _receivedText.value += "\nDisconnected: ${e.message}"
                    break
                }
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
