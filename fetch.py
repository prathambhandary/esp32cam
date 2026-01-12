import os
import time
import requests
from datetime import datetime, timedelta

ESP32_IP = "192.168.43.84"
CHECK_INTERVAL = 5  # seconds
BASE_DIR = "idle"
STATIC_DIR = "static"
CURSOR_FILE = "last_seen.txt"

os.makedirs(BASE_DIR, exist_ok=True)
os.makedirs(STATIC_DIR, exist_ok=True)
# print(1)
# Load last_seen from disk (fast resume)
if os.path.exists(CURSOR_FILE):
    with open(CURSOR_FILE, "r") as f:
        last_seen = f.read().strip() or None
    flag = True
    print("Resuming from:", last_seen)
else:
    last_seen = None
    flag = False
# print(2)
def save_cursor(name):
    try:
        with open(CURSOR_FILE, "w") as f:
            f.write(name)
    except Exception as e:
        print("Cursor save error:", e)

# ---------- Guessing function ----------
def next_name(name, n=1):
    """Return the next second timestamped filename."""
    try:
        t = datetime.strptime(name, "%Y%m%d_%H%M%S.jpg")
        t += timedelta(seconds=n)
        return t.strftime("%Y%m%d_%H%M%S.jpg")
    except Exception as e:
        print("next_name parse error:", name, e)
        return None

# ---------- Download ----------
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

        for attempt in range(1):
            try:
                r = requests.get(url, timeout=20)
                if r.status_code == 200:
                    with open(local_path, "wb") as f:
                        f.write(r.content)
                    with open(os.path.join(STATIC_DIR, "latest.jpg"), "wb") as f:
                        f.write(r.content)
                    # print("Downloaded:", name)
                    save_cursor(name)
                    return True
                elif r.status_code == 404:
                    # print("Failed: ", name)
                    return False  # guessing stop
                else:
                    print(f"HTTP {r.status_code}:", name)
            except Exception as e:
                print(f"Retry {attempt+1} failed:", e)
            time.sleep(0.2)
    except Exception as e:
        print("Parse/save error:", name, e)
    return False

# ---------- Main loop ----------
print("ESP32-CAM hour-wise guessing sync started")

# ---------- ANSI COLORS ----------
RESET = "\033[0m"
RED = "\033[91m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
CYAN = "\033[96m"
MAGENTA = "\033[95m"

# ---------- Main loop prints ----------
print(f"{CYAN}ESP32-CAM hour-wise guessing sync started{RESET}")

while True:
    try:
        if last_seen:
            if flag:
                guess = next_name(last_seen, n=14)
                flag = False
            else:
                guess = next_name(last_seen)
            if not guess:
                time.sleep(CHECK_INTERVAL)
                continue

            # check against current time
            try:
                guess_dt = datetime.strptime(guess, "%Y%m%d_%H%M%S.jpg")
                now = datetime.now()
                if guess_dt > now:
                    wait_secs = (guess_dt - now).total_seconds()
                    if wait_secs > 0:
                        print(f"{YELLOW}Waiting {30}s for next image: {guess}{RESET}")
                        # time.sleep(wait_secs+5)
                        time.sleep(30)
            except Exception as e:
                print(f"{RED}Time parse error:{RESET} {guess} -> {e}")

            success = download_image(guess)
            if success:
                last_seen = guess
                save_cursor(last_seen)
                print(f"{GREEN}✔ Downloaded:{RESET} {guess}")
                time.sleep(0.2)
                flag = True
            else:
                last_seen = guess
                print(f"{MAGENTA}⏳ File not ready yet:{RESET} {guess}")
                time.sleep(2)
                continue

        else:
            try:
                url = f"http://{ESP32_IP}/list_idle"
                print(f"{CYAN}Fetching initial list:{RESET} {url}")
                r = requests.get(url, timeout=30)
                if r.status_code == 200:
                    data = r.json()
                    if isinstance(data, list) and data:
                        last_seen = data[-1]
                        save_cursor(last_seen)
                        print(f"{GREEN}Starting from:{RESET} {last_seen}")
                else:
                    print(f"{RED}List HTTP {r.status_code}{RESET}")
                    time.sleep(CHECK_INTERVAL)
            except Exception as e:
                print(f"{RED}Initial list error:{RESET} {e}")
                time.sleep(CHECK_INTERVAL)

    except KeyboardInterrupt:
        print(f"{YELLOW}Stopped by user.{RESET}")
        break
    except Exception as e:
        print(f"{RED}Main loop error:{RESET} {e}")
        time.sleep(5)
      