import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import tkinter.font as tkFont
import serial
import time
from serial.tools import list_ports
import json
import os
import datetime
import csv

if not os.path.exists("protocols"):
    os.makedirs("protocols")
if not os.path.exists("experiments"):
    os.makedirs("experiments")
    
arduino = None
protocol_data = []
well_assignments = {}  # NEW: Store protocolIndex -> list of (row, col)
protocol_data_loaded = []  # Stores protocols loaded from file
temperature_timer = None
current_experiment_file = None
experiment_running = False

# ============================= Serial Helpers =============================
def get_serial_ports():
    return [p.device for p in list_ports.comports()]

def connect_serial():
    global arduino
    selected_port = port_combo.get()
    if not selected_port:
        messagebox.showwarning("Warning", "Please select a COM port.")
        return
    try:
        arduino = serial.Serial(selected_port, 115200, timeout=1)
        time.sleep(2)
        arduino.flush()
        log(f"Connected to {selected_port}")
    except serial.SerialException as e:
        messagebox.showerror("Error", f"Could not open port {selected_port}: {e}")

# ============================= Communication =============================
def send_to_arduino(command):
    if arduino and arduino.is_open:
        arduino.write((command + "\n").encode())
        time.sleep(0.1)
        read_from_arduino()
    else:
        log("ERROR: Serial port not open.")

def read_from_arduino():
    if arduino.in_waiting > 0:
        while arduino.in_waiting > 0:
            line = arduino.readline().decode().strip()
            log(line)

# ============================= GUI Actions =============================
def create_protocol():
    name = proto_name.get()
    color = proto_color.get()[0].upper()
    intensity = proto_intensity.get()
    active = proto_active.get()
    silent = proto_silent.get()
    pulse_on = proto_on.get()
    pulse_off = proto_off.get()
    total = proto_total.get()       # Duration (s)

    if not all([name, intensity, active, silent, pulse_on, pulse_off, total, color]):
        messagebox.showerror("Input Error", "Please fill in all protocol fields.")
        return
    
    # Prevent duplicate protocol names (including previously loaded)
    all_names = [p["name"] for p in protocol_data] + [p["name"] for p in protocol_data_loaded]
    if name in all_names:
        messagebox.showerror("Duplicate Protocol", f"A protocol named '{name}' already exists.")
        return
    
    cmd = f"<{name},{intensity},PROTOCOL,{active},{silent},{pulse_on},{pulse_off},{total},{color}>"
    send_to_arduino(cmd)
    index = len(protocol_data)  + len(protocol_data)
    protocol_data.append({
        "name": name,
        "color": color,
        "intensity": intensity,
        "active": active,
        "silent": silent,
        "on": pulse_on,
        "off": pulse_off,
        "total": total
    })
    well_assignments[index] = []  # init entry for well assignment

def assign_well():
    row = well_row.get().upper()
    col = well_col.get()
    index = assign_index.get()
    if row and col and index:
        cmd = f"<{row},{col},ASSIGN,{index}>"
        send_to_arduino(cmd)
        #
        i = int(index)
        if i not in well_assignments:
            well_assignments[i] = []
        well_assignments[i].append((row, col))

def assign_range():
    if arduino is None or not arduino.is_open:
        log("ERROR: Serial port not open.")
        return

    sr = start_row_entry.get().strip().upper()
    sc = start_col_entry.get().strip()
    er = end_row_entry.get().strip().upper()
    ec = end_col_entry.get().strip()
    idx = range_index_entry.get().strip()

    if sr and sc and er and ec and idx:
        command = f"<{sr},{sc},RANGE,{er},{ec},{idx}>"
        send_to_arduino(command)
        #
        i = int(idx)
        if i not in well_assignments:
            well_assignments[i] = []
        well_assignments[i].append((f"{sr}{sc}-{er}{ec}"))
    else:
        log("ERROR: Incomplete range fields.")

PROTOCOL_FILE = "protocols.json"

def save_protocols():
    # Load existing data if present
    existing_data = {"protocols": [], "assignments": {}}
    if os.path.exists(PROTOCOL_FILE):
        with open(PROTOCOL_FILE, 'r') as f:
            try:
                existing_data = json.load(f)
            except json.JSONDecodeError:
                log("Warning: Could not read existing protocol file. Overwriting.")

    # Extract existing names
    existing_names = {p["name"] for p in existing_data["protocols"]}

    # Filter out duplicates from current session
    new_protocols = []
    for p in protocol_data:
        if p["name"] in existing_names:
            log(f"⚠️ Skipping duplicate protocol name: {p['name']}")
        else:
            new_protocols.append(p)
            existing_names.add(p["name"])

    if not new_protocols:
        log("ℹ️ No new protocols to save (all duplicates).")
        return

    # Extend protocol list
    existing_data["protocols"].extend(new_protocols)

    # Merge assignments correctly (append to existing lists)
    for key, new_list in well_assignments.items():
        if str(key) in existing_data["assignments"]:
            existing_data["assignments"][str(key)].extend(new_list)
        else:
            existing_data["assignments"][str(key)] = new_list

    # Save to file
    with open(PROTOCOL_FILE, 'w') as f:
        json.dump(existing_data, f, indent=2)

    log(f"✅ Saved {len(new_protocols)} new protocols (total now: {len(existing_data['protocols'])}) to {PROTOCOL_FILE}")

    # Optionally clear current session data
    protocol_data.clear()
    well_assignments.clear()


def load_protocols():
    global protocol_data, well_assignments, protocol_data_loaded

    if not os.path.exists(PROTOCOL_FILE):
        log("No saved protocol file found.")
        return

    # Clear previous in-memory data
    protocol_data.clear()
    protocol_data_loaded = []  # New: store loaded separately
    well_assignments.clear()

    with open(PROTOCOL_FILE, 'r') as f:
        try:
            loaded = json.load(f)
        except json.JSONDecodeError:
            log("Error reading protocol file.")
            return

    protocols = loaded.get("protocols", [])
    assignments = loaded.get("assignments", {})

    # Send to Arduino and populate lists
    for i, p in enumerate(protocols):
        cmd = f"<{p['name']},{p['intensity']},PROTOCOL,{p['active']},{p['silent']},{p['on']},{p['off']},{p['total']},{p['color']}>"
        send_to_arduino(cmd)
        time.sleep(0.1)

    protocol_data_loaded = protocols.copy()
    protocol_data.extend(protocols)
    well_assignments.update({int(k): v for k, v in assignments.items()})

    log(f"Loaded {len(protocol_data)} protocols with assignments from {PROTOCOL_FILE}")

def start_experiment():
    global current_experiment_file, temperature_timer, experiment_running

    name = experiment_name.get()
    if name:
        send_to_arduino(f"<{name},0,START>")
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log(f"Experiment {name} started at {timestamp}\n")

        # Setup CSV log file
        current_experiment_file = os.path.join("experiments", f"exp_{name}.csv")
        with open(current_experiment_file, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(["Timestamp", "Temperature (C)"])  # Header
            #writer.writerow([timestamp, "Started"])  # Initial row

        experiment_running = True
        log_temperature()  # Log immediately
        temperature_timer = root.after(180000, log_temperature)  # 30 minutes in ms = 1800000 

def log_temperature():
    global temperature_timer

    if not experiment_running or current_experiment_file is None:
        return  # Don't proceed if experiment has stopped

    # Request temperature from Arduino (you must implement this on Arduino)
    send_to_arduino("<0,0,TEMP>")  # Your Arduino should respond with: TEMP:<value>

    # Wait a bit to read it
    time.sleep(0.5)

    # Parse last line in serial_text for TEMP
    lines = serial_text.get("1.0", tk.END).splitlines()
    temp_line = next((line for line in reversed(lines) if "TEMP:" in line), None)
    if temp_line:
        temp_value = temp_line.split("TEMP:")[-1].strip()
    else:
        temp_value = "N/A"

    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    log(f"Logged temp at {timestamp}: {temp_value}")

    with open(current_experiment_file, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([timestamp, temp_value])

    # Reschedule
    temperature_timer = root.after(180000, log_temperature)  # Schedule next check


def stop_experiment():
    global temperature_timer, experiment_running
    send_to_arduino("<0,0,STOP>")
    log("Experiment stopped.")
    experiment_running = False
    if temperature_timer:
        root.after_cancel(temperature_timer)
        temperature_timer = None


def list_protocols():
    serial_text.insert(tk.END, "\n--- Defined Protocols ---\n")
    for i, p in enumerate(protocol_data):
        serial_text.insert(tk.END,
            f"Index: {i}, Name: {p['name']}, Color: {p['color']}, Intensity: {p['intensity']}, "
            f"Active: {p['active']}s, Silent: {p['silent']}s, Pulse: {p['on']}s ON/{p['off']}s OFF, "
            f"Total: {p['total']}s\n"
        )
        if i in well_assignments:
            assigned = well_assignments[i]
            if assigned:
                serial_text.insert(tk.END, f"  → Previously Assigned to: {', '.join(str(x) for x in assigned)} (Choose <Reassign All>)\n")
            else:
                serial_text.insert(tk.END, "  → No well assignments\n")
    serial_text.see(tk.END)

def reassign_all_protocols():
    for idx, assignments in well_assignments.items():
        for entry in assignments:
            if isinstance(entry, tuple) and len(entry) == 2:
                row, col = entry
                cmd = f"<{row},{col},ASSIGN,{idx}>"
                send_to_arduino(cmd)
                time.sleep(0.05)
            elif isinstance(entry, str) and "-" in entry:
                srsc, erc = entry.split("-")
                cmd = f"<{srsc[0]},{srsc[1:]},RANGE,{erc[0]},{erc[1:]},{idx}>"
                send_to_arduino(cmd)
                time.sleep(0.05)
    log("Reassigned all protocols to original wells.")

def log(text):
    serial_text.insert(tk.END, text + "\n")
    serial_text.see(tk.END)

def shutdown():
    if arduino and arduino.is_open:
        send_to_arduino("<0,0,STOP>")
        arduino.close()
    root.destroy()

# ============================= GUI Setup =============================
root = tk.Tk()
root.title("Optogenetics Protocol Control System")
root.geometry("1000x700")
root.configure(bg="#460d72")  #  background
default_fg = "gray"
default_bg = "#cfd6d3"
highlight = "#333333"  # slightly lighter gray
default_font = tkFont.nametofont("TkDefaultFont")
default_font.configure(size=14)  # Or 16, 18, etc. for larger touch targets
root.option_add("*Font", default_font)


main_canvas = tk.Canvas(root)
scrollbar = tk.Scrollbar(root, orient="vertical", command=main_canvas.yview)
scrollable_frame = tk.Frame(main_canvas, bg=default_bg)

scrollable_frame.bind("<Configure>", lambda e: main_canvas.configure(scrollregion=main_canvas.bbox("all")))
main_canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
main_canvas.configure(yscrollcommand=scrollbar.set)

main_canvas.pack(side="left", fill="both", expand=True)
scrollbar.pack(side="right", fill="y")

######      STYLES BY ROD     ######
style = ttk.Style()
style.theme_use("default")

style.configure("TLabel", background="#460d72", foreground="white", font=("Helvetica", 14))
style.configure("TButton", background="#460d72", foreground="white", font=("Helvetica", 14))
style.configure("TCombobox", font=("Helvetica", 14))

# Replace 'root' with 'scrollable_frame' below this point
root = scrollable_frame

# COM Port
tk.Label(root, text="COM Port:").pack()
port_combo = ttk.Combobox(root, values=get_serial_ports(), width=20)
port_combo.pack()
tk.Button(root, text="Connect", command=connect_serial).pack(pady=5)

# --- Protocol Creation ---
tk.Label(root, text="--- Create Protocol ---").pack(pady=5)
proto_frame = tk.Frame(root)
proto_frame.pack()

entries = [
    ("Name", "proto_name"),
    ("Color", "proto_color"),
    ("Intensity (0-255)", "proto_intensity"),
    ("Active Time (s)", "proto_active"),
    ("Silent Time (s)", "proto_silent"),
    ("Pulse ON (s)", "proto_on"),
    ("Pulse OFF (s)", "proto_off"),
    ("Total Duration (s)", "proto_total")
]
for i, (label, var) in enumerate(entries):
    tk.Label(proto_frame, text=label).grid(row=i, column=0, sticky='e')
    
    if var == "proto_color":
        globals()[var] = ttk.Combobox(proto_frame, values=["Red", "Green", "Blue"], width=18)
    else:
        globals()[var] = tk.Entry(proto_frame)
    
    globals()[var].grid(row=i, column=1)


tk.Button(root, text="Create Protocol", command=create_protocol).pack(pady=5)

# --- Assign Protocol to Single Well ---
tk.Label(root, text="--- Assign to Well ---").pack(pady=5)
assign_frame = tk.Frame(root)
assign_frame.pack()

well_row = tk.Entry(assign_frame, width=5)
well_col = tk.Entry(assign_frame, width=5)
assign_index = tk.Entry(assign_frame, width=5)

tk.Label(assign_frame, text="Row:").grid(row=0, column=0)
well_row.grid(row=0, column=1)
tk.Label(assign_frame, text="Column:").grid(row=0, column=2)
well_col.grid(row=0, column=3)
tk.Label(assign_frame, text="Protocol Index:").grid(row=0, column=4)
assign_index.grid(row=0, column=5)
tk.Button(assign_frame, text="Assign", command=assign_well).grid(row=0, column=6)

# --- Assign Protocol to Range ---
tk.Label(root, text="--- Assign to Range ---").pack(pady=5)
range_frame = tk.Frame(root)
range_frame.pack()

# Labels and entries
tk.Label(range_frame, text="Start Row").grid(row=0, column=0)
start_row_entry = tk.Entry(range_frame, width=5)
start_row_entry.grid(row=0, column=1)

tk.Label(range_frame, text="Start Col").grid(row=0, column=2)
start_col_entry = tk.Entry(range_frame, width=5)
start_col_entry.grid(row=0, column=3)

tk.Label(range_frame, text="End Row").grid(row=0, column=4)
end_row_entry = tk.Entry(range_frame, width=5)
end_row_entry.grid(row=0, column=5)

tk.Label(range_frame, text="End Col").grid(row=0, column=6)
end_col_entry = tk.Entry(range_frame, width=5)
end_col_entry.grid(row=0, column=7)

tk.Label(range_frame, text="Index").grid(row=0, column=8)
range_index_entry = tk.Entry(range_frame, width=5)
range_index_entry.grid(row=0, column=9)

# Button
tk.Button(range_frame, text="Assign Range", command=assign_range).grid(row=0, column=10, padx=10)

# --- Save Protocol (.json) ---
proto_button_frame = tk.Frame(root)
proto_button_frame.pack(pady=5)
tk.Button(proto_button_frame, text="Save Protocols", command=save_protocols).pack(side=tk.LEFT, padx=5)
tk.Button(proto_button_frame, text="Load Protocols", command=load_protocols).pack(side=tk.LEFT, padx=5)
tk.Button(proto_button_frame, text="Reassign All", command=reassign_all_protocols).pack(side=tk.LEFT, padx=5)

#here

# --- Experiment Control ---
tk.Label(root, text="--- Experiment Control ---").pack(pady=5)
exp_frame = tk.Frame(root)
exp_frame.pack()

experiment_name = tk.Entry(exp_frame)
experiment_name.grid(row=0, column=0)
tk.Button(exp_frame, text="Start",font='Helvetica', command=start_experiment, bg="green", fg="white").grid(row=0, column=1)
tk.Button(exp_frame, text="Stop",font='Helvetica', command=stop_experiment,  bg="red", fg="white").grid(row=0, column=2)
tk.Button(exp_frame, text="List Protocols", command=list_protocols).grid(row=0, column=3)

# Final shutdown
tk.Button(root, text="Shutdown", bg="black", fg="white", command=shutdown).pack(pady=5)

# --- Output Log ---
tk.Label(root, text="--- Serial Monitor ---").pack(pady=2)
serial_text = scrolledtext.ScrolledText(root, height=15, width=100, bg="#1e1e1e", fg="white", insertbackground="white")
serial_text.pack()

root.mainloop()
