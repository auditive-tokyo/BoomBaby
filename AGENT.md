# BoomBaby Project

## プロジェクト概要

BoomBabyは、Boz LabのSasquatchとHim DSPのKick Ninjaから自分が使う機能だけを集約したキックエンハンスプラグインです。

## プラグイン構成

BoomBabyは3つのモジュールで構成されています：

1. **Sub** - サブ周波数の生成
2. **Click** - アタック部分の生成（Noise / Sample）
3. **Direct** - オリジナルのキック音（入力信号）またはSample Load

これらを組み合わせることで、より豊かで立体的なキックサウンドを実現します。

## 対応フォーマット

- AUv2
- VST3
- Standalone

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
- **コード改修後の必須確認手順:** 変更を行ったら必ず `make check && make lint` を実行し、問題がなければ `make run` で動作確認を行うこと。
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

- **マスターリミッター（検討中）**
  - 目的: マスター出力段にブリックウォールリミッターを挿入し、クリッピングを防止
  - 候補方式:
    1. **ソフトニー方式（推奨）**: Attack/Release エンベロープフォロワーでゲインリダクション計算。CPU 負荷ほぼゼロ、~80行。`LevelDetector` を流用可能
    2. **ハードクリップ**: `jlimit()` で振幅を制限。5行で実装可能だが歪みが出る
    3. **Look-ahead 方式**: 高品質だが遅延補正が必要になり複雑度が上がる
  - しきい値: -0.1 dBFS 固定か、MasterFader に Ceiling ノブを追加するか要検討
  - 実装箇所: `PluginProcessor::processBlock()` の `applyGain()` 直後
  - GUI: MasterFader のメーターにゲインリダクション量（GR）表示を追加できる

- **プリセット管理 + デフォルトプリセット**
  - 目的: 全パラメーター（エンベロープ含む）の初期値をハードコードではなく設定ファイルとして管理。ユーザーリセット・将来のプリセット追加に対応
  - 方式: **JUCE `BinaryData` 埋め込み**（外部ファイル依存なし、単一バイナリ配布を維持）
  - ディレクトリ構成:
    ```
    Resources/presets/
      default.xml       ← 出荷時デフォルト
      // 将来: fat_kick.xml, punchy.xml ...
    ```
  - CMakeLists.txt に追加:
    ```cmake
    juce_add_binary_data(BoomBabyBinaryData
        SOURCES Resources/presets/default.xml)
    ```
  - 実装手順:
    1. 全パラメーターを望ましい初期値にしてプラグインを起動
    2. `getStateInformation()` で XML をダンプして `Resources/presets/default.xml` として保存（フォーマット一致が保証される）
    3. `BinaryData` に追加
    4. `PluginProcessor` コンストラクタで `loadPresetFromXml(BinaryData::default_xml, ...)` を呼ぶ
    5. `setStateInformation()` に fallback: DAW データがなければ default を読む
  - 利点:
    - プリセット読み込みと DAW セッション復元が単一コードパスで統一
    - デフォルト値の変更がコード修正不要（XML 差し替えのみ）
    - ユーザーが「Default」を選ぶだけで全パラメーターをリセット可能

- **Undoが、envelopeと波形長view変更に対して効かない問題を調査する**
  - 概要: ノブや各種パラメータにはUndoが効くが、Envelopeの編集ではUndoできない
  - subのlengthや、click/directのdecayを変更してcmd zすると、値は正しく戻るが、波形そのもの、そして波形長が戻らない。波形長の方は単純に波形の長さに合わせてるだけかも？
  - 見た目の問題のみ。

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
