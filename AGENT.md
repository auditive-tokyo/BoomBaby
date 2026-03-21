# BoomBaby Project Rules

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
- **BinaryData 追加後の clangd エラー解消:** `juce_add_binary_data` に新ファイルを追加した後、clangd が `No member named '...' in namespace 'BinaryData'` を報告する場合は、`cd build-clangd && make BoomBabyPresets` を実行すること。これは CMake が `build-clangd/` に生成した Makefile のターゲットで、BinaryData の `.h`/`.cpp` を生成する。プロジェクトルートの `Makefile` にはないコマンド。実行後に VS Code で「Restart Language Server」を行うと赤波線が消える。

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
│   ├── PresetBar.h            // プリセットバー UI（[◀] 名前 [▶] [Save]、ヘッダオンリー）
│   ├── SampleChooserUtils.h   // サンプル選択ファイルチューザーユーティリティ（ヘッダオンリー）
│   ├── SubParams.cpp          // Sub パネル UI セットアップ / レイアウト
│   ├── UIConstants.h          // UI定数集約（色・レイアウト寸法・LabelSelector・SlopeSelector 等）
│   └── WaveformUtils.h        // 波形プレビュー描画ヘルパー（ClickParams/DirectParams 共通、ヘッダオンリー）
├── ParamIDs.h                 // APVTSパラメーターID定数集約（ヘッダオンリー）
├── PluginEditor.cpp
├── PluginEditor.h
├── PluginProcessor.cpp
├── PluginProcessor.h
├── PresetManager.cpp          // プリセット保存・読み込み・ナビゲーション・Factory展開
└── PresetManager.h            // PresetManager 宣言
```

## 進行中（時間があるときに進める）

- **ファクトリープリセット追加**（随時追加中）
  - プラグインを起動した状態で音を作り、PresetBar の **[Save]** で保存
  - 保存された `.bbpreset/state.xml` を `Resources/factory_presets/<名前>_state.xml` にコピー
  - `CMakeLists.txt` の `juce_add_binary_data(BoomBabyPresets SOURCES ...)` に追加
  - `Source/PresetManager.cpp` の `expandFactoryPresets()` 内 `entries` 配列に1行追加:
    ```cpp
    const std::array<FactoryEntry, N> entries = {{
        {"default", BinaryData::default_state_xml, BinaryData::default_state_xmlSize},
        {"<名前>",  BinaryData::<名前>_state_xml,  BinaryData::<名前>_state_xmlSize},
    }};
    ```
  - `std::array` のテンプレート引数 `N` をプリセット数に合わせて更新
  - `make cmake && make check` で確認
  - clangd エラーが出る場合は `cd build-clangd && make BoomBabyPresets` → VS Code で「Restart Language Server」
