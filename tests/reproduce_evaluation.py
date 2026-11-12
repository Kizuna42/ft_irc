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

def wait_for(client, needle, timeout=3):
    """Read until a line containing `needle` arrives; return it or None."""
    # Lines received after the match are kept for the next call.
    pending = getattr(client, "pending", [])
    client.pending = []
    deadline = time.time() + timeout
    while True:
        while pending:
            line = pending.pop(0)
            if needle in line:
                client.pending = pending
                return line
        if time.time() >= deadline:
            return None
        lines = client.receive(timeout=0.5)
        if lines is None:
            return None
        pending = list(lines)


def drain(client, duration=0.3):
    client.pending = []
    deadline = time.time() + duration
    while time.time() < deadline:
        if client.receive(timeout=0.1) is None:
            return


def test_protocol_extensions():
    print("\n=== Test 8: RFC 1459/2812 protocol details ===")

    # 1. Final parameter without a leading colon (registration + PRIVMSG + TOPIC)
    alice = IRCClient("ext_alice")
    if not alice.connect():
        fail("✗ Setup failed (ext_alice)")
        return
    alice.send(f"PASS {PASSWORD}")
    alice.send("NICK ext_alice")
    alice.send("USER ext_alice 0 * Alice")
    if wait_for(alice, " 001 "):
        print("✓ USER accepted with colon-less realname")
    else:
        fail("✗ USER without colon-prefixed realname was rejected")
        alice.close()
        return

    bob = IRCClient("ext_bob")
    if not bob.connect() or not bob.register():
        fail("✗ Setup failed (ext_bob)")
        alice.close()
        return

    alice.send("PRIVMSG ext_bob hello")
    line = wait_for(bob, "PRIVMSG ext_bob")
    if line and line.endswith(":hello"):
        print("✓ PRIVMSG delivered with colon-less text")
    else:
        fail(f"✗ PRIVMSG without colon not delivered correctly: {line!r}")

    # 2. JOIN with comma-separated channels
    alice.send("JOIN #ext_a,#ext_b")
    got_a = wait_for(alice, "JOIN #ext_a")
    got_b = wait_for(alice, "JOIN #ext_b")
    if got_a and got_b:
        print("✓ JOIN #a,#b joined both channels")
    else:
        fail(f"✗ Comma-separated JOIN failed: a={got_a!r} b={got_b!r}")
    drain(alice)

    alice.send("TOPIC #ext_a newtopic")
    line = wait_for(alice, "TOPIC #ext_a")
    if line and line.endswith(":newtopic"):
        print("✓ TOPIC set with colon-less topic")
    else:
        fail(f"✗ TOPIC without colon not applied: {line!r}")

    # Keys are matched positionally: #ext_k1 gets key1, #ext_k2 gets key2.
    alice.send("JOIN #ext_k1,#ext_k2")
    wait_for(alice, "JOIN #ext_k2")
    alice.send("MODE #ext_k1 +k key1")
    alice.send("MODE #ext_k2 +k key2")
    wait_for(alice, "MODE #ext_k2")
    drain(alice)
    bob.send("JOIN #ext_k1,#ext_k2 key1,key2")
    got_k1 = wait_for(bob, "JOIN #ext_k1")
    got_k2 = wait_for(bob, "JOIN #ext_k2")
    if got_k1 and got_k2:
        print("✓ JOIN with comma-separated keys")
    else:
        fail(f"✗ JOIN with comma-separated keys failed: {got_k1!r} {got_k2!r}")
    drain(bob)

    # 3. Case-insensitive nickname collision
    dup = IRCClient("EXT_ALICE")
    if dup.connect():
        dup.send(f"PASS {PASSWORD}")
        dup.send("NICK EXT_ALICE")
        if wait_for(dup, " 433 "):
            print("✓ NICK collision detected case-insensitively")
        else:
            fail("✗ NICK EXT_ALICE accepted while ext_alice exists")
        dup.close()

    bob.send("PRIVMSG EXT_ALICE :case test")
    line = wait_for(alice, "case test")
    if line:
        print("✓ PRIVMSG target nickname matched case-insensitively")
    else:
        fail("✗ PRIVMSG to differently-cased nickname not delivered")

    # 4. Invitation follows a NICK change
    alice.send("JOIN #ext_inv")
    wait_for(alice, "JOIN #ext_inv")
    alice.send("MODE #ext_inv +i")
    wait_for(alice, "MODE #ext_inv")
    carol = IRCClient("ext_carol")
    if not carol.connect() or not carol.register():
        fail("✗ Setup failed (ext_carol)")
    else:
        alice.send("INVITE ext_carol #ext_inv")
        wait_for(carol, "INVITE")
        carol.send("NICK ext_carol2")
        wait_for(carol, "NICK")
        carol.send("JOIN #ext_inv")
        if wait_for(carol, "JOIN #ext_inv"):
            print("✓ Invitation preserved across NICK change")
        else:
            fail("✗ Invited user could not join after NICK change")

        # The old nickname must no longer carry the invitation.
        dave = IRCClient("ext_carol")
        if dave.connect() and dave.register():
            dave.send("JOIN #ext_inv")
            if wait_for(dave, " 473 "):
                print("✓ Old nickname no longer invited")
            else:
                fail("✗ New user with the old nickname inherited the invite")
        dave.close()
    carol.close()

    alice.close()
    bob.close()


if __name__ == "__main__":
    try:
        test_basic_connection()
        test_channel_operations()
        test_messaging()
        test_partial_commands()
        test_operator_commands()
        test_flood_stability()
        test_slow_reader_flood()
        test_protocol_extensions()
    except KeyboardInterrupt:
        print("\nTests interrupted.")
        sys.exit(130)
    except Exception as e:
        fail(f"\nTests failed with error: {e}")

    if FAILURES:
        print(f"\n{len(FAILURES)} check(s) failed.")
        sys.exit(1)
    print("\nAll tests passed.")
