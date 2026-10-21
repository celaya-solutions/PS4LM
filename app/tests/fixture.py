"""Test fixture for app/tests/test_host.c: a tiny HTTP server and a TCP port
that closes every connection at once. Writes "<http_port> <close_port>" to the
file named by argv[1] when both listen."""
import http.server
import socket
import sys
import threading

health_calls = 0


class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        global health_calls
        if self.path == "/health":
            health_calls += 1
            code, body = (503, '{"error":"loading"}') if health_calls == 1 else (200, '{"status":"ok"}')
        elif self.path == "/slots":
            code, body = 200, '[{"id":0,"is_processing":true},{"id":1,"is_processing":false}]'
        elif self.path == "/big":
            code, body = 200, "x" * 5000
        else:
            code, body = 404, "no"
        data = body.encode()
        self.send_response(code)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, *a):
        pass


def closer(s):
    while True:
        c, _ = s.accept()
        c.close()


def watch_parent(ppid):
    """Exit when the test runner dies (a failed assert), so make does not hang."""
    import os
    import time
    while os.getppid() == ppid:
        time.sleep(0.2)
    os._exit(0)


import os
threading.Thread(target=watch_parent, args=(os.getppid(),), daemon=True).start()
httpd = http.server.HTTPServer(("127.0.0.1", 0), H)
cs = socket.socket()
cs.bind(("127.0.0.1", 0))
cs.listen(8)
threading.Thread(target=closer, args=(cs,), daemon=True).start()
with open(sys.argv[1] + ".tmp", "w") as f:
    f.write("%d %d\n" % (httpd.server_address[1], cs.getsockname()[1]))
os.rename(sys.argv[1] + ".tmp", sys.argv[1])
httpd.serve_forever()
