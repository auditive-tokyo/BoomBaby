# BabySquatch Project

## プロジェクト概要

BabySquatchは、Boz LabのSasquatchから自分が使う機能だけを集約したキックエンハンスプラグインです。

## プラグイン構成

BabySquatchは3つのモジュールで構成されています：

1. **Sub** - サブ周波数の生成
2. **Click** - アタック部分の生成（Square, Triangle, Sawなどの短いノイズ）
3. **Direct** - オリジナルのキック音（入力信号）

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

- 3パネルUI（SUB / CLICK / DIRECT）、各カラー付きロータリーノブ、Mute（グレー/赤）/ Solo（グレー/黄）トグルボタン、展開ボタン（▼）を下部に配置
- 各チャンネルノブ左に縦型レベルメーター（`LevelMeter` + `LevelDetector`）
  - DSP: `LevelDetector`（ヘッダオンリー）— `std::atomic<float>` ピーク計測、バリスティクス付き減衰
  - GUI: `LevelMeter`— `paint()` ベース、`juce::Timer` 30fps ポーリング、緑→黄→赤 グラデーション、-48dB～+6dB スケール、dBスケール目盛（0/-12/-24/-36/-48）
- グラデーションアーク（指数的グロー）
- ノブ中央にdB値表示、クリックでキーボード入力、ダブルクリックで0.0dBリセット
- 展開パネル（per-channel ▼ボタン、共有展開エリア）
- MIDI鍵盤（KeyboardComponent）
  - C0〜C7表示、PCキー演奏対応（A=C2ベース）
  - Z/Xでオクターブシフト
  - トリガー専用（FIXED/MIDI モード削除済み）
- **AMP / Pitch / DIST / BLEND Envelope エディタ**（Kick Ninja スタイル）
  - `EnvelopeData`：ヘッダオンリモデル。制御点なし→`defaultValue`（フラット）、1点→定数、2点→線形補間、3点以上→Catmull-Rom スプライン補間。ファントムポイント端点処理。`movePoint` の値クランプは呼び出し側が担当（AMP/Pitch でレンジが異なるため）
  - `EnvelopeCurveEditor`：`paint()` ベース。AMP / Pitch / DIST / BLEND の 4 エンベロープを受け取り、**位相累積方式**（`phase += pitchHz × dt × 2π`）でオフライン波形プレビュー。スプラインカーブ＋コントロールポイント描画。左ダブルクリックでポイント追加／削除、ドラッグで移動。横軸 ms タイムライン表示（自動間隔目盛り + ラベル）
  - 右上に **AMP / PITCH / DIST / BLEND タブボタン**を内蔵。AMP=青（subArc）　1 PITCH=シアン、DIST=オレンジ、BLEND=グリーン。`setOnEditTargetChanged()` コールバックで外部 UI（ノブラベル等）と同期
  - **Pitch Y軸**: 対数スケール 20～20000 Hz。**DIST Y軸**: 線形 0.0～1.0。**BLEND Y軸**: 線形 -1.0～+1.0（Y中央=0）
  - ロックフリー LUT 統合：`envLut_`（AMP gain）/ `pitchLut_`（Hz）/ `distLut_`（drive01）/ `blendLut_`（-1～+1）の 4 系統ダブルバッファ。オーディオスレッドはノートオンで `noteTimeSamples` リセット → サンプル毎に pitchLut→`setFrequencyHz()` + distLut→`setDist()` + blendLut→`setBlend()` + ampLut→ゲイン乗算
  - 各ノブは `xxxEnvData.setDefaultValue()` 経由で間接的に LUT を更新。エンベロープポイントがある間、対応ノブを無効化（ツールチップ表示）
- **Pitch Envelope 実装**（完了）
  - `SubOscillator`: `setNote(int)` → `triggerNote()` / `stopNote()` / `setFrequencyHz(float hz)` に変更。MIDIノート番号は使わず、ピッチは Pitch Envelope LUT から毎サンプル取得
  - `PluginProcessor`: `pitchLut_`（`EnvelopeLutManager`）追加、`pitchLut()` アクセサ公開。`handleMidiEvents` でノートオン→`triggerNote()`、ノートオフ→`stopNote()`
  - `PluginEditor`: `pitchEnvData`（デフォルト 200 Hz）+ `bakeAmpLut` / `bakePitchLut` 分離。PITCH ノブ（subKnobs[0]）: 20〜20000 Hz 対数スケール、エンベロープポイントがある間は無効化
- **Sub パラメータ整理**（完了）
  - Subロータリーノブ → `subGainDb`（最終段ゲイン）のみ制御
  - AMP ノブ（subKnobs[1]）→ `ampEnvData.setDefaultValue()` を担当（0〜200%）。エンベロープポイントがある間は無効化・ツールチップ表示
  - PITCH ノブ（subKnobs[0]）→ `pitchEnvData.setDefaultValue()` を担当（20〜20000 Hz）。同様に無効化制御
  - subKnobs[2〜7] はラベル設定済み（BLEND/DIST/H1〜H4）。BLEND（[2]）・H1〜H4（[4〜7]）は DSP 接続済み。Dist（[3]）は未接続（TBD）。波形選択ボタン（Tri/SQR/SAW）はノブ行の下に別途配置済み
- **Band-limited Wavetable 基盤**（Phase 1 完了）: `SubOscillator` を 4波形（Sine/Tri/Square/Saw）× 10帯域 2D テーブル構造に拡張。`WaveShape` enum、`buildAllTables()`、`bandIndexForFreq()`、`readTable()` 実装済み。波形選択 UI（Phase 4）と BLEND（Phase 3）により全波形が有効
- **H1〜H4 加算合成**（Phase 2 完了）: `HarmonicOsc` × 4 + `harmonicGains[4]`（atomic）を `SubOscillator` に内蔵。`setHarmonicGain(int n, float gain)` API、`subOscillator()` アクセサ、subKnobs[4〜7] 配線済み
- **BLEND クロスフェード**（Phase 3 完了）: `setBlend(float)` API（-1.0〜+1.0）追加、`getNextSample()` 内で `std::lerp` によるクロスフェード実装。b≤0: Sine↔Wavetable、b>0: Sine↔Additive。subKnobs[2]（BLEND）-100〜+100 配線済み
- **波形選択 UI**（Phase 4 完了）: Tri / SQR / SAW の `TextButton` 3本を展開パネル内のノブ行下に配置。リラジオグループ方式（手動排他）0または1ボタンが選拡可能。OFF時は Sineに戻る。選択色はお〈subArc（青系）。不選択=Sine、Triボタン=Tri、SQRボタン=Square、SAWボタン=Saw
- **波形プレビュー BLEND 連携**（完了）: `EnvelopeCurveEditor` に `setWaveShape()` / `setPreviewBlend()` / `setPreviewHarmonicGain()` API 追加。BLEND 負側=Sine↔WaveShape、正側=Sine↔Additive(H1〜H4) のモーフィング描画。ボタン onClick・BLEND/H1〜H4 ノブ変更時に `repaint()`
- **Sub パラメータ設計**（完了）: 展開パネル 8 ノブ（PITCH/AMP/BLEND/Dist/H1〜H4）配置・ラベル設定済み。全ノブ DSP 接続完了
  - PITCH（subKnobs[0]）: 20〜20000 Hz 対数スケール、Pitch Envelope 制御時は無効化
  - AMP（subKnobs[1]）: 0〜200%、AMP Envelope 制御時は無効化
  - BLEND（subKnobs[2]）: -100 ～ +100（-1.0〜+1.0）。BLEND Envelope LUT から毎サンプル制御
  - DIST（subKnobs[3]）: 0〜100%（drive 1.0〜10.0）。`tanh` 常時ソフトクリップ。DIST Envelope LUT から毎サンプル制御
  - H1〜H4（subKnobs[4-7]）: 0〜1.0、加算合成倍音ゲイン
  - 波形選択ボタン（Tri/SQR/SAW）は独立した TextButton パネル
- **SUB One-shot 長さ制御 + セクション境界表示**（完了）
  - Length ボックス（`length: NNNms`、10〜2000ms、初期値 300ms）を `EnvelopeCurveEditor` 上部に配置。入力値で `setDisplayDurationMs()` + LUT 焼き直しを実行
  - `paintTimeline()` に Attack(0〜10ms) / Body(10〜40ms) / Decay(40〜140ms) / Tail(140ms〜) の縦線（alpha 0.10）+ ATK/BODY/DECAY/TAIL ラベル（8pt、alpha 0.25）を描画
  - NoteOff を Sub 側で無視し、Length 到達で自動停止（latest-note priority、新 NoteOn で即リセット）
  - 末尾 5ms half-cosine フェードアウト窓を DSP（`renderSub()`）+ GUI プレビュー（`paintWaveform()`）の両方に適用
  - 連打時の旧ノート fade-out は「新ノートアタックがマスクするため不要」と判断し未実装

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
│   ├── SubOscillator.cpp    // Sub用Wavetable OSC実装（波形生成・補間）
│   └── SubOscillator.h      // Sub用Wavetable OSC宣言（公開API）
├── GUI
│   ├── CustomSliderLAF.h      // ノブ描画LookAndFeel（グラデーション/値表示）
│   ├── EnvelopeCurveEditor.cpp // エンベロープカーブエディタ実装（paint/マウス操作）
│   ├── EnvelopeCurveEditor.h  // エンベロープカーブエディタ宣言
│   ├── KeyboardComponent.cpp  // 鍵盤UI実装（MIDI/FIXED切替・PCキー入力）
│   ├── KeyboardComponent.h    // 鍵盤UI宣言（モード/固定ノート制御API）
│   ├── LevelMeter.cpp         // レベルメーター実装（paint・30fps Timer）
│   ├── LevelMeter.h           // レベルメーター宣言
│   ├── PanelComponent.cpp     // SUB/CLICK/DIRECT共通パネル実装
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

- **CapsLock 中はキーボードフォーカスを常に鍵盤に固定**
  - 現状: 展開パネルを開くか鍵盤をクリックした場合のみ `KeyboardComponent` がフォーカスを持つ。ロータリーノブ操作後などはフォーカスが外れ、PCキーからのMIDI入力が効かなくなる
  - 期待動作: CapsLockがONの間は、他のUI操作（ノブ・ボタン等）をしても常に `KeyboardComponent` がフォーカスを保持する
  - **実装前に確認すべきこと（検証項目）:**
    1. **VST/AU でビルドして DAW（Logic/Ableton 等）に差し込み、ノブ操作後も PCキー演奏が継続できるか確認する**
       - Standalone 固有の問題（OS レベルのウィンドウフォーカスリセット）であれば、VST/AU では `wantsKeyboardFocus = false`（Slider 等のデフォルト）が正しく機能し、MidiKeyboardComponent がフォーカスを保ち続けるはず
       - Kick Ninja が CapsLock なしで動作しているのは VST/AU であることが主な理由と考えられる
    2. 上記検証で VST/AU では問題なければ、**Standalone 専用の修正**として優先度を下げて検討する
    3. VST/AU でも問題が再現する場合に限り、以下の実装案を採用する
  - 実装案（Standalone 問題が確認された場合のみ）:
    1. `PluginEditor` の `focusOfChildComponentChanged()` または各コンポーネントの `mouseDown()` をオーバーライドし、CapsLock状態を `juce::ModifierKeys::getCapsLockState()` で確認
    2. CapsLockがONなら即座に `keyboard.grabKeyboardFocus()` を呼び直す
    3. または `KeyboardComponent` 側で `juce::ComponentPeer` レベルのフォーカス監視を行い、自律的に再取得する
  - 関連ファイル: `Source/GUI/KeyboardComponent.cpp`, `Source/PluginEditor.cpp`

- **全ノブに `ColouredSliderLAF` を適用してUI統一**
  - ✅ Sub チャンネルノブ（`PanelComponent`）: 適用済み
  - ✅ Sub 展開ノブ（`subKnobs[0〜7]`）: `subKnobLAF`（subArc/subThumb）を適用済み。`setTextValueSuffix` で dB サフィックスも各ノブで個別制御
  - ⬜ Click 展開ノブ（Click モジュール実装時）→ `clickKnobLAF`（`clickArc` / `clickThumb`）を同様に追加
  - ⬜ Direct 展開ノブ（Direct モジュール実装時）→ `directKnobLAF`（`directArc` / `directThumb`）を同様に追加
  - 実装パターン: `PluginEditor.h` に LAF メンバー追加 → `setupXxxKnobsRow()` で `setLookAndFeel(&xxxKnobLAF)` を呼ぶだけ（LAF はスライダーより先に宣言してライフタイムを確保）

- **Sub/Click 共通トリガー（トランジェント検出）**
  - 目的: 入力信号の単純な音量ではなく、立ち上がりの過渡成分を検出して Sub/Click を瞬時に発音する（VST/AU の Kick トラック挿入を想定）
  - **検証項目（TODO）**: Sasquatch の発火条件が「トランジェント検出」なのか「入力レベル or サイドチェイン閾値」なのかを確認する
  - 実装候補: 短時間エネルギー差分 or ハイパス後のエンベロープ検出でトリガー、ヒステリシス付きゲートで多重発火を防ぐ
  - Direct は入力そのものを通す設計のため、基本はトリガー不要（必要ならゲートやトランジェント連動はオプション扱い）

- **Click モジュール実装**
  Sasquatch v1の画面分析から、Clickセクションは Square と Saw をミックスした波形合成であることが判明。以下3方向性で実装を検討する。

  **方向性1: Square + Saw Wavetable モーフィング（最優先・低コスト）**
  - Sasquatch v1 相当のアプローチ。Square↔Saw の BLEND ノブ + 短い Decay エンベロープ（EnvelopeData 流用）で「カチッ」とした芯のある硬いアタックを生成
  - 既存の `SubOscillator` の band-limited wavetable（Square/Saw 実装済み）をそのまま流用可能
  - 注意: 極短トランジェントでは band-limit のメリットはほぼないが、コード再利用で実装コスト最小
  - 向いているサウンド: EDM/ハードスタイル/メタル、明確に「前に出る」タイトなクリック

  **方向性2: White Noise + Resonance Filter（本命・中コスト）**
  - Sasquatch v2 相当のアプローチ。ホワイトノイズを `juce::dsp::StateVariableTPTFilter`（SVF）でフィルタリングし、高 Q（レゾナンス）でビーター音を整形
  - ノイズ生成は `juce::Random` で十分。JUCEの `dsp::` モジュールで主要部品が揃っているため実装コスト中程度
  - パラメータ候補: Freq（フィルター中心周波数）、Focus（Q値/レゾナンス）、Decay
  - 向いているサウンド: 生ドラム補強、ポップス/ロック、自然に馴染む空気感のあるアタック

  **方向性3: サンプルロード機能（差別化・高コスト）**
  - 任意の WAV ファイルをロードしてクリック音として使用
  - 実装要素: `juce::AudioFormatManager` + `juce::AudioFormatReader` + ファイルピッカー UI + `getStateInformation`/`setStateInformation` へのパス保存
  - ポータビリティ注意: サンプルパスの管理（絶対パス vs 埋め込みバイナリ）が課題
  - 向いているサウンド: 制限なし。ユーザーが自分でビーター音を持ち込める

  **推奨実装順序**: 方向性1（既存流用で即戦力）→ 方向性2（メイン機能）→ 方向性3（v2以降の差別化）

- Click/Direct パネルの展開エリアに同様のエディタを配置
- エンベロープの保存／復元（`getStateInformation` / `setStateInformation`）
