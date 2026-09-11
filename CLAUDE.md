# CLAUDE.md

このリポジトリで作業する Claude Code 向けのガイド。毎セッション必要なことだけをここに置き、詳細は
`docs/` に書いてここからリンクする。

## このプロジェクトは何か

MEGA クラウドストレージ版の WinDirStat。アカウント内の容量の内訳を、上ペインのツリーと下ペインの
treemap で可視化する。**閲覧専用**で、ファイル操作（削除・移動・アップロード・ダウンロード）は持たない。
Qt Widgets 製、Windows / macOS / Linux 対応。

- 仕様・設計・決定事項: **`docs/DESIGN.md`**（日本語）。作業の前に該当節を読むこと。
- 参考実装: `../MegaExplorer`（同じ作者の MIT プロジェクト。MEGA SDK の組み込み、CMake/vcpkg 構成、
  SDK の癖への対処が揃っている）。コードは自由に流用してよい。ただし QML 製なので UI 層は流用しない。

## 状態

**コード未着手。** リポジトリの骨組み（ライセンス、git 設定、サブモジュール、設計メモ）だけがある。
次は CMake の雛形（OS 別プリセット、ターゲット分割）から。この節は大きな節目ごとに書き換える。

## 進め方（ほぼ LLM に任されている）

- ユーザーはこのプロジェクトの大半を LLM に任せる方針。**判断に迷っても止まらずに決め、決めたことと
  理由を残す**: 設計上の決定は `docs/DESIGN.md` §8 に追記、小さな判断はコミットメッセージの本文に書く。
  ユーザーに確認するのは、要件（`docs/DESIGN.md` §1）そのものを変える判断と、外部に見える操作だけ。
- **コミットは自由に行ってよい**（ユーザー承認済み）。1 つのまとまった変更につき 1 コミット。
  ファイルは名前を指定してステージする（`git add -A` / `git add .` は使わない）。ブランチは当面 `main` のみ。
- **push、GitHub リポジトリの作成、Issue/PR 操作はまだ未設定。行う前にユーザーに確認する。**
- 実アカウントでの動作確認はユーザーが行う。LLM 側の確認は単体テストとモック起動（下記）で行う。
- 仕様にない機能を足さない。やりたくなったら `docs/DESIGN.md` の未決事項に書いて提案する。

## ツール: コードには Serena を使う

Serena MCP が有効。`src/`、`main.cpp`、`tests/` の読み書きは Serena のシンボル単位のツール
（`get_symbols_overview` → `find_symbol` → `replace_symbol_body` 等）を優先する。セッションの最初の
コード作業の前に `initial_instructions` を 1 回呼ぶ。Markdown・JSON・CMake などコード以外のファイルは
組み込みの `Read`/`Edit` でよい。Serena が接続できないときは、黙ってファイル全体を読む方式に切り替えず、
そのことをユーザーに伝える。

## 守るべき設計ルール

詳細は `docs/DESIGN.md`。コードを書くときに破りやすいものだけ抜き出す:

- **MEGA SDK を触ってよいのは `src/mega/` だけ。** UI とコアは SDK の型（`MegaNode`、`MegaApi` 等）を
  一切 include しない。データは `IAccountSource` 経由で、読み込み完了時に組み立てた不変のスナップショット
  （`SizeNode` のツリー）として受け取る（§3, §4）。
- **実装の選択はコンポジションルート（`main.cpp`）の 1 箇所だけ。** `--mock` 系の起動引数で
  `MockAccountSource`、引数なしで `MegaAccountSource`。DI フレームワークは使わず、コンストラクタで渡す。
- **セッションを保存しない。** `dumpSession()` の結果、メールアドレス、パスワードをディスク・`QSettings`・
  ログのどこにも書かない（§5）。
- **SDK の状態キャッシュ**（`megaclient_statecache*.db`）は `AppLocalDataLocation/sdk-cache/<実行ID>/`
  にだけ作る。終了時に自分のディレクトリを削除し、起動時にはロックの取れる残骸だけを削除する（§5）。
  `AppDataLocation` は Windows ではローミング側なので使わない。
- **SDK のコールバックは SDK 内部スレッドで来る。** GUI に触れる処理は必ずキュー接続で GUI スレッドへ移す。
- **OS 依存コードを書かない。** パスは `QStandardPaths` / `QDir`、ファイルを開く処理は `QDesktopServices`。
  やむを得ない場合は `src/platform/` に隔離する。
- treemap のレイアウト計算は描画から切り離した純粋な関数にし、単体テストを付ける。

## ビルド

（CMake の雛形ができたら、実際に動いたコマンドでこの節を書き換える。）

- Qt 6.11.1 は `C:/Qt/6.11.1/msvc2022_64` にある（`mingw_64` もあるが使わない）。
- **Windows は MSVC + Visual Studio ジェネレータ必須。** Ninja は不可（SDK が Windows で
  `CMAKE_GENERATOR_TOOLSET` を固定するため）。MinGW は SDK が非対応。Linux/macOS は Ninja でよい。
- **CMake は必ず `C:/Qt/Tools/CMake_64/bin/cmake.exe`（3.30）をフルパスで呼ぶ。** `PATH` 上の `cmake` は
  Strawberry Perl の 3.29 で、MegaExplorer ではこの MSVC を認識できず configure が失敗し、しかも失敗前に
  `CMakeCache.txt` を上書きした。
- サブモジュール: `third_party/sdk`（`v10.17.0`、shallow）、`third_party/vcpkg`（**完全履歴。浅くしない**。
  baseline 解決に履歴が要る）。どちらも MegaExplorer と同じコミットで、vcpkg のバイナリキャッシュ
  （`%LOCALAPPDATA%\vcpkg\archives`）を共有している。上げるときは両方の整合を確認する。
  新しくクローンしたら `git submodule update --init --recursive` → `third_party/vcpkg/bootstrap-vcpkg.bat`。
- vcpkg の manifest features は最小限にする方針（MegaExplorer の `use-ffmpeg` / `use-pdfium` /
  `use-freeimage` / `use-libuv` は不要な見込み）。SDK の組み込み方と既知の罠は
  `../MegaExplorer/docs/BUILD.md` と `../MegaExplorer/docs/investigations/STUDY_CROSS_PLATFORM_BUILD.md`。
- 警告: 自前ターゲットのみ MSVC `/W4`、GCC/Clang `-Wall -Wextra`。**作業の終わりに自分のコードの
  新しい警告を 0 にする。**
- **性能は Release ビルドで判断する。** MSVC の Debug は `_ITERATOR_DEBUG_LEVEL=2` 等で桁違いに遅い。

## 検証

- 単体テスト（Qt Test）は `MegaDirStatCore` と `MegaDirStatMock` だけをリンクし、SDK なしで回す。
- 画面の確認はモック起動で行う（`--mock <fixture.json>`、`--mock-generate <件数>`。仕様は §4）。
  **実アカウントでログインしてスクリーンショットを撮らない**（ユーザーの実データが写る。
  `.screenshots/` は gitignore 済み）。
- UI の変更をブラウザやスクリーンショットで確認できなかった場合は、成功したと言わずにそう伝える。

## ライセンス

アプリは **MIT**。Qt は LGPLv3 で使うので、**GPL 専用の Qt モジュール（Qt Charts 等）を入れない**。
MEGA SDK は BSD-2-Clause。`meganz/MEGAsync` は Code Review Licence なので**コードを写さない**
（SDK の使い方の参考にとどめる）。依存を足したら第三者ライセンス表記の更新が必要（配布を始めるときに
MegaExplorer の `scripts/gen_third_party_notices.py` を移植する）。

## コードコメント

コードから読み取れないことだけを 1〜2 行で書く: 外部仕様の罠（Qt/MSVC/SDK の挙動がコードの形を
決めた理由）、もっともらしい「修正」への予防線、別ファイルに原因があるスレッド・寿命・所有権の理由。
コードの言い換え、変更履歴、検証の経緯は書かない（それはコミットメッセージや `docs/` の役目）。

## 改行とエンコーディング

`.gitattributes` でコミット時に LF に正規化している（`.bat` / `.cmd` のみ CRLF）。スクリプトでファイル
全体を書き直したときは、コミット前に `git diff --stat` で差分がファイル全体に及んでいないか確認する。
