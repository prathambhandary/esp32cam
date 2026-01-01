from flask import Flask, render_template
import os

app = Flask(__name__)

@app.route("/")
def index():
    count = len(os.listdir("idle"))+1500
    return render_template("index.html", count=count)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=7272, debug=True)