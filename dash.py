from flask import Flask, render_template, send_from_directory, jsonify
import os
from datetime import datetime

app = Flask(__name__)

BASE_DIR = "idle"
STATIC_DIR = "static"

@app.route("/")
def index():
    days = sorted(os.listdir(BASE_DIR), reverse=True) if os.path.exists(BASE_DIR) else []
    return render_template("index0.html", days=days)

@app.route("/hours/<date>")
def hours(date):
    path = os.path.join(BASE_DIR, date)
    hours = sorted(os.listdir(path), reverse=True) if os.path.exists(path) else []
    return jsonify(hours)

@app.route("/images/<date>/<hour>")
def images(date, hour):
    path = os.path.join(BASE_DIR, date, hour)
    imgs = sorted(os.listdir(path), reverse=True) if os.path.exists(path) else []
    return jsonify(imgs)

@app.route("/view/<date>/<hour>/<name>")
def view_image(date, hour, name):
    return send_from_directory(os.path.join(BASE_DIR, date, hour), name)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)