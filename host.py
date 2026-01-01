from flask import Flask, render_template
import os

app = Flask(__name__)

STATIC_DIR = "static"
SAVE_FOLDER = "/data/data/com.termux/files/home/storage/pictures/esp32/idle"

@app.route("/")
def index():
    count = len(os.listdir(SAVE_FOLDER))+1500
    return render_template("index.html", count=count)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=7272, debug=True)