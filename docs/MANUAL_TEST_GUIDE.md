# ft_irc 手動テスト完全ガイド

このドキュメントは、`subject.txt` および `evaluation.txt` に基づき、評価者が手動でプロジェクトを評価するためのステップバイステップガイドです。

## 1. 準備と基本チェック (Basic Checks)

評価を開始する前に、以下の項目を確認します。

### 1.1 コンパイルと起動

```bash
make re
./ircserv 6667 password
```

- **期待される結果**: コンパイルエラーが出ず、実行ファイル `ircserv` が生成されること。サーバーが起動し、エラーなく待機状態になること。

### 1.2 コード要件の確認 (静的解析)

別のターミナルを開き、以下のコマンドで要件を確認します。

**poll() の使用回数確認 (1回のみ許可)**
```bash
grep -r "poll" srcs/
```
- **期待される結果**: `poll()` (または `select`, `epoll` 等) の呼び出しがコード全体で1箇所のみであること（`Server.cpp` のメインループ内など）。

**fcntl() の使用法確認**
```bash
grep -r "fcntl" srcs/
```
- **期待される結果**: すべての `fcntl` 呼び出しが `fcntl(fd, F_SETFL, O_NONBLOCK)` の形式であること。それ以外のフラグ使用は禁止。

---

## 2. ネットワーク機能 (Networking)

### 2.1 nc (netcat) による基本接続とコマンド

サーバーが `6667` ポートで起動している状態で実行します。

```bash
nc -C localhost 6667
```
(`-C` オプションは CRLF を送信するために推奨されますが、本サーバーは LF のみでも動作するよう実装されています)

**入力:**
```text
PASS password
NICK tester
USER tester 0 * :Real Name
```

**期待される出力 (例):**
```text
:localhost 001 tester :Welcome to the Internet Relay Network tester!tester@127.0.0.1
:localhost 002 tester :Your host is localhost, running version 1.0
...
```

### 2.2 複数クライアントの同時接続

3つのターミナルを開き、それぞれで `nc localhost 6667` を実行して認証まで行います。

- **Client A (Alice)**
- **Client B (Bob)**
- **Client C (Charlie)**

**Client A (Alice) 入力:**
```text
JOIN #general
```
**Client B (Bob) 入力:**
```text
JOIN #general
PRIVMSG #general :Hello everyone!
```

**期待される動作:**
- Client A に Client B のメッセージ `:bob!bob@... PRIVMSG #general :Hello everyone!` が届くこと。
- サーバーがハングせず、すべてのクライアントがスムーズに操作できること。

---

## 3. ネットワークの特殊ケース (Networking Specials)

`evaluation.txt` に記載されている厳しいネットワーク条件のテストです。

### 3.1 部分コマンド (Partial Commands)

`nc` を使い、コマンドを分割して送信します（Ctrl+D を使用）。

```bash
nc localhost 6667
```

**入力手順:**
1. `PASS pass` と入力し、**Enterを押さずに** `Ctrl+D` を押す（送信させる）。
2. `word` と入力し、**Enter** を押す。

または `nc -C` を使わない場合で再現:
```text
P^DASS^D password^D<Enter>
```
※ `^D` は Ctrl+D

**検証:**
```text
NI^D CK^D partial^D<Enter>
US^D ER^D partial 0 * :Partial User^D<Enter>
```

**期待される結果:**
- サーバーは分割されたパケットを結合し、正常に `PASS password`, `NICK partial`, `USER ...` として処理すること。
- ログインに成功し、Welcomeメッセージが返ってくること。
- **重要**: この操作中、他の接続済みクライアントが影響を受けずに動作し続けること。

### 3.2 予期せぬ切断 (Unexpected Kill)

1. `nc` で接続し、認証済みのクライアントを用意。
2. その `nc` プロセスを `Ctrl+C` で強制終了。

**期待される結果:**
- サーバーがクラッシュしないこと。
- 同じチャンネルにいる他のクライアントに `QUIT` メッセージなどが届き、適切に処理されること。
- サーバーログに切断が記録され、ソケットがクローズされていること（再接続して確認）。

### 3.3 コマンド途中での切断

1. `nc` で接続。
2. `PRIVMSG #general :This is a ha` まで入力し、送信せずに `Ctrl+C` で終了。

**期待される結果:**
- サーバーが中途半端なバッファを持ったままハングしたり、異常状態にならないこと。
- 次のクライアントが正常に接続できること。

### 3.4 クライアント一時停止とフラッド (Stop & Flood)

1. クライアントA (`nc` または `irssi`) を接続し、チャンネル `#flood` に参加。
2. クライアントA のターミナルで `Ctrl+Z` を押し、プロセスを一時停止 (Suspend) する。
3. クライアントB から `#flood` に対して大量のメッセージを送信する（連打）。
   ```text
   PRIVMSG #flood :flood1
   PRIVMSG #flood :flood2
   ...
   PRIVMSG #flood :flood100
   ```
4. サーバーがハングしていないか確認（クライアントCで接続してみる）。
5. クライアントA のターミナルで `fg` を入力し、プロセスを復帰させる。

**期待される結果:**
- サーバーはクライアントAの書き込みバッファが詰まってもクラッシュせず、他のクライアント（B, C）を処理し続けること。
- クライアントA が復帰した後、溜まっていたメッセージ（またはバッファ許容量分のメッセージ）が一気に受信されること。
- メモリリークが発生していないこと。

---

## 4. クライアントコマンド (Client Commands)

正規の IRC クライアント (例: `irssi` または `LimeChat`, `Textual`) を推奨しますが、`nc` でも確認可能です。

### 4.1 認証と基本情報
- **PASS**: 間違ったパスワードで接続を試み、`464` エラーで切断されるか確認。
- **NICK**: 使用中のニックネームに変更しようとして `433` エラーが出るか確認。
- **USER**: 認証後に再送信し、`462` (Already registered) が返るか確認。

### 4.2 メッセージング
- **PRIVMSG**:
  - 存在しないユーザー宛て -> `401` (No such nick)
  - チャンネル宛て -> メンバー全員に配信
- **NOTICE**:
  - エラーが発生しても自動応答（エラーメッセージ）を返さないこと仕様確認。

### 4.3 オペレーターコマンド (Channel Operations)

**前提**: `#ops` チャンネルを作成し、最初に参加したユーザー (OpUser) がオペレーター権限 (`@`) を持ちます。別のユーザー (NormalUser) も参加させます。

#### KICK (追放)
- **NormalUser**: `KICK #ops OpUser` -> `482` (Chanop privs needed) エラー。
- **OpUser**: `KICK #ops NormalUser` -> NormalUser がチャンネルから強制退出させられる。

#### INVITE (招待)
- **OpUser**: `MODE #ops +i` (招待制モード設定)。
- **NormalUser**: `JOIN #ops` -> `473` (Invite only) エラー。
- **OpUser**: `INVITE NormalUser #ops` -> NormalUser に招待が届く。
- **NormalUser**: `JOIN #ops` -> 成功。

#### TOPIC (トピック)
- **OpUser**: `MODE #ops +t` (トピック制限モード)。
- **NormalUser**: `TOPIC #ops :New Topic` -> `482` エラー。
- **OpUser**: `TOPIC #ops :Op Topic` -> 変更成功、全員に通知。

#### MODE (モード変更)
以下のモード切り替えと挙動を確認します。
- `i`: 招待制 (Invite-only)
- `t`: トピック保護 (Topic protection)
- `k`: キー (パスワード) 設定
  - `MODE #ops +k secret`
  - NormalUser: `JOIN #ops` -> `475` (Bad channel key)
  - NormalUser: `JOIN #ops secret` -> 成功
- `o`: オペレーター権限の譲渡/剥奪
  - `MODE #ops +o NormalUser` -> NormalUser が `@` 付きになる。
  - `MODE #ops -o OpUser` -> 自分の権限を外す。
- `l`: ユーザー数制限 (Limit)
  - `MODE #ops +l 1`
  - 別のユーザーが `JOIN` しようとすると `471` (Channel is full)。

---

## 5. 最終確認

- テスト中、サーバーが一度もクラッシュ（Segmentation fault 等）していないこと。
- メモリリークチェック（可能であれば `valgrind` 使用）。
  ```bash
  valgrind --leak-check=full ./ircserv 6667 password
  ```
  ※ `Ctrl+C` で終了後、"definitely lost" が 0 であることを推奨。

このガイドの手順をすべてパスすれば、Mandatory Part は満点です。

