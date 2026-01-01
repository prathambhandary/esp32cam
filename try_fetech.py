import os
import time
import requests

ESP32_IP = "192.168.43.84"  # ESP32 IP
CHECK_INTERVAL = 30        # seconds
# SAVE_FOLDER = "idle"
STATIC_DIR = "static"
SAVE_FOLDER = "/data/data/com.termux/files/home/storage/pictures/esp32/idle"

delay_offset = 1700

if not os.path.exists(SAVE_FOLDER):
    os.makedirs(SAVE_FOLDER)

last_downloaded = len(os.listdir(SAVE_FOLDER))+delay_offset
print(last_downloaded)


downloaded_files = set(os.listdir(SAVE_FOLDER))

def get_last_image_name():
    try:
        r = requests.get(f"http://{ESP32_IP}/last", timeout=20)
        if r.status_code == 200:
            name = r.text.strip()
            if name:
                return name  # idle_0007.jpg
    except Exception as e:
        print("Error getting last image:", e)
    return None

def download_image(filename):
    url = f"http://{ESP32_IP}/image/idle_{filename}"
    if len(url) == 39:
        url = f"http://{ESP32_IP}/image/idle_0{filename}"

    # print(url)

    try:
        resp = requests.get(url, timeout=15)
        print("Response code for", filename, ":", resp.status_code)
        time.sleep(10)
        if resp.status_code == 200:
            path = os.path.join(SAVE_FOLDER, filename)
            with open(path, "wb") as f:
                f.write(resp.content)
            path = os.path.join(STATIC_DIR, "latest.jpg")
            with open(path, "wb") as f:
                f.write(resp.content)
            downloaded_files.add(filename)
            print("Downloaded:", filename)
            # time.sleep(CHECK_INTERVAL-0.65)  # brief pause
        else:
            print("Failed to download:", filename)
    except Exception as e:
        print("Error downloading", filename, e)

print("Starting laptop auto-fetch...")

print("Laptop auto-fetch started")

while True:
    latest = get_last_image_name()

    if latest:
        while last_downloaded < int(latest[5:9])+1:
            next_filename = f"{last_downloaded:04d}.jpg"
            if next_filename not in downloaded_files:
                if download_image(next_filename):
                    last_downloaded += 1
            else:
                last_downloaded += 1

        time.sleep(CHECK_INTERVAL-10)