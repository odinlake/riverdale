import socketserver
import os
import subprocess
import signal
import threading
from datetime import datetime, timedelta


PROC_SLIDESHOW = []
PROC_GRAFANA = []
STATUS = {}


def check_pid(pid):
    """ Check For the existence of a unix pid. """
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    else:
        return True


def kill_procs(procs):
    """Issue SIGTERM to all processes that are alive and weed out those that are not."""
    alive = []
    for proc in procs:
        pid = proc.pid
        if check_pid(pid):
            alive.append(proc)
            print("killing:", pid, "...")
            try:
                os.killpg(pid, signal.SIGTERM)
            except:
                print("can't kill", pid, "so assuming it's dead already...")
                alive.pop()
    return alive


def start_proc(cmd):
    """Start a new process."""
    nproc = subprocess.Popen(cmd, preexec_fn=os.setsid)
    print("started:", nproc.pid, "...")
    return nproc


def maintain_grafana(force = False):
    """Grafana needs to be restarted periodically (it seems midori/grafana leak resources)."""
    global PROC_GRAFANA
    status = STATUS.get('grafana', 'on')
    tt = datetime.utcnow()
    t0 = STATUS.get('grafana-time')
    trigger = force or (t0 is None) or (tt - t0 > timedelta(hours=4))
    if status == 'on' and trigger:
        STATUS['grafana-time'] = tt
        nproc = start_proc(["sh", "/home/pi/display-grafana.sh"])
        PROC_GRAFANA = kill_procs(PROC_GRAFANA)
        PROC_GRAFANA.append(nproc)
    if status == 'off':
        PROC_GRAFANA = kill_procs(PROC_GRAFANA)


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

        if self.data == b":slideshow-on":
            self.start_slideshow()
        if self.data == b":slideshow-off":
            self.kill_slideshow()

        if self.data == b":grafana-on":
            self.start_slideshow()
        if self.data == b":grafana-off":
            self.kill_slideshow()

    def kill_slideshow(self):
        global PROC_SLIDESHOW
        PROC_SLIDESHOW = kill_procs(PROC_SLIDESHOW)

    def start_slideshow(self):
        nproc = start_proc(["sh", "/home/pi/start-slideshow.sh"])
        self.kill_slideshow()
        PROC_SLIDESHOW.append(nproc)

    def kill_grafana(self):
        STATUS['grafana'] = 'off'
        maintain_grafana()

    def start_grafana(self):
        STATUS['grafana'] = 'on'
        maintain_grafana(True)


if __name__ == "__main__":
    threading.Timer(45.0, maintain_grafana)
    HOST, PORT = "0.0.0.0", 8101
    server = socketserver.TCPServer((HOST, PORT), MyTCPHandler)
    server.serve_forever()


