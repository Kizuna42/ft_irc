# テスト方法詳細

このドキュメントでは、ft_ircサーバーの実践的なテスト方法を説明します。

## 目次

1. [ncを使った基本テスト](#ncを使った基本テスト)
2. [部分コマンド送信テスト](#部分コマンド送信テスト)
3. [irssiでの実践的テスト](#irssiでの実践的テスト)
4. [複数クライアント同時接続テスト](#複数クライアント同時接続テスト)
5. [テストシナリオ集](#テストシナリオ集)
6. [よくある問題と解決方法](#よくある問題と解決方法)

---

## ncを使った基本テスト

### サーバーの起動

```bash
./ircserv 6667 password
```

### ncで接続

```bash
nc localhost 6667
```

### 基本的な認証フロー

```
PASS password
NICK alice
USER alice 0 * :Alice Smith
```

**期待される応答:**
```
:localhost 001 alice :Welcome to the Internet Relay Network alice!alice@127.0.0.1
```

### チャンネル参加

```
JOIN #test
```

**期待される応答:**
```
:alice!alice@127.0.0.1 JOIN #test
:localhost 331 alice #test :No topic is set
:localhost 353 alice = #test :@alice
:localhost 366 alice #test :End of NAMES list
```

### メッセージ送信

```
PRIVMSG #test :Hello, world!
```

（自分には返ってこない - 正常）

### 切断

```
QUIT :Leaving
```

---

## 部分コマンド送信テスト

### ncの-Cオプション

```bash
nc -C localhost 6667
```

`-C`オプションは`\r\n`を自動的に送信します。

### Ctrl+Dで部分送信

```bash
nc localhost 6667
PASS^D
pass^D
word^D
<Enter>
```

`^D`は`Ctrl+D`を押すことを意味します。

**動作:**
1. "PASS"を送信（改行なし）
2. "pass"を追加送信
3. "word"を追加送信
4. Enterで`\r\n`を送信
5. サーバーは"PASSpassword"として受信

### テストケース

#### ケース1: コマンドの分割

```
PRI^D
VMSG^D
 #test^D
 :Hello^D
<Enter>
```

サーバーは"PRIVMSG #test :Hello"として処理するべき

#### ケース2: 複数コマンドの同時送信

```
NICK alice<Enter>USER alice 0 * :Alice<Enter>
```

サーバーは2つのコマンドを個別に処理するべき

---

## irssiでの実践的テスト

### irssiのインストール

```bash
# macOS
brew install irssi

# Linux
sudo apt-get install irssi
```

### irssiで接続

```bash
irssi
```

irssi内で:
```
/connect localhost 6667 password
```

### 基本操作

#### ニックネーム変更

```
/nick alice
```

#### チャンネル参加

```
/join #general
```

#### メッセージ送信

```
Hello, everyone!
```

#### プライベートメッセージ

```
/msg bob Hi there!
```

#### チャンネル退出

```
/part #general Goodbye!
```

#### 切断

```
/quit Leaving
```

---

## 複数クライアント同時接続テスト

### ターミナルを複数開く

**ターミナル1 (Alice):**
```bash
nc localhost 6667
PASS password
NICK alice
USER alice 0 * :Alice
JOIN #test
```

**ターミナル2 (Bob):**
```bash
nc localhost 6667
PASS password
NICK bob
USER bob 0 * :Bob
JOIN #test
```

### メッセージのやり取り

**Alice:**
```
PRIVMSG #test :Hello, Bob!
```

**Bob側で表示されるはず:**
```
:alice!alice@127.0.0.1 PRIVMSG #test :Hello, Bob!
```

**Bob:**
```
PRIVMSG #test :Hi, Alice!
```

**Alice側で表示されるはず:**
```
:bob!bob@127.0.0.1 PRIVMSG #test :Hi, Alice!
```

---

## テストシナリオ集

### シナリオ1: 認証エラー

#### 間違ったパスワード

```
PASS wrongpassword
NICK alice
USER alice 0 * :Alice
```

**期待される応答:**
```
:localhost 464 * :Password incorrect
```

#### パスワードなし

```
NICK alice
USER alice 0 * :Alice
JOIN #test
```

**期待される応答:**
```
:localhost 451 * :You have not registered
```

---

### シナリオ2: ニックネーム重複

**クライアント1:**
```
PASS password
NICK alice
USER alice 0 * :Alice
```

**クライアント2:**
```
PASS password
NICK alice
USER alice2 0 * :Alice2
```

**期待される応答（クライアント2）:**
```
:localhost 433 * alice :Nickname is already in use
```

---

### シナリオ3: チャンネルモード

#### 招待制チャンネル

**Alice（オペレーター）:**
```
JOIN #private
MODE #private +i
```

**Bob:**
```
JOIN #private
```

**期待される応答（Bob）:**
```
:localhost 473 bob #private :Cannot join channel (+i)
```

**Alice:**
```
INVITE bob #private
```

**Bob:**
```
JOIN #private
```

（成功するはず）

---

### シナリオ4: オペレーターコマンド

#### KICK

**Alice（オペレーター）:**
```
KICK #test bob :Spamming
```

**期待される動作:**
- Bobが#testから削除される
- すべてのメンバーにKICK通知が送信される

#### 非オペレーターがKICKを試みる

**Bob（非オペレーター）:**
```
KICK #test alice :Test
```

**期待される応答:**
```
:localhost 482 bob #test :You're not channel operator
```

---

### シナリオ5: クライアント切断

#### 正常な切断

```
QUIT :Leaving
```

**期待される動作:**
- すべてのチャンネルにQUIT通知が送信される
- クライアントが削除される
- 空のチャンネルが削除される

#### 強制切断（Ctrl+C）

```
^C
```

**期待される動作:**
- サーバーがrecv()で0を受信
- クライアントが削除される
- すべてのチャンネルにQUIT通知が送信される

---

## よくある問題と解決方法

### 問題1: "Address already in use"

**原因:** ポートが既に使用されている

**解決方法:**
```bash
# 使用中のプロセスを確認
lsof -i :6667

# プロセスを終了
kill -9 <PID>

# または別のポートを使用
./ircserv 6668 password
```

---

### 問題2: メッセージが受信されない

**原因:** 送信バッファに追加されているが、send()が呼ばれていない

**確認方法:**
```cpp
// デバッグログを追加
std::cout << "SendBuffer size: " << client->getSendBuffer().size() << std::endl;
```

**解決方法:**
- POLLOUTイベントが正しく処理されているか確認
- handleClientSend()が呼ばれているか確認

---

### 問題3: 部分受信が正しく処理されない

**原因:** `\r\n`のチェックが正しく行われていない

**テスト方法:**
```bash
nc localhost 6667
PRIVMSG #test :Hello^D
 World^D
<Enter>
```

**期待される動作:**
- "PRIVMSG #test :Hello World"として処理される

---

### 問題4: メモリリーク

**確認方法:**
```bash
valgrind --leak-check=full ./ircserv 6667 password
```

**よくある原因:**
- newしたオブジェクトをdeleteしていない
- チャンネルが削除されていない
- クライアント切断時のクリーンアップ不足

---

### 問題5: セグメンテーションフォルト

**よくある原因:**
- NULLポインタのデリファレンス
- 削除済みオブジェクトへのアクセス
- イテレータの無効化

**デバッグ方法:**
```bash
gdb ./ircserv
(gdb) run 6667 password
# クラッシュしたら
(gdb) backtrace
(gdb) print client
(gdb) print *client
```

---

## テストチェックリスト

### 基本機能

- [ ] サーバーが起動する
- [ ] クライアントが接続できる
- [ ] 認証フローが正しく動作する
- [ ] チャンネルに参加できる
- [ ] メッセージを送受信できる
- [ ] クライアントが切断できる

### エラーハンドリング

- [ ] 間違ったパスワードでエラーが返される
- [ ] ニックネーム重複でエラーが返される
- [ ] 存在しないチャンネルでエラーが返される
- [ ] 権限不足でエラーが返される

### 非ブロッキングI/O

- [ ] 部分受信が正しく処理される
- [ ] 部分送信が正しく処理される
- [ ] 複数のクライアントが同時に動作する

### チャンネル機能

- [ ] チャンネルが自動作成される
- [ ] 最初のユーザーがオペレーターになる
- [ ] チャンネルモードが正しく動作する
- [ ] 空のチャンネルが削除される

### オペレーターコマンド

- [ ] KICKが正しく動作する
- [ ] INVITEが正しく動作する
- [ ] TOPICが正しく動作する
- [ ] MODEが正しく動作する

### メモリ管理

- [ ] メモリリークがない（valgrind）
- [ ] クライアント切断時にリソースが解放される
- [ ] チャンネル削除時にリソースが解放される

---

## まとめ

### テストツール

✅ **nc**: 基本的なテスト、部分送信テスト
✅ **irssi**: 実践的なテスト、ユーザー体験の確認
✅ **valgrind**: メモリリークの検出
✅ **gdb**: デバッグ、クラッシュの原因特定

### テストの重要性

📌 **基本機能**: すべての基本機能が動作することを確認
📌 **エラーケース**: エラーハンドリングが正しく動作することを確認
📌 **エッジケース**: 部分送受信、同時接続などの特殊ケースを確認
📌 **メモリ管理**: メモリリークがないことを確認

### 次のステップ

- [12_IMPLEMENTATION_TIPS.md](12_IMPLEMENTATION_TIPS.md) - 実装のヒント
- [10_ERROR_HANDLING.md](10_ERROR_HANDLING.md) - エラーハンドリング

---

**前のドキュメント**: [10_ERROR_HANDLING.md](10_ERROR_HANDLING.md)
**次のドキュメント**: [12_IMPLEMENTATION_TIPS.md](12_IMPLEMENTATION_TIPS.md)

