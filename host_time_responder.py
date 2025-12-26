# host_time_responder.py
# Requires: pip install pyserial
# Listens for GETTIME on any serial USB port and replies with HOSTTIME YYYY-MM-DD HH:MM:SS

import serial
import serial.tools.list_ports
import time
from datetime import datetime

BAUD = 115200
SCAN_INTERVAL = 2.0


def handle_port(port):
    try:
        s = serial.Serial(port.device, BAUD, timeout=1)
        print(f"Opened {port.device}")
    except Exception as e:
        return
    try:
        synced = False
        while True:
            try:
                line = s.readline().decode(errors='ignore').strip()
            except Exception:
                break
            if not line:
                time.sleep(0.01)
                continue
            print(f"[{port.device}] RX: {line}")
            # Reply when device explicitly asks for time, or when it prints RTC output
            if not synced and (line == "GETTIME" or line.startswith("RTC:")):
                now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                reply = f"HOSTTIME {now}\n"
                try:
                    s.write(reply.encode())
                    print(f"[{port.device}] TX: {reply.strip()}")
                    synced = True
                except Exception as e:
                    print(f"Write failed: {e}")
                    break
    finally:
        try:
            s.close()
        except:
            pass
        print(f"Closed {port.device}")


def main():
    monitored = {}
    while True:
        ports = {p.device: p for p in serial.tools.list_ports.comports()}
        # start new ports
        for dev, p in ports.items():
            if dev not in monitored:
                import threading
                t = threading.Thread(target=handle_port, args=(p,), daemon=True)
                t.start()
                monitored[dev] = t
        # cleanup finished threads
        to_remove = []
        for dev, t in monitored.items():
            if not t.is_alive():
                to_remove.append(dev)
        for dev in to_remove:
            monitored.pop(dev, None)
        time.sleep(SCAN_INTERVAL)


if __name__ == "__main__":
    main()
