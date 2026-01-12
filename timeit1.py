from http.server import BaseHTTPRequestHandler, HTTPServer
import time
import json

# ---------- Color codes ----------
RESET = "\033[0m"
GREEN = "\033[92m"
RED = "\033[91m"
CYAN = "\033[96m"
YELLOW = "\033[93m"
PURPLE = "\033[95m"
DIM = "\033[2m"

def log(msg, color=RESET):
    ts = time.strftime("%H:%M:%S")
    print(f"{DIM}[{ts}]{RESET} {color}{msg}{RESET}")

class Handler(BaseHTTPRequestHandler):

    def log_message(self, format, *args):
        # disable default noisy HTTP logs
        return

    def do_GET(self):
        client_ip = self.client_address[0]
        log(f"⇢ GET {self.path} from {client_ip}", CYAN)

        if self.path == "/time_now":
            now = time.time()
            data = {
                "epoch": int(now),
                "iso": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(now))
            }

            body = json.dumps(data).encode()

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

            log(f"✓ 200 OK | epoch={data['epoch']}", GREEN)

        else:
            self.send_response(404)
            self.end_headers()
            log(f"✗ 404 Not Found → {self.path}", RED)


if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", 8090), Handler)

    log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━", PURPLE)
    log("⏱  TIME SYNC SERVER ONLINE", PURPLE)
    log("🌐 Endpoint  : /time_now", YELLOW)
    log("📡 Listening : 0.0.0.0:8090", YELLOW)
    log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━", PURPLE)

    server.serve_forever()
