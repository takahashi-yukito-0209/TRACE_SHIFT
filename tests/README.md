# 検証の実行方法

テストはゲーム本体とは別の実行ファイルです。現状のCIからは起動していません。
WindowsのVisual Studio「x64 Native Tools Command Prompt」でリポジトリのルートへ移動して実行します。

## 単独で実行できる検証

```bat
tests\RunCoreTests.cmd
tests\RunSceneAvailabilityTests.cmd
tests\RunParticleEditorTests.cmd
```

- `RunCoreTests`：JSON、ファイル保存、挑戦状態のリセット、編集後の配置計算、実ステージJSONを検証します。通常リセット256通り、再生準備2048通り、HUD集計135168通りを含みます。
- `RunSceneAvailabilityTests`：Release・Developmentのシーン名検証と、ステージの段階解放・開発用全解放の状態を確認します。
- `RunParticleEditorTests`：ImGuiのパーティクル編集と設定の保存・読み込みを、GPUを初期化せず検証します。

実行にはMSVC、Windows SDKとC++20が必要です。生成物と保存テストの一時ファイルは `generated/tests` に置きます。

## ゲームのDevelopmentビルドを使う検証

先にゲーム本体を `Development|x64` でビルドします。ソースファイルを削除した後は、古いobjが残らないようクリーンしてからビルドしてください。

```bat
tests\RunClonePlatformTrialTests.cmd
tests\RunStageGimmickSwitchTests.cmd
tests\RunTitleDemoTimelineTests.cmd
tests\RunEffectEditorTests.cmd
```

- `RunClonePlatformTrialTests`：3区間の進行・復帰位置、分身への追従、再合流、分身寿命、開始・落下演出の時間、ポーズ操作を検証します。
- `RunStageGimmickSwitchTests`：通常スイッチ、必要重量、通常扉と再生準備の状態を検証します。
- `RunTitleDemoTimelineTests`：タイトルデモの時間進行と、待機を含む記録・分身の消失タイミングを検証します。
- `RunEffectEditorTests`：時空破砕・時間停止・時間逆行のImGuiと再生状態を、実際のGPU描画なしで検証します。

## 現在のゲーム仕様

- ステージ1は分身を使う3区間コースです。区間1・2の到達で次区間へ進み、最終区間の到達でタイトルへ戻ります。
- 提出用仕様として、ステージ選択へクリア履歴を書き込みません。標準ではステージ1のみ入場でき、開発用ImGuiで全解放へ切り替えられます。
- Rとポーズからのやり直しは、現在区間の記録と分身を消して再開します。Tによる再生準備では保存済み記録を保持します。
- 分身は記録の終端に約1秒残りながら薄くなり、表示と足場判定が同時に消えます。
- Releaseでは数字キー・Qによる画面効果の検証切り替えを受け付けません。エフェクトの手動再生は開発用ImGuiのボタンを使用します。
- 旧scene.jsonの自動配置と専用編集UIは削除しました。現在のステージ編集・保存・ギズモ操作は別系統として残しています。

## 実画面で確認する項目

自動テストはゲーム全体の描画や実入力を保証しません。提出前には以下を確認します。

1. タイトル、操作説明、設定、ステージ選択、未解放表示が開ける。
2. ステージ開始演出の終了後に操作でき、3区間を通してプレイできる。
3. 区間2・3でR・T・落下時の復帰先が現在区間になっている。
4. ポーズ中の停止、操作説明、確認付きリセット・退出が動く。
5. 最終到達でタイトルへ戻り、再入場で最初の区間から始まる。
6. Developmentで実ステージの移動・複製・保存・再読込・ギズモ操作ができる。保存確認では元のステージを上書きせず、generated/tests内の別ファイルを使う。
7. 開発用エフェクトの再生ボタンと粒子編集が動き、Releaseで検証キーが画面効果を変えない。

素材の再生成方法は `tools/README.md` を参照してください。
