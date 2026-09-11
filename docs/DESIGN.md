# MegaDirStat 設計メモ

> **状態: MEGA 接続まで実装済み（2026-09-12）。** コア・モック・UI（ツリー＋treemap）に加え、MEGA SDK の
> 組み込み、`MegaAccountSource`（ログイン・2FA・fetchNodes・走査）、ログイン画面、SDK キャッシュの後始末が
> 入った。実アカウントでの動作は未確認（ユーザーが確認する）。

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

- レイアウトは **Rows 方式**（KDirStat / WinDirStat の既定と同じ考え方、2026-09-12 に squarified から変更）。
  - フォルダの矩形が横長なら横の行を上から下へ、縦長なら縦の列を左から右へ積む。向きはフォルダごとに 1 回だけ決める。
  - 子はサイズ降順に行へ詰め、次の子が行の太さの 0.4 倍より細くなるところで行を閉じる。
  - 結果として**どのフォルダでも最大のものが左上**に来て、サイズが一方向に読める。squarified は正方形に
    近いセルが得られる代わりに帯の向きが毎回変わり、並びが渦を巻いて見えたためやめた。
  - WinDirStat は GPL、本アプリは MIT なので**本家のコードは写さない**。アルゴリズムの説明から自前で実装している
    （`src/core/TreemapLayout.cpp` の `layoutRows`）。
- **見た目はフラット**（2026-09-12 決定）。WinDirStat の cushion shading（凸型のグラデーション）は
  古く見えるため採らない。代わりに:
  - 塗りは単色。色はファイル種別（拡張子）ごとで、総バイト数の多い上位 12 種に明暗どちらの
    テーマでも読める中間色を割り当て、それ以外はグレー。
  - セル間の隙間は**どこでも一律** 1px（物理ピクセルで `round(dpr)`）。当初はフォルダ内側に余白を取って
    階層を枠で見せていたが、余白が階層ごとに積み重なって境界の太さがまちまちに見えたためやめた。
  - **小さいセルはまとめる**（しきい値 8 論理 px。当初 16px だったが、Rows 方式ではまとめたセルが
    多すぎたため 2026-09-12 にユーザーの指定で 8px に下げた）。1 辺が 8px 未満のフォルダは分割せず 1 セル。
    フォルダ内で面積が 8×8px 未満、または 1 辺が 8px 未満になる子は、それより小さい兄弟ごと
    「N 個の小さな項目」1 セルにまとめる（2 個以上のときだけ）。クリックするとツリーではそのフォルダを
    選択するが、treemap の枠は WinDirStat と同じくクリックしたセルにだけ付ける（フォルダ全体は囲まない）。
  - まとめたセルは、**そのフォルダ（サブツリー全体）でバイト数の最も多い拡張子の色を、スレートグレー
    `#64748B` と半々に混ぜた色**で塗る（2026-09-12 変更）。当初は一律の濃いグレーだったが、寄せ集めが
    「大きな 1 ファイル」に見えて塊の中に穴が空き、並び順が読めなくなったため。混ぜ先を背景色でなく
    固定のグレーにしているので、明暗どちらのテーマでも同じ中間色になる。
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
// src/core/IAccountSource.h（抜粋）
class IAccountSource : public QObject
{
    Q_OBJECT
public:
    enum class LoginResult { Ok, NeedsTwoFactor, Failed };

    virtual bool requiresLogin() const = 0;   // 未ログイン、または初回読み込みの失敗でサインアウトした後
    virtual void login(const QString& email, const QString& password,
                       const QString& twoFactorCode) = 0;   // 初回はコード空。NeedsTwoFactor なら同じ資格情報＋コードで再度
    virtual void load() = 0;          // → progress(...) を何度か、最後に loaded(...) か failed(...)
    virtual void logout() = 0;        // 終了前に 1 回（main.cpp）。MEGA は最大 5 秒ブロックする

signals:
    void loginFinished(IAccountSource::LoginResult result, const QString& error);
    void progress(const QString& stage, const QString& detail, qint64 done, qint64 total);  // total -1 = 不明
    void loaded(SnapshotPtr snapshot);
    void failed(const QString& error);
};
```

- ソースは資格情報を保持しない。2FA の再試行で使うメール・パスワードはログイン画面の入力欄にあるものを
  もう一度渡す（ログイン成功でパスワード欄は消す）。
- エラー文言（`error`）と進捗の文言（`stage` / `detail`）はソースが `tr()` で作る。SDK のエラーコードを
  知っているのはソースだけなので。

### MEGA の読み込みの流れ（`MegaAccountSource`）

MegaExplorer の `AuthController` / `LoginView.qml` と同じ段階表示にしている。

| 段階 | 表示 | 進捗 |
|---|---|---|
| ログイン（`login` / `multiFactorAuthLogin`） | Signing you in… | ビジー |
| `fetchNodes` 送信〜応答長が分かるまで | Requesting your file list… | ビジー |
| 応答のダウンロード | Downloading your file list…（`12 MB of 40 MB`） | バイト数 |
| 更新が 8 秒止まる、または受信完了 | Decrypting your file list… | ビジー（SDK は復号中の進捗を出さない） |
| `SizeNode` の組み立て（ワーカースレッド） | Measuring folders…（`N items`） | 走査済みノード数 / `getNumNodes()` |

- 走査は Cloud Drive と Rubbish Bin の `getChildren` 再帰（Q3）。バージョンは含まない（Q4、`getChildren` は
  現行版だけを返す）。`getNumNodes()` はバージョン・Vault・共有も数えるので、バーは 100% の手前で終わる。
- 使用量（`AccountUsage`）は走査と並行して `getAccountDetails` で取る。失敗しても読み込みは失敗にせず、
  使用量を不明（-1）にする。
- 再読込（2 回目以降の `load()`）は `fetchNodes` をやり直さず、`catchup` で届いている変更を反映させてから
  メモリ上のツリーを走査し直す。
- 初回の `fetchNodes` が失敗したらサーバー側もログアウトしてから `failed` を出す。UI はサインイン画面に
  エラーを出して戻る。
- 2FA の判定: `login` が `API_EMFAREQUIRED` → `NeedsTwoFactor`。コード付きの試行が `API_ENOENT` /
  `API_EFAILED` / `API_EEXPIRED` → 「コードが違う」（megaapi.h に専用コードがないため。MegaExplorer と同じ）。

### ログイン画面（`LoginView`）

- **ダイアログではなくウィンドウ内のページ**（2026-09-12 決定）。サインイン → 2FA コード → 読み込み中 →
  （モックのみ）読み込みエラー、を 1 つの `QStackedWidget` で切り替える。ページの高さは共通なので切り替えで
  フォームが跳ねない。最初のスナップショットが届いたらツリー＋treemap に切り替え、以後の再読込は
  ステータスバーで進捗を出す。MegaExplorer の LoginView と同じ構成で、ログインに続く数分の読み込み待ちを
  同じ場所で見せられるため。
- 2FA ページは 6 桁の数字入力（バリデータ付き）と Back / Confirm。自動送信はしない（MegaExplorer と同じ）。
- ログイン中・読み込み中にキャンセルする手段はない（MegaExplorer と同じ）。ウィンドウを閉じれば終わる。

実装は 2 つ:

| 実装 | 置き場所 | 内容 |
|---|---|---|
| `MegaAccountSource` | `src/mega/` | MEGA SDK でログイン → `fetchNodes` → ノードを走査して `SizeNode` を組み立てる。SDK をリンクするのはここだけ |
| `MockAccountSource` | `src/mock/` | ログイン不要。JSON フィクスチャを読むか、指定件数のツリーを乱数で生成する。遅延・失敗も再現できる |

- 選択は `main.cpp`（コンポジションルート）の 1 箇所だけ。起動引数で切り替える:
  - `--mock <fixture.json>` … フィクスチャを読む（`tests/fixtures/` に数種類置く）
  - `--mock-generate <件数> [--seed N]` … 大規模ツリーを生成し、treemap の描画性能を確認する
  - `--mock-delay <ms>` / `--mock-fail` … 読み込み中表示やエラー表示の確認用
  - `--mock-login` … サインイン画面から始める（どの資格情報でも通る。パスワード `wrong` だけ失敗）
  - `--mock-2fa` … `--mock-login` に加えて 2FA コードを求める（`123456` だけ通る）
  - `--window-size <WxH>` … 初期ウィンドウサイズ（スクリーンショット用）
  - 引数なし … `MegaAccountSource`（サインイン画面を表示）
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
  - 実装は `src/core/RunCacheDir`（SDK に依存しないので単体テスト付き）。ロックはディレクトリの中ではなく
    隣の `<id>.lock` に置く（中に置くとディレクトリを消す前にロックを外す必要がある）。ロックを先に取って
    からディレクトリを作るので、ロックのないディレクトリは持ち主がいない残骸とみなして消す。
    `setStaleLockTime(0)` で「古いロック」の判定をプロセスの生存確認だけにしている（既定の 30 秒だと、
    長く開いている別インスタンスのロックを Unix では奪えてしまう）。消すのは 32 桁の 16 進名だけ。
  - 終了処理は `main.cpp` で `app.exec()` の後（ウィンドウが消えてから）に `logout()` を呼ぶ。ログアウトは
    オフライン時に固まらないよう 5 秒で打ち切り、`MegaApi` を破棄（SDK スレッドの join と DB のクローズ）
    してからディレクトリを消す。
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
  変数（`CMAKE_TOOLCHAIN_FILE` / `VCPKG_*`）は共通の `base` プリセットに、トリプレットと features は
  OS ごとに置いている。Linux/macOS プリセットは `QT_DIR` 環境変数で Qt の場所を渡す想定で、実機では未検証。
- **manifest features は `use-openssl` だけ**（2026-09-12、Windows で確認）。`USE_FREEIMAGE` / `USE_FFMPEG` /
  `USE_PDFIUM` / `USE_LIBUV` / `USE_READLINE` を OFF にしている。これで vcpkg の依存は 16 パッケージ
  （cryptopp, curl, icu, libsodium, sqlite3, openssl など）になり、すべて静的リンクで DLL が出ない。
  FreeImage を外したので `ENABLE_ISOLATED_GFX`（gfxworker）も既定で OFF になり、MegaExplorer の BUILD.md
  にある swscale のリンク回避策も不要。初回 configure は MegaExplorer とバイナリキャッシュを共有して 70 秒、
  SDK 込みの初回ビルドは約 3.5 分（Debug）。
- `SDKlib` には `SYSTEM` プロパティを付けている（CMake 3.25+）。利用側から SDK のヘッダがシステム扱いに
  なり、自前ターゲットの `/W4` が megaapi.h の警告を拾わない。
- SDK を `add_subdirectory` すると実行ファイルとテストの出力先が `build/msvc-debug/<Config>/` にまとまる
  （テストも `tests/<Config>/` ではなくここに出る）。
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
| treemap の見た目 | cushion shading をやめ、フラット塗り＋一律 1px の隙間。8px 未満はまとめる（§2） |
| treemap のレイアウト | squarified をやめ Rows 方式（大きいものが左上）。切り替え機能は持たない（§2） |
| UI 文字列 | 英語＋`tr()` |
| ログイン UI | ダイアログでなくウィンドウ内のページ（サインイン / 2FA / 読み込み中）。MegaExplorer と同じ段階表示（§4） |
| 2FA | 認証アプリの 6 桁コード（`multiFactorAuthLogin`）。資格情報はソースに保持せず、UI の入力欄から再送 |
| 再読込 | `fetchNodes` をやり直さず `catchup` → 再走査（§4） |
| SDK の features | `use-openssl` のみ。サムネイル・プレビュー・ローカルサーバー系は OFF（§6） |

### 未決

| # | 論点 | 選択肢 | メモ |
|---|---|---|---|
| Q3 | 対象ルート | Cloud Drive のみ / Rubbish Bin も / 受信共有も | 容量クォータに効くのは Cloud Drive + Rubbish Bin + バージョン |
| Q4 | ファイルバージョンの扱い | 含める / 含めない / 別表示 | MEGA ではバージョンも使用量に計上される |
| Q5 | Linux / macOS の検証手段 | 実機 / VM / GitHub Actions | 実機がないなら CI で最低限ビルドだけでも通す |
