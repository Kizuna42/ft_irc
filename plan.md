# IRC Server 実装計画

## アーキテクチャ設計

### 主要クラス構成

```
Server           - メインサーバークラス、poll()管理、クライアント/チャンネル管理
Client           - クライアント情報（fd, nickname, username, 認証状態、バッファ）
Channel          - チャンネル情報（メンバー、オペレーター、モード、トピック）
CommandHandler   - IRCコマンドのパース・実行
Message          - IRCメッセージのパース・生成
```

### ディレクトリ構成

```
ft_irc/
├── Makefile
├── includes/
│   ├── Server.hpp
│   ├── Client.hpp
│   ├── Channel.hpp
│   ├── CommandHandler.hpp
│   └── Message.hpp
├── srcs/
│   ├── main.cpp
│   ├── Server.cpp
│   ├── Client.cpp
│   ├── Channel.cpp
│   ├── CommandHandler.cpp
│   ├── Message.cpp
│   └── commands/
│       ├── auth.cpp      (PASS, NICK, USER)
│       ├── channel.cpp   (JOIN, PART)
│       ├── message.cpp   (PRIVMSG, NOTICE)
│       └── operator.cpp  (KICK, INVITE, TOPIC, MODE)
└── README.md
```

## 実装手順

### 1. プロジェクトセットアップ

- Makefile 作成（C++98, -Wall -Wextra -Werror）
- 基本的なヘッダーファイルとクラス定義

### 2. Server クラス - 基本ソケット処理

- ソケット作成、bind、listen
- poll()によるイベント監視（1 つのみ使用）
- 非ブロッキングモード設定（`fcntl(fd, F_SETFL, O_NONBLOCK)`）
- 新規接続の accept 処理

### 3. Client クラス - 接続管理

- クライアント情報の保持
- 部分受信データのバッファリング（`\r\n`までの蓄積）
- 送信バッファ管理（非ブロッキング送信対応）
- 接続切断時のクリーンアップ

### 4. メッセージパーサー

- IRC プロトコル形式のパース（prefix, command, params）
- 部分コマンドの処理（`nc`で`^D`による分割送信テスト対応）
- コマンドと引数の分離

### 5. 認証システム

**コマンド実装：**

- `PASS <password>` - サーバーパスワード認証
- `NICK <nickname>` - ニックネーム設定（重複チェック）
- `USER <username> <hostname> <servername> <realname>` - ユーザー情報設定
- 認証完了まで他コマンドを拒否

**数値応答：**

- 001 RPL_WELCOME
- 461 ERR_NEEDMOREPARAMS
- 462 ERR_ALREADYREGISTRED
- 464 ERR_PASSWDMISMATCH

### 6. Channel クラス - チャンネル管理

**基本機能：**

- `JOIN <channel>` - チャンネル参加（最初のユーザーがオペレーター）
- `PART <channel> [reason]` - チャンネル退出
- チャンネルメンバーリスト管理
- オペレーターリスト管理

**チャンネルモード：**

- `i` - 招待制
- `t` - オペレーターのみトピック変更可
- `k <key>` - パスワード
- `o <nick>` - オペレーター権限
- `l <limit>` - ユーザー数制限

### 7. メッセージング

- `PRIVMSG <target> <message>` - ユーザー/チャンネルへのメッセージ
- `NOTICE <target> <message>` - 通知（エラー応答なし）
- チャンネル内の全メンバーへの配信（送信者以外）
- ユーザー間のダイレクトメッセージ

### 8. オペレーターコマンド

- `KICK <channel> <user> [reason]` - ユーザー追放
- `INVITE <nickname> <channel>` - ユーザー招待
- `TOPIC <channel> [topic]` - トピック表示/変更
- `MODE <channel> <flags> [params]` - チャンネルモード変更

**権限チェック：**

- オペレーター権限の確認
- 一般ユーザーによる実行を拒否

### 9. エラーハンドリング

**必須対応：**

- メモリ不足時のクラッシュ防止
- 切断されたクライアントの検出（recv() == 0）
- 部分送信・受信の処理
- 不正なコマンドへの適切なエラー応答
- poll()のタイムアウト処理

**評価テストケース：**

- `nc`による部分コマンド送信（`^D`で分割）
- クライアント強制終了（`kill`）
- 中断されたクライアント（`^Z`）とチャンネルフラッド

### 10. 主要な数値応答実装

```
001 RPL_WELCOME
331 RPL_NOTOPIC / 332 RPL_TOPIC
353 RPL_NAMREPLY
366 RPL_ENDOFNAMES
401 ERR_NOSUCHNICK
403 ERR_NOSUCHCHANNEL
442 ERR_NOTONCHANNEL
461 ERR_NEEDMOREPARAMS
462 ERR_ALREADYREGISTRED
464 ERR_PASSWDMISMATCH
471 ERR_CHANNELISFULL
473 ERR_INVITEONLYCHAN
475 ERR_BADCHANNELKEY
482 ERR_CHANOPRIVSNEEDED
```

## テスト計画

### 基本テスト

1. `./ircserv 6667 password`で起動
2. `nc 127.0.0.1 6667`で接続・コマンド送信
3. irssi で接続：`/connect 127.0.0.1 6667 password`
4. 複数クライアント同時接続テスト

### 部分コマンドテスト

```bash
nc -C 127.0.0.1 6667
# PASS^D, password^D, \n の順に送信
# JOIN^D, #test^D, \n の順に送信
```

### ストレステスト

- クライアントを強制終了（`kill`）
- クライアントを一時停止（`^Z`）してチャンネルフラッド
- メモリリークチェック（valgrind/leaks）

## 重要な実装ポイント

### poll()の使用

- **1 つのみ使用**（評価で厳密にチェックされる）
- すべての accept, read, write 前に poll()を呼ぶ
- `errno == EAGAIN`での再試行は禁止

### 非ブロッキング I/O

- すべての fd に対して`fcntl(fd, F_SETFL, O_NONBLOCK)`を設定
- これ以外の fcntl()の使用は禁止
- 部分送信・受信の適切な処理
- **Linux 環境での評価を想定**（macOS 特有の動作は参考程度）

### メモリ管理

- クライアント切断時のリソース解放
- チャンネルが空になった時の削除
- std::map/std::vector でクライアント・チャンネル管理

### C++98 準拠

- `<cstring>`, `<cstdlib>`等の C++ヘッダーを優先
- Boost 等の外部ライブラリ禁止
- `-std=c++98`でコンパイル可能に
