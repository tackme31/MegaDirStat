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

この節は次のセッションへの引き継ぎを兼ねる。節目ごとに書き換える（最終更新 2026-09-12）。

### できていること

- コア（`SizeNode` スナップショット、Rows 方式 treemap レイアウト＋16px 未満のまとめ、`IAccountSource`）、
  モック（JSON フィクスチャ／決定的な乱数生成、遅延・失敗の再現）、UI（上下分割、既定で折りたたみのツリー、
  フラットな treemap、選択の双方向同期、ツールチップ）。モックデータで起動できる。
- `scripts/verify.sh`（ビルド＋警告ゼロ＋ctest）、`scripts/run.ps1`、`ui-style` スキル（スクショ）。
- treemap の見た目はユーザーと何度か調整して「かなりいい感じ」と言われた状態。

### 次にやること（この順）

1. **MEGA SDK をビルドに組み込む。** `CMakePresets.json` の `msvc-debug` に vcpkg の変数を足し、
   `CMakeLists.txt` で `add_subdirectory(third_party/sdk)`。手本は `../MegaExplorer/CMakePresets.json` と
   `../MegaExplorer/CMakeLists.txt`、罠は `../MegaExplorer/docs/BUILD.md`。manifest features は最小限を
   狙う（`use-ffmpeg` / `use-pdfium` / `use-freeimage` / `use-libuv` を外せるか試す。外せれば BUILD.md の
   swscale 回避策も不要）。初回 configure は vcpkg のビルドで長い（バイナリキャッシュが効けば短い）。
   新ターゲット `MegaDirStatMega`（`src/mega/`）だけが SDK をリンクする。
2. **`MegaAccountSource`**（`src/mega/`）: ログイン → `fetchNodes` → ノードを走査して `SizeNode` を組み立て
   （ワーカースレッドで）→ `loaded`。フォルダサイズは自前集計（`finalizeTree`）でよい。対象ルートは当面
   Cloud Drive と Rubbish Bin（DESIGN.md §8 Q3）。MegaExplorer の `src/mega/MegaSdkClient.cpp` と
   `MegaSdkListeners.h` が SDK の使い方の手本（同じ作者の MIT、流用可）。
3. **ログインダイアログ**（メール＋パスワード、2FA の PIN 入力も要る）。`main.cpp` で `--mock` 系が無いときに
   `MegaAccountSource` を選び、`MainWindow::start()` で `requiresLogin()` ならダイアログを出す。
4. **SDK キャッシュの後始末**（DESIGN.md §5: `AppLocalDataLocation/sdk-cache/<実行ID>/` と `QLockFile`、
   起動時の残骸削除、終了時の logout → 削除）。単体テストを付けられるよう、ディレクトリ操作は SDK と
   切り離したクラスにする。
5. 実アカウントでの確認はユーザーに頼む（LLM はログインしない）。

### 既知の課題・保留

- **Serena に C++ 言語サーバーが無い**（下の「ツール」節）。直すなら `compile_commands.json` を作る手段
  （例: Ninja ジェネレータで別ディレクトリに configure するだけのプリセット）と `.serena/project.yml` の
  `language_servers` 設定が要る。未着手。
- Rows 方式にしてから、16px 未満としてまとめた灰色セルが squarified 時より多い。ユーザーが気にしたら
  しきい値（`src/ui/TreemapWidget.cpp` の `kMinCellPx`）か行を閉じる比率（`kMinAspect`）を調整する。
- treemap クリック → ツリー選択の同期は LLM 側では未確認（`ui_shot.py drive` はユーザー確認が要る）。
- ツリーの列ヘッダでのソートは未実装。Linux/macOS のビルドは未検証。

## 進め方（ほぼ LLM に任されている）

- ユーザーはこのプロジェクトの大半を LLM に任せる方針。**判断に迷っても止まらずに決め、決めたことと
  理由を残す**: 設計上の決定は `docs/DESIGN.md` §8 に追記、小さな判断はコミットメッセージの本文に書く。
  ユーザーに確認するのは、要件（`docs/DESIGN.md` §1）そのものを変える判断と、外部に見える操作だけ。
- **コミットは自由に行ってよい**（ユーザー承認済み）。1 つのまとまった変更につき 1 コミット。
  ファイルは名前を指定してステージする（`git add -A` / `git add .` は使わない）。ブランチは当面 `main` のみ。
- **push、GitHub リポジトリの作成、Issue/PR 操作はまだ未設定。行う前にユーザーに確認する。**
- 実アカウントでの動作確認はユーザーが行う。LLM 側の確認は単体テストとモック起動（下記）で行う。
- 仕様にない機能を足さない。やりたくなったら `docs/DESIGN.md` の未決事項に書いて提案する。
- ユーザーの好み: 見た目はモダン・フラット（グラデーションなし）。UI の変更は `ui-style` でスクショを撮って
  自分で確認してから、ユーザーに確認用に起動して渡す（`./scripts/run.ps1 -NoBuild -AppArgs ...`）。
- WinDirStat は GPL。挙動の参考にはするが**コードは写さない**（本アプリは MIT）。

## ツール: コードには Serena を使う

Serena MCP が有効。`src/`、`main.cpp`、`tests/` の読み書きは Serena のシンボル単位のツール
（`get_symbols_overview` → `find_symbol` → `replace_symbol_body` 等）を優先する。セッションの最初の
コード作業の前に `initial_instructions` を 1 回呼ぶ。Markdown・JSON・CMake などコード以外のファイルは
組み込みの `Read`/`Edit` でよい。Serena が接続できないときは、黙ってファイル全体を読む方式に切り替えず、
そのことをユーザーに伝える。

**現状、Serena にこのプロジェクトの C++ 言語サーバーが設定されていない**（`.serena/project.yml` の
`language_servers: []`、シンボル系ツールは "No language servers available" で失敗する）。C++ の言語
サーバー（clangd）は `compile_commands.json` を必要とし、Visual Studio ジェネレータはそれを出さないため。
設定されるまではコードも組み込みツールで扱ってよい。

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

普段使うのは次の 3 つ（Windows）:

```
bash scripts/verify.sh              # ビルド＋警告ゲート＋ctest。作業の終わりに必ず通す
./scripts/run.ps1 -AppArgs '--mock-generate','20000'   # ビルドして起動（PowerShell から直接。`powershell -File` 経由だと配列が 1 つの文字列に潰れる）
python .claude/skills/ui-style/scripts/ui_shot.py cycle <name>        # ビルド→起動→スクショ（ui-style スキル）
```

手で叩く場合:

```
C:/Qt/Tools/CMake_64/bin/cmake.exe --preset msvc-debug
C:/Qt/Tools/CMake_64/bin/cmake.exe --build --preset msvc-debug
C:/Qt/Tools/CMake_64/bin/ctest.exe --preset msvc-debug
```

- バイナリ: `build/msvc-debug/Debug/MegaDirStat.exe`（Release は `--build --preset msvc-release` で
  `build/msvc-debug/Release/`）。単体で起動するには Qt の `bin` を `PATH` に足す（`run.ps1` がやる）。
- ターゲット: `MegaDirStatCore`（`src/core`）、`MegaDirStatMock`（`src/mock`）、`MegaDirStat`
  （`src/ui` + `main.cpp`）、テスト `tst_*`（`tests/`、Qt Test、Core と Mock のみリンク）。
- ソースを追加・削除したら `CMakeLists.txt` に書く（glob は使っていない）。VS ジェネレータは
  `CMakeLists.txt` の変更を検知して次のビルドで自動的に再 configure する。
- 警告: 自前ターゲットは `MegaDirStatWarnings`（MSVC `/W4 /external:W0 /permissive- /utf-8`、
  GCC/Clang `-Wall -Wextra -Wpedantic`）を PRIVATE でリンクする。**`verify.sh` は自分のコードの警告が
  1 つでもあれば失敗する。** moc 生成物の警告はフルビルドでしか出ないので、ヘッダの `Q_OBJECT` 周りを
  触ったら `verify.sh --full` を使う。

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
- **性能は Release ビルドで判断する。** MSVC の Debug は `_ITERATOR_DEBUG_LEVEL=2` 等で桁違いに遅い。

## 検証

- 単体テスト（Qt Test）は `MegaDirStatCore` と `MegaDirStatMock` だけをリンクし、SDK なしで回す。
- 画面の確認はモック起動で行う（`--mock <fixture.json>`、`--mock-generate <件数>`。仕様は §4）。
  **見た目の確認・調整は `ui-style` スキル**（`.claude/skills/ui-style/`）を使う。`ui_shot.py` は
  モック引数なしでは起動を拒否する作りなので、実アカウントが写ることはない。
  **実アカウントでログインしてスクリーンショットを撮らない**（`.screenshots/` は gitignore 済み）。
- `ui_shot.py drive`（マウス・キーボード操作の注入）はユーザーの操作を奪うので、使う前に必ず確認する。
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
