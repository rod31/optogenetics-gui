# Optogenetics Protocol GUI

## 🧠 Overview
A modular Python GUI that controls an Arduino-based LED system for optogenetics experiments. Designed to let researchers create and trigger real-time stimulation protocols based on intensity, color, frequency, and duration — all from an easy-to-use interface.

## ⚙️ Technologies Used
- Python (Tkinter for GUI, PySerial for Arduino communication)
- Arduino Nano
- Serial communication (USB)
- Basic electronics (LEDs, resistors, MOSFET)

## 🚀 Features
- Custom protocol editor (pulse width, frequency, color)
- Real-time GUI control with Arduino synchronization
- Logging for timestamp and temperature (coming soon)
- User-friendly interface designed for neurobiology labs

## 🖥️ How to Run
```bash
# Install dependencies
pip install pyserial

# Run the GUI
python gui.py
