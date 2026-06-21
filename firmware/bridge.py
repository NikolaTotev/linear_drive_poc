"""TCP-server <-> UART transparent line bridge for the Linear Drive Pico-W.

Forwards every complete '\\n'-terminated line verbatim in both directions:
  TCP client  --> UART (Arduino)
  UART (Arduino) --> TCP client

Non-blocking: uses uselect.poll() on the listening/client sockets and
uart.any() for the UART side, so neither direction can starve the other.
"""

import uselect
import utime

# --- Config constants ---
TCP_HOST = "0.0.0.0"
TCP_PORT = 5000

MAX_LINE_LEN = 64     # protocol max line length (bytes, excluding terminator)
MAX_BUF_LEN = 128     # if a buffer with no '\n' exceeds this, discard it

IDLE_SLEEP_MS = 2     # small sleep when nothing to do


class Bridge:
    def __init__(self, listen_sock, uart):
        self.listen_sock = listen_sock
        self.uart = uart

        self.poller = uselect.poll()
        self.poller.register(self.listen_sock, uselect.POLLIN)

        self.client = None
        self.client_addr = None

        # Receive buffers (bytes)
        self.tcp_buf = b""
        self.uart_buf = b""

    # ------------------------------------------------------------------
    # Client management
    # ------------------------------------------------------------------
    def _accept_client(self, sock):
        try:
            conn, addr = sock.accept()
        except OSError:
            return

        conn.setblocking(False)
        self.client = conn
        self.client_addr = addr
        self.tcp_buf = b""
        self.poller.register(self.client, uselect.POLLIN)
        print("client connected", addr)

    def _drop_client(self):
        if self.client is not None:
            try:
                self.poller.unregister(self.client)
            except (KeyError, OSError, ValueError):
                pass
            try:
                self.client.close()
            except OSError:
                pass
            print("client disconnected")

        self.client = None
        self.client_addr = None
        self.tcp_buf = b""

    # ------------------------------------------------------------------
    # TCP -> UART
    # ------------------------------------------------------------------
    def _read_tcp(self, sock):
        try:
            data = sock.recv(256)
        except OSError:
            return

        if not data:
            # Peer closed the connection.
            self._drop_client()
            return

        self.tcp_buf += data
        self._process_tcp_buf()

    def _process_tcp_buf(self):
        while True:
            idx = self.tcp_buf.find(b"\n")
            if idx == -1:
                if len(self.tcp_buf) > MAX_BUF_LEN:
                    print("tcp buf overflow, discarding")
                    self.tcp_buf = b""
                return

            line = self.tcp_buf[: idx + 1]
            self.tcp_buf = self.tcp_buf[idx + 1 :]

            # Strip optional trailing '\r' before '\n', re-append '\n'.
            stripped = line.rstrip(b"\r\n")
            if len(stripped) > MAX_LINE_LEN:
                print("tcp line too long, dropping")
                continue

            out = stripped + b"\n"
            self.uart.write(out)
            print("app->arduino:", stripped)

    # ------------------------------------------------------------------
    # UART -> TCP
    # ------------------------------------------------------------------
    def _read_uart(self):
        n = self.uart.any()
        if not n:
            return

        data = self.uart.read(n)
        if not data:
            return

        self.uart_buf += data
        self._process_uart_buf()

    def _process_uart_buf(self):
        while True:
            idx = self.uart_buf.find(b"\n")
            if idx == -1:
                if len(self.uart_buf) > MAX_BUF_LEN:
                    print("uart buf overflow, discarding")
                    self.uart_buf = b""
                return

            line = self.uart_buf[: idx + 1]
            self.uart_buf = self.uart_buf[idx + 1 :]

            stripped = line.rstrip(b"\r\n")
            if len(stripped) > MAX_LINE_LEN:
                print("uart line too long, dropping")
                continue

            out = stripped + b"\n"
            print("arduino->app:", stripped)

            if self.client is not None:
                try:
                    self.client.write(out)
                except OSError:
                    self._drop_client()

    # ------------------------------------------------------------------
    # Main loop
    # ------------------------------------------------------------------
    def run_forever(self):
        while True:
            self.step()

    def step(self):
        events = self.poller.poll(IDLE_SLEEP_MS)

        for sock, ev in events:
            if sock is self.listen_sock:
                if ev & uselect.POLLIN:
                    if self.client is None:
                        self._accept_client(sock)
                    else:
                        # Already have a client: drain & reject extra
                        # connection attempts so accept() doesn't block.
                        try:
                            conn, addr = sock.accept()
                            conn.close()
                        except OSError:
                            pass
                continue

            if self.client is not None and sock is self.client:
                if ev & (uselect.POLLHUP | uselect.POLLERR):
                    self._drop_client()
                elif ev & uselect.POLLIN:
                    self._read_tcp(sock)

        self._read_uart()

        if not events:
            utime.sleep_ms(IDLE_SLEEP_MS)
