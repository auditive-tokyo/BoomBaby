.PHONY: build run launch install clean cmake check lint tidy tidy-fix help

help:
	@echo "使用可能なコマンド:"
	@echo "  make build     - プロジェクトをビルド"
	@echo "  make run       - ビルドしてStandalone起動"
	@echo "  make launch    - ビルド済みアプリを起動（ビルドなし）"
	@echo "  make install   - ビルドしてVST3/AUをインストール"
	@echo "  make cmake     - CMakeプロジェクトを再生成"
	@echo "  make check     - コンパイルをチェック（エラーのみ表示）"
	@echo "  make lint      - コンパイラ警告をチェック"
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
