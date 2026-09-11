---
name: release
description: >-
  Cut a MegaDirStat release from main: bump the minor version, build and
  package the Release zip, tag it, push, and create the GitHub release with gh.
  `/release 0.3.0` overrides the version. Only when the user types /release.
disable-model-invocation: true
---

# /release — リリースを 1 本出す

`main` の今の状態を出す。**`/release` を打ったこと自体が push と GitHub 公開の指示**なので、
途中で承認は取らない（CLAUDE.md の「push の前に確認」はこのスキルでは適用済み）。何かが
失敗したらそこで止まって報告する。

## 1. 前提確認（1 つでも欠けたら何もせずに理由を言って終わる）

```
git rev-parse --abbrev-ref HEAD            # main
git status --porcelain                     # 空
git fetch origin --tags
git rev-list --count HEAD..origin/main     # 0（origin が先行していない）
bash scripts/verify.sh                     # 警告ゼロ＋テスト
```

`HEAD` が `origin/main` より先行しているのはよい（そのコミットごと出る）。

## 2. 版を決めて上げる

- 引数があればそれ（`v` 付きなら剥がす）。
- 無ければ最新タグ（`git describe --tags --abbrev=0`）の **MINOR を 1 上げ、PATCH を 0**
  にする（`v0.2.0` → `0.3.0`）。
- **タグが 1 つも無い（初回）なら、`CMakeLists.txt` の版をそのまま出す**（まだ一度も出して
  いない版なので上げる理由がない）。

`git tag -l vX.Y.Z` が空、`gh release view vX.Y.Z` が not found であることを確かめる。

版を変えるときは、`CMakeLists.txt` の `project(MegaDirStat VERSION ...)`（唯一の版の在処。
`MEGADIRSTAT_VERSION` も zip 名もここから出る）を書き換え、notices を再生成する——
`THIRD-PARTY-NOTICES.txt` にアプリの版が 1 行入っていて、zip に同梱されるため:

```
python scripts/gen_third_party_notices.py
git diff --stat      # CMakeLists.txt と THIRD-PARTY-NOTICES.txt が 1 行ずつのはず
git add CMakeLists.txt THIRD-PARTY-NOTICES.txt
git commit           # Subject: Bump the version to X.Y.Z（trailer は他のコミットに合わせる）
```

notices の差分が版の行以外にも出たら、依存が変わっているということなので止まって理由を見る。
版を変えないとき（初回）も `python scripts/gen_third_party_notices.py --check` は通しておく。

## 3. パッケージ

PowerShell から:

```
./scripts/package.ps1
```

Release ビルド → CPack の zip → 中身の検査 → zip を一時ディレクトリに展開し、Qt を `PATH`
から外して起動、ウィンドウが出ることの確認、までをこのスクリプトがやる。**手で zip を
作らない**（検査が抜ける）。出来上がりは `build/msvc-debug/package/MegaDirStat-X.Y.Z-win64.zip`。
**zip 名の版が 2. の版と一致しているか見る。**

## 4. タグ、push、公開

```
git tag -a vX.Y.Z -m "MegaDirStat X.Y.Z"
git push origin main
git push origin vX.Y.Z
gh release create vX.Y.Z --verify-tag --title "vX.Y.Z" \
    --notes "<1〜2 行>" build/msvc-debug/package/MegaDirStat-X.Y.Z-win64.zip
```

リリース文は**英語で 1〜2 行**。前のタグからの `git log --oneline` を眺めて、ユーザーに見える
変化を一言で書く（初回なら "First release." 程度）。内部の変更しかなければ
"Minor fixes and internal changes." でよい。`--draft` / `--prerelease` は指示されたときだけ。

## 5. 報告

リリース URL、zip 名とサイズ、含まれるコミット数を 2〜3 行で。

## 途中で落ちたとき

push より前ならローカルだけなので戻せるが、**戻す操作（`git tag -d`、`git reset --hard HEAD~1`）
は実行前に見せて確認を取る。** push した後に問題が見つかったら、タグや release を消さずに
次の版を出すのが既定。
