# BabySquatch Project

## プロジェクト概要

BabySquatchは、Boz LabのSasquatchから自分が使う機能だけを集約したキックエンハンスプラグインです。

## プラグイン構成

BabySquatchは3つのモジュールで構成されています：

1. **Oomph** - サブ周波数の生成
2. **Click** - アタック部分の生成（Square, Triangle, Sawなどの短いノイズ）
3. **Dry** - オリジナルのキック音（入力信号）

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

## 実装済み機能

- 3パネルUI（OOMPH / CLICK / DRY）、各カラー付きロータリーノブ、Mute（グレー/赤）/ Solo（グレー/黄）トグルボタン、展開ボタン（▼）を下部に配置
- 各チャンネルノブ左に縦型レベルメーター（`LevelMeter` + `LevelDetector`）
  - DSP: `LevelDetector`（ヘッダオンリー）— `std::atomic<float>` ピーク計測、バリスティクス付き減衰
  - GUI: `LevelMeter`— `paint()` ベース、`juce::Timer` 30fps ポーリング、緑→黄→赤 グラデーション、-48dB～+6dB スケール、dBスケール目盛（0/-12/-24/-36/-48）
- グラデーションアーク（指数的グロー）
- ノブ中央にdB値表示、クリックでキーボード入力、ダブルクリックで0.0dBリセット
- 展開パネル（per-channel ▼ボタン、共有展開エリア）
- MIDI鍵盤（KeyboardComponent）
  - C0〜C7表示、PCキー演奏対応（A=C2ベース）
  - Z/Xでオクターブシフト（両モード共通）
  - MIDI / FIXED モード切り替えボタン
  - FIXEDモード：単音固定、同じ鍵盤クリックで解除
- **AMP / Pitch Envelope エディタ**（Kick Ninja スタイル）
  - `EnvelopeData`：ヘッダオンリーモデル。制御点なし→`defaultValue`（フラット）、1点→定数、2点→線形補間、3点以上→Catmull-Rom スプライン補間。ファントムポイント端点処理。`movePoint` の値クランプは呼び出し側が担当（AMP/Pitch でレンジが異なるため）
  - `EnvelopeCurveEditor`：`paint()` ベース。AMP + Pitch の両エンベロープを受け取り、**位相累積方式**（`phase += pitchHz × dt × 2π`）でオフライン波形プレビュー。スプラインカーブ＋コントロールポイント描画。左ダブルクリックでポイント追加／削除、ドラッグで移動。横軸 ms タイムライン表示（自動間隔目盛り + ラベル）
  - 右上に **AMP / PITCH タブボタン**を内蔵。クリックで編集対象を切替（AMP=橙、PITCH=シアン）。`setOnEditTargetChanged()` コールバックで外部 UI（ノブラベル等）と同期
  - **Pitch Y軸**: 対数スケール 20〜20000 Hz（`valueToY`/`yToValue` が `EditTarget` に応じてスケール切替）
  - ロックフリー LUT 統合：`envLut_`（AMP gain × 512点）+ `pitchLut_`（Hz × 512点）の2系統ダブルバッファ。オーディオスレッドはノートオンで `noteTimeSamples` リセット → サンプル毎に pitchLut → `setFrequencyHz()` + ampLut → ゲイン乗算
- **Pitch Envelope 実装**（完了）
  - `OomphOscillator`: `setNote(int)` → `triggerNote()` / `stopNote()` / `setFrequencyHz(float hz)` に変更。MIDIノート番号は使わず、ピッチは Pitch Envelope LUT から毎サンプル取得
  - `PluginProcessor`: `pitchLut_`（`EnvelopeLutManager`）追加、`pitchLut()` アクセサ公開。`handleMidiEvents` でノートオン→`triggerNote()`、ノートオフ→`stopNote()`
  - `PluginEditor`: `pitchEnvData`（デフォルト 200 Hz）+ `bakeAmpLut` / `bakePitchLut` 分離。PITCH ノブ（oomphKnobs[0]）: 20〜20000 Hz 対数スケール、エンベロープポイントがある間は無効化
- **Oomph パラメータ整理**（完了）
  - Oomphロータリーノブ → `oomphGainDb`（最終段ゲイン）のみ制御
  - AMP ノブ（oomphKnobs[1]）→ `ampEnvData.setDefaultValue()` を担当（0〜200%）。エンベロープポイントがある間は無効化・ツールチップ表示
  - PITCH ノブ（oomphKnobs[0]）→ `pitchEnvData.setDefaultValue()` を担当（20〜20000 Hz）。同様に無効化制御
  - oomphKnobs[2〜7] はラベル設定済み（BLEND/DIST/H1〜H4）。BLEND（[2]）・H1〜H4（[4〜7]）は DSP 接続済み。Dist（[3]）は未接続（TBD）。波形選択ボタン（Tri/SQR/SAW）はノブ行の下に別途配置済み
- **Band-limited Wavetable 基盤**（Phase 1 完了）: `OomphOscillator` を 4波形（Sine/Tri/Square/Saw）× 10帯域 2D テーブル構造に拡張。`WaveShape` enum、`buildAllTables()`、`bandIndexForFreq()`、`readTable()` 実装済み。波形選択 UI（Phase 4）と BLEND（Phase 3）により全波形が有効
- **H1〜H4 加算合成**（Phase 2 完了）: `HarmonicOsc` × 4 + `harmonicGains[4]`（atomic）を `OomphOscillator` に内蔵。`setHarmonicGain(int n, float gain)` API、`oomphOscillator()` アクセサ、oomphKnobs[4〜7] 配線済み
- **BLEND クロスフェード**（Phase 3 完了）: `setBlend(float)` API（-1.0〜+1.0）追加、`getNextSample()` 内で `std::lerp` によるクロスフェード実装。b≤0: Sine↔Wavetable、b>0: Sine↔Additive。oomphKnobs[2]（BLEND）-100〜+100 配線済み
- **波形選択 UI**（Phase 4 完了）: Tri / SQR / SAW の `TextButton` 3本を展開パネル内のノブ行下に配置。リラジオグループ方式（手動排他）0または1ボタンが選拡可能。OFF時は Sineに戻る。選択色はお〈oomphArc（青系）。不選択=Sine、Triボタン=Tri、SQRボタン=Square、SAWボタン=Saw
- **波形プレビュー BLEND 連携**（完了）: `EnvelopeCurveEditor` に `setWaveShape()` / `setPreviewBlend()` API 追加。BLEND=0→Sine、BLEND=-100→選択波形、中間→lerp モーフィング描画。ボタン onClick・BLEND ノブ変更時に `repaint()`

## 描画方針

- **現在**: CPU描画（`paint()` ベース）が主体
  - `EnvelopeCurveEditor`, `CustomSliderLAF`, `LevelMeter`（予定）等
- **`WaveformDisplay`**: 生OpenGLシェーダー使用（将来用・**現在未使用**・未接続）
  - 高密度リアルタイム波形描画（数千〜数万ポイント）が必要な場合向け
- **GPU化の切り替え**: `openGLContext.attachTo(*this)` を `PluginEditor` コンストラクタに1行追加するだけで、全ての `paint()` コードがそのままGPU加速される（JUCEの `Graphics` APIがバックエンドを抽象化）
- **結論**: 今は `paint()` で実装し続けて問題なし。将来パフォーマンス要求が上がった場合でも、既存コードの変更なく GPU化可能

## Source構成（概要）

`tree` の生出力を貼り付け（更新時は `cd Source && tree` を再実行して置き換える）:

```text
.
├── DSP
│   ├── ChannelState.h         // チャンネルMute/Solo/レベル管理（ヘッダオンリー、BabySquatchAudioProcessorから委譲）
│   ├── EnvelopeData.h         // AMP Envelopeデータモデル（Catmull-Rom・ヘッダオンリー）
│   ├── EnvelopeLutManager.h   // LUTダブルバッファ管理（ヘッダオンリー、ロックフリーbake/read）
│   ├── LevelDetector.h        // ロックフリーピーク検出（ヘッダオンリー、オーディオスレッド writer / UI reader）
│   ├── OomphOscillator.cpp    // Oomph用Wavetable OSC実装（波形生成・補間）
│   └── OomphOscillator.h      // Oomph用Wavetable OSC宣言（公開API）
├── GUI
│   ├── CustomSliderLAF.h      // ノブ描画LookAndFeel（グラデーション/値表示）
│   ├── EnvelopeCurveEditor.cpp // エンベロープカーブエディタ実装（paint/マウス操作）
│   ├── EnvelopeCurveEditor.h  // エンベロープカーブエディタ宣言
│   ├── KeyboardComponent.cpp  // 鍵盤UI実装（MIDI/FIXED切替・PCキー入力）
│   ├── KeyboardComponent.h    // 鍵盤UI宣言（モード/固定ノート制御API）
│   ├── LevelMeter.cpp         // レベルメーター実装（paint・30fps Timer）
│   ├── LevelMeter.h           // レベルメーター宣言
│   ├── PanelComponent.cpp     // OOMPH/CLICK/DRY共通パネル実装
│   ├── PanelComponent.h       // 共通パネル宣言（ノブ・展開ボタン）
│   ├── UIConstants.h          // UI定数集約（色・レイアウト寸法）
│   ├── WaveformDisplay.cpp    // OpenGL波形描画実装（将来用・現在未接続）
│   └── WaveformDisplay.h      // OpenGL波形描画宣言（将来用・現在未接続）
├── PluginEditor.cpp           // Editor実装（レイアウト/UIイベント/LUTベイク配線）
├── PluginEditor.h             // Editor宣言（UI構成とメンバー）
├── PluginProcessor.cpp        // Processor実装（MIDI処理・DSP・ChannelState/EnvelopeLutManagerへ委譲）
└── PluginProcessor.h          // Processor宣言（AudioProcessorIF・channelState()/envLut()アクセサ）

3 directories, 20 files
```

## TODO

### Phase 3以降（将来の拡張）

- **Oomph パラメータ設計（残作業）**
  - **実装済み**: OOMPHの展開パネル上部に8個のロータリーノブ（`oomphKnobs[8]` / `oomphKnobLabels[8]`）。PITCH（oomphKnobs[0]）・AMP（oomphKnobs[1]）は DSP 接続済み。Oomphロータリーノブは `oomphGainDb` 専用に整理済み
  - **ノブ割り当て（確定）**:
    - oomphKnobs[0] → **PITCH**（実装済み）: 20〜20000 Hz
    - oomphKnobs[1] → **AMP**（実装済み）: 0〜200%
    - oomphKnobs[2] → **BLEND**: -100〜+100、デフォルト 0
    - oomphKnobs[3] → **Dist**（TBD）
    - oomphKnobs[4] → **H1**: 第1倍音（基底音 × 1 = ルート音）
    - oomphKnobs[5] → **H2**: 第2倍音（基底音 × 2 = 1オクターブ上）
    - oomphKnobs[6] → **H3**: 第3倍音（基底音 × 3 = 1オクターブ + 完全5度上）
    - oomphKnobs[7] → **H4**: 第4倍音（基底音 × 4 = 2オクターブ上）
  - **H1〜H4 の仕組み**（加算合成 / Additive Synthesis）:
    - サイン波OSCとは独立したパラレルの倍音サイン波を加算する構造
    - 各スライダーで「その次数の倍音の音量」を制御
    - 波形選択（Tri/Square/Saw）が本来持つ倍音構成とは独立して機能する
    - 例: Tri（奇数倍音のみ）を選択中でも H2・H4（偶数倍音）を追加可能 → ハイブリッド音色
  - **各基本波形の倍音構成**（DSP設計の参考）:
    - **Triangle**: 奇数倍音のみ（3×, 5×, 7×...）、高域は急減衰。滑らかなボディ音
    - **Square**: 奇数倍音のみ（3×, 5×, 7×...）、Triより高域倍音が強い。中高域強調
    - **Saw**: 偶数・奇数両方（2×, 3×, 4×...）。最も密度が濃く鋭い音

- **KeyboardComponent FIXEDモードのキーボード入力問題**
  - 現状: FIXEDモードでもmacのキーボード入力に反応してしまい、noteが選択されてしまう
  - 期待動作: FIXEDモードではマウス操作のみ有効、キーボード入力は無効化
  - 実装:
    1. `KeyboardComponent` の `keyPressed` / `keyStateChanged` ハンドラーで `isMidiMode` チェック追加
    2. FIXEDモードの場合はキーボードイベントを無視（`return false` でイベントを親に伝播）
    3. または `setWantsKeyboardFocus(isMidiMode)` でフォーカス制御
  - 関連ファイル: `Source/GUI/KeyboardComponent.cpp`

- **CapsLock 中はキーボードフォーカスを常に鍵盤に固定**
  - 現状: 展開パネルを開くか鍵盤をクリックした場合のみ `KeyboardComponent` がフォーカスを持つ。ロータリーノブ操作後などはフォーカスが外れ、PCキーからのMIDI入力が効かなくなる
  - 期待動作: CapsLockがONの間は、他のUI操作（ノブ・ボタン等）をしても常に `KeyboardComponent` がフォーカスを保持する
  - 実装案:
    1. `PluginEditor` の `focusOfChildComponentChanged()` または各コンポーネントの `mouseDown()` をオーバーライドし、CapsLock状態を `juce::ModifierKeys::getCapsLockState()` で確認
    2. CapsLockがONなら即座に `keyboard.grabKeyboardFocus()` を呼び直す
    3. または `KeyboardComponent` 側で `juce::ComponentPeer` レベルのフォーカス監視を行い、自律的に再取得する
  - 関連ファイル: `Source/GUI/KeyboardComponent.cpp`, `Source/PluginEditor.cpp`

- **エンベロープ横軸セクション境界表示**
  - 現状: ms タイムラインは実装済み。セクション境界は未実装
  - 追加: **Attack / Body / Decay / Tail** の4セクション境界線 + ラベル表示（Kick Ninja スタイル）
  - 各セクション境界は `EnvelopeCurveEditor` 内で定義可能なメンバ変数（例: `attackEndMs`, `bodyEndMs`, `decayEndMs`）または `EnvelopeData` 側で保持
  - `paint()` 内で縦線 + テキストラベル描画
  - UI的に境界を調整可能にするか（ドラッグで移動）は要検討

- **全ノブに `ColouredSliderLAF` を適用してUI統一**
  - `Source/GUI/CustomSliderLAF.h` に `ColouredSliderLAF` クラスが既存（`arcColour` / `thumbColour` をコンストラクタ引数で差し替えるだけで色変更可）
  - 現状: Oomph チャンネルノブ（`PanelComponent`）には適用済みだが、展開パネル内の `oomphKnobs[8]` には未適用
  - 対応方針:
    - Oomph 展開ノブ → `UIConstants::Colours::oomphArc`（橙）ベースの `ColouredSliderLAF` インスタンスを適用
    - Click 展開ノブ（将来）→ Click カラー（緑系）
    - Dry 展開ノブ（将来）→ Dry カラー（紫系）
  - `PluginEditor.h` に `ColouredSliderLAF oomphKnobLAF` メンバーを追加し、全 `oomphKnobs[i]` に `setLookAndFeel(&oomphKnobLAF)` を呼ぶだけで実装可能
  - LAF はスライダーのライフタイムより長く生きている必要あり（メンバー保持で解決）

- **Click モジュール実装**（より高度な処理: 短いトランジェント/ノイズバースト生成等）

- **FIXED / MIDI モードの見直し**
  - Kick Ninja準拠方式（トリガー専用）では、全鍵盤が同じ音を鳴らすため FIXED/MIDI の区別が不要
  - キーボードUIの簡素化を検討（モード切替ボタン削除、単一トリガーモード化）
  - または将来的にキーボード自体をオプション表示に変更

- Click/Dry パネルの展開エリアに同様のエディタを配置
- エンベロープの保存／復元（`getStateInformation` / `setStateInformation`）
