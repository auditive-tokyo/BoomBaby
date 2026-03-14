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
│   ├── KeyboardComponent.cpp  // 鍵盤UI実装
│   ├── KeyboardComponent.h    // 鍵盤UI宣言
│   ├── LutBaker.h             // Sub波形 LUT ベイク処理（ヘッダオンリー）
│   ├── MasterFader.cpp        // マスターフェーダー実装（横向きフェーダー＋L/Rメーター）
│   ├── MasterFader.h          // マスターフェーダー宣言
│   ├── PanelComponent.cpp     // SUB/CLICK/DIRECT共通パネル実装
│   ├── PanelComponent.h       // 共通パネル宣言（ChannelFader・M/S ボタン）
│   ├── SubParams.cpp          // Sub パネル UI セットアップ / レイアウト
│   ├── UIConstants.h          // UI定数集約（色・レイアウト寸法）
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

- **Infobox（ホバー説明 UI）（検討中）**
  - 目的: キーボード右のスペースに固定の説明エリアを設置し、各ノブ・パラメーターにホバーしたときに使い方を表示
  - 実装方針: **中央集権型文字列テーブル + カスタム `InfoBox` コンポーネント**
    - `Source/GUI/InfoBox.h / InfoBox.cpp`: 右端固定の表示コンポーネント
    - `Source/GUI/InfoStrings.h`: ID → 説明文のテーブル（`std::unordered_map`、バイナリ埋め込み）
    - 各コンポーネントに `setComponentID("sub_level")` 等を設定し、汎用 `mouseEnter` / `mouseExit` ハンドラで `infoBox.show(id)` / `infoBox.hide()` を呼ぶ
  - 外部ファイル（.json / .txt）方式は配布・パス解決が面倒なので採用しない
  - `juce::TooltipWindow` はスタイル固定で右端常駐 UI に不向きなので採用しない

- **CI / CD パイプライン構築**
  - 目的: GitHub Actions でビルド・静的解析を自動化し、プッシュごとに品質を担保する
  - 優先タスク:
    1. **`compile_commands.json` の CI 生成**: SonarQube Cloud が Compilation Database モードで解析できるよう、スキャン前に以下を実行するステップを追加
       ```yaml
       - name: Generate compile_commands.json
         run: cmake -S . -B build-clangd -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
       ```
    2. **SonarQube Cloud スキャンステップ**: `sonar.cfamily.compile-commands=build-clangd/compile_commands.json` を指定して精度の高い C++ 解析を実現
    3. **ビルド検証ステップ**: macOS runner で `make check` を実行し、コンパイルエラー・ワーニングを PR ごとにチェック
  - SonarQube Cloud 設定変更（管理画面）:
    - C file suffixes: `.c`（`.h` を削除）
    - C++ file suffixes: `.cpp`, `.h`（`.h` を追加）
    - `sonar.cfamily.analysisMode=compileCommands`

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

- **LUT 解像度劣化の修正（Click/Direct Amp エンベロープ）**
  - 問題: 512 点の LUT が decay 全体にマッピングされるため、エンベロープが短い区間に集中していると decay を大きくした際に解像度が劣化し音が変わる（Sub では `effectiveLutDuration` で修正済み）
  - Click/Direct は `computeMaxTimeSamples()` が `getLutDurationMs()` を停止判定に使うため、Sub と同じ手法だと停止タイミングまで縮まって decay が効かなくなる
  - 修正方針: Click/Direct にも Sub 同様の独立した停止判定用 duration を持たせ、LUT duration と停止判定を分離する
  - 実装手順:
    1. **ClickEngine**: Sample モード用に `sampleDecayMs_` atomic を新設（既存 `decayMs_` は Noise 専用）。`computeMaxTimeSamples()` の mode==2 で `sampleDecayMs_` を参照するよう変更。`setSampleDecayMs(float)` セッター追加
    2. **DirectEngine**: `maxDurationMs_` atomic を新設。`computeMaxTimeSamples()` で `maxDurationMs_` を参照するよう変更。`setMaxDurationMs(float)` セッター追加
    3. **Processor 側**: `applyClickParam` に `clickSampleDecay` → `setSampleDecayMs()` ハンドラを追加。`applyDirectParam` に `directDecay` → `setMaxDurationMs()` ハンドラを追加。`prepareToPlay` で初期値を適用
    4. **Editor 側**: 全 `bakeLut` 呼び出しで clickAmp/directAmp に `effectiveLutDuration` を適用（前回と同じ変更）
    5. ビルド＋動作確認

- **波形表示の長さを全チャンネル最長に合わせる**
  - 現状: `envelopeCurveEditor.setDisplayDurationMs()` が Sub の Length のみ参照
  - 変更: Sub Length / Click Sample Decay / Direct Decay の中で最長の値を `displayDurationMs` として使う
  - 実装手順:
    1. ヘルパー関数 `updateDisplayDuration()` を作成し、3 値の `std::max` を `setDisplayDurationMs()` に渡す
    2. Sub Length / Click Sample Decay / Direct Decay の各 `onValueChange` と `syncUIFromState()` / `onEnvelopeChanged()` からこのヘルパーを呼ぶ
    3. 既存の `setDisplayDurationMs(v)` 単発呼び出しを置き換え

- **Decay ダブルクリックデフォルト値の修正**
  - 現状: Click Sample Decay / Direct Decay のダブルクリックリターン値が Sub Length に連動して変わる
  - 変更: Sub Length への連動を廃止し、固定デフォルト値にする
    - Click Noise Decay: 30ms 固定（変更なし、現状通り）
    - Click Sample Decay: 300ms 固定（ダブルクリックで 300ms に戻る）
    - Direct Decay: 300ms 固定（ダブルクリックで 300ms に戻る）
  - 実装: SubParams.cpp の `length.slider.onValueChange` から `setDoubleClickReturnValue` 連動を削除。ClickParams.cpp / DirectParams.cpp の初期化で `setDoubleClickReturnValue(true, 300.0)` を固定で設定
