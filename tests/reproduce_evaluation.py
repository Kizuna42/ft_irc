import socket
import time
import threading
import sys
import os

# Configuration
HOST = os.environ.get('IRC_HOST', '127.0.0.1')
PORT = int(os.environ.get('IRC_PORT', '6667'))
PASSWORD = os.environ.get('IRC_PASSWORD', 'password123')

FAILURES = []

def fail(message):
    print(message)
    FAILURES.append(message)

class IRCClient:
    def __init__(self, nickname, username="user", password=PASSWORD):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.nickname = nickname
        self.username = username
        self.password = password
        self.connected = False
        self.buffer = ""

    def connect(self, rcvbuf=None):
        try:
            if rcvbuf is not None:
                self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
            self.sock.connect((HOST, PORT))
            self.connected = True
            return True
        except Exception as e:
            print(f"[{self.nickname}] Connection failed: {e}")
            return False

    def send(self, data):
        if not self.connected:
            return
        try:
            # IRC messages end with CRLF
            if not data.endswith('\r\n'):
                data += '\r\n'
            self.sock.sendall(data.encode('utf-8'))
        except Exception as e:
            print(f"[{self.nickname}] Send failed: {e}")

    def send_partial(self, data):
        """Send data in chunks to simulate network fragmentation"""
        if not self.connected:
            return
        try:
            if not data.endswith('\r\n'):
                data += '\r\n'
            
            # Split into small chunks
            chunks = [data[i:i+2] for i in range(0, len(data), 2)]
            for chunk in chunks:
                self.sock.send(chunk.encode('utf-8'))
                time.sleep(0.05) # Small delay between chunks
        except Exception as e:
            print(f"[{self.nickname}] Partial send failed: {e}")

    def receive(self, timeout=2):
        if not self.connected:
            return None
        
        self.sock.settimeout(timeout)
        try:
            data = self.sock.recv(4096).decode('utf-8', errors='ignore')
            if not data:
                self.connected = False
                return None
            self.buffer += data
            
            lines = []
            while '\r\n' in self.buffer:
                part, self.buffer = self.buffer.split('\r\n', 1)
                lines.append(part)
            return lines
        except socket.timeout:
            return []
        except Exception as e:
            return None

    def register(self):
        self.send(f"PASS {self.password}")
        self.send(f"NICK {self.nickname}")
        self.send(f"USER {self.username} 0 * :{self.nickname}")
        
        # Wait for welcome message (001)
        start_time = time.time()
        while time.time() - start_time < 5:
            lines = self.receive()
            if lines:
                for line in lines:
                    if " 001 " in line:
                        return True
                    if " 433 " in line: # Nickname in use
                        return False
            time.sleep(0.1)
        return False

    def close(self):
        if self.connected:
            self.send("QUIT :Test ended")
            self.sock.close()
            self.connected = False

def test_basic_connection():
    print("\n=== Test 1: Basic Connection & Authentication ===")
    client = IRCClient("test_basic")
    if client.connect():
        if client.register():
            print("✓ Connection and Registration successful")
        else:
            fail("✗ Registration failed")
        client.close()
    else:
        fail("✗ Connection failed")

def test_channel_operations():
    print("\n=== Test 2: Channel Operations (JOIN, TOPIC, PART) ===")
    client = IRCClient("test_chan")
    if not client.connect() or not client.register():
        fail("✗ Setup failed")
        return

    # JOIN
    client.send("JOIN #test")
    time.sleep(0.5)
    lines = client.receive()
    joined = False
    for line in lines or []:
        if "JOIN" in line and "#test" in line:
            joined = True
    
    if joined:
        print("✓ JOIN #test successful")
    else:
        fail("✗ JOIN #test failed")

    # TOPIC
    client.send("TOPIC #test :Hello World")
    time.sleep(0.5)
    lines = client.receive()
    topic_set = False
    for line in lines or []:
        if "TOPIC" in line and "Hello World" in line:
            topic_set = True
    
    if topic_set:
        print("✓ TOPIC set successful")
    else:
        fail("✗ TOPIC set failed")

    # PART
    client.send("PART #test")
    time.sleep(0.5)
    lines = client.receive()
    parted = False
    for line in lines or []:
        if "PART" in line and "#test" in line:
            parted = True
            
    if parted:
        print("✓ PART #test successful")
    else:
        fail("✗ PART #test failed")

    client.close()

def test_messaging():
    print("\n=== Test 3: Messaging (PRIVMSG) ===")
    receiver = IRCClient("receiver")
    sender = IRCClient("sender")
    
    receiver.connect()
    receiver.register()
    
    sender.connect()
    sender.register()
    
    time.sleep(1)
    
    # Test Private Message
    sender.send("PRIVMSG receiver :Hello Private")
    time.sleep(0.5)
    
    lines = receiver.receive()
    received_pm = False
    for line in lines or []:
        if "PRIVMSG receiver :Hello Private" in line and "sender" in line:
            received_pm = True
            
    if received_pm:
        print("✓ Private Message successful")
    else:
        fail(f"✗ Private Message failed. Got: {lines}")

    # Test Channel Message
    receiver.send("JOIN #msgtest")
    sender.send("JOIN #msgtest")
    time.sleep(1)
    
    # Flush buffers
    receiver.receive(timeout=0.1)
    
    sender.send("PRIVMSG #msgtest :Hello Channel")
    time.sleep(0.5)
    
    lines = receiver.receive()
    received_cm = False
    for line in lines or []:
        if "PRIVMSG #msgtest :Hello Channel" in line and "sender" in line:
            received_cm = True

    if received_cm:
        print("✓ Channel Message successful")
    else:
        fail(f"✗ Channel Message failed. Got: {lines}")
        
    receiver.close()
    sender.close()

def test_partial_commands():
    print("\n=== Test 4: Partial Commands (Fragmentation) ===")
    client = IRCClient("test_partial")
    if not client.connect():
        fail("✗ Setup failed")
        return
        
    # Send PASS and NICK normally
    client.send(f"PASS {PASSWORD}")
    client.send(f"NICK test_partial")
    
    # Send USER in chunks
    # This simulates "Use ctrl+D to send the command in several parts"
    # Also nc -C behavior
    print("Sending USER command in chunks...")
    client.send_partial(f"USER test_partial 0 * :Partial User")
    
    start_time = time.time()
    registered = False
    while time.time() - start_time < 5:
        lines = client.receive()
        if lines:
            for line in lines:
                if " 001 " in line:
                    registered = True
                    break
        if registered:
            break
            
    if registered:
        print("✓ Server handled partial command correctly")
    else:
        fail("✗ Server failed to handle partial command")
        
    client.close()

def test_operator_commands():
    print("\n=== Test 5: Operator Commands ===")
    # First user joining a channel becomes operator in this implementation
    op = IRCClient("op_user")
    user = IRCClient("reg_user")
    
    op.connect(); op.register()
    user.connect(); user.register()
    
    op.send("JOIN #optest")
    time.sleep(0.5)
    user.send("JOIN #optest")
    time.sleep(0.5)
    
    # Check if op is operator (MODE command reply or just testing commands)
    # Let's try to KICK user
    
    print("Testing KICK...")
    op.send("KICK #optest reg_user :Get out")
    time.sleep(0.5)
    
    lines = user.receive()
    kicked = False
    for line in lines or []:
        if "KICK #optest reg_user" in line:
            kicked = True
            
    if kicked:
        print("✓ KICK successful")
    else:
        fail("✗ KICK failed")
        
    # Invite back
    print("Testing INVITE...")
    op.send("INVITE reg_user #optest")
    time.sleep(0.5)
    
    lines = user.receive()
    invited = False
    for line in lines or []:
        if "INVITE reg_user #optest" in line:
            invited = True
            
    if invited:
        print("✓ INVITE successful")
    else:
        fail("✗ INVITE failed")

    # Test MODE
    print("Testing MODE +k...")
    op.send("MODE #optest +k secret")
    time.sleep(0.5)
    
    lines = op.receive() # Op should receive mode update
    mode_set = False
    for line in lines or []:
        if "MODE #optest +k secret" in line:
            mode_set = True
            
    if mode_set:
        print("✓ MODE +k successful")
    else:
        fail("✗ MODE +k failed")

    op.close()
    user.close()

def test_flood_stability():
    print("\n=== Test 6: Flood Stability ===")
    client = IRCClient("flooder")
    if not client.connect() or not client.register():
        fail("✗ Setup failed")
        return

    print("Sending 1000 PINGs...")
    start = time.time()
    for i in range(1000):
        client.send(f"PING :flood_{i}")
        # No sleep, test buffering
    
    # Verify server is still alive by sending a valid command and getting reply
    client.send("PRIVMSG flooder :Am I alive?")
    
    try:
        # Drain buffer
        replies = 0
        timeout_start = time.time()
        while time.time() - timeout_start < 5:
            lines = client.receive(timeout=0.5)
            if lines:
                replies += len(lines)
                for line in lines:
                    if "Am I alive?" in line:
                        print(f"✓ Server survived flood (Processed {replies}+ lines)")
                        client.close()
                        return
    except:
        pass
        
    fail("✗ Server did not respond after flood")
    client.close()

def test_slow_reader_flood():
    print("\n=== Test 7: Flooding a channel with a member that never reads ===")
    # A member that stops reading eventually overflows its server-side send
    # buffer and is disconnected. This happens in the middle of a channel
    # broadcast, which must not corrupt server state.
    sleeper = IRCClient("sleeper")
    flooder = IRCClient("chanflood")
    watcher = IRCClient("watcher")
    if not sleeper.connect(rcvbuf=4096) or not sleeper.register():
        fail("✗ Setup failed (sleeper)")
        return
    if not flooder.connect() or not flooder.register():
        fail("✗ Setup failed (flooder)")
        return
    if not watcher.connect() or not watcher.register():
        fail("✗ Setup failed (watcher)")
        return

    # The sleeper joins last so that it is the last member iterated over
    # during a broadcast.
    for c in (flooder, watcher, sleeper):
        c.send("JOIN #flood")
        time.sleep(0.2)

    payload = "x" * 400
    stop = threading.Event()

    def drain():
        # Keep the watcher reading so that only the sleeper overflows.
        while not stop.is_set() and watcher.connected:
            watcher.receive(timeout=0.2)

    t = threading.Thread(target=drain)
    t.start()
    try:
        # Enough data to fill the kernel socket buffers on loopback and then
        # the server's own per-client send buffer.
        for batch in range(400):
            lines = "".join(f"PRIVMSG #flood :{batch}-{i} {payload}\r\n" for i in range(50))
            flooder.sock.settimeout(10)
            flooder.sock.sendall(lines.encode("utf-8"))
            flooder.receive(timeout=0.001)
    finally:
        time.sleep(1)
        stop.set()
        t.join()

    watcher.buffer = ""
    flooder.send("PRIVMSG #flood :still alive")
    alive = False
    start = time.time()
    while time.time() - start < 5 and not alive:
        for line in watcher.receive(timeout=0.5) or []:
            if "still alive" in line:
                alive = True

    if alive:
        print("✓ Server survived broadcast to a non-reading member")
    else:
        fail("✗ Server did not relay messages after slow-reader flood")

    newcomer = IRCClient("newcomer")
    if newcomer.connect() and newcomer.register():
        print("✓ Server still accepts new clients")
    else:
        fail("✗ Server stopped accepting new clients")
    newcomer.close()

    for c in (sleeper, flooder, watcher):
        try:
            c.close()
        except Exception:
            pass

if __name__ == "__main__":
    try:
        test_basic_connection()
        test_channel_operations()
        test_messaging()
        test_partial_commands()
        test_operator_commands()
        test_flood_stability()
        test_slow_reader_flood()
    except KeyboardInterrupt:
        print("\nTests interrupted.")
        sys.exit(130)
    except Exception as e:
        fail(f"\nTests failed with error: {e}")

    if FAILURES:
        print(f"\n{len(FAILURES)} check(s) failed.")
        sys.exit(1)
    print("\nAll tests passed.")
