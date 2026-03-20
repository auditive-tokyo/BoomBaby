# BoomBaby Project

## Overview

BoomBaby is a personal kick enhancement plugin built for one specific use case. The developer was a long-time Sasquatch (Boz Labs) user who switched to an Apple M4 Pro Mac in mid-2025, only to find that Sasquatch required Rosetta.

After searching for a native alternative, Kick Ninja (Him DSP) turned up — a genuinely great plugin, but a VST Instrument rather than a VST Effect. For a producer who frequently works with pre-bounced kicks in collabs and mixing sessions, having to route through MIDI just to process an audio file was too much friction.

Rather than compromise, the developer analyzed both plugins by ear, extracted the features that actually mattered, and built a single streamlined plugin tailored exactly to the developer's own workflow. The beta is released as open source — with the hope that it might be useful to talented beatmakers and producers around the world.

## プラグイン構成

BoomBabyは3つのモジュールで構成されています：

1. **Sub** - サブ周波数の生成
2. **Click** - アタック部分の生成（Noise / Sample）
3. **Direct** - オリジナルのキック音（入力信号）またはSample Load

これらを組み合わせることで、より豊かで立体的なキックサウンドを実現します。

## 対応フォーマット

- AUv2
- VST3

## ビルド環境

- macOS
- CMake/Xcode
- C++23
- JUCE Framework

## エージェント動作ルール (必須)

- 変更を加える前に必ずユーザーに確認を取ること。ユーザーの明示的な承認がない限り、コードや設定を変更しない。
- ユーザーが指示したことだけを実行すること。提案や代替案は提示しても、実行はユーザーの許可がある場合のみ行う。
- 推測や勝手な想像でコードを書き換えないこと。意図が不明な場合は必ず質問して確認する。
- 変更を行う際は、どのファイルを、なぜ、どう修正したかを簡潔に報告し、必要ならビルド／確認手順を添えること。
- **コード改修後の必須確認手順:** 変更を行ったら必ず `make check && make lint` を実行し、問題がなければ `make test` でユニットテストが通ることを確認してから `make run` で動作確認を行うこと。
- **CMake／ファイル構成変更時の手順:** 新規ファイルの追加・削除、または `CMakeLists.txt` の変更を行った場合は、まず `make cmake` を実行してプロジェクトを再生成し、その後に `make check && make lint` → `make run` の順で確認すること。

## JUCE Documentation

This project uses JUCE framework. An MCP server (`juce-docs`) is available.

- When implementing JUCE classes, **always** look up the class docs first using the `get-juce-class-docs` tool
- When unsure which class to use, use `search-juce-classes` to explore options
- Prefer the MCP docs over your training data, as they reflect the actual installed version

## 描画方針

- **現在**: CPU描画（`paint()` ベース）が主体
  - `EnvelopeCurveEditor`, `CustomSliderLAF`, `LevelMeter` 等
- **GPU化の切り替え**: `openGLContext.attachTo(*this)` を `PluginEditor` コンストラクタに1行追加するだけで、全ての `paint()` コードがそのままGPU加速される（JUCEの `Graphics` APIがバックエンドを抽象化）
- **結論**: 今は `paint()` で実装し続けて問題なし。将来パフォーマンス要求が上がった場合でも、既存コードの変更なく GPU化可能

## Source構成（概要）

`tree` の生出力を貼り付け（更新時は `cd Source && tree` を再実行して置き換える）:

```text
.
├── DSP
│   ├── ChannelState.h         // チャンネルMute/Solo/レベル管理（ヘッダオンリー）
│   ├── ClickEngine.cpp        // Click DSP 実装（Noise/Sample モード、BPF1カスケード、HPF/LPF）
│   ├── ClickEngine.h          // Click DSP 宣言
│   ├── DirectEngine.cpp       // Direct DSP 実装（入力パススルー / サンプル再生）
│   ├── DirectEngine.h         // Direct DSP 宣言
│   ├── EnvelopeData.h         // エンベロープデータモデル（Catmull-Rom・ヘッダオンリー）
│   ├── EnvelopeLutManager.h   // LUTダブルバッファ管理（ヘッダオンリー、ロックフリー）
│   ├── LevelDetector.h        // ロックフリーピーク検出（ヘッダオンリー）
│   ├── SamplePlayer.cpp       // サンプル再生エンジン実装
│   ├── SamplePlayer.h         // サンプル再生エンジン宣言
│   ├── Saturator.h            // Drive + ClipType 共通 DSP ヘルパー（ヘッダオンリー、Sub/Click/Direct 共用）
│   ├── SubEngine.cpp          // Sub DSP 実装（Wavetable OSC、LUT 駆動）
│   ├── SubEngine.h            // Sub DSP 宣言
│   ├── SubOscillator.cpp      // Sub用Wavetable OSC実装
│   ├── SubOscillator.h        // Sub用Wavetable OSC宣言
│   └── TransientDetector.h    // トランジェント検出（ヘッダオンリー、Auto Trigger 用）
├── GUI
│   ├── ChannelFader.cpp       // チャンネルフェーダー実装（メーター＋フェーダー一体）
│   ├── ChannelFader.h         // チャンネルフェーダー宣言（Sub/Click/Direct 共通）
│   ├── ClickModeStateUtils.h  // Clickモード状態保存・復元ユーティリティ（ヘッダオンリー）
│   ├── ClickParams.cpp        // Click パネル UI セットアップ / レイアウト
│   ├── CustomSliderLAF.h      // ノブ描画LookAndFeel（グラデーション/値表示）+ CustomSlider（自然スクロール対応）
│   ├── DirectParams.cpp       // Direct パネル UI セットアップ / レイアウト
│   ├── EnvelopeCurveEditor.cpp // エンベロープカーブエディタ実装
│   ├── EnvelopeCurveEditor.h  // エンベロープカーブエディタ宣言
│   ├── InfoBox.cpp            // ホバー時の説明テキスト表示コンポーネント実装
│   ├── InfoBox.h              // InfoBox 宣言
│   ├── InfoBoxText.h          // InfoBox表示テキスト定数集約（InfoText 名前空間、ヘッダオンリー）
│   ├── KeyboardComponent.cpp  // 鍵盤UI実装
│   ├── KeyboardComponent.h    // 鍵盤UI宣言
│   ├── LutBaker.h             // Sub波形 LUT ベイク処理（ヘッダオンリー）
│   ├── MasterFader.cpp        // マスターフェーダー実装（横向きフェーダー＋L/Rメーター）
│   ├── MasterFader.h          // マスターフェーダー宣言
│   ├── PanelComponent.cpp     // SUB/CLICK/DIRECT共通パネル実装
│   ├── PanelComponent.h       // 共通パネル宣言（ChannelFader・M/S ボタン）
│   ├── SampleChooserUtils.h   // サンプル選択ファイルチューザーユーティリティ（ヘッダオンリー）
│   ├── SubParams.cpp          // Sub パネル UI セットアップ / レイアウト
│   ├── UIConstants.h          // UI定数集約（色・レイアウト寸法・LabelSelector・SlopeSelector 等）
│   └── WaveformUtils.h        // 波形プレビュー描画ヘルパー（ClickParams/DirectParams 共通、ヘッダオンリー）
├── ParamIDs.h                 // APVTSパラメーターID定数集約（ヘッダオンリー）
├── PluginEditor.cpp
├── PluginEditor.h
├── PluginProcessor.cpp
└── PluginProcessor.h
```

## TODO

- **プリセットシステム（サンプル込み）**
  - 目的: パラメーター＋エンベロープ＋サンプルを一括で保存・呼び出し。ファクトリープリセットとユーザープリセットの両方に対応
  - **プリセットフォーマット: `.bbpreset` フォルダ**
    ```
    Fat Kick.bbpreset/
      state.xml           ← getStateInformation 同一フォーマット（サンプルパスは相対化）
      click_sample.wav    ← あれば（元ファイルのコピー）
      direct_sample.wav   ← あれば（元ファイルのコピー）
    ```
  - **保存先ディレクトリ（クロスプラットフォーム）**
    ```
    <userDocumentsDirectory>/Auditive/BoomBaby/Presets/
    ```
    `juce::File::getSpecialLocation(userDocumentsDirectory)` で OS 自動解決:
    - macOS: `~/Documents/Auditive/BoomBaby/Presets/`
    - Windows: `C:\Users\<name>\Documents\Auditive\BoomBaby\Presets\`
    - 初回起動時に `createDirectory()` で作成（インストーラー不要）
  - **ファクトリープリセット**
    - `Resources/presets/` にソース管理
    - `BinaryData` としてバイナリに埋め込み
    - **バージョンチェック方式で展開** — `Factory/.version` ファイルにプラグインバージョンを記録し、不一致時に再展開:
      ```cpp
      auto factoryDir = presetsDir.getChildFile("Factory");
      auto versionFile = factoryDir.getChildFile(".version");
      const auto current = juce::String(ProjectInfo::versionString);

      if (!versionFile.existsAsFile()
          || versionFile.loadFileAsString().trim() != current) {
          factoryDir.createDirectory();
          expandFactoryPresets(factoryDir);  // BinaryData → .bbpreset 展開
          versionFile.replaceWithText(current);
      }
      ```
    - 動作:
      | 状況 | `.version` | Factory フォルダ | 動作 |
      |------|-----------|-----------------|------|
      | 初インストール | なし | なし | 作成＆展開 |
      | プラグイン更新 | 旧バージョン | あり | 再展開（新プリセット追加対応） |
      | 同一バージョン起動 | 一致 | あり | **何もしない** |
      | ユーザーが Factory 削除 | なし | なし | 再作成＆再展開 |
    - ユーザープリセットは `<Presets>/` 直下なので Factory 再展開の影響を受けない
  - **プリセット保存フロー（DAW 上で音作り → 保存）**
    1. DAW でパラメーター / サンプル / エンベロープを設定
    2. UI 上部の **[Save]** をクリック → プリセット名入力
    3. `<Presets>/<名前>.bbpreset/` フォルダを作成
    4. `state.xml` 書き出し（サンプルパスは `./click_sample.wav` に相対化）
    5. Click / Direct のサンプルファイルをプリセットフォルダにコピー
  - **プリセット読み込みフロー**
    1. UI 上部のブラウザからプリセット選択
    2. `state.xml` を読み込み → `setStateInformation` 相当の処理
    3. サンプルパスを `.bbpreset/` フォルダ内の実体に解決してロード
  - **UI: プリセットバー（プラグイン最上部）**
    ```
    ┌──────────────────────────────────────────────────┐
    │ [◀] Fat Kick                          [▶] [Save] │
    ├──────────────────────────────────────────────────┤
    │  SUB  │  CLICK  │  DIRECT                        │
    ```
    - `[◀][▶]` で前後プリセット切替
    - プリセット名クリック → ドロップダウンリスト（Factory / User セクション分け）
    - `[Save]` で現在の状態をユーザープリセットとして保存
  - **実装フェーズ**
    - Phase 1: プリセットフォルダ構造 + 保存 / 読み込みロジック + UI プリセットバー
    - Phase 2: ファクトリープリセット（BinaryData 埋め込み + 初回展開）
    - Phase 3: プリセット削除 / リネーム / 上書き確認

- **ユニットテスト導入**
  - フレームワーク: **Catch2 v3**（`FetchContent` で取得、ヘッダ軽量、CTest/CI 親和性高）
  - CMake 構成:
    - DSP ソース（`Source/DSP/*.cpp`）を `add_library(BoomBabyDSP OBJECT ...)` として分離
    - プラグインターゲット・テストターゲット両方がリンク（ソース構造の変更なし）
    - `Tests/` ディレクトリにテストファイルを配置
    - `make test` で CTest 経由実行
  - テスト対象（DSP — 必須）:
    - `Saturator`: 零入力→零出力（Tube bias 回帰テスト）、全ClipTypeの単調性、drive=0で恒等性
    - `EnvelopeLutManager`: `computeAmp` の値域（0〜1）、duration外は0、LUT切替の整合性
    - `ClickEngine`: triggerNote→フィルターリセット、silence in→silence out
    - `DirectEngine`: renderPassthrough で入力ゼロ→出力ゼロ（Tube bias 回帰テスト）
    - `SubEngine`: render出力の値域、周波数精度
    - `LevelDetector`: ピーク検出の正確性、リリース特性
  - テスト対象（GUI純関数 — 推奨）:
    - `WaveformUtils::computePreview()`: 境界値、時間外アクセス
    - `LutBaker` のベイク関数: 入出力サイズ整合性
    - Component 描画テスト: **スキップ**（MessageManager 依存、費用対効果低）
  - CI: GitHub Actions で `make test` + SonarQube 連携
