#include "PlayScene.h"
#include "../../engine/particle/ParticleManager.h"
#include "ImGuiManager.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/2d/TextureManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/3d/SkyBox.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/io/InputManager.h"
#include "../../engine/utility/mathUtility.h"
#include "../../engine/utility/Logger.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <utility>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr float kStoppedDeltaTime = 0.0f; // ヒットストップや時間停止中に使用する停止時間
constexpr float kHitStopFinishedThreshold = 0.0f; // ヒットストップが終了したとみなす残り時間
constexpr float kKeyboardDissolveThreshold = 0.45f; // キー切り替え時に見えやすいDissolve閾値
constexpr bool kLoadEnvironmentMapOnStartup = false; // 遷移直後に環境マップを読み込むか

struct PostProcessShortcutDesc {
    uint8_t key; // 切り替えに使用するDirectInputキー
    PostEffectType effectType; // キーに対応するポストエフェクト種別
};

constexpr std::array<PostProcessShortcutDesc, 11> kPostProcessShortcutDescs = {
    PostProcessShortcutDesc { DIK_0, PostEffectType::Copy },
    PostProcessShortcutDesc { DIK_1, PostEffectType::Grayscale },
    PostProcessShortcutDesc { DIK_2, PostEffectType::Vignette },
    PostProcessShortcutDesc { DIK_3, PostEffectType::BoxFilter },
    PostProcessShortcutDesc { DIK_4, PostEffectType::GaussianFilter },
    PostProcessShortcutDesc { DIK_5, PostEffectType::LuminanceOutline },
    PostProcessShortcutDesc { DIK_6, PostEffectType::DepthOutline },
    PostProcessShortcutDesc { DIK_7, PostEffectType::RadialBlur },
    PostProcessShortcutDesc { DIK_8, PostEffectType::Dissolve },
    PostProcessShortcutDesc { DIK_9, PostEffectType::Random },
    PostProcessShortcutDesc { DIK_Q, PostEffectType::Distortion },
}; // 開発ビルドだけで使うポストエフェクトの検証用切り替えキー

/// <summary>
/// ImGui操作中にゲーム側ショートカットを止める必要があるか判定する。
/// </summary>
bool ShouldBlockGameShortcutInput()
{
#ifdef USE_IMGUI
    if (!ImGui::GetCurrentContext()) {
        return false;
    }

    const ImGuiIO& imguiIo = ImGui::GetIO(); // ImGuiの入力取得状態
    return imguiIo.WantCaptureKeyboard || ImGui::IsAnyItemActive();
#else
    return false;
#endif
}

constexpr const char* kEnvironmentMapTextureName = "rostock_laage_airport_4k.dds"; // 環境マップ用DDS名
constexpr const char* kCircleTextureName = "circle.png"; // 円形パーティクルに使用するテクスチャ名
constexpr const char* kCircleFlashTextureName = "circle2.png"; // 発光系スプライトに使用するテクスチャ名
constexpr const char* kGradationLineTextureName = "gradationLine.png"; // リングと円柱に使用するテクスチャ名
constexpr std::array<const char*, 3> kSceneTextureNames = {
    kCircleTextureName,
    kGradationLineTextureName,
    kEnvironmentMapTextureName,
}; // シーン初期化時に読み込むテクスチャ名

/// <summary>
/// ヒットストップが残っているか判定する
/// </summary>
bool HasActiveHitStop(float hitStopRemainingTime)
{
    return hitStopRemainingTime > kHitStopFinishedThreshold;
}

/// <summary>
/// 指定したテクスチャをPlayScene遷移直後に読み込むか判定する。
/// </summary>
bool ShouldLoadTextureOnStartup(const char* textureName)
{
    if (!textureName) {
        return false;
    }

    if (!kLoadEnvironmentMapOnStartup && std::string(textureName) == kEnvironmentMapTextureName) {
        return false;
    }

    return true;
}
}

/// <summary>
/// コンストラクタ
/// </summary>
PlayScene::PlayScene(std::shared_ptr<GameApp::StageSelectionState> stageSelection)
    : stageSelection_(stageSelection)
{
}

/// <summary>
/// デストラクタ
/// </summary>
PlayScene::~PlayScene() { }

/// <summary>
/// 指定したテクスチャ名からシーン用スプライトを生成する。
/// </summary>
void PlayScene::CreateSceneSprite(const std::string& textureName)
{
    if (ctx_.textureManager) {
        ctx_.textureManager->LoadTexture(textureName);
    }

    auto sprite = std::make_unique<Sprite>(); // 生成するスプライト
    sprite->SetSpriteId(IssueSpriteId());
    sprite->Initialize(ctx_.spriteCommon, textureName, ctx_.imguiManager);
    sprite->Update();
    sprites_.push_back(std::move(sprite));
    RebuildSpritePointerView();
}

/// <summary>
/// 指定した番号のシーン用スプライトを削除する。
/// </summary>
void PlayScene::DeleteSceneSprite(size_t spriteIndex)
{
    if (sprites_.size() <= spriteIndex) {
        return;
    }

    if (ctx_.imguiManager) {
        const size_t remainingSpriteCount = sprites_.size() - 1; // 削除後に残るスプライト数
        ctx_.imguiManager->NotifySpriteDeleted(spriteIndex, remainingSpriteCount);
    }

    sprites_.erase(sprites_.begin() + spriteIndex);
    RebuildSpritePointerView();
}

/// <summary>
/// シーンで使用するテクスチャを読み込む。
/// </summary>
void PlayScene::LoadSceneTextures()
{
    if (!ctx_.textureManager) {
        return;
    }

    for (const char* textureName : kSceneTextureNames) {
        if (!ShouldLoadTextureOnStartup(textureName)) {
            continue;
        }
        ctx_.textureManager->LoadTexture(textureName);
    }
}

/// <summary>
/// 環境マップ用のSkyBoxを初期化する。
/// </summary>
void PlayScene::InitializeSkyBox()
{
    if (!kLoadEnvironmentMapOnStartup) {
        return;
    }

    if (!ctx_.textureManager || !ctx_.srvManager || !ctx_.directXCommon) {
        return;
    }
    const uint32_t environmentMapSrvIndex = ctx_.textureManager->GetSrvIndex(kEnvironmentMapTextureName); // 環境マップのSRV番号
    if (environmentMapSrvIndex == UINT32_MAX) {
        return;
    }

    skybox_ = std::make_unique<SkyBox>();
    skybox_->Initialize(ctx_.directXCommon, ctx_.srvManager, environmentMapSrvIndex);
    if (ctx_.object3dCommon) {
        ctx_.object3dCommon->SetEnvironmentMapSrvIndex(environmentMapSrvIndex);
    }
}

/// <summary>
/// 現在のゲームで使用するステージと演出を初期化する。
/// </summary>
void PlayScene::Initialize(const SceneContext& ctx)
{
    ctx_ = ctx;
    objectEditorState_ = {};
    lastAutoOpenedStageObjectIndex_ = -1;

    LoadSceneTextures();
    InitializeSkyBox();
    InitializePastSelfTutorial();
    if (stageSelection_) {
        const std::array<PastSelfTutorialRoute, GameApp::kSelectableStageCount> routes {
            PastSelfTutorialRoute::OneCloneBasics, PastSelfTutorialRoute::TwoCloneCooperation, PastSelfTutorialRoute::FinalChallenge
        }; // 選択画面の項目と既存ルートの対応
        stageSelection_->selectedStageIndex = (std::min)(stageSelection_->selectedStageIndex, routes.size() - 1);
        SelectPastSelfTutorialRoute(routes[stageSelection_->selectedStageIndex]);
    }
    InitializeParticleObjects();
    if (kEnableLegacyPreview) {
        InitializeParticleEffects();
    } else {
        InitializeParticleManager();
    }
    InitializeTemporalEffectSprites();
    InitializePostProcessTargets();
    InitializeStageSelectionOverlay();
    InitializePauseMenu();
}

/// <summary>
/// シーンが保持している表示用オブジェクトを解放する。
/// </summary>
void PlayScene::ReleaseSceneObjects()
{
    pauseMenu_.Close();
    pauseDimmer_.reset();
    pausePanel_.reset();
    pauseCursor_.reset();
    pauseControls_.reset();
    pauseTexts_.clear();
    stageIntroSprite_.reset();
    stageOverlayBackground_.reset();
    stageStartRing_.reset();
    for (auto& spark : stageFailureSparks_) { // 接触火花の表示リソース
        spark.reset();
    }
    stageFailureImpactEnabled_ = false;
    stageStartBody_.reset();
    stageStartDim_.reset();
    stageStartSound_.reset();
    stageStartSequence_ = {};
    stageFailureSequence_ = {};
    stageFailureHazardBlockIndex_.reset();
    stageFailureDim_.reset();
    stageFailureSound_.reset();
    sprites_.clear();
    spritePointerView_.clear();
    nextSpriteId_ = 1;
    objects3d_.clear();
    nextObjectId_ = 1;
    objectEditorState_ = {};
    lastAutoOpenedStageObjectIndex_ = -1;
    particleEmitterPointerView_.clear();
    nextParticleEmitterId_ = 1;
    temporalAfterimageSprites_.clear();
    timeReversalSprites_.clear();
    timeReversalAfterimageSprites_.clear();
    timeReversalConvergenceSprite_.reset();
    player_.Finalize();
    pastSelfTutorialStageBlocks_.clear();
    pastSelfTutorialGimmickLayouts_.clear();
    pastSelfTutorialDefaultGimmickLayouts_.clear();
    pastSelfTutorialSwitch_.Finalize();
    pastSelfTutorialDoor_.Finalize();
    pastSelfTutorialTimedSwitch_.Finalize();
    pastSelfTutorialTimedDoor_.Finalize();
    pastSelfTutorialToggleSwitch_.Finalize();
    pastSelfTutorialToggleGate_.Finalize();
    pastSelfTutorialToggleElevator_.Finalize();
    pastSelfTutorialWeightSwitch_.Finalize();
    pastSelfTutorialGoalBridge_.Finalize();
    pastSelfTutorialOneWayGate_.Finalize();
    pastSelfTutorialGoal_.Reset();
    pastSelfTutorialState_ = {};
    pastSelfRecorder_.Clear();
    pastSelfCloneManager_.Finalize();
}

/// <summary>
/// SkyBoxを解放する。
/// </summary>
void PlayScene::ReleaseSkyBox()
{
    if (skybox_) {
        skybox_->Finalize();
        skybox_.reset();
    }
}

/// <summary>
/// 終了処理を行う。
/// </summary>
void PlayScene::Finalize()
{
    std::cout << "PlayScene Finalize\n";
    StopCameraShake();

    FinalizePostProcessTargets();
    ClearSceneParticles();
    ReleaseSceneObjects();
    timeReversalEffect_.ResetState();
    ReleaseParticleObjects();
    ReleaseSkyBox();
    temporalRiftEffect_.ResetState(ctx_.camera);
    timeStopEffect_.ResetState();
    ctx_ = {};
}

/// <summary>
/// 開発ビルドでポストエフェクトの検証用切り替え入力を処理する。
/// </summary>
void PlayScene::HandlePostProcessShortcutInput()
{
#ifdef USE_IMGUI
    if (ShouldBlockGameShortcutInput()) {
        return;
    }

    InputManager* inputManager = InputManager::GetInstance(); // ポストエフェクト切り替え入力を取得する入力管理
    if (!inputManager) {
        return;
    }

    for (const PostProcessShortcutDesc& shortcutDesc : kPostProcessShortcutDescs) {
        if (!inputManager->IsKeyJustPressed(shortcutDesc.key)) {
            continue;
        }

        ApplyPostProcessShortcut(shortcutDesc.effectType);
        return;
    }
#endif
}

/// <summary>
/// キー入力で選択されたポストエフェクトを適用する。
/// </summary>
void PlayScene::ApplyPostProcessShortcut(PostEffectType effectType)
{
    if (!IsValidPostEffectType(effectType)) {
        return;
    }

    postProcess_.SetEnabled(true);
    postProcess_.SetEffectType(effectType);
    if (effectType == PostEffectType::Dissolve) {
        postProcess_.SetDissolveThreshold(kKeyboardDissolveThreshold);
    }

    Logger::Log(std::string("Post effect changed: ") + GetPostEffectTypeName(effectType) + "\n");
}

/// <summary>
/// 時間演出とポストプロセスの状態を更新する。
/// </summary>
void PlayScene::UpdateTemporalEffects(float deltaTime)
{
    const float hitStopRemainingTime = temporalRiftEffect_.GetHitStopRemainingTime(); // 現在のヒットストップ残り時間
    const bool hasActiveHitStop = HasActiveHitStop(hitStopRemainingTime); // ヒットストップ中か
    const float effectDeltaTime = hasActiveHitStop ? kStoppedDeltaTime : deltaTime; // ヒットストップを反映した演出時間
    UpdateTimeReversalTransformHistory();
    UpdateTemporalRiftEffect(effectDeltaTime);
    UpdateTimeReversalEffect(effectDeltaTime);
    UpdateTimeStopEffect(deltaTime);
    UpdateImpactResponse(deltaTime);
    if (!hasActiveHitStop) {
        UpdateTemporalAfterimages();
    }
    postProcess_.Update(deltaTime);
}

/// <summary>
/// 再生中エフェクトに対応するポストエフェクト中心を計算する。
/// </summary>
Vector2 PlayScene::CalculatePostEffectCenter() const
{
    if (timeStopEffect_.IsPlaying()) {
        return CalculateWorldScreenUv(timeStopEffect_.GetEffectPosition());
    }
    if (timeReversalEffect_.IsPlaying()) {
        return CalculateWorldScreenUv(timeReversalEffect_.GetEffectPosition());
    }

    return temporalRiftEffect_.GetScreenUv();
}

/// <summary>
/// ポストエフェクトの中心座標を更新する。
/// </summary>
void PlayScene::UpdatePostEffectCenters()
{
    temporalRiftEffect_.SetScreenUv(CalculateTemporalRiftScreenUv());
    const Vector2 postEffectCenter = CalculatePostEffectCenter(); // 再生中エフェクトに対応する画面中心
    postProcess_.SetRadialBlurCenter(postEffectCenter);
    postProcess_.SetDistortionCenter(postEffectCenter);
}

/// <summary>
/// 更新処理
/// </summary>
void PlayScene::Update(float dt)
{
    if (ctx_.isSceneTransitionActive && ctx_.isSceneTransitionActive()) {
        // 明転中は操作とゲーム進行を止め、初期化後の描画行列だけ整える。
        if (ctx_.camera) {
            ctx_.camera->Update();
        }
        UpdatePastSelfTutorialVisuals();
        UpdateSceneObjects(0.0f);
        return;
    }
    if (stageFailureSequence_.IsActive()) {
        UpdateStageFailurePresentation(dt);
        return;
    }
    if (HandleStageCompletion()) {
        return;
    }
    if (HandlePauseMenuInput()) {
        // GPU描画側にも停止した時間を渡し、直前フレームの時間で進むことを防ぐ。
        if (auto* particleManager = ParticleManager::GetInstance()) { // 停止中の描画時間を同期する管理クラス
            particleManager->Update(0.0f);
        }
        return;
    }
    UpdateStageStartPresentation(dt);
    if (stageStartSequence_.IsHolding()) {
        UpdatePastSelfTutorialVisuals();
        UpdateSceneObjects(0.0f);
        return;
    }
    HandlePostProcessShortcutInput();
    UpdateTemporalEffects(dt);

    if (ctx_.camera) {
        ctx_.camera->Update();
    }
    UpdatePastSelfTutorial(dt);
    if (stageFailureSequence_.IsActive()) {
        if (auto* particleManager = ParticleManager::GetInstance()) { // 失敗が始まったフレームからGPUの時間も止める管理クラス
            particleManager->Update(0.0f);
        }
        return;
    }
    if (HandleStageCompletion()) {
        return;
    }
    UpdatePostEffectCenters();
    UpdateParticleSystems(dt);

    UpdateSceneObjects(dt);

    UpdateAfterimageSprites();
    UpdateTimeReversalSprites();
}

/// <summary>
/// シーン内の3Dオブジェクトを更新する。
/// </summary>
void PlayScene::UpdateSceneObjects(float deltaTime)
{
    if (!ctx_.camera) {
        return;
    }

    const Matrix4x4 viewMatrix = ctx_.camera->GetViewMatrix(); // 3Dオブジェクト更新に使用するビュー行列
    const Matrix4x4 projectionMatrix = ctx_.camera->GetProjectionMatrix(); // 3Dオブジェクト更新に使用する射影行列
    for (auto& object3d : objects3d_) { // 更新対象の3Dオブジェクト
        if (object3d) {
            object3d->UpdateAnimation(deltaTime);
            object3d->Update(viewMatrix, projectionMatrix);
        }
    }
}

/// <summary>
/// 描画処理を行う
/// </summary>
void PlayScene::Draw()
{
    if (DrawPostProcessedScene()) {
        return;
    }

    if (ctx_.beginSceneTransition) {
        ctx_.beginSceneTransition(-1);
    }
    DrawSceneContent();
    DrawDebugLines3D();
    DrawSprites();
    DrawDebugLines2D();
    if (ctx_.drawSceneTransition) {
        ctx_.drawSceneTransition();
    }
}

/// <summary>
/// シーンに入るときの処理
/// </summary>
void PlayScene::OnEnter()
{
    std::cout << "PlayScene OnEnter\n";
    InitializeParticleManager();
}

/// <summary>
/// シーンから出るときの処理
/// </summary>
void PlayScene::OnExit() { std::cout << "PlayScene OnExit\n"; }

/// <summary>
/// 所有中のスプライトから参照用ビューを作り直す。
/// </summary>
void PlayScene::RebuildSpritePointerView()
{
    spritePointerView_.clear();
    spritePointerView_.reserve(sprites_.size());
    for (auto& sprite : sprites_) {
        if (sprite) {
            spritePointerView_.push_back(sprite.get());
        }
    }
}

/// <summary>
/// 所有中のパーティクルエミッターから参照用ビューを作り直す。
/// </summary>
void PlayScene::RebuildParticleEmitterPointerView()
{
    particleEmitterPointerView_.clear();
    particleEmitterPointerView_.reserve(3);
    particleEmitterPointerView_.push_back(&pmEmitter_);
    particleEmitterPointerView_.push_back(&ringEmitter_);
    particleEmitterPointerView_.push_back(&cylinderEmitter_);
}

/// <summary>
/// 次に生成する3Dオブジェクトへ割り当てるIDを取得する。
/// </summary>
uint32_t PlayScene::IssueObjectId()
{
    const uint32_t issuedObjectId = nextObjectId_; // 今回割り当てる3DオブジェクトID
    nextObjectId_++;
    return issuedObjectId;
}

/// <summary>
/// 次に生成するスプライトへ割り当てるIDを取得する。
/// </summary>
uint32_t PlayScene::IssueSpriteId()
{
    const uint32_t issuedSpriteId = nextSpriteId_; // 今回割り当てるスプライトID
    nextSpriteId_++;
    return issuedSpriteId;
}

/// <summary>
/// 次に生成するパーティクルエミッターへ割り当てるIDを取得する。
/// </summary>
uint32_t PlayScene::IssueParticleEmitterId()
{
    const uint32_t issuedEmitterId = nextParticleEmitterId_; // 今回割り当てるパーティクルエミッターID
    nextParticleEmitterId_++;
    return issuedEmitterId;
}

/// <summary>
/// シーンが所有するオブジェクトポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillObject3dPointers(std::vector<Object3d*>* out)
{
    if (!out) {
        return;
    }

    out->clear();
    std::vector<PastSelfTutorialEditorObject> editorObjects; // 現在のルートで実際に使用するステージ編集対象
    BuildPastSelfTutorialEditorObjects(&editorObjects);
    out->reserve(editorObjects.size());
    for (const PastSelfTutorialEditorObject& editorObject : editorObjects) {
        out->push_back(editorObject.object);
    }
}

/// <summary>
/// Level EditorとGizmoで共有する選択中3Dオブジェクト番号を取得する。
/// </summary>
int PlayScene::GetSelectedSceneObjectIndex() const
{
    return ctx_.imguiManager ? ctx_.imguiManager->GetSelectedObjectIndex() : -1;
}

/// <summary>
/// Level EditorからGizmo対象の3Dオブジェクトを選択する。
/// </summary>
void PlayScene::SelectSceneObjectForEditor(size_t objectIndex)
{
    if (!ctx_.imguiManager) {
        return;
    }

    std::vector<PastSelfTutorialEditorObject> editorObjects; // 選択可能な実ステージオブジェクト一覧
    BuildPastSelfTutorialEditorObjects(&editorObjects);
    if (objectIndex >= editorObjects.size()) {
        return;
    }

    ctx_.imguiManager->SetSelectedObjectIndex(static_cast<int>(objectIndex));
}

/// <summary>
/// Scene ViewのGizmoで編集されたTransformを実ステージの判定へ反映する。
/// </summary>
void PlayScene::NotifyObjectTransformEdited(size_t objectIndex)
{
    std::vector<PastSelfTutorialEditorObject> editorObjects; // ギズモ番号に対応する実ステージオブジェクト一覧
    BuildPastSelfTutorialEditorObjects(&editorObjects);
    if (objectIndex >= editorObjects.size()) {
        return;
    }
    ApplyPastSelfTutorialEditorTransform(editorObjects[objectIndex]);
}

/// <summary>
/// シーンが所有するパーティクルエミッターポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillParticleEmitterPointers(std::vector<::ParticleEmitter*>* out)
{
    if (!out) {
        return;
    }

    RebuildParticleEmitterPointerView();
    out->clear();
    out->reserve(particleEmitterPointerView_.size());
    out->insert(out->end(), particleEmitterPointerView_.begin(), particleEmitterPointerView_.end());
}

/// <summary>
/// シーンが所有するスプライトポインタ群を ImGui に渡すために埋めるフック
/// </summary>
void PlayScene::FillSpritePointers(std::vector<Sprite*>* out)
{
    if (!out) {
        return;
    }

    RebuildSpritePointerView();
    out->clear();
    out->reserve(spritePointerView_.size());
    out->insert(out->end(), spritePointerView_.begin(), spritePointerView_.end());
}
