import psutil
import json
import requests


URL = "http://192.168.1.15:8080/api/v1/sXvMI72lOr1nZeOIYe9u/telemetry"


def send(data, quiet=True):
    r = requests.post(URL, data=data)
    if not quiet:
        print(r.text)

        
def logcpu(quiet=True):
    data = json.dumps({
        "cpu": psutil.cpu_percent(),
        "memory": psutil.virtual_memory()[2],
    })
    if not quiet:
        print(data)
    send(data, quiet=quiet)
    

if __name__ == "__main__":
    logcpu(False)

    
