#pragma once
#include "../../engine/base/IScene.h"
#include "../../engine/base/RenderTarget.h"
#include "../../engine/base/PostProcess.h"
#include "../player/PastSelfClone.h"
#include "StageSelectionState.h"

#include <memory>
#include <array>
#include <vector>

namespace MyEngine {
class Object3d;
class Sprite;
}

/// <summary>
/// タイトルシーンを管理するクラス
/// </summary>
class TitleScene : public MyEngine::IScene {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    explicit TitleScene(std::shared_ptr<GameApp::StageSelectionState> stageSelection = nullptr, bool stageSelect = false);

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~TitleScene();

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(const MyEngine::SceneContext& ctx) override;

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize() override;

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update(float dt) override;

    /// <summary>
    /// 描画処理
    /// </summary>
    void Draw() override;

    /// <summary>
    /// タイトルのパーティクル編集UIを描画する。
    /// </summary>
    void DrawImGui() override;

    /// <summary>
    /// シーンに入るときの処理
    /// </summary>
    void OnEnter() override;

    /// <summary>
    /// シーンから出るときの処理
    /// </summary>
    void OnExit() override;

    /// <summary>
    /// Scene View用のオフスクリーン描画だけにするか設定する。
    /// </summary>
    void SetSceneViewOnly(bool enabled) override;

    /// <summary>
    /// Scene Viewに表示するSRV番号を取得する。
    /// </summary>
    uint32_t GetSceneViewSrvIndex() const override;

    /// <summary>
    /// ウィンドウリサイズ時にScene View用RenderTargetを追従させる。
    /// </summary>
    void OnWindowResize(uint32_t width, uint32_t height) override;

    /// <summary>
    /// シーンが所有する3Dオブジェクトのポインタを収集する
    /// </summary>
    void FillObject3dPointers(std::vector<MyEngine::Object3d*>* out) override;

    /// <summary>
    /// シーンが所有するスプライトのポインタを収集する
    /// </summary>
    void FillSpritePointers(std::vector<MyEngine::Sprite*>* out) override;

    /// <summary>
    /// シーン名を取得する
    /// </summary>
    std::string GetName() const override { return stageSelectEntry_ ? "StageSelect" : "Title"; }

private:
    /// <summary>
    /// タイトル用の3Dオブジェクトを描画する
    /// </summary>
    void DrawWorldObjects();

    /// <summary>
    /// 所有中の3Dオブジェクトから参照用ビューを作り直す。
    /// </summary>
    void RebuildObjectPointerView();

    /// <summary>
    /// 所有中のスプライトから参照用ビューを作り直す。
    /// </summary>
    void RebuildSpritePointerView();

    /// <summary>
    /// ParticleManagerへタイトルシーン用の描画状態を登録する。
    /// </summary>
    void RegisterParticleManagerState();

    /// <summary>
    /// デモ全体を収めるタイトル用カメラを設定する。
    /// </summary>
    void ConfigureDemoCamera();

    /// <summary>
    /// タイトル専用の舞台、本体、分身と表示スプライトを作成する。
    /// </summary>
    void InitializeDemo();

    /// <summary>
    /// 記録、巻き戻し、分身再生と扉の通過を繰り返す。
    /// </summary>
    void UpdateDemo(float dt);

    /// <summary>
    /// 舞台または半透明演出用のブロックを作成して所有一覧へ追加する。
    /// </summary>
    MyEngine::Object3d* CreateDemoBlock(const Math::Vector3& scale, const Math::Vector3& position, const Math::Vector4& color, bool translucent = false);

    /// <summary>
    /// 顔と向きの目印、配線を作成する。
    /// </summary>
    void InitializeDemoPresentation();

    /// <summary>
    /// 本体と分身の顔を現在位置と向きへ追従させる。
    /// </summary>
    void UpdateDemoFace(const std::array<MyEngine::Object3d*, 4>& parts, const Math::Transform& transform, float alpha);

    /// <summary>
    /// 分身の出現、顔の追従、配線の点灯を更新する。
    /// </summary>
    void UpdateDemoPresentation(bool switchPressed);

    /// <summary>
    /// ロゴと開始表示を描画サイズに合わせて配置する。
    /// </summary>
    void UpdateTitleSprites();

    /// <summary>
    /// 右側メニューと設定・操作説明の表示を初期化する。
    /// </summary>
    void InitializeTitleMenu();

    /// <summary>
    /// メニューの選択、決定、戻る操作と音量変更を処理する。
    /// </summary>
    void UpdateTitleMenuInput();

    /// <summary>
    /// メニューとパネルの位置、選択色、音量バーを更新する。
    /// </summary>
    void UpdateTitleMenuLayout();

    /// <summary>
    /// 音量設定の見出し、目盛り、つまみと選択表示をまとめて配置する。
    /// </summary>
    void UpdateVolumeSettingsLayout(float width, float height);

    /// <summary>
    /// デモとロゴの上へメニューまたは開いているパネルを描画する。
    /// </summary>
    void DrawTitleMenu();

private:
    /// <summary>
    /// ステージ選択用の文字と実配置プレビューを作成する。
    /// </summary>
    void InitializeStageSelect();

    /// <summary>
    /// 実ステージJSONからルート別のプレビューを構築する。
    /// </summary>
    void InitializeStagePreviews();

    /// <summary>
    /// 選択移動、プレイ開始、タイトルへ戻る入力を処理する。
    /// </summary>
    void UpdateStageSelectInput();

    /// <summary>
    /// 選択ラインの補間と選択・決定時の演出時間を更新する。
    /// </summary>
    void UpdateStageSelectFeedback(float dt);

    /// <summary>
    /// 選択文字とプレビューのカメラ・描画行列を更新する。
    /// </summary>
    void UpdateStageSelect(float dt);

    /// <summary>
    /// 選択ルート全体を右側に収めるカメラと描画行列を設定する。
    /// </summary>
    void UpdateStagePreviewCamera(size_t stageIndex);

    /// <summary>
    /// 入場不可のステージを低解像度で描画し、縦横のぼかしを適用する。
    /// </summary>
    bool DrawLockedStagePreview(size_t stageIndex);

    /// <summary>
    /// 入場不可プレビューを覆う半透明のもや粒子を初期化する。
    /// </summary>
    void InitializeStageFog();

    /// <summary>
    /// プレビュー内で漂うもやの位置・大きさ・透明度を更新する。
    /// </summary>
    void UpdateStageFog(float dt);

    /// <summary>
    /// 現在の切り取り範囲内へ半透明のもや粒子を重ねる。
    /// </summary>
    void DrawStageFog();

    /// <summary>
    /// 現在の選択ステージと開始項目を共通遷移付きで描画する。
    /// </summary>
    void DrawStageSelect();

    std::shared_ptr<GameApp::StageSelectionState> stageSelection_; // Gameが所有する起動中の選択・攻略履歴
    bool stageSelectEntry_ = false; // ステージ選択シーンとして生成されたか
    size_t selectedStageItem_ = 0; // ステージ3件または戻るの選択位置
    std::array<std::vector<std::unique_ptr<MyEngine::Object3d>>, GameApp::kSelectableStageCount> stagePreviews_; // 実配置から構築したルート別の表示オブジェクト
    std::vector<MyEngine::Sprite*> stageSelectSprites_; // 選択画面にだけ描画するスプライト参照
    std::array<MyEngine::Sprite*, GameApp::kSelectableStageCount> stageLabels_ {}; // ステージ番号と名前
    std::array<MyEngine::Sprite*, GameApp::kSelectableStageCount> stageStatuses_ {}; // 挑戦可能またはクリア済みの表示
    MyEngine::Sprite* stageDetail_ = nullptr; // 選択ステージの攻略テーマ
    MyEngine::Sprite* stageCursor_ = nullptr; // 選択行を示す細い矩形
    MyEngine::Sprite* stageBack_ = nullptr; // タイトルへ戻る項目
    MyEngine::Sprite* stagePreviewError_ = nullptr; // プレビューを作成できない時の表示
    MyEngine::Sprite* stagePreviewEdges_ = nullptr; // もやの四辺を背景色へなじませる境界マスク
    std::array<MyEngine::Sprite*, 2> stageSelectRules_ {}; // 見出し下と一覧右側の細い区切り線
    MyEngine::RenderTarget stageBlurIntermediateTarget_; // 入場不可プレビューの横方向ぼかし結果
    std::array<std::unique_ptr<MyEngine::Sprite>, 18> stageFogSprites_; // 既存Sprite描画で重ねる画面専用のもや粒子
    float stageFogTime_ = 0.0f; // もやのゆっくりした揺らぎに使う経過秒数
    float stageCursorY_ = 0.34f; // 描画高さに対する現在の選択ライン位置
    float stageCursorStartY_ = 0.34f; // ライン移動を始めた時点の位置
    float stageCursorMoveTime_ = 1.0f; // ライン移動開始からの経過秒数
    float stageFogSelectionTime_ = 1.0f; // ステージ選択時のもや反応の経過秒数
    float stageLockedConfirmTime_ = 1.0f; // 未公開ステージを決定した時の反応の経過秒数

    MyEngine::SceneContext ctx_; // シーンへ渡された共通コンテキスト
    std::vector<std::unique_ptr<MyEngine::Object3d>> objects3d_; // タイトル表示用3Dオブジェクト
    std::vector<std::unique_ptr<MyEngine::Object3d>> translucentObjects_; // 半透明で描画する分身の顔の所有一覧
    std::vector<MyEngine::Object3d*> objectPointerView_; // ImGuiなど外部参照用の3Dオブジェクト一覧
    std::vector<std::unique_ptr<MyEngine::Sprite>> sprites_; // タイトル表示用スプライト
    std::vector<MyEngine::Sprite*> spritePointerView_; // ImGuiなど外部参照用のスプライト一覧
    std::unique_ptr<MyEngine::Object3d> particlePlane_; // GPUパーティクル確認用の描画平面
    MyEngine::RenderTarget sceneRenderTarget_; // Scene Viewに表示するタイトル描画結果
    MyEngine::RenderTarget demoRenderTarget_; // 白黒処理に渡す舞台とロゴの描画結果
    MyEngine::PostProcess demoPostProcess_; // 舞台とロゴの白黒と通常色の切り替え
    bool sceneViewOnly_ = false; // Scene View用RTだけへ描画するか
    PastSelfClone demoClone_; // 通常プレイと共通の記録形式を再生する分身
    MyEngine::Object3d* demoPlayer_ = nullptr; // 所有オブジェクト一覧内の本体
    MyEngine::Object3d* demoSwitch_ = nullptr; // 分身が踏むスイッチ
    MyEngine::Object3d* demoDoor_ = nullptr; // スイッチと連動して上昇する扉
    MyEngine::Object3d* demoGoal_ = nullptr; // 到達時に発光するゴール
    std::array<MyEngine::Object3d*, 4> demoPlayerFace_ {}; // 本体の両目、口、向きの目印
    std::array<MyEngine::Object3d*, 4> demoCloneFace_ {}; // 分身に追従する両目、口、向きの目印
    std::vector<MyEngine::Object3d*> demoWireSegments_; // スイッチから扉へ順に点灯する配線
    float demoTime_ = 0.0f; // デモ一巡内の経過秒数
    enum class MenuView { Main, Controls, Settings }; // タイトル内で開いている表示
    MenuView menuView_ = MenuView::Main; // 現在のメニュー表示
    size_t selectedMenuItem_ = 0; // 右側メニューで選択中の項目番号
    size_t selectedSettingItem_ = 0; // 音量と戻るの選択番号
    std::array<MyEngine::Sprite*, 3> menuLabels_ {}; // ゲーム開始、操作説明、設定の表示
    MyEngine::Sprite* menuSelection_ = nullptr; // 選択中の項目を示す細い色付き矩形
    MyEngine::Sprite* panelDimmer_ = nullptr; // パネルを開いた時だけ背景を暗くする矩形
    MyEngine::Sprite* panelBackground_ = nullptr; // 操作説明と設定の背景矩形
    MyEngine::Sprite* panelContent_ = nullptr; // 現在のパネル本文
    MyEngine::Sprite* panelBack_ = nullptr; // パネルを閉じる項目
    MyEngine::Sprite* volumeTrack_ = nullptr; // 全体音量のスライダー軌道
    MyEngine::Sprite* volumeFill_ = nullptr; // 現在の音量を示すスライダー部分
    MyEngine::Sprite* volumeValue_ = nullptr; // 0から100パーセントの音量表示
    MyEngine::Sprite* volumeLabel_ = nullptr; // スライダーに対応する全体音量の項目名
    MyEngine::Sprite* volumeThumb_ = nullptr; // 現在の音量位置を示す細いつまみ
    std::array<MyEngine::Sprite*, 2> settingsPanelRules_ {}; // 見出しと戻る項目を分ける横線
    MyEngine::Sprite* settingsSelection_ = nullptr; // 選択中の音量行または戻る項目を示す目印
    float titleMasterVolume_ = 1.0f; // 音声管理から取得した全体音量
    float menuBlinkTime_ = 0.0f; // デモのループに依存しない選択文字の点滅時刻
};
