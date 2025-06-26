# Optogenetics Protocol Control System

This project is a Python-based GUI designed for touchscreen control of optogenetics experiments using a Raspberry Pi and Arduino. It enables researchers to create and manage LED stimulation protocols and log temperature and timestamped experiment data.

## Features

- Full GUI interface using Tkinter with touchscreen support
- Serial communication with Arduino Mega for LED and temperature control
- Protocol creation, storage, loading, and assignment to well plates
- Periodic temperature logging and experiment timestamp tracking
- CSV export of metadata
- Designed for Raspberry Pi with HDMI + USB touchscreen monitor

## Tech Stack

- Python (Tkinter, PySerial)
- Arduino (C++/Serial)
- Raspberry Pi 4B
- Touchscreen monitor
- CSV, JSON for storage

## Screenshots

![GUI](assets/screenshot.png)

## Getting Started

1. Clone the repository  
   `git clone https://github.com/YOUR_USERNAME/optogenetics-gui`

2. Install dependencies  
   `pip install pyserial`

3. Connect Arduino and touchscreen monitor

4. Run the GUI  
   `python gui.py`

## License

MIT
