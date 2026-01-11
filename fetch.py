import os
import time
import requests

ESP32_IP = "192.168.43.84"
CHECK_INTERVAL = 5  # seconds

STATIC_DIR = "static"
SAVE_FOLDER = "idle"
os.makedirs(SAVE_FOLDER, exist_ok=True)
os.makedirs(STATIC_DIR, exist_ok=True)

last_seen = None

def get_remote_list():
    global last_seen
    url = f"http://{ESP32_IP}/list_idle"
    if last_seen:
        url += f"?after={last_seen}"
    try:
        r = requests.get(url, timeout=10)
        if r.status_code == 200:
            try:
                files = r.json()
                if isinstance(files, list):
                    return files
            except Exception as e:
                print("JSON decode error:", e)
    except requests.exceptions.RequestException as e:
        print("Network error getting file list:", e)
    except Exception as e:
        print("Unexpected error getting file list:", e)
    return []

def download_image(name):
    url = f"http://{ESP32_IP}/image/{name}"
    for attempt in range(3):  # retry up to 3 times
        try:
            r = requests.get(url, timeout=20)
            if r.status_code == 200:
                # save full archive
                try:
                    with open(os.path.join(SAVE_FOLDER, name), "wb") as f:
                        f.write(r.content)
                    # save/update latest.jpg
                    with open(os.path.join(STATIC_DIR, "latest.jpg"), "wb") as f:
                        f.write(r.content)
                    print("Downloaded:", name)
                    return True
                except Exception as e:
                    print("File write error:", name, e)
            else:
                print(f"Failed {name}: HTTP {r.status_code}")
        except requests.exceptions.RequestException as e:
            print(f"Network error downloading {name} (attempt {attempt+1}):", e)
        except Exception as e:
            print(f"Unexpected error downloading {name} (attempt {attempt+1}):", e)
        time.sleep(2)  # wait before retry
    return False

print("ESP32-CAM full idle sync started")

while True:
    try:
        local_files = set(os.listdir(SAVE_FOLDER))
        remote_files = get_remote_list()

        for name in sorted(remote_files):
            if name not in local_files:
                success = download_image(name)
                if success:
                    last_seen = name
                time.sleep(0.5)  # gentle to ESP32

        time.sleep(CHECK_INTERVAL)
    except KeyboardInterrupt:
        print("Stopping downloader...")
        break
    except Exception as e:
        print("Unexpected error in main loop:", e)
        time.sleep(5)  # wait a bit before retrying
