# Windows / FL Studio — Local Rig

## GitHub Actions で作成する（推奨）

1. この変更をフォークへ反映します。
2. GitHub の **Actions → Windows VST3 → Run workflow** を実行します。
3. ビルド・テストが成功した実行の **Local-Rig-Windows-x64** artifact をダウンロードします。
4. ZIP 内の `Local Rig.vst3` フォルダーを、フォルダーごと
   `C:\Program Files\Common Files\VST3\` にコピーします。管理者権限が必要な場合があります。
5. FL Studio の **Options → Manage plugins → Find installed plugins** で再スキャンし、
   Mixer のエフェクトスロットに **Local Rig** を読み込みます。

ログイン、TONE3000 API キー、ASIO SDK、署名用の秘密情報は不要です。
この workflow は Windows のビルドと自動テストを行いますが、FL Studio 内での実機試験は別途必要です。

## 手元でビルドする

必要なもの：Windows x64、Git、CMake 3.24 以上、Visual Studio 2022 の
**Desktop development with C++**（MSVC v143 と Windows SDK）。
VS の x64 Native Tools PowerShell / Command Prompt から実行します。

```powershell
git submodule update --init --recursive
cmake --preset windows-vst3
cmake --build --preset windows-vst3
ctest --preset windows-vst3
```

成果物：`build-windows\plugin\TONE3000_artefacts\Release\VST3\Local Rig.vst3`。
Linux で生成した同名の VST3 は Windows では使えません。

## 使い方

- NAM／IR を使わず、各エフェクトだけでも動作します。
- `.nam`（A2 アーキテクチャ）／IR `.wav` をドラッグ＆ドロップ、または Load NAM / IR で読み込みます。
- 左のリストでモデルを選択し、順序・レベル・Mix を調整します。
- エフェクトタブの Enabled を ON にします。追加した7エフェクトの初期状態は OFF です。
- Time は ms 指定です。テンポ同期・エフェクトの自由な並べ替え・バイパス後の残響保持は未実装です。
- FL Studio のパラメーター一覧／Last tweaked から各エフェクトをオートメーションできます。
- Preset name に名前を入力して Save preset。モデルデータもプリセットへ埋め込まれます。
- オーディオデバイスと MIDI 入力の設定は FL Studio 側で行います。

通常、プリセットなどのユーザーデータは `%APPDATA%\LocalRig` に保存されます。
既存の TONE3000 インスタンスとは別プラグインです。元のインスタンスを自動置換しません。

## 実機確認

まず小さい音量で、44.1/48/96 kHz、異なるバッファサイズ、複数インスタンス、
プロジェクト保存→再起動→復元、オートメーション、オフライン書き出し、
ディレイ／リバーブの残響まで書き出されることを確認してください。
開発用の自動テストだけでは、FL Studio 固有の動作や最終的な音質評価までは保証できません。
