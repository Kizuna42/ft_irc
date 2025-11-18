# テスト方法詳細

このドキュメントでは、ft_ircサーバーの実践的なテスト方法を説明します。

## 目次

1. [ncを使った基本テスト](#ncを使った基本テスト)
2. [部分コマンド送信テスト](#部分コマンド送信テスト)
3. [irssiでの実践的テスト](#irssiでの実践的テスト)
4. [irssiを用いた実機テストの詳細手順](#irssiを用いた実機テストの詳細手順)
5. [複数クライアント同時接続テスト](#複数クライアント同時接続テスト)
6. [テストシナリオ集](#テストシナリオ集)
7. [よくある問題と解決方法](#よくある問題と解決方法)
8. [自動テストスイートとValgrindシナリオ](#自動テストスイートとvalgrindシナリオ)

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

**接続コマンドの構文:**
```
/connect <hostname> <port> <password>
```

**例:**
- デフォルトポート（6667）: `/connect localhost 6667 password`
- カスタムポート（6669）: `/connect localhost 6669 password`
- IPアドレス指定: `/connect 127.0.0.1 6667 password`

**注意事項:**
- ポート番号はサーバー起動時に指定したものと一致させる必要があります
- パスワードもサーバー起動時に指定したものと一致させる必要があります
- 接続が成功すると、Welcomeメッセージが表示されます

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

## irssiを用いた実機テストの詳細手順

### テスト環境の準備

#### 1. サーバーの起動

**ターミナル1 (サーバー):**
```bash
./ircserv 6667 password
```

サーバーが正常に起動していることを確認します。

#### 2. 複数のirssiクライアントの起動

**ターミナル2 (クライアント1 - Alice):**
```bash
irssi
```

**ターミナル3 (クライアント2 - Bob):**
```bash
irssi
```

**ターミナル4 (クライアント3 - Charlie):**
```bash
irssi
```

### 詳細なテスト手順

#### ステップ1: 接続と認証

**各irssiクライアントで以下を実行:**

```
/connect localhost 6667 password
```

**注意:** サーバーを別のポートで起動した場合（例: `./ircserv 6669 password`）、接続コマンドも対応するポート番号に変更してください:

```
/connect localhost 6669 password
```

**期待される動作:**
- 接続が成功する
- Welcomeメッセージが表示される: `Welcome to the Internet Relay Network ...`
- サーバー情報が表示される

**確認ポイント:**
- すべてのクライアントが正常に接続できること
- エラーメッセージが表示されないこと
- サーバーログに接続が記録されること

**接続エラーの場合:**
- サーバーが起動しているか確認
- ポート番号が正しいか確認（サーバー起動時のポートと一致させる）
- パスワードが正しいか確認

#### ステップ2: ニックネームの設定

**Alice:**
```
/nick alice
```

**Bob:**
```
/nick bob
```

**Charlie:**
```
/nick charlie
```

**期待される動作:**
- ニックネームが変更される
- エラーメッセージが表示されないこと

**確認ポイント:**
- ニックネームが正しく設定されること
- 重複したニックネームを試した場合、エラーが返されること

#### ステップ3: チャンネルへの参加

**Alice:**
```
/join #test
```

**期待される応答:**
```
--- alice has joined #test
--- #test: No topic is set
--- #test: alice (@alice)
```

**Bob:**
```
/join #test
```

**期待される動作:**
- AliceにBobの参加通知が届く
- Bobに#testの情報が表示される
- ニックネームリストに`alice`と`bob`が表示される

**Charlie:**
```
/join #test
```

**期待される動作:**
- 全員にCharlieの参加通知が届く
- 全員のニックネームリストが更新される

**確認ポイント:**
- JOINメッセージが全員に配信されること
- NAMESリストが正しく更新されること
- 最初に参加したユーザー（Alice）がオペレーター（@）になること

#### ステップ4: チャンネルメッセージの送受信

**Alice:**
```
Hello, everyone!
```

**期待される動作:**
- Alice自身にはメッセージが表示されない（または表示されるが、これはirssiの設定による）
- BobとCharlieにメッセージが届く

**Bob:**
```
Hi Alice! Nice to meet you.
```

**期待される動作:**
- Bob自身にはメッセージが表示されない
- AliceとCharlieにメッセージが届く

**確認ポイント:**
- メッセージが正しく全員に配信されること
- 送信者情報が正しく表示されること（例: `<alice> Hello, everyone!`）
- メッセージの順序が正しいこと

#### ステップ5: プライベートメッセージ

**Alice:**
```
/msg bob Hello Bob, this is private!
```

**期待される動作:**
- Bobにのみメッセージが届く
- Charlieにはメッセージが届かない
- Bobに`<alice>`という形式でメッセージが表示される

**Bob:**
```
/msg alice Hi Alice! This is a private conversation.
```

**確認ポイント:**
- プライベートメッセージが正しく送受信されること
- チャンネルメンバーには配信されないこと

#### ステップ6: チャンネルトピックの設定

**Alice（オペレーター）:**
```
/topic #test This is a test channel
```

**期待される動作:**
- 全チャンネルメンバーにトピック変更通知が届く
- トピックが設定される

**確認ポイント:**
- オペレーターがトピックを設定できること
- 非オペレーターがトピックを設定しようとした場合、エラーが返されること
- トピック変更通知が全員に届くこと

#### ステップ7: チャンネルモードの設定

**Alice（オペレーター）:**
```
/mode #test +i
```

**期待される動作:**
- チャンネルが招待制になる
- 新しいユーザーがJOINできなくなる

**新しいクライアント（David）で試す:**
```
/connect localhost 6667 password
/nick david
/join #test
```

**期待される応答:**
```
*** Cannot join channel (+i)
```

**Alice:**
```
/invite david #test
```

**David:**
```
/join #test
```

**期待される動作:**
- 招待後はJOINできること

**確認ポイント:**
- モード設定が正しく動作すること
- エラーメッセージが適切に表示されること
- INVITEコマンドが正しく動作すること

#### ステップ8: KICKコマンド

**Alice（オペレーター）:**
```
/kick #test charlie Spamming
```

**期待される動作:**
- Charlieが#testから削除される
- 全メンバーにKICK通知が届く
- CharlieにKICK理由が表示される

**確認ポイント:**
- オペレーターがKICKできること
- 非オペレーターがKICKしようとした場合、エラーが返されること
- KICK理由が正しく表示されること

#### ステップ9: チャンネル退出

**Bob:**
```
/part #test See you later!
```

**期待される動作:**
- Bobが#testから退出する
- AliceとCharlieに退出通知が届く
- ニックネームリストが更新される

**確認ポイント:**
- PARTメッセージが全員に届くこと
- チャンネルメンバーリストが更新されること

#### ステップ10: 切断

**Alice:**
```
/quit Goodbye everyone!
```

**期待される動作:**
- Aliceの接続が切断される
- チャンネルメンバーにQUIT通知が届く
- チャンネルメンバーリストが更新される
- 空になったチャンネルが削除される（最後のメンバーが退出した場合）

**確認ポイント:**
- QUITメッセージが正しく配信されること
- サーバーが正常にクリーンアップすること

### irssiでの確認コマンド

#### サーバー情報の確認

```
/server
```

サーバー接続情報が表示されます。

#### チャンネルリストの確認

```
/list
```

利用可能なチャンネルリストが表示されます。

#### チャンネルメンバーの確認

```
/names #test
```

#testのメンバーリストが表示されます。

#### ウィンドウの切り替え

```
Alt + 1, 2, 3, ...
```

異なるウィンドウ（サーバー、チャンネル、プライベートメッセージ）を切り替えられます。

### 実機テストで確認すべき重要なポイント

#### 1. メッセージの配信

- ✅ チャンネルメッセージが全員に届くこと
- ✅ プライベートメッセージが正しい相手に届くこと
- ✅ 自分自身にメッセージが返ってこないこと（または適切に処理されること）

#### 2. イベント通知

- ✅ JOIN、PART、QUIT、KICKなどが正しく通知されること
- ✅ MODE変更が通知されること
- ✅ TOPIC変更が通知されること
- ✅ NICK変更が通知されること

#### 3. 権限管理

- ✅ オペレーター権限が正しく機能すること
- ✅ 非オペレーターが制限されたコマンドを実行できないこと
- ✅ エラーメッセージが適切に表示されること

#### 4. 同時接続

- ✅ 複数のクライアントが同時に動作すること
- ✅ メッセージの順序が正しいこと
- ✅ サーバーがクラッシュしないこと

#### 5. エラーハンドリング

- ✅ 不正なコマンドでエラーが返されること
- ✅ 存在しないチャンネル/ユーザーでエラーが返されること
- ✅ 権限不足でエラーが返されること

### irssiでのトラブルシューティング

#### 接続できない場合

**確認事項:**
- サーバーが起動しているか
- ポート番号が正しいか
- パスワードが正しいか

**デバッグ方法:**
```
/connect -server localhost 6667 password
```

#### メッセージが届かない場合

**確認事項:**
- 正しいチャンネルに参加しているか
- ニックネームが正しいか
- サーバーログでメッセージが送信されているか

**確認方法:**
irssiで`/window server`でサーバーウィンドウを開き、生のメッセージを確認できます。

#### チャンネルに参加できない場合

**確認事項:**
- チャンネルが招待制（+i）になっていないか
- チャンネルがキー設定（+k）になっていないか
- ユーザー数制限（+l）に達していないか

### より高度なテストシナリオ

#### シナリオ1: 大量のメッセージ送信

複数のクライアントから同時に大量のメッセージを送信し、サーバーがクラッシュしないことを確認します。

#### シナリオ2: ニックネーム変更

接続中にニックネームを変更し、すべてのチャンネルメンバーに通知が届くことを確認します。

#### シナリオ3: チャンネルモードの組み合わせ

複数のモード（+i、+t、+k、+lなど）を組み合わせて設定し、それぞれが正しく機能することを確認します。

#### シナリオ4: 異常切断

クライアントをCtrl+Cで強制終了し、サーバーが正常にクリーンアップすることを確認します。

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

## 自動テストスイートとValgrindシナリオ

### Python製自動テストスイート (`tests/irc_basic_suite.py`)

このリポジトリには、複数クライアント・モード・異常系をまとめて検証するための**自動テストスクリプト**が含まれています。

- **場所**: `tests/irc_basic_suite.py`
- **前提**: `./ircserv <port> <password>` が起動済み
- **デフォルト設定**:
  - `IRC_HOST=127.0.0.1`
  - `IRC_PORT=6667`
  - `IRC_PASSWORD=password`

#### 実行例

```bash
./ircserv 6667 password &
python3 tests/irc_basic_suite.py
```

#### カバーしている主なテスト

- **認証まわり**
  - 正常な `PASS/NICK/USER` フロー（`001` Welcome）
  - 間違ったパスワード（`464 Password incorrect`）
  - PASS なしでの JOIN 試行（`451 You have not registered`）
  - ニックネーム重複（`433 Nickname is already in use`）
- **メッセージ**
  - チャンネル宛て `PRIVMSG` ブロードキャスト
  - ユーザー宛て `PRIVMSG`（`PRIVMSG nick :...`）
  - ユーザー宛て `NOTICE`（`NOTICE nick :...`）
  - パラメータ不足の `NOTICE` に対し**エラーを返さない**ことの確認
  - 分割された `PRIVMSG`（部分コマンド）を 1 つに復元できること
- **チャンネル・モード / オペレーター**
  - 招待制チャンネル `+i` と `INVITE` による参加制御（`473`）
  - KICK コマンド:
    - 非オペレーターの KICK 拒否（`482 You're not channel operator`）
    - オペレーターによる KICK 成功と全員への通知
  - `MODE +t` による TOPIC 制限と、非オペレーターのエラー（`482`）
  - キーチャンネル `MODE +k` と誤キー（`475`）／正しいキーでの JOIN
  - ユーザー数制限 `MODE +l` と満員時の JOIN 拒否（`471`）
  - オペレーター付与 `MODE +o` により、付与前は KICK 失敗・付与後は KICK 成功となること
- **切断**
  - `QUIT` によるチャンネル参加者への通知とクリーンなクライアント削除

テスト結果は次のような形式で表示されます:

```text
TEST_RESULTS_START
[OK] basic_auth_success
  :localhost 001 alice1 :Welcome ...
...
SUMMARY 17 / 17
TEST_RESULTS_END
```

評価時には、このスクリプトを用いて**仕様達成と回 regressions** を素早く確認できます。

### Valgrind シナリオ (`tests/irc_valgrind_scenario.sh`)

より現実的な負荷（複数クライアント・複数チャンネル）をかけた状態で、Valgrind によるメモリリークチェックを行うためのシナリオです。

- **場所**: `tests/irc_valgrind_scenario.sh`
- **権限付与**:

```bash
chmod +x tests/irc_valgrind_scenario.sh
```

- **実行例**:

```bash
tests/irc_valgrind_scenario.sh 6668 password
```

#### シナリオの内容

- 指定ポートで `ircserv` を **Valgrind 経由で起動**
- 環境変数 `IRC_HOST/IRC_PORT/IRC_PASSWORD` を通じて
  `tests/irc_basic_suite.py` を実行
- テスト完了後にサーバを停止し、Valgrind ログ末尾のサマリを表示

典型的な実行結果は以下のようになります:

```text
HEAP SUMMARY:
    in use at exit: 40 bytes in 2 blocks
  total heap usage: 1,732 allocs, 1,730 frees, 259,075 bytes allocated

LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
     possibly lost: 0 bytes in 0 blocks
   still reachable: 40 bytes in 2 blocks
```

「definitely lost」が 0 であることを確認することで、少なくともこのシナリオ範囲では**メモリリークがない**ことを保証できます。

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

