# Optogenetics Protocol GUI

## 🧠 Overview
A modular Python GUI that controls an Arduino-based LED system for optogenetics experiments. Designed to let researchers create and trigger real-time stimulation protocols based on intensity, color, frequency, and duration — all from an easy-to-use interface.

## ⚙️ Technologies Used
- Python (Tkinter for GUI, PySerial for Arduino communication)
- Arduino MEGA
- Raspberry Pi (Model 4b)
- Serial communication (USB)
- Basic electronics (96-well LED plate, resistors, thermistor, computer fan)

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
python scroll_gui.py
```
## 📁 Repo Structure
- `scroll_gui.py` — main Python GUI
- `arduino_firmware.ino` — Arduino sketch for controlling LEDs
- `protocols.json` — sample JSON configs

## 🧪 Project Background
Built as part of my senior design project at Drexel University, this system supports optogenetic research workflows by improving experiment repeatability and accessibility.

## 📸 Screenshots
[Gui](https://github.com/rod31/optogenetics-gui/blob/main/screenshot1.png)

## 🎥 Demo

[Video](https://github.com/rod31/optogenetics-gui/blob/main/demo.mp4)

## 📫 Contact
Have feedback or want to collaborate?  
📧 rodrigo01.aragao@gmail.com  
🔗 [LinkedIn](https://www.linkedin.com/in/rb-aragao)

