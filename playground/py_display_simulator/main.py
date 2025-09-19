import sys
import serial
import serial.tools.list_ports
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout,
    QTextEdit, QLabel, QComboBox, QPushButton, QGroupBox
)
from PyQt5.QtGui import QPixmap, QImage, QPainter, QColor
from PyQt5.QtCore import Qt, QThread, pyqtSignal

# Constants
DISPLAY_WIDTH = 800
DISPLAY_HEIGHT = 480

class SerialReaderThread(QThread):
    """
    A QThread to read from the serial port in the background.
    """
    log_received = pyqtSignal(str)
    image_received = pyqtSignal(bytes)

    def __init__(self, port, baudrate=115200):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.serial_port = None
        self.running = False

    def run(self):
        try:
            self.serial_port = serial.Serial(self.port, self.baudrate, timeout=1)
            self.running = True
            self.log_received.emit(f"Connected to {self.port} at {self.baudrate} bps.\n")

            while self.running:
                # This is a placeholder for the actual protocol parsing
                # A real implementation would have a more robust protocol
                line = self.serial_port.readline()
                if line:
                    try:
                        # Attempt to decode as UTF-8 log message
                        log_msg = line.decode('utf-8').strip()
                        if log_msg.startswith("IMG_START"):
                            # Placeholder for image data protocol
                            # Expect image data size after header
                            image_size = DISPLAY_WIDTH * DISPLAY_HEIGHT // 8
                            img_data = self.serial_port.read(image_size)
                            if len(img_data) == image_size:
                                self.image_received.emit(img_data)
                            else:
                                self.log_received.emit(f"Error: Incomplete image data received.\n")
                        else:
                            self.log_received.emit(log_msg + '\n')
                    except UnicodeDecodeError:
                        # Data is not a valid UTF-8 string, could be part of an image
                        self.log_received.emit(f"Received non-UTF8 data: {line}\n")

        except serial.SerialException as e:
            self.log_received.emit(f"Error: {e}\n")
        finally:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
            self.log_received.emit("Disconnected.\n")

    def stop(self):
        self.running = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.wait()

class SimulatorWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Glance E-Paper Simulator")
        self.setGeometry(100, 100, 900, 600)

        self.serial_thread = None

        # Main layout
        main_layout = QVBoxLayout()
        self.setLayout(main_layout)

        # --- Controls ---
        controls_group = QGroupBox("Controls")
        controls_layout = QHBoxLayout()
        self.port_selector = QComboBox()
        self.refresh_ports()
        self.connect_button = QPushButton("Connect")
        self.disconnect_button = QPushButton("Disconnect")
        self.disconnect_button.setEnabled(False)
        controls_layout.addWidget(QLabel("COM Port:"))
        controls_layout.addWidget(self.port_selector)
        controls_layout.addWidget(self.connect_button)
        controls_layout.addWidget(self.disconnect_button)
        controls_group.setLayout(controls_layout)
        main_layout.addWidget(controls_group)

        # --- Display and Log ---
        display_log_layout = QHBoxLayout()
        main_layout.addLayout(display_log_layout)

        # --- E-Paper Display ---
        display_group = QGroupBox("E-Paper Display (800x480)")
        display_layout = QVBoxLayout()
        self.image_label = QLabel("Waiting for image...")
        self.image_label.setFixedSize(DISPLAY_WIDTH, DISPLAY_HEIGHT)
        self.image_label.setStyleSheet("background-color: white; border: 1px solid black;")
        self.image_label.setAlignment(Qt.AlignCenter)
        display_layout.addWidget(self.image_label)
        display_group.setLayout(display_layout)
        display_log_layout.addWidget(display_group)

        # --- Log Textbox ---
        log_group = QGroupBox("Log Output")
        log_layout = QVBoxLayout()
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        log_layout.addWidget(self.log_text)
        log_group.setLayout(log_layout)
        display_log_layout.addWidget(log_group)

        # --- Connections ---
        self.connect_button.clicked.connect(self.connect_serial)
        self.disconnect_button.clicked.connect(self.disconnect_serial)

    def refresh_ports(self):
        self.port_selector.clear()
        ports = serial.tools.list_ports.comports()
        for port in sorted(ports):
            self.port_selector.addItem(port.device)

    def connect_serial(self):
        port = self.port_selector.currentText()
        if not port:
            self.log_text.append("No COM port selected.")
            return

        self.serial_thread = SerialReaderThread(port)
        self.serial_thread.log_received.connect(self.log_text.append)
        self.serial_thread.image_received.connect(self.update_image)
        self.serial_thread.start()

        self.connect_button.setEnabled(False)
        self.disconnect_button.setEnabled(True)
        self.port_selector.setEnabled(False)

    def disconnect_serial(self):
        if self.serial_thread:
            self.serial_thread.stop()
        self.connect_button.setEnabled(True)
        self.disconnect_button.setEnabled(False)
        self.port_selector.setEnabled(True)

    def update_image(self, img_data):
        self.log_text.append("Received image data, updating display...")
        # Create a QImage from the 1-bit monochrome data
        image = QImage(DISPLAY_WIDTH, DISPLAY_HEIGHT, QImage.Format_Mono)
        image.fill(Qt.color1) # Fill with white

        # The data is 1-bit per pixel, but QImage.Format_Mono expects bytes.
        # We need to carefully set the bits.
        # This is a simplified approach; a more direct memory copy might be faster.
        painter = QPainter(image)
        painter.setPen(Qt.black)

        for y in range(DISPLAY_HEIGHT):
            for x in range(DISPLAY_WIDTH):
                byte_index = (y * DISPLAY_WIDTH + x) // 8
                bit_index = 7 - (x % 8)
                if byte_index < len(img_data):
                    if not (img_data[byte_index] & (1 << bit_index)):
                        painter.drawPoint(x, y)
        painter.end()

        pixmap = QPixmap.fromImage(image)
        self.image_label.setPixmap(pixmap)

    def closeEvent(self, event):
        self.disconnect_serial()
        event.accept()


if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = SimulatorWindow()
    window.show()
    sys.exit(app.exec_())
