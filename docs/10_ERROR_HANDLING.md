# エラーハンドリング詳細

このドキュメントでは、ft_ircプロジェクトにおけるエラーハンドリング戦略を説明します。

## 目次

1. [システムコールエラーの処理](#システムコールエラーの処理)
2. [メモリ不足時の対応](#メモリ不足時の対応)
3. [クライアント切断時のクリーンアップ](#クライアント切断時のクリーンアップ)
4. [部分送受信の処理](#部分送受信の処理)
5. [デバッグ方法とログ出力](#デバッグ方法とログ出力)

---

## システムコールエラーの処理

### socket()

```cpp
int fd = socket(AF_INET, SOCK_STREAM, 0);
if (fd < 0) {
    throw std::runtime_error("Error: socket creation failed");
}
```

**エラー時の対応**: 例外をスローしてサーバー起動を中止

---

### bind()

```cpp
if (bind(_serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(_serverFd);
    throw std::runtime_error("Error: bind failed");
}
```

**エラー時の対応**: ソケットをクローズして例外をスロー

**よくある原因**:
- ポートが既に使用されている
- 権限不足（1024以下のポート）

---

### listen()

```cpp
if (listen(_serverFd, SOMAXCONN) < 0) {
    close(_serverFd);
    throw std::runtime_error("Error: listen failed");
}
```

**エラー時の対応**: ソケットをクローズして例外をスロー

---

### accept()

```cpp
int clientFd = accept(_serverFd, (struct sockaddr*)&clientAddr, &clientLen);
if (clientFd < 0) {
    std::cerr << "Error: accept failed: " << strerror(errno) << std::endl;
    return;  // 処理を続行
}
```

**エラー時の対応**: エラーログを出力して処理を続行

**重要**: accept()の失敗は致命的ではないため、サーバーは継続

---

### recv()

```cpp
ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

if (bytes <= 0) {
    if (bytes == 0) {
        // 接続が正常に閉じられた
        std::cout << "Client disconnected (fd: " << fd << ")" << std::endl;
        closeConnection(fd, "Connection closed");
    } else {
        // エラー
        std::cerr << "Error: recv failed: " << strerror(errno) << std::endl;
        closeConnection(fd, "Read error");
    }
    return;
}
```

**エラーケース**:
- `bytes == 0`: 接続が閉じられた（正常）
- `bytes < 0`: エラー発生

---

### send()

```cpp
ssize_t bytes = send(fd, buffer.c_str(), buffer.size(), 0);

if (bytes < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        // 実際のエラー
        std::cerr << "Error: send failed: " << strerror(errno) << std::endl;
        closeConnection(fd, "Send error");
    }
    return;  // EAGAIN/EWOULDBLOCKの場合は次のPOLLOUTで再試行
}
```

**エラーケース**:
- `EAGAIN`/`EWOULDBLOCK`: 送信バッファが満杯（エラーではない）
- その他: 実際のエラー

---

### poll()

```cpp
int pollCount = poll(&_pollFds[0], _pollFds.size(), -1);

if (pollCount < 0) {
    if (errno == EINTR)
        continue;  // シグナルで中断された場合は再試行
    std::cerr << "Error: poll failed: " << strerror(errno) << std::endl;
    break;  // ループを抜ける
}
```

**エラーケース**:
- `EINTR`: シグナルで中断された（再試行）
- その他: 致命的エラー（サーバー停止）

---

## メモリ不足時の対応

### new演算子

```cpp
try {
    Client* client = new Client(clientFd);
    _clients[clientFd] = client;
} catch (const std::exception& e) {
    std::cerr << "Error: Failed to create client (memory allocation failed): " 
              << e.what() << std::endl;
    close(clientFd);  // fdをクローズ
}
```

**エラー時の対応**:
1. エラーログを出力
2. ファイルディスクリプタをクローズ
3. 処理を続行（サーバーは停止しない）

---

### STLコンテナ

```cpp
try {
    _pollFds.push_back(pfd);
} catch (const std::bad_alloc& e) {
    std::cerr << "Error: Failed to add to poll vector: " << e.what() << std::endl;
    close(clientFd);
    delete client;
}
```

**エラー時の対応**:
1. エラーログを出力
2. 確保したリソースを解放
3. 処理を続行

---

## クライアント切断時のクリーンアップ

### removeClient() - 完全なクリーンアップ

```cpp
void Server::removeClient(int fd, const std::string& reason) {
    Client* client = getClient(fd);
    if (!client)
        return;

    // 1. QUIT通知をブロードキャスト
    std::string quitReason = reason.empty() ? "Client disconnected" : reason;
    std::string quitMsg = ":" + client->getPrefix() + " QUIT :" + quitReason + "\r\n";
    broadcastToClientChannels(client, quitMsg, client);

    // 2. すべてのチャンネルから削除
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); 
         it != _channels.end(); ) {
        Channel* channel = it->second;
        if (channel->isMember(client)) {
            channel->removeMember(client);
        }

        // 3. 空のチャンネルを削除
        if (channel->getMemberCount() == 0) {
            delete channel;
            _channels.erase(it++);
        } else {
            ++it;
        }
    }

    // 4. クライアントリストから削除
    _clients.erase(fd);
    delete client;

    // 5. poll配列から削除
    for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); 
         it != _pollFds.end(); ++it) {
        if (it->fd == fd) {
            _pollFds.erase(it);
            break;
        }
    }
}
```

**クリーンアップの順序**:
1. QUIT通知のブロードキャスト
2. チャンネルから削除
3. 空チャンネルの削除
4. クライアントオブジェクトの削除
5. poll配列から削除

---

## 部分送受信の処理

### 部分受信

```cpp
void Server::handleClientData(int fd) {
    Client* client = getClient(fd);
    if (!client)
        return;

    char buffer[512];
    ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) {
        closeConnection(fd);
        return;
    }

    // バッファに追加
    buffer[bytes] = '\0';
    client->appendRecvBuffer(std::string(buffer, bytes));

    // 完全なメッセージを処理
    while (true) {
        Client* current = getClient(fd);
        if (!current || !current->hasCompleteMessage())
            break;

        std::string message = current->extractMessage();
        if (!message.empty())
            processClientMessage(current, message);
    }
}
```

**重要なポイント**:
- whileループで複数メッセージを処理
- 各ループでクライアントの存在をチェック（切断される可能性）

---

### 部分送信

```cpp
void Server::handleClientSend(int fd) {
    Client* client = getClient(fd);
    if (!client || client->getSendBuffer().empty())
        return;

    const std::string& buffer = client->getSendBuffer();
    ssize_t bytes = send(fd, buffer.c_str(), buffer.size(), 0);

    if (bytes < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            closeConnection(fd, "Send error");
        }
        return;
    }

    // 送信できた分だけバッファから削除
    client->clearSendBuffer(bytes);
}
```

**重要なポイント**:
- send()は要求より少ないバイト数しか送信できないことがある
- 送信できた分だけバッファから削除
- 残りは次のPOLLOUTで送信

---

## デバッグ方法とログ出力

### ログレベル

```cpp
// エラーログ（常に出力）
std::cerr << "Error: " << message << std::endl;

// 情報ログ（デバッグ時のみ）
#ifdef DEBUG
std::cout << "Debug: " << message << std::endl;
#endif
```

---

### 有用なログ出力

#### 接続・切断

```cpp
// 新規接続
std::cout << "New connection from " << inet_ntoa(clientAddr.sin_addr) << std::endl;

// 切断
std::cout << "Client disconnected (fd: " << fd << ")" << std::endl;
```

#### コマンド処理

```cpp
#ifdef DEBUG
std::cout << "Command: " << msg.getCommand() 
          << " from " << client->getNickname() << std::endl;
#endif
```

#### エラー

```cpp
std::cerr << "Error: recv failed: " << strerror(errno) << std::endl;
std::cerr << "Error: Failed to create client: " << e.what() << std::endl;
```

---

### デバッグツール

#### valgrind - メモリリークチェック

```bash
valgrind --leak-check=full --show-leak-kinds=all ./ircserv 6667 password
```

**チェック項目**:
- メモリリーク
- 無効なメモリアクセス
- 初期化されていない値の使用

#### gdb - デバッガ

```bash
gdb ./ircserv
(gdb) run 6667 password
(gdb) break Server::handleClientData
(gdb) continue
(gdb) print client->_recvBuffer
```

---

### エラーハンドリングのベストプラクティス

#### 1. リソースリークを防ぐ

```cpp
// ✅ 正しい
Client* client = new Client(clientFd);
try {
    _clients[clientFd] = client;
    _pollFds.push_back(pfd);
} catch (const std::exception& e) {
    delete client;  // 確保したリソースを解放
    close(clientFd);
    throw;
}

// ❌ 間違い
Client* client = new Client(clientFd);
_clients[clientFd] = client;  // 例外が発生したらリーク
_pollFds.push_back(pfd);
```

#### 2. イテレータの無効化を防ぐ

```cpp
// ✅ 正しい
for (std::map<std::string, Channel*>::iterator it = _channels.begin(); 
     it != _channels.end(); ) {
    if (shouldDelete(it->second)) {
        delete it->second;
        _channels.erase(it++);  // 後置インクリメント
    } else {
        ++it;
    }
}

// ❌ 間違い
for (std::map<std::string, Channel*>::iterator it = _channels.begin(); 
     it != _channels.end(); ++it) {
    if (shouldDelete(it->second)) {
        _channels.erase(it);  // イテレータが無効化される
    }
}
```

#### 3. NULL/NULLPTRチェック

```cpp
// ✅ 正しい
Client* client = getClient(fd);
if (!client)
    return;

// ❌ 間違い
Client* client = getClient(fd);
client->appendRecvBuffer(data);  // clientがNULLの可能性
```

---

## まとめ

### エラーハンドリングの原則

✅ **システムコールエラー**: 適切にチェックして処理
✅ **メモリ不足**: 例外をキャッチしてリソースを解放
✅ **クライアント切断**: 完全なクリーンアップを実施
✅ **部分送受信**: バッファを使用して対応
✅ **ログ出力**: エラーと重要なイベントを記録

### 重要なポイント

📌 **リソースリーク防止**: 例外発生時もリソースを解放
📌 **イテレータの安全性**: erase()時は後置インクリメント
📌 **NULLチェック**: ポインタ使用前に必ずチェック
📌 **エラーログ**: デバッグに必要な情報を出力

### 次のステップ

- [11_TESTING.md](11_TESTING.md) - テスト方法
- [12_IMPLEMENTATION_TIPS.md](12_IMPLEMENTATION_TIPS.md) - 実装のヒント

---

**前のドキュメント**: [09_DATA_FLOW.md](09_DATA_FLOW.md)
**次のドキュメント**: [11_TESTING.md](11_TESTING.md)

