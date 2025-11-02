# 実装のヒントとベストプラクティス

このドキュメントでは、ft_ircプロジェクトの実装におけるヒントとベストプラクティスを説明します。

## 目次

1. [C++98での効率的なコーディング](#c98での効率的なコーディング)
2. [poll()の正しい使い方](#pollの正しい使い方)
3. [メモリリーク防止](#メモリリーク防止)
4. [イテレータの安全な使用](#イテレータの安全な使用)
5. [const correctness](#const-correctness)
6. [よくある落とし穴と回避方法](#よくある落とし穴と回避方法)

---

## C++98での効率的なコーディング

### STLコンテナの選択

#### std::vector - 順序付きリスト

```cpp
// ✅ 良い使い方
std::vector<Client*> members;  // 順序が重要な場合
members.push_back(client);     // O(1) amortized
```

**用途**: メンバーリスト、poll配列

---

#### std::map - キーと値のマッピング

```cpp
// ✅ 良い使い方
std::map<int, Client*> clients;  // fdからClientへのマッピング
clients[fd] = client;            // O(log n)
Client* c = clients[fd];         // O(log n)
```

**用途**: fd→Client、name→Channel

---

#### std::set - 一意な要素の集合

```cpp
// ✅ 良い使い方
std::set<Client*> operators;     // オペレーターリスト
operators.insert(client);        // O(log n)
bool isOp = operators.find(client) != operators.end();  // O(log n)
```

**用途**: オペレーターリスト、招待リスト

---

### 参照渡しの活用

```cpp
// ❌ 値渡し（コピーが発生）
void processMessage(std::string message) {
    // ...
}

// ✅ const参照渡し（コピーなし）
void processMessage(const std::string& message) {
    // ...
}
```

**ルール**: 大きなオブジェクト（string, vector, map等）は const参照で渡す

---

### 文字列操作の効率化

```cpp
// ❌ 非効率
std::string result = str1 + str2 + str3 + str4;  // 複数回のコピー

// ✅ 効率的
std::ostringstream oss;
oss << str1 << str2 << str3 << str4;
std::string result = oss.str();
```

---

### イテレータの活用

```cpp
// ✅ イテレータを使用
for (std::vector<Client*>::iterator it = members.begin(); 
     it != members.end(); ++it) {
    (*it)->sendMessage(msg);
}

// ❌ C++11のrange-based for（使用不可）
for (Client* client : members) {  // C++98では使えない
    client->sendMessage(msg);
}
```

---

## poll()の正しい使い方

### ✅ 正しいパターン

```cpp
// 1つのpoll()ですべてのfdを監視
std::vector<struct pollfd> _pollFds;

// サーバーソケットを追加
struct pollfd pfd;
pfd.fd = _serverFd;
pfd.events = POLLIN;
pfd.revents = 0;
_pollFds.push_back(pfd);

// メインループ
while (_running) {
    int result = poll(&_pollFds[0], _pollFds.size(), -1);
    
    for (size_t i = 0; i < _pollFds.size(); ++i) {
        if (_pollFds[i].revents & POLLIN)
            handleRead(_pollFds[i].fd);
        if (_pollFds[i].revents & POLLOUT)
            handleWrite(_pollFds[i].fd);
    }
}
```

---

### ❌ 間違ったパターン

```cpp
// 複数のpoll()を使用（禁止）
poll(&read_fds[0], read_fds.size(), -1);
poll(&write_fds[0], write_fds.size(), -1);  // ❌

// EAGAINでの再試行ループ（禁止）
while (recv(fd, buffer, size, 0) == -1 && errno == EAGAIN) {
    // ❌ poll()なしで再試行
}
```

---

### poll()のイベント処理

```cpp
// ✅ 正しい順序
if (_pollFds[i].revents & POLLIN)
    handleClientData(_pollFds[i].fd);
if (_pollFds[i].revents & POLLOUT)
    handleClientSend(_pollFds[i].fd);
if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
    closeConnection(_pollFds[i].fd);
```

**重要**: POLLINとPOLLOUTは両方とも発生する可能性がある

---

## メモリリーク防止

### RAII（Resource Acquisition Is Initialization）

```cpp
// ✅ デストラクタでリソースを解放
class Server {
public:
    ~Server() {
        stop();  // すべてのリソースを解放
        delete _commandHandler;
    }
};
```

---

### 例外安全性

```cpp
// ✅ 例外が発生してもリソースを解放
Client* client = NULL;
try {
    client = new Client(clientFd);
    _clients[clientFd] = client;
    _pollFds.push_back(pfd);
} catch (const std::exception& e) {
    delete client;  // 確保したリソースを解放
    close(clientFd);
    std::cerr << "Error: " << e.what() << std::endl;
}
```

---

### コンテナ内のポインタ

```cpp
// ✅ コンテナをクリアする前にdeleteする
for (std::map<int, Client*>::iterator it = _clients.begin(); 
     it != _clients.end(); ++it) {
    delete it->second;  // Clientオブジェクトを削除
}
_clients.clear();  // mapをクリア
```

---

### valgrindでの確認

```bash
valgrind --leak-check=full --show-leak-kinds=all ./ircserv 6667 password
```

**確認項目**:
- "definitely lost": 確実にリーク
- "indirectly lost": 間接的にリーク
- "still reachable": プログラム終了時に到達可能（通常は問題なし）

---

## イテレータの安全な使用

### erase()時の注意

```cpp
// ✅ 正しい: 後置インクリメント
for (std::map<std::string, Channel*>::iterator it = _channels.begin(); 
     it != _channels.end(); ) {
    if (channel->getMemberCount() == 0) {
        delete it->second;
        _channels.erase(it++);  // 後置インクリメント
    } else {
        ++it;
    }
}

// ❌ 間違い: イテレータが無効化される
for (std::map<std::string, Channel*>::iterator it = _channels.begin(); 
     it != _channels.end(); ++it) {
    if (channel->getMemberCount() == 0) {
        _channels.erase(it);  // イテレータが無効化される
        // ここで++itするとクラッシュ
    }
}
```

---

### ループ中のコンテナ変更

```cpp
// ✅ 正しい: 変更を別のコンテナに記録
std::vector<int> toRemove;
for (size_t i = 0; i < _pollFds.size(); ++i) {
    if (shouldRemove(_pollFds[i].fd))
        toRemove.push_back(i);
}

// 逆順で削除（インデックスがずれない）
for (int i = toRemove.size() - 1; i >= 0; --i) {
    _pollFds.erase(_pollFds.begin() + toRemove[i]);
}
```

---

## const correctness

### const メンバー関数

```cpp
class Client {
public:
    // ✅ 状態を変更しない関数はconstを付ける
    int getFd() const { return _fd; }
    const std::string& getNickname() const { return _nickname; }
    bool isAuthenticated() const { return _isAuthenticated; }
    
    // ✅ 状態を変更する関数はconstを付けない
    void setNickname(const std::string& nickname) { _nickname = nickname; }
    void appendRecvBuffer(const std::string& data) { _recvBuffer += data; }
};
```

---

### const 参照パラメータ

```cpp
// ✅ 読み取り専用パラメータはconst参照
void processMessage(const std::string& message) {
    // messageは変更できない
}

// ✅ 変更するパラメータは非const参照
void extractMessage(std::string& buffer) {
    // bufferを変更できる
}
```

---

### const ポインタ

```cpp
// ✅ ポインタが指すオブジェクトを変更しない
void broadcast(const std::string& message, const Client* exclude) {
    // excludeが指すClientオブジェクトは変更できない
}

// ✅ ポインタ自体を変更しない
void setClient(Client* const client) {
    // clientポインタ自体は変更できない（別のClientを指せない）
    // が、clientが指すオブジェクトは変更できる
}
```

---

## よくある落とし穴と回避方法

### 落とし穴1: NULLポインタのデリファレンス

```cpp
// ❌ 危険
Client* client = getClient(fd);
client->appendRecvBuffer(data);  // clientがNULLの可能性

// ✅ 安全
Client* client = getClient(fd);
if (!client)
    return;
client->appendRecvBuffer(data);
```

---

### 落とし穴2: 削除済みオブジェクトへのアクセス

```cpp
// ❌ 危険
Client* client = getClient(fd);
closeConnection(fd);  // clientが削除される
client->getNickname();  // 削除済みオブジェクトへのアクセス

// ✅ 安全
Client* client = getClient(fd);
std::string nickname = client->getNickname();
closeConnection(fd);
// nicknameはコピーなので安全に使用できる
```

---

### 落とし穴3: イテレータの無効化

```cpp
// ❌ 危険
while (client->hasCompleteMessage()) {
    std::string message = client->extractMessage();
    processClientMessage(client, message);
    // processClientMessage内でclientが削除される可能性
    // 次のループでclient->hasCompleteMessage()を呼ぶとクラッシュ
}

// ✅ 安全
while (true) {
    Client* current = getClient(fd);
    if (!current || !current->hasCompleteMessage())
        break;
    std::string message = current->extractMessage();
    processClientMessage(current, message);
}
```

---

### 落とし穴4: string::erase()の戻り値

```cpp
// ❌ 間違い
std::string str = "Hello";
str.erase(0, 2);  // "llo"
std::string result = str.erase(0, 1);  // エラー: erase()はvoidを返す

// ✅ 正しい
std::string str = "Hello";
str.erase(0, 2);  // "llo"
str.erase(0, 1);  // "lo"
std::string result = str;  // "lo"
```

---

### 落とし穴5: vector::erase()とインデックス

```cpp
// ❌ 危険: インデックスがずれる
for (size_t i = 0; i < vec.size(); ++i) {
    if (shouldRemove(vec[i])) {
        vec.erase(vec.begin() + i);
        // 次の要素がi番目に移動するが、++iで飛ばしてしまう
    }
}

// ✅ 安全: 逆順で削除
for (int i = vec.size() - 1; i >= 0; --i) {
    if (shouldRemove(vec[i])) {
        vec.erase(vec.begin() + i);
    }
}
```

---

## コーディングスタイル

### 命名規則

```cpp
// クラス名: PascalCase
class Server { };
class CommandHandler { };

// メンバー変数: _プレフィックス + camelCase
class Client {
private:
    int _fd;
    std::string _nickname;
    bool _isAuthenticated;
};

// 関数名: camelCase
void handleClientData(int fd);
bool isValidNickname(const std::string& nickname);

// 定数: UPPER_CASE
const int MAX_CLIENTS = 100;
const int BUFFER_SIZE = 512;
```

---

### インデント

```cpp
// タブを使用
if (condition) {
	// インデント
	doSomething();
}
```

---

### ブレース

```cpp
// ✅ 推奨: K&Rスタイル
if (condition) {
    doSomething();
} else {
    doSomethingElse();
}

// ✅ 許容: Allmanスタイル
if (condition)
{
    doSomething();
}
else
{
    doSomethingElse();
}
```

---

## まとめ

### ベストプラクティス

✅ **STLの活用**: 適切なコンテナを選択
✅ **const correctness**: constを積極的に使用
✅ **参照渡し**: 大きなオブジェクトはconst参照で渡す
✅ **RAII**: リソースはデストラクタで解放
✅ **例外安全性**: 例外発生時もリソースを解放

### 避けるべきパターン

❌ **NULLチェック忘れ**: ポインタ使用前に必ずチェック
❌ **イテレータの無効化**: erase()時は後置インクリメント
❌ **メモリリーク**: newしたら必ずdelete
❌ **複数のpoll()**: poll()は1つだけ使用
❌ **C++11以降の機能**: auto, nullptr, lambda等は使用不可

### 開発のヒント

💡 **段階的実装**: 小さな機能から実装してテスト
💡 **頻繁なテスト**: 変更後は必ずテスト
💡 **valgrindの使用**: メモリリークを早期発見
💡 **コードレビュー**: 他の人にコードを見てもらう

---

**前のドキュメント**: [11_TESTING.md](11_TESTING.md)
**トップに戻る**: [00_INDEX.md](00_INDEX.md)

