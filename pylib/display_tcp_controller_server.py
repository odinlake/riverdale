import socketserver
import os
import subprocess
import signal


class MyTCPHandler(socketserver.BaseRequestHandler):
    """
    The RequestHandler class for our server.
    """
    def __init__(self):
        self.proc_slideshow = None

    def handle(self):
        self.data = self.request.recv(1024).strip()
        print("{} wrote:".format(self.client_address[0]))
        print(self.data)
        if self.data == "activate":
            self.start_slideshow()
        if self.data == "deactivate":
            self.kill_slideshow()

    def kill_slideshow(self):
        if self.proc_slideshow:
            os.killpg(self.proc_slideshow.pid, signal.SIGTERM)
            self.proc_slideshow = None

    def start_slideshow(self):
        nproc = subprocess.Popen(
            "sh ~/start-slideshow.sh", stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            shell=True, preexec_fn=os.setsid
        )
        self.kill_slideshow()
        self.proc_slideshow = nproc
        

if __name__ == "__main__":
    HOST, PORT = "0.0.0.0", 8101
    server = socketserver.TCPServer((HOST, PORT), MyTCPHandler)
    server.serve_forever()


