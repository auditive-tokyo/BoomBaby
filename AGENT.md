# BabySquatch Project

## プロジェクト概要

BabySquatchは、Boz LabのSasquatchから自分が使う機能だけを集約したキックエンハンスプラグインです。

## プラグイン構成

BabySquatchは3つのモジュールで構成されています：

1. **Sub** - サブ周波数の生成
2. **Click** - アタック部分の生成（Tone / Noise / Sample）
3. **Direct** - オリジナルのキック音（入力信号）またはSample Load

これらを組み合わせることで、より豊かで立体的なキックサウンドを実現します。

## 対応フォーマット

- AU
- VST3
- Standalone

## ビルド環境

- macOS
- CMake/Xcode
- C++20
- JUCE Framework

## エージェント動作ルール (必須)

- 変更を加える前に必ずユーザーに確認を取ること。ユーザーの明示的な承認がない限り、コードや設定を変更しない。
- ユーザーが指示したことだけを実行すること。提案や代替案は提示しても、実行はユーザーの許可がある場合のみ行う。
- 推測や勝手な想像でコードを書き換えないこと。意図が不明な場合は必ず質問して確認する。
- 変更を行う際は、どのファイルを、なぜ、どう修正したかを簡潔に報告し、必要ならビルド／確認手順を添えること。
- **コード改修後の必須確認手順:** 変更を行ったら必ず `make check && make lint` を実行し、問題がなければ `make run` で動作確認を行うこと。
- **CMake／ファイル構成変更時の手順:** 新規ファイルの追加・削除、または `CMakeLists.txt` の変更を行った場合は、まず `make cmake` を実行してプロジェクトを再生成し、その後に `make check && make lint` → `make run` の順で確認すること。

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
│   ├── ClickEngine.cpp        // Click DSP 実装（Tone/Noise モード、BPF × 2、HPF/LPF）
│   ├── ClickEngine.h          // Click DSP 宣言
│   ├── EnvelopeData.h         // エンベロープデータモデル（Catmull-Rom・ヘッダオンリー）
│   ├── EnvelopeLutManager.h   // LUTダブルバッファ管理（ヘッダオンリー、ロックフリー）
│   ├── LevelDetector.h        // ロックフリーピーク検出（ヘッダオンリー）
│   ├── SubEngine.cpp          // Sub DSP 実装（Wavetable OSC、LUT 駆動）
│   ├── SubEngine.h            // Sub DSP 宣言
│   ├── SubOscillator.cpp      // Sub用Wavetable OSC実装
│   └── SubOscillator.h        // Sub用Wavetable OSC宣言
├── GUI
│   ├── ClickParams.cpp        // Click パネル UI セットアップ / レイアウト
│   ├── CustomSliderLAF.h      // ノブ描画LookAndFeel（グラデーション/値表示）
│   ├── DirectParams.cpp       // Direct パネル UI セットアップ / レイアウト
│   ├── EnvelopeCurveEditor.cpp // エンベロープカーブエディタ実装
│   ├── EnvelopeCurveEditor.h  // エンベロープカーブエディタ宣言
│   ├── KeyboardComponent.cpp  // 鍵盤UI実装
│   ├── KeyboardComponent.h    // 鍵盤UI宣言
│   ├── LevelMeter.cpp         // レベルメーター実装
│   ├── LevelMeter.h           // レベルメーター宣言
│   ├── PanelComponent.cpp     // SUB/CLICK/DIRECT共通パネル実装
│   ├── PanelComponent.h       // 共通パネル宣言（フェーダー・展開ボタン）
│   ├── SubParams.cpp          // Sub パネル UI セットアップ / レイアウト
│   └── UIConstants.h          // UI定数集約（色・レイアウト寸法）
├── PluginEditor.cpp
├── PluginEditor.h
├── PluginProcessor.cpp
└── PluginProcessor.h
```

## TODO

- **Sub/Click 共通トリガー（トランジェント検出）**
  - 目的: 入力信号の立ち上がり過渡成分を検出して Sub/Click を瞬時に発音（VST/AU の Kick トラック挿入を想定）
  - 検証項目: Sasquatch の発火条件が「トランジェント検出」か「入力レベル/サイドチェイン閾値」かを確認
  - 実装候補: 短時間エネルギー差分 or ハイパス後エンベロープ検出でトリガー、ヒステリシス付きゲートで多重発火防止

- **レイテンシー補正（Delay Compensation）**
  - 背景: BabySquatch は Direct（入力パススルー）と Sub/Click（生成信号）を混合するため、どちらかに処理遅延が生じると内部でタイミングズレが発生する
  - 実装方針（2段階）:
    1. **内部補正**: Direct（Dry）側に `juce::dsp::DelayLine` で同量の遅延を与え、Sub/Click（Wet）との位相を揃える
    2. **ホスト報告**: `setLatencySamples(n)` で DAW に総遅延サンプル数を通知し、他トラックとの同期を確保
  - トリガー: ルックアヘッド型トランジェント検出を実装した際に必須となる（ルックアヘッド分だけ Wet が遅れるため）
  - 現状: 大きな遅延源がないため未着手で問題なし。トランジェント検出実装時に同時対応する
