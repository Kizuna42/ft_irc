# データフロー詳細

このドキュメントでは、ft_ircシステム全体のデータフローを詳細に説明します。

## 目次

1. [クライアント接続から切断までの完全なフロー](#クライアント接続から切断までの完全なフロー)
2. [メッセージ受信から処理までの流れ](#メッセージ受信から処理までの流れ)
3. [送信バッファからクライアントへの送信](#送信バッファからクライアントへの送信)
4. [チャンネル内のメッセージブロードキャスト](#チャンネル内のメッセージブロードキャスト)
5. [実際の通信例](#実際の通信例)

---

## クライアント接続から切断までの完全なフロー

```mermaid
sequenceDiagram
    participant C as Client
    participant S as Server
    participant P as poll()
    participant CH as CommandHandler
    participant Chan as Channel
    
    Note over C,S: 1. 接続確立
    C->>S: TCP接続要求
    S->>S: accept()
    S->>S: setNonBlocking(clientFd)
    S->>S: new Client(clientFd)
    S->>S: _pollFds.push_back(pfd)
    
    Note over C,S: 2. 認証フロー
    C->>S: PASS secretpassword
    S->>P: POLLIN on clientFd
    P->>S: データ受信可能
    S->>S: recv() & appendRecvBuffer()
    S->>CH: execute(client, msg)
    CH->>CH: handlePass()
    CH->>S: sendToClient(応答)
    
    C->>S: NICK alice
    S->>CH: execute(client, msg)
    CH->>CH: handleNick()
    
    C->>S: USER alice 0 * :Alice
    S->>CH: execute(client, msg)
    CH->>CH: handleUser()
    CH->>S: sendToClient(RPL_WELCOME)
    S->>C: 001 Welcome message
    
    Note over C,S: 3. チャンネル参加
    C->>S: JOIN #general
    S->>CH: execute(client, msg)
    CH->>Chan: createChannel("#general")
    CH->>Chan: addMember(client)
    CH->>Chan: addOperator(client)
    Chan->>Chan: broadcast(JOIN)
    S->>C: JOIN, TOPIC, NAMES
    
    Note over C,S: 4. メッセージ送信
    C->>S: PRIVMSG #general :Hello!
    S->>CH: execute(client, msg)
    CH->>Chan: broadcast(PRIVMSG, exclude=client)
    Chan->>S: appendSendBuffer(他のメンバー)
    S->>C: PRIVMSG配信
    
    Note over C,S: 5. 切断
    C->>S: QUIT :Leaving
    S->>CH: execute(client, msg)
    CH->>S: disconnectClient(client)
    S->>Chan: removeMember(client)
    Chan->>Chan: broadcast(QUIT)
    S->>S: close(clientFd)
    S->>S: delete client
```

---

## メッセージ受信から処理までの流れ

### 詳細フローチャート

```mermaid
flowchart TD
    A[poll POLLIN イベント] --> B[Server::handleClientData]
    B --> C[recv データ受信]
    C --> D{recv結果}
    D -->|0| E[接続切断]
    D -->|< 0| F[エラー]
    D -->|> 0| G[Client::appendRecvBuffer]
    
    E --> H[closeConnection]
    F --> H
    
    G --> I{hasCompleteMessage?}
    I -->|No| J[次のpoll待ち]
    I -->|Yes| K[Client::extractMessage]
    K --> L[Message::parse]
    L --> M[CommandHandler::execute]
    M --> N{コマンド種別}
    
    N -->|認証| O[handlePass/Nick/User]
    N -->|チャンネル| P[handleJoin/Part]
    N -->|メッセージ| Q[handlePrivmsg/Notice]
    N -->|オペレーター| R[handleKick/Invite/Topic/Mode]
    
    O --> S[応答生成]
    P --> S
    Q --> S
    R --> S
    
    S --> T[Client::appendSendBuffer]
    T --> I
```

### ステップ詳細

#### 1. poll()イベント検出

```cpp
// Server::start()
int pollCount = poll(&_pollFds[0], _pollFds.size(), -1);

for (size_t i = 0; i < _pollFds.size(); ++i) {
    if (_pollFds[i].revents & POLLIN)
        handleClientData(_pollFds[i].fd);
}
```

#### 2. データ受信

```cpp
// Server::handleClientData()
char buffer[512];
ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
client->appendRecvBuffer(std::string(buffer, bytes));
```

#### 3. 完全なメッセージのチェック

```cpp
// Client::hasCompleteMessage()
return _recvBuffer.find("\r\n") != std::string::npos;
```

#### 4. メッセージの抽出

```cpp
// Client::extractMessage()
size_t pos = _recvBuffer.find("\r\n");
std::string message = _recvBuffer.substr(0, pos);
_recvBuffer.erase(0, pos + 2);
return message;
```

#### 5. メッセージのパース

```cpp
// Message::parse()
// Prefix, Command, Params, Trailingを抽出
```

#### 6. コマンドの実行

```cpp
// CommandHandler::execute()
// コマンドに応じた処理を実行
```

---

## 送信バッファからクライアントへの送信

### 送信フロー

```mermaid
flowchart TD
    A[応答生成] --> B[Client::appendSendBuffer]
    B --> C[送信バッファに追加]
    C --> D[次のpoll待ち]
    D --> E[poll POLLOUT イベント]
    E --> F[Server::handleClientSend]
    F --> G{sendBuffer空?}
    G -->|Yes| H[何もしない]
    G -->|No| I[send データ送信]
    I --> J{send結果}
    J -->|> 0| K[Client::clearSendBuffer]
    J -->|EAGAIN| L[次のPOLLOUT待ち]
    J -->|エラー| M[closeConnection]
    K --> N{バッファ空?}
    N -->|Yes| H
    N -->|No| L
```

### 実装詳細

#### 1. 送信バッファへの追加

```cpp
void Server::sendToClient(Client* client, const std::string& message) {
    if (client)
        client->appendSendBuffer(message);
}

void Client::appendSendBuffer(const std::string& data) {
    _sendBuffer += data;
}
```

#### 2. 送信処理

```cpp
void Server::handleClientSend(int fd) {
    Client* client = getClient(fd);
    if (!client || client->getSendBuffer().empty())
        return;

    const std::string& buffer = client->getSendBuffer();
    ssize_t bytes = send(fd, buffer.c_str(), buffer.size(), 0);

    if (bytes > 0) {
        client->clearSendBuffer(bytes);
    } else if (bytes < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            closeConnection(fd, "Send error");
        }
    }
}
```

---

## チャンネル内のメッセージブロードキャスト

### ブロードキャストフロー

```mermaid
sequenceDiagram
    participant C1 as Client1 (送信者)
    participant S as Server
    participant CH as CommandHandler
    participant Chan as Channel
    participant C2 as Client2
    participant C3 as Client3
    
    C1->>S: PRIVMSG #general :Hello!
    S->>CH: execute(client1, msg)
    CH->>Chan: broadcast(message, exclude=client1)
    
    loop 各メンバー
        Chan->>Chan: メンバーをチェック
        alt メンバー != 送信者
            Chan->>C2: appendSendBuffer(message)
            Chan->>C3: appendSendBuffer(message)
        end
    end
    
    Note over S,C2: 次のPOLLOUT時
    S->>C2: send(message)
    S->>C3: send(message)
```

### 実装詳細

```cpp
void Channel::broadcast(const std::string& message, Client* exclude) {
    for (std::vector<Client*>::iterator it = _members.begin(); 
         it != _members.end(); ++it) {
        if (*it != exclude)
            (*it)->appendSendBuffer(message);
    }
}
```

---

## 実際の通信例

### 例1: 認証フロー

```
クライアント → サーバー: PASS secretpassword\r\n
サーバー → クライアント: (応答なし、内部で認証状態更新)

クライアント → サーバー: NICK alice\r\n
サーバー → クライアント: (応答なし、内部で認証状態更新)

クライアント → サーバー: USER alice 0 * :Alice Smith\r\n
サーバー → クライアント: :localhost 001 alice :Welcome to the Internet Relay Network alice!alice@192.168.1.100\r\n
```

### 例2: チャンネル参加

```
クライアント → サーバー: JOIN #general\r\n

サーバー → クライアント: :alice!alice@192.168.1.100 JOIN #general\r\n
サーバー → クライアント: :localhost 331 alice #general :No topic is set\r\n
サーバー → クライアント: :localhost 353 alice = #general :@alice\r\n
サーバー → クライアント: :localhost 366 alice #general :End of NAMES list\r\n
```

### 例3: メッセージ送信

```
Alice → サーバー: PRIVMSG #general :Hello, everyone!\r\n

サーバー → Bob: :alice!alice@192.168.1.100 PRIVMSG #general :Hello, everyone!\r\n
サーバー → Charlie: :alice!alice@192.168.1.100 PRIVMSG #general :Hello, everyone!\r\n
(Aliceには送信されない)
```

### 例4: オペレーターコマンド

```
Alice → サーバー: KICK #general bob :Spamming\r\n

サーバー → Alice: :alice!alice@192.168.1.100 KICK #general bob :Spamming\r\n
サーバー → Bob: :alice!alice@192.168.1.100 KICK #general bob :Spamming\r\n
サーバー → Charlie: :alice!alice@192.168.1.100 KICK #general bob :Spamming\r\n
(すべてのメンバーに送信)
```

---

## データフローのタイミング図

### 部分受信の例

```
時刻 t0: recv() → "PRIVMSG #ge"
         _recvBuffer = "PRIVMSG #ge"
         hasCompleteMessage() = false
         → 処理せずに待機

時刻 t1: recv() → "neral :Hello\r\n"
         _recvBuffer = "PRIVMSG #general :Hello\r\n"
         hasCompleteMessage() = true
         extractMessage() → "PRIVMSG #general :Hello"
         _recvBuffer = ""
         → コマンド処理実行
```

### 部分送信の例

```
時刻 t0: appendSendBuffer(":server 001 alice :Welcome...\r\n")  // 50バイト
         _sendBuffer = ":server 001 alice :Welcome...\r\n"

時刻 t1: POLLOUT イベント
         send() → 30バイト送信成功
         clearSendBuffer(30)
         _sendBuffer = "ome...\r\n"  // 残り20バイト

時刻 t2: POLLOUT イベント
         send() → 20バイト送信成功
         clearSendBuffer(20)
         _sendBuffer = ""  // 完了
```

---

## まとめ

### データフローの特徴

✅ **非ブロッキング**: すべてのI/O操作は非ブロッキング
✅ **イベント駆動**: poll()でイベントを検出して処理
✅ **バッファリング**: 部分送受信に対応
✅ **ブロードキャスト**: チャンネル内のメッセージ配信

### 重要なポイント

📌 **poll()は1つ**: すべてのfdを1つのpoll()で監視
📌 **部分送受信**: バッファを使用して対応
📌 **送信者除外**: ブロードキャスト時は送信者を除外
📌 **即座に送信しない**: 送信バッファに追加し、POLLOUTで送信

### 次のステップ

- [10_ERROR_HANDLING.md](10_ERROR_HANDLING.md) - エラーハンドリング
- [11_TESTING.md](11_TESTING.md) - テスト方法

---

**前のドキュメント**: [08_COMMANDS_DETAIL.md](08_COMMANDS_DETAIL.md)
**次のドキュメント**: [10_ERROR_HANDLING.md](10_ERROR_HANDLING.md)

