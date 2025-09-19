# Python Display Simulator

This is a simulator for the Glance e-paper display, built with Python and PyQt.

## Setup

1.  **Create a virtual environment (recommended):**
    ```bash
    python3 -m venv venv
    source venv/bin/activate
    ```

2.  **Install dependencies:**
    ```bash
    pip install -r requirements.txt
    ```

## Usage

Run the main application:
```bash
python3 main.py
```

The application will launch a GUI window. You can select a COM port from the dropdown menu and click "Connect" to start receiving data.

*   Log messages from the device will appear in the text area.
*   Image data sent from the device will be rendered in the image area.
