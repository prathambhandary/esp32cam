import os
import time
import requests
from datetime import datetime

ESP32_IP = "192.168.43.84"
CHECK_INTERVAL = 15

BASE_DIR = "idle"
STATIC_DIR = "static"
CURSOR_FILE = "last_seen.txt"

os.makedirs(BASE_DIR, exist_ok=True)
os.makedirs(STATIC_DIR, exist_ok=True)

# ---------- ANSI COLORS ----------
RESET = "\033[0m"
DIM = "\033[2m"
BOLD = "\033[1m"
RED = "\033[91m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
MAGENTA = "\033[95m"
CYAN = "\033[96m"
WHITE = "\033[97m"

# ---------- Load last seen ----------
if os.path.exists(CURSOR_FILE):
    with open(CURSOR_FILE, "r") as f:
        last_seen = f.read().strip() or None
    print(f"{CYAN}[↻] Resuming session from:{RESET} {WHITE}{last_seen}{RESET}")
else:
    last_seen = None
    print(f"{YELLOW}[!] No cursor found — cold start{RESET}")


def save_cursor(value):
    try:
        with open(CURSOR_FILE, "w") as f:
            f.write(value)
    except Exception as e:
        print(f"{RED}[X] Cursor write failed:{RESET} {e}")


# ---------- Fetch metadata ----------
def get_last_timestamp():
    try:
        r = requests.get(f"http://{ESP32_IP}/last_timestamp", timeout=15)
        if r.status_code != 200:
            return None

        data = r.json()
        return data.get("name"), data.get("epoch")

    except Exception as e:
        print(f"{RED}[NET] Metadata probe failed:{RESET} {e}")
        time.sleep(10)
        return None


# ---------- Download latest ----------
def download_image(name):
    if not name.endswith(".jpg") or name.startswith("1970"):
        return False

    try:
        date_part = name[0:8]
        hour_part = name[9:11]
        save_dir = os.path.join(BASE_DIR, date_part, hour_part)
        os.makedirs(save_dir, exist_ok=True)

        local_path = os.path.join(save_dir, name)
        url = f"http://{ESP32_IP}/image/{name}"

        for attempt in range(2):
            try:
                r = requests.get(url, timeout=15, stream=True)

                if r.status_code == 404:
                    return False

                if r.status_code != 200:
                    print(f"{YELLOW}[HTTP] {r.status_code} @ {name}{RESET}")
                    continue

                data = r.content

                if len(data) < 10_000:
                    print(f"{YELLOW}[⚠] Corrupt frame dropped:{RESET} {name}")
                    return False

                if not data.endswith(b"\xff\xd9"):
                    print(f"{YELLOW}[⚠] JPEG incomplete:{RESET} {name}")
                    return False

                with open(local_path, "wb") as f:
                    f.write(data)

                with open(os.path.join(STATIC_DIR, "latest.jpg"), "wb") as f:
                    f.write(data)

                save_cursor(name)
                return True

            except requests.exceptions.Timeout:
                print(f"{MAGENTA}[TIMEOUT] Waiting on ESP32…{RESET}")
            except requests.exceptions.RequestException as e:
                print(f"{RED}[NET] Packet loss:{RESET} {e}")
            except Exception as e:
                print(f"{RED}[FAIL] Write error:{RESET} {e}")

            time.sleep(0.7)

    except Exception as e:
        print(f"{RED}[FS] Path error:{RESET} {e}")

    return False


# ---------- Main loop ----------
print(f"""{BOLD}{GREEN}
┌──────────────────────────────────────────┐
│  ESP32-CAM LIVE EXFIL CHANNEL ESTABLISHED │
└──────────────────────────────────────────┘
{RESET}""")

while True:
    try:
        meta = get_last_timestamp()
        if not meta:
            time.sleep(CHECK_INTERVAL)
            continue

        name, epoch = meta

        if not name or name.startswith("1970"):
            print(f"{DIM}[SYNC] Clock not ready{RESET}")
            time.sleep(CHECK_INTERVAL)
            continue

        if name == last_seen:
            print(f"{DIM}[IDLE] No new frame{RESET}")
            time.sleep(CHECK_INTERVAL)
            continue

        print(f"{CYAN}[→] Target frame:{RESET} {WHITE}{name}{RESET}")

        ok = download_image(name)
        if ok:
            last_seen = name
            ts = datetime.fromtimestamp(epoch).strftime("%Y-%m-%d %H:%M:%S")
            print(f"{GREEN}[✓] Frame captured:{RESET} {WHITE}{name}{RESET} {DIM}@ {ts}{RESET}")
            time.sleep(10)
        else:
            print(f"{MAGENTA}[…] Frame not ready — backing off{RESET}")
            time.sleep(10)

    except KeyboardInterrupt:
        print(f"\n{YELLOW}[CTRL] Session terminated by operator{RESET}")
        break
    except Exception as e:
        print(f"{RED}[PANIC] Loop failure:{RESET} {e}")
        time.sleep(5)
