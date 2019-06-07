import socketserver


class MyTCPHandler(socketserver.BaseRequestHandler):
    """
    The RequestHandler class for our server.
    """
    def handle(self):
        self.data = self.request.recv(1024).strip()
        print("{} wrote:".format(self.client_address[0]))
        print(self.data)


if __name__ == "__main__":
    HOST, PORT = "localhost", 8101
    server = socketserver.TCPServer((HOST, PORT), MyTCPHandler)
    server.serve_forever()


