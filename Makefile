.PHONY: build run launch install clean cmake check lint tidy tidy-fix test help

help:
	@echo "使用可能なコマンド:"
	@echo "  make build     - プロジェクトをビルド"
	@echo "  make run       - ビルドしてStandalone起動"
	@echo "  make launch    - ビルド済みアプリを起動（ビルドなし）"
	@echo "  make install   - ビルドしてVST3/AUをインストール"
	@echo "  make cmake     - CMakeプロジェクトを再生成"
	@echo "  make check     - コンパイルをチェック（エラーのみ表示）"
	@echo "  make lint      - コンパイラ警告をチェック"
	@echo "  make test      - ユニットテストをビルド＆実行"
	@echo "  make coverage  - HTMLカバレッジレポートを生成してブラウザで開く"
	@echo "  make tidy      - （非推奨: JUCE では動作不安定）"
	@echo "  make tidy-fix  - （非推奨）"
	@echo "  make clean     - ビルドディレクトリをクリーン"

build:
	cd build && xcodebuild -scheme "BoomBaby_All" -configuration Debug build

run: build
	open build/BoomBaby_artefacts/Debug/Standalone/BoomBaby.app

launch:
	open build/BoomBaby_artefacts/Debug/Standalone/BoomBaby.app

install: build
	cp -R build/BoomBaby_artefacts/Debug/VST3/BoomBaby.vst3 ~/Library/Audio/Plug-Ins/VST3/
	cp -R build/BoomBaby_artefacts/Debug/AU/BoomBaby.component ~/Library/Audio/Plug-Ins/Components/
	@echo "✓ プラグインをインストールしました。DAWで再スキャンしてください。"

cmake:
	cd build && cmake .. -G Xcode
	cd build-clangd && cmake ..

clean:
	rm -rf build/* build-clangd/*

check:
	cd build && cmake .. -G Xcode 2>/dev/null && xcodebuild -project BoomBaby.xcodeproj -scheme "BoomBaby_All" -configuration Debug build 2>&1 | grep -E "(error|warning):" || echo "✓ ビルドエラー・ワーニングなし"

lint:
	@echo "基本的なコード検査（コンパイラ警告）..."
	@cd build && xcodebuild -project BoomBaby.xcodeproj -scheme "BoomBaby_All" -configuration Debug build 2>&1 | \
		grep -E "(warning|error):" | \
		grep -v "Run script build phase" | \
		head -50 || echo "✓ 警告・エラーなし"

tidy:
	@echo "注意: clang-tidy は JUCE プロジェクトでは正常に動作しない可能性があります。"
	@echo "代わりに 'make lint' または 'make check' を使用してください。"
	@echo ""
	@echo "それでも実行する場合は以下を手動実行:"
	@echo "  /opt/homebrew/opt/llvm/bin/clang-tidy -p build-clangd Source/YourFile.cpp"

tidy-fix:
	@echo "clang-tidy の自動修正は JUCE プロジェクトでは推奨されません。"
	@echo "手動でコードを修正してください。"

test:
	cd build && xcodebuild -scheme "BoomBabyTests" -configuration Debug build 2>&1 | grep -E "(error:|Build succeeded|FAILED)" || true
	@cd build && LLVM_PROFILE_FILE=cov_%p.profraw ctest -C Debug --output-on-failure
	@echo ""
	@echo "── Coverage Report ──────────────────────────────────"
	@cd build && xcrun llvm-profdata merge -sparse cov_*.profraw -o cov.profdata 2>/dev/null && \
	  xcrun llvm-cov report ./BoomBabyTests_artefacts/Debug/BoomBabyTests \
	    -instr-profile=cov.profdata \
	    -ignore-filename-regex='(JUCE|Catch2|_deps|Tests/)' && \
	  rm -f cov_*.profraw || echo "(カバレッジデータなし)"

coverage:
	@echo "テストを実行してカバレッジデータを収集中..."
	@cd build && xcodebuild -scheme "BoomBabyTests" -configuration Debug build 2>&1 | grep -E "(error:|Build succeeded|FAILED)" || true
	@cd build && LLVM_PROFILE_FILE=cov_%p.profraw ctest -C Debug -Q
	@echo "HTMLレポートを生成中..."
	@cd build && xcrun llvm-profdata merge -sparse cov_*.profraw -o cov.profdata 2>/dev/null && \
	  xcrun llvm-cov show ./BoomBabyTests_artefacts/Debug/BoomBabyTests \
	    -instr-profile=cov.profdata \
	    -ignore-filename-regex='(JUCE|Catch2|_deps|Tests/)' \
	    -format=html \
	    -output-dir=../coverage-report \
	    -show-line-counts-or-regions \
	    -show-instantiations=false && \
	  rm -f cov_*.profraw cov.profdata && \
	  echo "✓ レポート生成完了: coverage-report/index.html" && \
	  open ../coverage-report/index.html || echo "(カバレッジデータなし)"
