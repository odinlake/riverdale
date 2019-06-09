import socketserver
import os
import subprocess
import signal


PROC_SLIDESHOW = []


def check_pid(pid):
    """ Check For the existence of a unix pid. """
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    else:
        return True
                    

class MyTCPHandler(socketserver.BaseRequestHandler):
    """
    The RequestHandler class for our server.
    """
    def handle(self):
        self.data = self.request.recv(1024).strip()
        print("{} wrote:".format(self.client_address[0]))
        print(self.data)
        if self.data == b"activate":
            self.start_slideshow()
        if self.data == b"deactivate":
            self.kill_slideshow()

    def kill_slideshow(self):
        global PROC_SLIDESHOW
        alive = []
        for proc in PROC_SLIDESHOW:
            pid = proc.pid
            print("checking", pid, "...")
            if check_pid(pid):
                alive.append(proc)
                print("killing:", pid, "...")
                try:
                    os.killpg(pid, signal.SIGTERM)
                except:
                    print("can't kill it, so assuming dead already...")
                    alive.pop()
        PROC_SLIDESHOW = alive

    def start_slideshow(self):
        nproc = subprocess.Popen(
            ["sh", "/home/pi/start-slideshow.sh"], 
            preexec_fn=os.setsid
        )
        print("started:", nproc.pid, "...")
        self.kill_slideshow()
        PROC_SLIDESHOW.append(nproc)
        

if __name__ == "__main__":
    HOST, PORT = "0.0.0.0", 8101
    server = socketserver.TCPServer((HOST, PORT), MyTCPHandler)
    server.serve_forever()


