# MegaDirStat 設計メモ

> **状態: 雛形まで実装済み（2026-09-12）。** コア・モック・UI（ツリー＋treemap）が動き、モックデータで
> 起動できる。MEGA SDK の組み込み（§6 の vcpkg 設定と `MegaAccountSource`）は未着手。

MEGA クラウドストレージ版の WinDirStat。アカウント内のフォルダ／ファイルが容量をどう占めているかを、
ツリーと treemap で可視化する。閲覧専用で、ファイル操作（削除・移動・ダウンロード等）は当面持たない。

参考実装: `../MegaExplorer`（同じ作者の MIT プロジェクト。MEGA SDK の組み込み・ビルド構成を流用できる）。

## 1. 要件（初期スコープ）

| # | 要件 | 補足 |
|---|---|---|
| R1 | 画面を上下に分割する | 上下の比率はスプリッタでユーザーが変更可能 |
| R2 | 上ペインはツリー表示。**デフォルトは未展開** | ルート直下のみ見えている状態で起動 |
| R3 | 下ペインは treemap | WinDirStat 同様、面積 = サイズ |
| R4 | **ログインセッションを保存しない** | 起動のたびにログイン。資格情報もセッショントークンもディスクに残さない（§5） |
| R5 | クロスプラットフォーム（Windows / macOS / Linux） | Qt Widgets + CMake |
| R6 | **ログインせずにモックデータで起動できる** | 開発・動作確認用。データ取得層を抽象化して差し替える（§4） |

### スコープ外（当面）

- ファイル操作（削除・移動・リネーム・アップロード・ダウンロード）
- 変更の常時監視・自動更新
- 共有・リンク作成

## 2. 画面構成（Qt Widgets）

```
+--------------------------------------------------+
|  ツールバー（再読込 / ログアウト など）            |
+--------------------------------------------------+
|  ツリー（名前 | サイズ | 割合バー | ファイル数 …） |
|   ▶ Cloud Drive                                   |
|   ▶ Rubbish Bin                                   |
|   ▶ Incoming Shares（要検討）                     |
+==================== QSplitter ===================+
|                                                  |
|                  treemap                         |
|                                                  |
+--------------------------------------------------+
|  ステータスバー（合計容量 / 使用量 / 読込状況）   |
+--------------------------------------------------+
```

- `QMainWindow` の中央に縦方向の `QSplitter`、上に `QTreeView`、下に自前の `TreemapWidget`。
- ツリーのモデルは `QAbstractItemModel` を自前で実装し、§3 のスナップショットを直接参照する。

### ツリー（上）

- 列: 名前、サイズ、親に対する割合（バー表示。`QStyledItemDelegate` で描く）、ファイル数、更新日時。
- 兄弟はサイズ降順で並べる（WinDirStat と同じ既定）。列ヘッダでのソート変更は**未実装**。
- 起動直後は**全て折りたたみ**。展開状態も保存しない。
- スナップショットは全体がメモリにあり、`QTreeView` は展開された行の子しか問い合わせないので、
  `fetchMore` による遅延公開は不要（`setUniformRowHeights(true)` で大量行でも軽い）。

### treemap（下）

- レイアウトは squarified treemap（Bruls et al. 2000）。
- **見た目はフラット**（2026-09-12 決定）。WinDirStat の cushion shading（凸型のグラデーション）は
  古く見えるため採らない。代わりに:
  - 塗りは単色。色はファイル種別（拡張子）ごとで、総バイト数の多い上位 12 種に明暗どちらの
    テーマでも読める中間色を割り当て、それ以外はグレー。
  - セル間に 1px（物理ピクセルで `round(dpr)`）の隙間を入れ、背景色を見せる。
  - 階層はフォルダの内側に 2 論理 px の余白を取り、フォルダ面を背景よりわずかに濃く塗って枠として見せる。
    余白を取ると中身が潰れる小さいフォルダには取らない。
  - 小さすぎて分割できないフォルダは 1 セルとして濃いグレーで塗る。
  - 選択中のノードは OS のハイライト色で縁取る。
- ツリーと双方向に選択を同期する: treemap をクリック → ツリーの該当ノードを展開・選択、
  ツリーで選択 → treemap 上で強調表示。
- レイアウト計算（`src/core/TreemapLayout`、純粋な関数、単体テスト対象）とペイントを分ける。
  描画結果は `QImage` にキャッシュし、リサイズ・パレット変更・スナップショット差し替えのときだけ作り直す。
- UI 文字列は英語で書き、すべて `tr()` を通す（将来の日本語化に備える）。

## 3. データモデル: 読み込み済みスナップショット

MEGA SDK は `fetchNodes` 完了時点でノードツリー全体をメモリに持ち、以降の走査はネットワークを使わない。
また treemap は末端ファイルまで全部必要になる（WinDirStat も全ツリーをメモリに持つ）。そこで
**読み込み完了時に、アプリ独自の不変なツリー（スナップショット）を一度だけ組み立て、UI はそれだけを見る**。

```cpp
// src/core（Qt Widgets にも MEGA SDK にも依存しない）
struct SizeNode
{
    QString name;
    NodeKind kind;              // Folder / File
    qint64 size;                // フォルダは子孫の合計（組み立て時に集計）
    qint64 fileCount;
    QDateTime modified;
    SizeNode* parent;
    std::vector<std::unique_ptr<SizeNode>> children;  // サイズ降順
};
```

- UI 側は SDK の型（`MegaNode` 等）に一切触れない。MEGA 固有の処理はすべて §4 の実装内に閉じる。
- 組み立てはワーカースレッドで行い、完成したものを GUI スレッドに渡す。SDK のコールバックも
  SDK 内部スレッドで来るので、GUI への反映は必ずキュー接続で行う（MegaExplorer の `GuiThread.h` と同じ）。
- メモリは SDK 側と二重になるが、1 ノード百数十バイトとして 64 万ノードで 100 MB 前後に収まる見込み。
  問題が出たら名前のインターン化などで詰める。

## 4. データ取得層の抽象化とモック（R6）

```cpp
// src/core/IAccountSource.h
class IAccountSource : public QObject
{
    Q_OBJECT
public:
    virtual bool requiresLogin() const = 0;
    virtual void login(const QString& email, const QString& password) = 0;
    virtual void load() = 0;          // → progress(...) を何度か、最後に loaded(...) か failed(...)
    virtual void logout() = 0;

signals:
    void loginFinished(bool ok, const QString& error);
    void progress(qint64 done, qint64 total);
    void loaded(std::shared_ptr<const SizeNode> root, const AccountUsage& usage);
    void failed(const QString& error);
};
```

実装は 2 つ:

| 実装 | 置き場所 | 内容 |
|---|---|---|
| `MegaAccountSource` | `src/mega/` | MEGA SDK でログイン → `fetchNodes` → ノードを走査して `SizeNode` を組み立てる。SDK をリンクするのはここだけ |
| `MockAccountSource` | `src/mock/` | ログイン不要。JSON フィクスチャを読むか、指定件数のツリーを乱数で生成する。遅延・失敗も再現できる |

- 選択は `main.cpp`（コンポジションルート）の 1 箇所だけ。起動引数で切り替える:
  - `--mock <fixture.json>` … フィクスチャを読む（`tests/fixtures/` に数種類置く）
  - `--mock-generate <件数> [--seed N]` … 大規模ツリーを生成し、treemap の描画性能を確認する
  - `--mock-delay <ms>` / `--mock-fail` … 読み込み中表示やエラー表示の確認用
  - `--window-size <WxH>` … 初期ウィンドウサイズ（スクリーンショット用）
  - 引数なし … `MegaAccountSource`（ログインダイアログを表示）。**未実装**のため現在はメッセージを出して終了する
- UI・treemap のレイアウト・ツリーモデルの単体テストはすべてモックで行い、実アカウントには触れない。
- MegaExplorer の `megatool`（テスト用アカウントを操作する CLI）は**当面持ってこない**。あちらで必要だった
  理由（保存済みセッションが本番アカウントでないかの確認、無人ループ用のフィクスチャ作成）が、
  セッションを保存せずモックで開発する本プロジェクトには当てはまらないため。実アカウントでの確認は手動で行う。
- フィクスチャの形式（案）:

```json
{
  "usage": { "used": 53687091200, "total": 214748364800 },
  "roots": [
    { "name": "Cloud Drive", "children": [
      { "name": "Photos", "children": [
        { "name": "IMG_0001.jpg", "size": 4200000, "modified": "2025-01-02T10:00:00Z" }
      ]}
    ]},
    { "name": "Rubbish Bin", "children": [] }
  ]
}
```

## 5. セッションを保存しない（R4）と SDK キャッシュの後始末

- `MegaApi::dumpSession()` の結果をどこにも書かない。MegaExplorer の `ISessionStore` /
  `WindowsSessionStore`（DPAPI）に相当するものは**作らない**。これにより MegaExplorer の調査
  （`docs/investigations/STUDY_CROSS_PLATFORM_BUILD.md`）で唯一の OS 依存とされた部分が丸ごと消える。
- 起動のたびに `fetchNodes` でノードツリーをフル取得する（何度も起動するアプリではないため許容）。
  参考: MegaExplorer では 64 万ノードのアカウントでキャッシュなし 385 秒、キャッシュありなら 0.6 秒だった。
  読み込み中は進捗を表示する。
- **SDK の状態キャッシュ**: SDK は `MegaApi` の `basePath` に `megaclient_statecache*.db`
  （ノード情報を含む SQLite）を必ず作る。これを次のように扱う:
  - 置き場所: `QStandardPaths::AppLocalDataLocation` 配下の `sdk-cache/<実行ごとのランダムID>/`。
    `AppDataLocation` は Windows ではローミング側（`%APPDATA%`）なので使わない。
    Unix ではディレクトリを `0700` で作る。
  - **終了時**: `logout()`（サーバー側のセッションも閉じる）→ `MegaApi` 破棄 → 自分のディレクトリを削除。
  - **起動時**: `sdk-cache/` 配下で、自分以外の残骸（異常終了で残ったもの）を削除する。
    同時に 2 つ起動したときに相手の使用中キャッシュを消さないよう、各ディレクトリに `QLockFile` を置き、
    ロックを取れたディレクトリだけを削除対象にする。
- モック起動時は `MegaApi` を作らないので、キャッシュディレクトリも作らない。

## 6. ビルド構成

MegaExplorer の構成を踏襲し、OS ごとのプリセットを最初から並べる。

- Qt 6.11（Core / Gui / Widgets / Test）/ CMake 3.21+ / C++20。
- MEGA SDK は `third_party/sdk` サブモジュール（`v10.17.0`、`add_subdirectory`）、依存は
  `third_party/vcpkg` サブモジュール経由。どちらも MegaExplorer と同じコミットに固定しており、
  vcpkg のバイナリキャッシュ（`%LOCALAPPDATA%\vcpkg\archives`）が効く。
  vcpkg は**浅いクローン不可**（baseline 解決に履歴が要る）。
- ツールチェーン: **Windows = MSVC + Visual Studio ジェネレータ**（SDK が Windows で
  `CMAKE_GENERATOR_TOOLSET` を固定するため Ninja 不可）、Linux = GCC + Ninja、macOS = Clang + Ninja。
  MinGW は SDK が非対応なので使わない。
- `CMakePresets.json` に `msvc-debug` / `linux-debug` / `macos-debug` を用意する（済）。vcpkg 関連の
  変数（`CMAKE_TOOLCHAIN_FILE` / `VCPKG_*`）は SDK を組み込む段階で足す。Linux/macOS プリセットは
  `QT_DIR` 環境変数で Qt の場所を渡す想定で、実機では未検証。
- ターゲット構成（案）:
  - `MegaDirStatCore` … `src/core`（スナップショット、treemap レイアウト、`IAccountSource`）。Qt Core/Gui のみ
  - `MegaDirStatMega` … `src/mega`。SDK をリンクする唯一のターゲット
  - `MegaDirStatMock` … `src/mock`
  - `MegaDirStat` … `src/ui` + `main.cpp`（Widgets）
  - `MegaDirStatTests` … Qt Test。Core と Mock だけをリンクし、SDK のビルドを待たずに回せる
- vcpkg の manifest features は最小限にする。MegaExplorer はサムネイル・動画・PDF のために
  `use-freeimage;use-ffmpeg;use-pdfium;use-libuv` を入れているが、本アプリには不要な見込み。
  外せれば初回ビルドが大幅に短くなる（要検証）。OS 別差分: Linux は `use-readline` を足すか
  `USE_READLINE=OFF`、macOS は `use-openssl` を外す。
- 警告: MSVC は `/W4`、GCC/Clang は `-Wall -Wextra`。自前ターゲットにだけ付ける（SDK には付けない）。

## 7. ライセンス

- 本アプリは **MIT**（`LICENSE`）。
- Qt は LGPLv3 で使う前提。**Qt Charts など GPL 専用モジュールは使わない**（treemap は自前描画）。
- MEGA SDK は BSD-2-Clause。
- `meganz/MEGAsync` のソースは Code Review Licence なので**コードを写さない**（SDK の使い方の参考のみ）。
- 配布物には `LICENSE` と第三者ライセンス表記を同梱する（MegaExplorer の
  `scripts/gen_third_party_notices.py` を流用できる）。

## 8. 決定事項と未決事項

### 決定済み（2026-09-12）

| 論点 | 決定 |
|---|---|
| UI 技術 | **Qt Widgets** |
| SDK 状態キャッシュ | 毎回フル取得。AppData（ローカル）に置き、起動時・終了時に後始末する（§5） |
| ライセンス | **MIT** |
| 開発時の検証 | データ取得層を抽象化し、モックで起動できるようにする（§4）。`megatool` は持ってこない |
| リポジトリ | `main` ブランチ。サブモジュールは `third_party/sdk`（v10.17.0、shallow）と `third_party/vcpkg`（完全履歴） |
| treemap の見た目 | cushion shading をやめ、フラット塗り＋隙間＋フォルダ余白（§2） |
| UI 文字列 | 英語＋`tr()` |

### 未決

| # | 論点 | 選択肢 | メモ |
|---|---|---|---|
| Q3 | 対象ルート | Cloud Drive のみ / Rubbish Bin も / 受信共有も | 容量クォータに効くのは Cloud Drive + Rubbish Bin + バージョン |
| Q4 | ファイルバージョンの扱い | 含める / 含めない / 別表示 | MEGA ではバージョンも使用量に計上される |
| Q5 | Linux / macOS の検証手段 | 実機 / VM / GitHub Actions | 実機がないなら CI で最低限ビルドだけでも通す |
