#include "TitleScene.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/2d/TextureManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/3d/PrimitiveFactory.h"
#include "../../engine/particle/ParticleManager.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/base/ImGuiManager.h"
#include "../../engine/io/InputManager.h"
#include "../../engine/sound/Sound.h"
#include <iostream>
#include <array>
#include <algorithm>
#include <cmath>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr const char* kTitleGpuParticleGroupName = "TitleGpuParticle"; // タイトル確認用GPUパーティクルグループ名
constexpr const char* kTitleGpuParticleTextureName = "circle.png"; // タイトル確認用GPUパーティクルテクスチャ
constexpr std::array<float, 4> kTitleSceneClearColor = { 0.10f, 0.12f, 0.16f, 1.0f }; // タイトルScene Viewのクリア色
constexpr bool kDisableAlphaCutoutSampler = false; // 円形テクスチャの透明度をそのまま使う設定
constexpr bool kShowTitleParticles = false; // デモを見やすくするためタイトルのパーティクル更新と描画を停止する
constexpr float kSelectedMenuBlinkPeriod = 1.2f; // 選択文字が明るくなって戻るまでの点滅周期
constexpr float kSelectedMenuMinimumAlpha = 0.4f; // 点滅中も文字を読み取れる最低表示率
constexpr const char* kDemoModel = "block/block.obj"; // プレイシーンと共通のブロックモデル
constexpr float kAttemptBegin = 0.6f; // 本体だけでスイッチへ向かい始める時刻
constexpr float kAttemptSwitchArrival = 2.2f; // 初回の本体がスイッチ中央へ到着する時刻
constexpr float kAttemptSwitchApproachEnd = kAttemptSwitchArrival - 0.4f; // スイッチ手前で一度入力を離す時刻
constexpr float kAttemptSwitchAdjustBegin = kAttemptSwitchArrival - 0.18f; // 位置を確認して短く踏み直し始める時刻
constexpr float kAttemptLeaveSwitch = 3.1f; // 開いた扉を見て本体がスイッチから離れる時刻
constexpr float kAttemptStepOffEnd = kAttemptLeaveSwitch + 0.14f; // スイッチから一歩だけ離れて停止する時刻
constexpr float kAttemptStepOffDistance = 0.11f; // スイッチの押下範囲を抜ける最小限の移動距離
constexpr float kAttemptReturnBegin = kAttemptStepOffEnd + 0.7f; // 閉じた扉を確認してから記録準備へ移る時刻
constexpr float kAttemptReturnEnd = kAttemptReturnBegin + 1.2f; // 記録を始める位置へ移動し終える時刻
constexpr float kRecordBegin = kAttemptReturnEnd + 0.3f; // 記録準備後にスイッチを押す役の記録を始める時刻
constexpr float kRecordMoveSeconds = 2.4f; // 記録でスイッチへ到達するまでの時間
constexpr float kRecordHoldSeconds = 5.2f; // 再生時に本体が扉を通過するまで記録へ含める待機時間
constexpr float kRecordEnd = kRecordBegin + kRecordMoveSeconds + kRecordHoldSeconds; // スイッチ上の待機も含めて記録を終了する時刻
constexpr float kRewindBegin = kRecordEnd + 0.6f; // 記録終了を見せた後に巻き戻す時刻
constexpr float kRewindEnd = kRewindBegin + 1.2f; // 本体が開始位置へ戻る時刻
constexpr float kReplayBegin = kRewindEnd + 0.6f; // 分身が出現する時刻
constexpr float kColorRestoreTime = kReplayBegin + 0.2f; // 分身出現の静止を見せて通常色へ戻す時刻
constexpr float kCloneMoveBegin = kReplayBegin + 0.6f; // 分身の姿を見せた後に再生移動を始める時刻
constexpr float kSignalBegin = kCloneMoveBegin + kRecordMoveSeconds; // 分身がスイッチへ到達する時刻
constexpr float kDoorBegin = kSignalBegin + 1.0f; // 点灯が扉へ届いてから開き始める時刻
constexpr float kDoorEnd = kDoorBegin + 0.7f; // 扉が開ききる時刻
constexpr float kCrossBegin = kDoorEnd + 0.5f; // 開いた扉を見せた後に本体が進み始める時刻
constexpr float kCrossCheckBegin = kCrossBegin + 1.2f; // 成功時も扉の手前で一度入力を離す時刻
constexpr float kCrossCheckEnd = kCrossCheckBegin + 0.16f; // 開いたままの扉を確認して通過を再開する時刻
constexpr float kCrossEnd = kCrossBegin + 3.0f; // 本体がゴールへ到達する時刻
constexpr float kDemoDuration = kCrossEnd + 2.0f; // 単独での失敗と分身による成功を含むデモ一巡の長さ
constexpr float kStartX = -1.0f; // 左の記録経路と右の出口経路を分ける出発位置
constexpr float kSwitchX = -5.0f; // 出口と反対側に置く本体と分身共通のスイッチ中心位置
constexpr float kDoorX = 2.0f; // スイッチから7単位離した扉の中心位置
constexpr float kGoalX = 5.0f; // ゴールの中心位置
constexpr float kPlayerLaneZ = 0.0f; // 本体と分身が共通で移動する2.5D平面
constexpr float kDoorLift = 2.0f; // 本体が下を通れる扉の上昇量
constexpr Vector4 kPlayerColor = { 0.95f, 0.97f, 1.0f, 1.0f }; // 本体の通常色
constexpr Vector4 kRecordColor = { 1.0f, 0.25f, 0.48f, 1.0f }; // 記録中の本体色
constexpr Vector4 kSwitchIdleColor = { 0.12f, 0.35f, 0.23f, 1.0f }; // スイッチ非作動色
constexpr Vector4 kSwitchActiveColor = { 0.12f, 1.0f, 0.48f, 1.0f }; // スイッチと扉の作動色
constexpr Vector4 kWireIdleColor = { 0.07f, 0.16f, 0.13f, 1.0f }; // 配線の消灯色
constexpr float kDemoFacingYaw = 1.57079633f; // 移動平面の左右を向く90度の回転角
constexpr float kRecordApproachEnd = 1.55f; // 記録移動でスイッチ手前へ到着する相対時刻
constexpr float kRecordAdjustBegin = 1.8f; // 手前で位置を確認して短く踏み直す相対時刻
constexpr float kRecordAdjustEnd = 2.2f; // スイッチ上で位置合わせを終える相対時刻
constexpr float kSwitchApproachOffset = 0.24f; // 最後の位置合わせで進む距離
constexpr float kGoalHopBegin = kCrossEnd + 0.35f; // ゴールへ到着してから喜びの跳躍を始める時刻
constexpr float kGoalHopEnd = kGoalHopBegin + 0.45f; // 喜びの跳躍から着地する時刻
constexpr const char* kTitleSceneEditorWindowName = "Scene Editor"; // タイトル用エフェクト編集ウィンドウ名

/// <summary>
/// 区間内の時刻を滑らかな移動率へ変換する。
/// </summary>
float DemoProgress(float time, float begin, float end)
{
    const float rate = (std::clamp)((time - begin) / (end - begin), 0.0f, 1.0f); // 区間内の移動率
    return rate * rate * (3.0f - 2.0f * rate);
}

/// <summary>
/// 出現後の待機とループ境界を除き、記録再生と終了後の残像へ渡す経過時間を計算する。
/// </summary>
float CalculateDemoCloneDeltaTime(float previousTime, float currentTime)
{
    const float beginTime = currentTime < previousTime ? 0.0f : previousTime; // 一巡した場合は新しい周回の先頭を基準にする時刻
    return (std::max)(currentTime - kCloneMoveBegin, 0.0f) - (std::max)(beginTime - kCloneMoveBegin, 0.0f);
}

/// <summary>
/// 入力開始に短く加速し、入力を離すと素早く止まる移動率を計算する。
/// </summary>
float DemoTravelProgress(float time, float begin, float end)
{
    constexpr float kAccelerationRate = 0.08f; // 入力開始から最高速度へ達するまでの時間割合
    constexpr float kBrakingRate = 0.04f; // 入力を離して停止するまでの時間割合
    constexpr float kTravelArea = 1.0f - (kAccelerationRate + kBrakingRate) * 0.5f; // 加減速を含む移動量の正規化係数
    const float rate = (std::clamp)((time - begin) / (end - begin), 0.0f, 1.0f); // 移動区間内の時間率
    if (rate < kAccelerationRate) {
        return rate * rate / (2.0f * kAccelerationRate * kTravelArea);
    }
    if (rate > 1.0f - kBrakingRate) {
        const float remaining = 1.0f - rate; // 減速区間の残り時間率
        return 1.0f - remaining * remaining / (2.0f * kBrakingRate * kTravelArea);
    }
    return (rate - kAccelerationRate * 0.5f) / kTravelArea;
}

/// <summary>
/// 左右の向きが切り替わる時に最短方向へ回転する。
/// </summary>
float BlendDemoYaw(float start, float end, float rate)
{
    constexpr float kFullTurn = 6.28318531f; // Y回転の一周分の角度
    return start + std::remainder(end - start, kFullTurn) * rate;
}

/// <summary>
/// 足元を動かさずに体を縮め、踏み込みや着地を表現する。
/// </summary>
void CompressDemoBody(PlayerState& state, float amount)
{
    const float previousHeight = state.transform.scale.y; // 変形前の体の高さ
    state.transform.scale.y *= 1.0f - amount;
    state.transform.translate.y += (state.transform.scale.y - previousHeight) * 0.5f;
}

/// <summary>
/// スイッチまでの記録移動に使うプレイヤー状態を作成する。
/// </summary>
PlayerState CreateDemoRecordState(float time)
{
    const float approachRate = DemoTravelProgress(time, 0.0f, kRecordApproachEnd); // スイッチ手前までの移動率
    const float adjustRate = DemoProgress(time, kRecordAdjustBegin, kRecordAdjustEnd); // 一度止まってからの短い位置合わせ率
    const float x = kStartX + (kSwitchX + kSwitchApproachOffset - kStartX) * approachRate - kSwitchApproachOffset * adjustRate; // 左のスイッチへ進む記録経路上のX位置
    PlayerState state {}; // 分身に保存する本体状態
    state.transform.scale = { 0.9f, 1.2f, 0.9f };
    state.transform.rotate.y = kDemoFacingYaw;
    const float switchStep = DemoProgress(kStartX - x, kStartX - kSwitchX - 1.4f, kStartX - kSwitchX - 0.6f); // 左のスイッチの段差へ足元を合わせる率
    state.transform.translate = { x, 0.6f + 0.18f * switchStep, 0.0f };
    state.isMoving = (time > 0.0f && time < kRecordApproachEnd) || (time > kRecordAdjustBegin && time < kRecordAdjustEnd);
    return state;
}

/// <summary>
/// スイッチを離れて扉の閉鎖を確認し、その場で止まってから記録準備へ移る姿勢を作成する。
/// </summary>
PlayerState CreateDemoAttemptState(float time)
{
    PlayerState state = CreateDemoRecordState(0.0f); // 記録と同じ大きさで動く初回の本体状態
    if (time < kAttemptLeaveSwitch) {
        const float approachRate = DemoTravelProgress(time, kAttemptBegin, kAttemptSwitchApproachEnd); // 最初はスイッチ手前まで進む入力率
        const float adjustRate = DemoTravelProgress(time, kAttemptSwitchAdjustBegin, kAttemptSwitchArrival); // 確認後に短く踏み込む入力率
        state.transform.translate.x = kStartX + (kSwitchX + kSwitchApproachOffset - kStartX) * approachRate - kSwitchApproachOffset * adjustRate;
        state.isMoving = (time > kAttemptBegin && time < kAttemptSwitchApproachEnd) ||
            (time > kAttemptSwitchAdjustBegin && time < kAttemptSwitchArrival);
        state.transform.rotate.y = BlendDemoYaw(kDemoFacingYaw, -kDemoFacingYaw,
            DemoProgress(time, kAttemptLeaveSwitch - 0.2f, kAttemptLeaveSwitch));
    } else if (time < kAttemptReturnBegin) {
        const float rate = DemoTravelProgress(time, kAttemptLeaveSwitch, kAttemptStepOffEnd); // スイッチの入力を解除する一歩分の移動率
        state.transform.translate.x = kSwitchX + kAttemptStepOffDistance * rate;
        state.transform.rotate.y = -kDemoFacingYaw;
        state.isMoving = time < kAttemptStepOffEnd;
    } else {
        const float preparationX = kSwitchX + kAttemptStepOffDistance; // 閉鎖を確認して停止した位置
        const float rate = DemoTravelProgress(time, kAttemptReturnBegin, kAttemptReturnEnd); // 確認後に記録開始位置へ移る移動率
        state.transform.translate.x = preparationX + (kStartX - preparationX) * rate;
        state.transform.rotate.y = BlendDemoYaw(-kDemoFacingYaw, kDemoFacingYaw,
            DemoProgress(time, kAttemptReturnEnd, kRecordBegin));
        state.isMoving = time < kAttemptReturnEnd;
    }
    const float switchStep = DemoProgress(kStartX - state.transform.translate.x,
        kStartX - kSwitchX - 1.4f, kStartX - kSwitchX - 0.6f); // 往復中もスイッチの段差へ足元を合わせる率
    state.transform.translate.y = 0.6f + 0.18f * switchStep;
    return state;
}

/// <summary>
/// 単独での失敗、分身の見送り、扉の通過、ゴールでの反応を作成する。
/// </summary>
PlayerState CreateDemoPlayerState(float time, const PlayerState& cloneState, float stopX)
{
    if (time < kRecordBegin) {
        return CreateDemoAttemptState(time);
    }
    PlayerState state = CreateDemoRecordState(time - kRecordBegin); // 記録中の本体状態
    if (time < kRewindBegin) {
        return state;
    }
    if (time < kRewindEnd) {
        const float rewindRate = DemoProgress(time, kRewindBegin, kRewindEnd); // 過去の動きを逆順に戻す率
        state = CreateDemoRecordState((1.0f - rewindRate) * (kRecordEnd - kRecordBegin));
        state.transform.rotate.y = BlendDemoYaw(kDemoFacingYaw, -kDemoFacingYaw, DemoProgress(time, kRewindBegin, kRewindBegin + 0.2f));
        return state;
    }

    state = CreateDemoRecordState(0.0f);
    state.transform.translate.z = kPlayerLaneZ;
    const float watchYaw = cloneState.transform.translate.x <= state.transform.translate.x ? kDemoFacingYaw : -kDemoFacingYaw; // 奥行きへ向かず分身のいる左右を向く角度
    const float watchRate = DemoProgress(time, kRewindEnd, kReplayBegin); // 分身出現の静止に入る前に向き直りを終える率
    state.transform.rotate.y = BlendDemoYaw(-kDemoFacingYaw, watchYaw, watchRate);
    if (time < kCrossBegin) {
        const float reactionRate = DemoProgress(time, kDoorEnd + 0.15f, kCrossBegin); // 扉が開いたことに気づいて出発方向へ向く率
        state.transform.rotate.y = BlendDemoYaw(state.transform.rotate.y, -kDemoFacingYaw, reactionRate);
        CompressDemoBody(state, 0.07f * std::sin(reactionRate * 3.14159265f));
        return state;
    }

    const float checkX = kDoorX - 1.0f; // 扉の手前で開放状態を確認する本体位置
    const float approachRate = DemoTravelProgress(time, kCrossBegin, kCrossCheckBegin); // まず扉の手前まで進む移動率
    const float passRate = DemoTravelProgress(time, kCrossCheckEnd, kCrossEnd); // 確認後に扉を通ってゴールへ進む移動率
    const float x = kStartX + (checkX - kStartX) * approachRate + (kGoalX - checkX) * passRate; // 短い入力解除を挟む本体の通過経路
    state.transform.translate = { x, 0.6f + 0.12f * DemoProgress(x, kGoalX - 1.5f, kGoalX - 0.3f), kPlayerLaneZ };
    state.transform.rotate.y = -kDemoFacingYaw;
    state.isMoving = time < kCrossCheckBegin || (time > kCrossCheckEnd && time < kCrossEnd);
    if (time >= kCrossEnd) {
        if (time < kGoalHopBegin) {
            CompressDemoBody(state, 0.12f * DemoProgress(time, kCrossEnd + 0.18f, kGoalHopBegin));
        } else if (time < kGoalHopEnd) {
            const float jumpRate = (time - kGoalHopBegin) / (kGoalHopEnd - kGoalHopBegin); // 一度だけ跳ねる跳躍の時間率
            state.transform.translate.y += 0.35f * 4.0f * jumpRate * (1.0f - jumpRate);
            CompressDemoBody(state, 0.12f * (1.0f - DemoProgress(time, kGoalHopBegin, kGoalHopBegin + 0.08f)));
            state.isGrounded = false;
        } else {
            const float landing = DemoProgress(time, kGoalHopEnd, kGoalHopEnd + 0.08f) *
                (1.0f - DemoProgress(time, kGoalHopEnd + 0.08f, kGoalHopEnd + 0.25f)); // 着地後に一度沈んで姿勢を戻す率
            CompressDemoBody(state, 0.1f * landing);
        }
    }
    return state;
}

/// <summary>
/// タイトルシーンをScene Viewへ表示するためのRenderTarget設定を作成する。
/// </summary>
RenderTargetDesc CreateTitleSceneRenderTargetDesc(DirectXCommon* directXCommon)
{
    RenderTargetDesc desc {}; // Scene View用RenderTarget設定
    desc.width = directXCommon ? static_cast<uint32_t>(directXCommon->GetRenderWidth()) : 1u;
    desc.height = directXCommon ? static_cast<uint32_t>(directXCommon->GetRenderHeight()) : 1u;
    desc.format = directXCommon ? directXCommon->GetSwapChainFormat() : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.useDepth = true;
    desc.createColorSrv = true;
    desc.createDepthSrv = false;
    desc.resizeWithWindow = true;
    desc.clearColor = kTitleSceneClearColor;
    return desc;
}

/// <summary>
/// タイトルシーンでGPUパーティクル確認用の描画平面を作成する。
/// </summary>
std::unique_ptr<Object3d> CreateTitleParticleDrawObject(const SceneContext& ctx)
{
    auto particleObject = std::make_unique<Object3d>(); // GPUパーティクルを描画する平面Object
    particleObject->Initialize(ctx.object3dCommon, ctx.imguiManager);
    particleObject->SetMesh(PrimitiveFactory::CreatePlane());
    particleObject->SetTexture(kTitleGpuParticleTextureName);
    particleObject->SetEnableLighting(false);
    particleObject->SetUseAlphaCutoutSampler(kDisableAlphaCutoutSampler);
    return particleObject;
}
}

/// <summary>
/// コンストラクタ
/// </summary>
TitleScene::TitleScene(std::shared_ptr<GameApp::StageSelectionState> stageSelection, bool stageSelect)
    : stageSelection_(stageSelection ? stageSelection : std::make_shared<GameApp::StageSelectionState>())
    , stageSelectEntry_(stageSelect)
{
}

/// <summary>
/// デストラクタ
/// </summary>
TitleScene::~TitleScene() { }

/// <summary>
/// 初期化処理
/// </summary>
void TitleScene::Initialize(const SceneContext& ctx)
{
    ctx_ = ctx;

    if (stageSelectEntry_) {
        if (ctx_.directXCommon) {
            sceneRenderTarget_.Initialize(ctx_.directXCommon, ctx_.srvManager, CreateTitleSceneRenderTargetDesc(ctx_.directXCommon));
        }
        InitializeStageSelect();
        return;
    }

    particlePlane_ = CreateTitleParticleDrawObject(ctx_);
    if (ctx_.directXCommon) {
        sceneRenderTarget_.Initialize(ctx_.directXCommon, ctx_.srvManager, CreateTitleSceneRenderTargetDesc(ctx_.directXCommon));
        demoRenderTarget_.Initialize(ctx_.directXCommon, ctx_.srvManager, CreateTitleSceneRenderTargetDesc(ctx_.directXCommon));
        demoPostProcess_.Initialize(ctx_.directXCommon);
    }

    RegisterParticleManagerState();
    InitializeDemo();
    ConfigureDemoCamera();
    UpdateDemo(0.0f);
    UpdateTitleSprites();
}

/// <summary>
/// 終了処理
/// </summary>
void TitleScene::Finalize()
{
    std::cout << "TitleScene Finalize\n";

    ParticleManager* particleManager = ctx_.particleManager; // タイトルで登録したパーティクル状態をクリアする管理
    if (!particleManager) {
        particleManager = ParticleManager::GetInstance();
    }
    if (particleManager) {
        particleManager->ClearSceneParticles();
    }

    sceneRenderTarget_.Finalize();
    demoRenderTarget_.Finalize();
    stageBlurIntermediateTarget_.Finalize();
    demoPostProcess_.Finalize();
    demoClone_.Finalize();
    demoPlayer_ = nullptr;
    demoSwitch_ = nullptr;
    demoDoor_ = nullptr;
    demoGoal_ = nullptr;
    demoPlayerFace_.fill(nullptr);
    demoCloneFace_.fill(nullptr);
    demoWireSegments_.clear();
    for (auto& preview : stagePreviews_) { // シーン破棄時に解放するプレビューの所有一覧
        preview.clear();
    }
    stageSelectSprites_.clear();
    for (auto& fog : stageFogSprites_) { // シーン切替時に解放するもやの描画リソース
        fog.reset();
    }
    stageFogTime_ = 0.0f;
    translucentObjects_.clear();
    particlePlane_.reset();
    sprites_.clear();
    spritePointerView_.clear();
    objects3d_.clear();
    objectPointerView_.clear();
    ctx_ = {};
}

/// <summary>
/// 更新処理
/// </summary>
void TitleScene::Update(float dt)
{
    if (stageSelectEntry_) {
        UpdateStageSelect(dt);
        return;
    }
    ParticleManager* particleManager = ctx_.particleManager; // GPUパーティクル確認に使う管理クラス
    if (!particleManager) {
        particleManager = ParticleManager::GetInstance();
    }
    if (kShowTitleParticles && particleManager) {
        particleManager->Update(dt);
    }

    if (!ctx_.isSceneTransitionActive || !ctx_.isSceneTransitionActive()) {
        UpdateTitleMenuInput();
    }
    menuBlinkTime_ = std::fmod(menuBlinkTime_ + (std::max)(dt, 0.0f), kSelectedMenuBlinkPeriod);

    UpdateDemo(dt);
    ConfigureDemoCamera();
    UpdateTitleSprites();
    UpdateTitleMenuLayout();
    if (ctx_.camera) {
        ctx_.camera->Update();
    }

    if (!ctx_.camera) {
        return;
    }

    const Matrix4x4 viewMatrix = ctx_.camera->GetViewMatrix(); // 3Dオブジェクト更新に使用するビュー行列
    const Matrix4x4 projectionMatrix = ctx_.camera->GetProjectionMatrix(); // 3Dオブジェクト更新に使用する射影行列
    demoClone_.UpdateObject(viewMatrix, projectionMatrix);
    for (auto& object : objects3d_) {
        if (object) {
            object->Update(viewMatrix, projectionMatrix);
        }
    }

    for (auto& object : translucentObjects_) {
        object->Update(viewMatrix, projectionMatrix);
    }

    for (auto& sprite : sprites_) {
        if (sprite) {
            sprite->Update();
        }
    }
}

/// <summary>
/// 舞台とロゴへ時間演出を適用し、操作メニューを通常色で重ねる。
/// </summary>
void TitleScene::Draw()
{
    if (stageSelectEntry_) {
        DrawStageSelect();
        return;
    }
    const bool useDemoPostProcess = demoRenderTarget_.IsValid() && demoRenderTarget_.HasColorSrv()
        && demoPostProcess_.IsReady() && demoPostProcess_.IsCopyReady() && demoPostProcess_.IsGrayscaleReady(); // 舞台とロゴへ白黒処理を適用できるか
    if (useDemoPostProcess) {
        demoRenderTarget_.Begin(true);
        DrawWorldObjects();
        if (ctx_.spriteCommon && sprites_.size() >= 2) {
            ctx_.spriteCommon->SetCommonDrawSetting();
            // ロゴはループ暗転より手前に置き、舞台と同じ白黒処理へ渡す。
            sprites_[1]->Draw();
            sprites_[0]->Draw();
        }
        demoRenderTarget_.End();
    }

    const bool useSceneViewTarget = sceneViewOnly_ && sceneRenderTarget_.IsValid(); // ImGui Scene Viewへ描画するか
    if (useSceneViewTarget) {
        sceneRenderTarget_.Begin(true);
    }
    if (ctx_.beginSceneTransition) {
        ctx_.beginSceneTransition(useSceneViewTarget ? sceneRenderTarget_.GetHandle() : -1);
    }

    if (useDemoPostProcess) {
        const bool recordingWorld = demoTime_ >= kRecordBegin && demoTime_ < kColorRestoreTime; // 記録から分身確定まで白黒にする区間
        demoPostProcess_.DrawTexture(demoRenderTarget_.GetColorSrvIndex(),
            recordingWorld ? PostEffectType::Grayscale : PostEffectType::Copy);
    } else {
        DrawWorldObjects();
    }

    if (ctx_.spriteCommon) {
        ctx_.spriteCommon->SetCommonDrawSetting();
        if (sprites_.size() >= 2) {
            if (!useDemoPostProcess) {
                // 白黒処理が使えない場合も、暗転とロゴの重なり順を保つ。
                sprites_[1]->Draw();
                sprites_[0]->Draw();
            }
            // 操作メニューと設定画面は白黒処理の対象に含めない。
            DrawTitleMenu();
        }
    }

    if (useSceneViewTarget) {
        if (ctx_.drawSceneTransition) {
            ctx_.drawSceneTransition();
        }
        sceneRenderTarget_.End();
    } else if (ctx_.drawSceneTransition) {
        ctx_.drawSceneTransition();
    }
}

/// <summary>
/// タイトルの演出編集とステージ解放の確認用UIを描画する。
/// </summary>
void TitleScene::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin(kTitleSceneEditorWindowName);
    if (ImGui::BeginTabBar("TitleSceneEditorTabs")) {
        if (ImGui::BeginTabItem("Stages")) {
            if (stageSelection_) {
                if (ImGui::Checkbox("Unlock all stages (testing)", &stageSelection_->unlockAllStages)) {
                    UpdateStageSelect(0.0f);
                }
                ImGui::TextUnformatted("Default: clear earlier stages to unlock the next.");
                ImGui::TextUnformatted("Clear history and override last only until the application exits.");
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Effects")) {
            ParticleManager* particleManager = ctx_.particleManager; // GPUパーティクル設定を編集する管理クラス
            if (!particleManager) {
                particleManager = ParticleManager::GetInstance();
            }
            if (particleManager) {
                particleManager->DrawImGui(nullptr);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
#endif
}

/// <summary>
/// タイトル用の3Dオブジェクトを描画する
/// </summary>
void TitleScene::DrawWorldObjects()
{
    if (ctx_.object3dCommon) {
        ctx_.object3dCommon->SetCommonDrawSetting();
        for (auto& object : objects3d_) {
            if (object) {
                object->Draw();
            }
        }
        const BlendMode previousBlend = ctx_.object3dCommon->GetBlendMode(); // 透過演出の前の描画設定
        ctx_.object3dCommon->SetBlendMode(BlendMode::Alpha);
        for (auto& object : translucentObjects_) {
            if (object->GetMaterialColor().w > 0.001f) {
                object->Draw();
            }
        }
        demoClone_.Draw();
        ctx_.object3dCommon->SetBlendMode(previousBlend);
    }

    ParticleManager* particleManager = ctx_.particleManager; // GPUパーティクル確認に使う管理クラス
    if (!particleManager) {
        particleManager = ParticleManager::GetInstance();
    }
    if (kShowTitleParticles && particleManager) {
        particleManager->Draw();
    }
}

/// <summary>
/// シーンに入るときの処理
/// </summary>
void TitleScene::OnEnter()
{
    std::cout << "TitleScene OnEnter\n";
    if (stageSelectEntry_) {
        selectedStageItem_ = 0;
        stageSelection_->selectedStageIndex = 0;
        UpdateStageSelect(0.0f);
        return;
    }
    RegisterParticleManagerState();
    demoTime_ = 0.0f;
    menuView_ = MenuView::Main;
    selectedMenuItem_ = 0;
    selectedSettingItem_ = 0;
    menuBlinkTime_ = 0.0f;
    demoClone_.Stop();
    ConfigureDemoCamera();
    UpdateDemo(0.0f);
}

/// <summary>
/// シーンから出るときの処理
/// </summary>
void TitleScene::OnExit()
{
    std::cout << "TitleScene OnExit\n";
}

/// <summary>
/// ParticleManagerへタイトルシーン用の描画状態を登録する。
/// </summary>
void TitleScene::RegisterParticleManagerState()
{
    if (!particlePlane_) {
        return;
    }

    ParticleManager* particleManager = ctx_.particleManager; // GPUパーティクル確認に使う管理クラス
    if (!particleManager) {
        particleManager = ParticleManager::GetInstance();
    }

    if (particleManager) {
        particleManager->ClearSceneParticles();
        particleManager->SetParticlePlane(particlePlane_.get());
        particleManager->CreateParticleGroup(kTitleGpuParticleGroupName, kTitleGpuParticleTextureName);
        particleManager->SetParticleObject(kTitleGpuParticleGroupName, particlePlane_.get());
    }
}

/// <summary>
/// デモ全体を収めるタイトル用カメラを設定する。
/// </summary>
void TitleScene::ConfigureDemoCamera()
{
    if (!ctx_.camera) {
        return;
    }

    const float aspect = ctx_.directXCommon ? static_cast<float>(ctx_.directXCommon->GetRenderWidth()) /
        (std::max)(static_cast<float>(ctx_.directXCommon->GetRenderHeight()), 1.0f) : 16.0f / 9.0f; // 左側へ舞台全体を収める描画縦横比
    const float distance = (std::max)(24.0f, 13.5f / (0.6f * 2.0f * std::tan(0.31f) * aspect)); // 舞台幅を画面の6割以内へ制限する距離
    const float cameraX = 0.36f * distance * std::tan(0.31f) * aspect; // 舞台中心を画面左32パーセント付近へ移す横位置
    ctx_.camera->SetRotate({ -0.12f, 0.0f, 0.0f });
    ctx_.camera->SetTranslate({ cameraX, 3.8f * distance / 21.0f, -distance });
    ctx_.camera->SetFovY(0.62f);
    ctx_.camera->Update();
}

/// <summary>
/// 舞台または半透明演出用のブロックを作成して所有一覧へ追加する。
/// </summary>
Object3d* TitleScene::CreateDemoBlock(const Vector3& scale, const Vector3& position, const Vector4& color, bool translucent)
{
    auto object = std::make_unique<Object3d>(); // 作成するタイトル用ブロック
    object->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
    object->SetModel(kDemoModel);
    object->SetScale(scale);
    object->SetTranslate(position);
    object->SetMaterialColor(color);
    object->SetUseTexture(false);
    object->SetEnableLighting(false);
    object->SetUseAlphaDiscard(false);
    Object3d* result = object.get(); // 所有一覧へ移した後も有効な参照
    if (translucent) {
        translucentObjects_.push_back(std::move(object));
    } else {
        objects3d_.push_back(std::move(object));
    }
    return result;
}

/// <summary>
/// 顔と向きの目印、配線を作成する。
/// </summary>
void TitleScene::InitializeDemoPresentation()
{
    const std::array<Vector3, 4> faceScales = { Vector3 { 0.16f, 0.22f, 0.04f }, Vector3 { 0.16f, 0.22f, 0.04f },
        Vector3 { 0.3f, 0.07f, 0.04f }, Vector3 { 0.32f, 0.13f, 0.22f } }; // 両目、口、右向きの目印の大きさ
    for (size_t index = 0; index < faceScales.size(); ++index) {
        demoPlayerFace_[index] = CreateDemoBlock(faceScales[index], {}, { 0.04f, 0.09f, 0.12f, 1.0f });
        demoCloneFace_[index] = CreateDemoBlock(faceScales[index], {}, { 0.7f, 1.0f, 1.0f, 0.0f }, true);
    }

    // 横視点から読める床の側面へ配線を置く。
    demoWireSegments_.push_back(CreateDemoBlock({ 0.08f, 0.3f, 0.06f }, { kSwitchX, -0.08f, -0.93f }, kWireIdleColor));
    for (int index = 0; index < 12; ++index) {
        const float segmentWidth = (kDoorX - kSwitchX) / 12.0f; // スイッチから扉までを等分した配線の幅
        demoWireSegments_.push_back(CreateDemoBlock({ segmentWidth, 0.08f, 0.06f },
            { kSwitchX + (index + 0.5f) * segmentWidth, -0.23f, -0.93f }, kWireIdleColor));
    }
    demoWireSegments_.push_back(CreateDemoBlock({ 0.08f, 0.3f, 0.06f }, { kDoorX, -0.08f, -0.93f }, kWireIdleColor));

}

/// <summary>
/// 本体と分身の顔を現在位置と向きへ追従させる。
/// </summary>
void TitleScene::UpdateDemoFace(const std::array<Object3d*, 4>& parts, const Transform& transform, float alpha)
{
    const std::array<Vector3, 4> offsets = { Vector3 { -0.21f, 0.18f, -0.48f }, Vector3 { 0.21f, 0.18f, -0.48f },
        Vector3 { 0.0f, -0.17f, -0.48f }, Vector3 { 0.0f, 0.67f, -0.35f } }; // 正面を向いた時の両目、口、向きの目印の位置
    const float cosYaw = std::cos(transform.rotate.y); // 向きに合わせて顔を回すcos値
    const float sinYaw = std::sin(transform.rotate.y); // 向きに合わせて顔を回すsin値
    for (size_t index = 0; index < parts.size(); ++index) {
        Object3d* part = parts[index]; // 現在更新する顔の部品
        const Vector3 offset = offsets[index]; // 回転前の部品位置
        part->SetTranslate({ transform.translate.x + offset.x * cosYaw + offset.z * sinYaw,
            transform.translate.y + offset.y * transform.scale.y / 1.2f, transform.translate.z - offset.x * sinYaw + offset.z * cosYaw });
        part->SetRotate(transform.rotate);
        Vector4 color = part->GetMaterialColor(); // 部品の色を保って表示率を反映する色
        color.w = alpha;
        part->SetMaterialColor(color);
    }
}

/// <summary>
/// 分身の出現、顔の追従、配線の点灯を更新する。
/// </summary>
void TitleScene::UpdateDemoPresentation(bool switchPressed)
{
    const Transform playerTransform = { demoPlayer_->GetScale(), demoPlayer_->GetRotate(), demoPlayer_->GetTranslate() }; // 本体の顔へ反映する姿勢
    UpdateDemoFace(demoPlayerFace_, playerTransform, 1.0f);
    const float cloneAlpha = demoClone_.IsVisible() ? DemoProgress(demoTime_, kReplayBegin, kColorRestoreTime) : 0.0f; // 色が戻るまでに分身の姿を確定する表示率
    demoClone_.SetMaterialColors({ 1.0f, 0.05f, 0.95f, 0.78f * cloneAlpha }, { 0.9f, 0.45f, 1.0f, 0.65f * cloneAlpha });
    UpdateDemoFace(demoCloneFace_, demoClone_.GetCurrentState().transform, cloneAlpha);

    const float signalBegin = demoTime_ < kRecordBegin ? kAttemptSwitchArrival :
        (demoTime_ < kReplayBegin ? kRecordBegin + kRecordAdjustEnd : kSignalBegin); // 単独試行、記録、再生でスイッチを踏み終える時刻
    const float signalDuration = demoTime_ < kReplayBegin ? kDoorEnd - kDoorBegin : kDoorBegin - kSignalBegin; // 各踏み込みで扉が開くまでに配線を点灯する時間
    const float signalRate = switchPressed ? (std::clamp)((demoTime_ - signalBegin) / signalDuration, 0.0f, 1.0f) : 0.0f; // 押している間だけ進む配線点灯率
    for (size_t index = 0; index < demoWireSegments_.size(); ++index) {
        const float segmentBegin = static_cast<float>(index) / static_cast<float>(demoWireSegments_.size()); // この配線区間へ光が届く進行率
        const bool active = switchPressed && signalRate > segmentBegin; // スイッチから順に点灯するか
        demoWireSegments_[index]->SetMaterialColor(active ? kSwitchActiveColor : kWireIdleColor);
    }
}

/// <summary>
/// タイトル専用の舞台、本体、分身と表示スプライトを作成する。
/// </summary>
void TitleScene::InitializeDemo()
{
    if (!ctx_.object3dCommon) {
        return;
    }
    CreateDemoBlock({ 13.5f, 0.8f, 1.8f }, { 0.0f, -0.4f, 0.0f }, { 0.24f, 0.28f, 0.31f, 1.0f });
    CreateDemoBlock({ 0.25f, 3.5f, 0.25f }, { kDoorX, 1.75f, 0.8f }, { 0.4f, 0.46f, 0.5f, 1.0f });
    CreateDemoBlock({ 1.1f, 0.25f, 1.85f }, { kDoorX, 3.5f, 0.0f }, { 0.4f, 0.46f, 0.5f, 1.0f });
    demoSwitch_ = CreateDemoBlock({ 1.5f, 0.18f, 1.3f }, { kSwitchX, 0.09f, kPlayerLaneZ }, kSwitchIdleColor);
    demoDoor_ = CreateDemoBlock({ 0.35f, 2.8f, 1.5f }, { kDoorX, 1.4f, kPlayerLaneZ }, kSwitchIdleColor);
    demoGoal_ = CreateDemoBlock({ 1.8f, 0.12f, 1.3f }, { kGoalX, 0.06f, kPlayerLaneZ }, { 0.35f, 0.3f, 0.08f, 1.0f });
    demoPlayer_ = CreateDemoBlock({ 0.9f, 1.2f, 0.9f }, { kStartX, 0.6f, 0.0f }, kPlayerColor);
    demoClone_.Initialize(ctx_.object3dCommon, ctx_.imguiManager, kDemoModel);
    demoClone_.SetMaterialColors({ 1.0f, 0.05f, 0.95f, 0.78f }, { 0.9f, 0.45f, 1.0f, 0.65f });

    PastSelfRecorder recorder; // 通常プレイと共通の記録を生成する一時レコーダー
    constexpr float kSampleInterval = 1.0f / 60.0f; // 記録の時間刻み
    const int recordSamples = static_cast<int>(std::ceil((kRecordEnd - kRecordBegin) / kSampleInterval)); // 移動とスイッチ待機を60Hzで記録する区間数
    recorder.Start();
    recorder.Update(0.0f, CreateDemoRecordState(0.0f));
    for (int sample = 1; sample <= recordSamples; ++sample) {
        recorder.Update(kSampleInterval, CreateDemoRecordState(sample * kSampleInterval));
    }
    recorder.Stop();
    demoClone_.Load(recorder.GetFrames());
    InitializeDemoPresentation();

    if (ctx_.spriteCommon) {
        TextureManager* textureManager = ctx_.textureManager ? ctx_.textureManager : TextureManager::GetInstance(); // タイトル画像の事前読み込み先
        for (const char* texture : { "title_logo.png" }) {
            // Spriteは未ロード画像をチェッカーへ置き換えるため、作成前に登録する。
            textureManager->LoadTexture(texture);
            auto sprite = std::make_unique<Sprite>(); // ロゴ、開始表示、背景切り替え用のスプライト
            sprite->Initialize(ctx_.spriteCommon, texture, ctx_.imguiManager);
            sprite->SetAnchorPoint({ 0.5f, 0.5f });
            sprites_.push_back(std::move(sprite));
        }
        auto fadeSprite = std::make_unique<Sprite>(); // デモのループ境界を隠す画像なしの単色矩形
        fadeSprite->InitializeSolidColor(ctx_.spriteCommon, ctx_.srvManager);
        fadeSprite->SetAnchorPoint({ 0.5f, 0.5f });
        sprites_.push_back(std::move(fadeSprite));
        InitializeTitleMenu();
    }
}

/// <summary>
/// 本体だけでの失敗から、記録・分身による役割分担での扉通過までを繰り返す。
/// </summary>
void TitleScene::UpdateDemo(float dt)
{
    if (!demoPlayer_ || !demoDoor_ || !demoSwitch_ || !demoGoal_) {
        return;
    }
    const float previousTime = demoTime_; // 再生終了後も正しい差分時間を求める更新前時刻
    const float nextTime = demoTime_ + (std::max)(dt, 0.0f); // 今回更新後のデモ時刻
    if (nextTime >= kDemoDuration) {
        demoClone_.Stop();
    }
    demoTime_ = std::fmod(nextTime, kDemoDuration);
    if (demoTime_ >= kReplayBegin) {
        if (!demoClone_.IsVisible() && demoClone_.GetPlaybackTime() == 0.0f) {
            demoClone_.Start();
        }
        demoClone_.Update(CalculateDemoCloneDeltaTime(previousTime, demoTime_), {});
    }
    const PlayerState watchedClone = demoClone_.IsVisible() ? demoClone_.GetCurrentState() : CreateDemoRecordState(0.0f); // 出現前も分身の開始位置を見送る対象状態
    const float playerTime = demoTime_ >= kReplayBegin && demoTime_ < kColorRestoreTime ? kReplayBegin : demoTime_; // 分身を確定する0.2秒間だけ本体の姿勢を止める時刻
    const float stopX = kDoorX - demoDoor_->GetScale().x * 0.5f - demoPlayer_->GetScale().z * 0.5f; // 横向きの本体が閉じた扉へ接する位置
    PlayerState state = CreateDemoPlayerState(playerTime, watchedClone, stopX); // 閉じた扉の制限を反映する本体の演出状態
    const bool cloneOnSwitch = demoClone_.IsVisible() &&
        std::fabs(demoClone_.GetCurrentState().transform.translate.x - kSwitchX) <= 0.1f; // 分身がスイッチ中央を踏んでいるか
    const bool playerOnSwitch = std::fabs(state.transform.translate.x - kSwitchX) <= 0.1f; // 本体がスイッチ中央を踏んでいるか
    const bool switchPressed = playerOnSwitch || cloneOnSwitch; // 本体と分身のどちらでも同じ入力として扱う押下状態
    const float doorBegin = demoTime_ < kRecordBegin ? kAttemptSwitchArrival :
        (demoTime_ < kReplayBegin ? kRecordBegin + kRecordAdjustEnd : kDoorBegin); // 単独試行、記録、再生で扉を開き始める時刻
    const float doorRate = switchPressed ? DemoProgress(demoTime_, doorBegin, doorBegin + (kDoorEnd - kDoorBegin)) : 0.0f; // 離れたら閉まり、分身が残ると開き続ける扉の表示率
    if (doorRate < 1.0f && state.transform.translate.x > stopX) {
        // 誰もスイッチを踏まない場合は、時刻が進んでも閉じた扉を越えない。
        state.transform.translate.x = (std::min)(state.transform.translate.x, stopX);
        state.transform.translate.y = 0.6f;
        state.transform.scale = { 0.9f, 1.2f, 0.9f };
        state.isGrounded = true;
    }
    demoPlayer_->SetTranslate(state.transform.translate);
    demoPlayer_->SetRotate(state.transform.rotate);
    demoPlayer_->SetScale(state.transform.scale);
    demoPlayer_->SetMaterialColor(demoTime_ >= kRecordBegin && demoTime_ < kRecordEnd ? kRecordColor : kPlayerColor);
    demoSwitch_->SetMaterialColor(switchPressed ? kSwitchActiveColor : kSwitchIdleColor);
    demoDoor_->SetTranslate({ kDoorX, 1.4f + kDoorLift * doorRate, kPlayerLaneZ });
    demoDoor_->SetMaterialColor(switchPressed ? kSwitchActiveColor : kSwitchIdleColor);
    const bool goalReached = state.transform.translate.x >= kGoalX - 0.01f; // 扉を越えて本体が実際にゴールへ到達したか
    demoGoal_->SetMaterialColor(goalReached ? Vector4 { 1.0f, 0.85f, 0.2f, 1.0f } : Vector4 { 0.35f, 0.3f, 0.08f, 1.0f });
    UpdateDemoPresentation(switchPressed);
}

/// <summary>
/// ロゴと開始表示を描画サイズに合わせて配置する。
/// </summary>
void TitleScene::UpdateTitleSprites()
{
    if (!ctx_.directXCommon || sprites_.size() < 2) {
        return;
    }
    const float width = static_cast<float>(ctx_.directXCommon->GetRenderWidth()); // 描画領域の横幅
    const float height = static_cast<float>(ctx_.directXCommon->GetRenderHeight()); // 描画領域の高さ
    const float logoWidth = (std::min)(width * 0.65f, height * 1.2f); // 縦横比を保ったロゴ横幅
    sprites_[0]->SetPosition({ width * 0.5f, height * 0.19f });
    sprites_[0]->SetSize({ logoWidth, logoWidth * 0.2f });
    const float fade = (std::max)(1.0f - demoTime_ / 0.4f, (demoTime_ - (kDemoDuration - 0.4f)) / 0.4f); // 一巡の境界を隠す背景暗転率
    sprites_[1]->SetPosition({ width * 0.5f, height * 0.5f });
    sprites_[1]->SetSize({ width, height });
    sprites_[1]->SetColor({ kTitleSceneClearColor[0], kTitleSceneClearColor[1], kTitleSceneClearColor[2], (std::clamp)(fade, 0.0f, 1.0f) });
}

/// <summary>
/// 右側メニューと設定・操作説明に使うスプライトを作成する。
/// </summary>
void TitleScene::InitializeTitleMenu()
{
    TextureManager* textureManager = ctx_.textureManager ? ctx_.textureManager : TextureManager::GetInstance(); // 文字画像を登録する管理
    for (const char* texture : { "title_menu.png", "title_controls.png", "title_settings.png" }) {
        textureManager->LoadTexture(texture);
    }
    const auto createSprite = [this](const char* texture) { // 文字または単色矩形を既存の所有一覧へ追加する処理
        auto sprite = std::make_unique<Sprite>(); // 今回作成するメニュー部品
        if (texture) {
            sprite->Initialize(ctx_.spriteCommon, texture, ctx_.imguiManager);
        } else {
            sprite->InitializeSolidColor(ctx_.spriteCommon, ctx_.srvManager);
        }
        sprite->SetAnchorPoint({ 0.0f, 0.5f });
        Sprite* result = sprite.get(); // 所有一覧へ移した後の参照
        sprites_.push_back(std::move(sprite));
        return result;
    };
    for (size_t index = 0; index < menuLabels_.size(); ++index) {
        menuLabels_[index] = createSprite("title_menu.png");
        menuLabels_[index]->SetTextureLeftTop({ 0.0f, static_cast<float>(index) * 64.0f });
        menuLabels_[index]->SetTextureSize({ 512.0f, 64.0f });
    }
    menuSelection_ = createSprite(nullptr);
    panelDimmer_ = createSprite(nullptr);
    panelBackground_ = createSprite(nullptr);
    panelContent_ = createSprite("title_controls.png");
    panelBack_ = createSprite("title_menu.png");
    panelBack_->SetTextureLeftTop({ 0.0f, 3.0f * 64.0f });
    panelBack_->SetTextureSize({ 512.0f, 64.0f });
    volumeTrack_ = createSprite(nullptr);
    volumeFill_ = createSprite(nullptr);
    volumeValue_ = createSprite("title_menu.png");
    volumeValue_->SetTextureSize({ 192.0f, 64.0f });
    volumeLabel_ = createSprite("title_menu.png");
    volumeLabel_->SetTextureLeftTop({ 0.0f, 4.0f * 64.0f });
    volumeLabel_->SetTextureSize({ 512.0f, 64.0f });
    volumeThumb_ = createSprite(nullptr);
    volumeThumb_->SetAnchorPoint({ 0.5f, 0.5f });
    for (Sprite*& rule : settingsPanelRules_) {
        rule = createSprite(nullptr);
    }
    settingsSelection_ = createSprite(nullptr);
    if (ctx_.soundSystem) {
        titleMasterVolume_ = ctx_.soundSystem->GetMasterVolume();
    }
    UpdateTitleMenuLayout();
}

/// <summary>
/// 選択、決定、戻る操作と10パーセント刻みの全体音量変更を処理する。
/// </summary>
void TitleScene::UpdateTitleMenuInput()
{
    InputManager* input = InputManager::GetInstance(); // メニュー操作の入力状態
    if (!input) {
        return;
    }
    const bool up = input->IsKeyJustPressed(DIK_UP) || input->IsKeyJustPressed(DIK_W) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_DPAD_UP); // 上の項目へ移る入力
    const bool down = input->IsKeyJustPressed(DIK_DOWN) || input->IsKeyJustPressed(DIK_S) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_DPAD_DOWN); // 下の項目へ移る入力
    const bool confirm = input->IsKeyJustPressed(DIK_RETURN) || input->IsKeyJustPressed(DIK_SPACE) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_A); // 選択中の項目を実行する入力
    const bool back = input->IsKeyJustPressed(DIK_ESCAPE) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_B); // 開いているパネルを閉じる入力
    if (menuView_ == MenuView::Main) {
        if (up != down) {
            selectedMenuItem_ = (selectedMenuItem_ + (up ? menuLabels_.size() - 1 : 1)) % menuLabels_.size();
        }
        if (confirm) {
            if (selectedMenuItem_ == 0 && ctx_.requestSceneChange) {
                ctx_.requestSceneChange("StageSelect");
            } else if (selectedMenuItem_ != 0) {
                menuView_ = selectedMenuItem_ == 1 ? MenuView::Controls : MenuView::Settings;
                selectedSettingItem_ = 0;
                panelContent_->SetTexture(menuView_ == MenuView::Controls ? "title_controls.png" : "title_settings.png");
            }
        }
        return;
    }
    if (back || (confirm && menuView_ == MenuView::Controls)) {
        menuView_ = MenuView::Main;
        return;
    }
    if (menuView_ != MenuView::Settings) {
        return;
    }
    if (up != down) {
        selectedSettingItem_ = 1 - selectedSettingItem_;
    }
    // 同時入力では、タイトルとセレクトと同じく移動後の項目を決定する。
    if (confirm && selectedSettingItem_ == 1) {
        menuView_ = MenuView::Main;
        return;
    }
    const bool left = input->IsKeyJustPressed(DIK_LEFT) || input->IsKeyJustPressed(DIK_A) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_DPAD_LEFT); // 音量を下げる入力
    const bool right = input->IsKeyJustPressed(DIK_RIGHT) || input->IsKeyJustPressed(DIK_D) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_DPAD_RIGHT); // 音量を上げる入力
    if (selectedSettingItem_ == 0 && left != right && ctx_.soundSystem) {
        const float volume = (std::clamp)(titleMasterVolume_ + (right ? 0.1f : -0.1f), 0.0f, 1.0f); // 今回設定する全体音量
        if (ctx_.soundSystem->SetMasterVolume(volume)) {
            titleMasterVolume_ = ctx_.soundSystem->GetMasterVolume();
        }
    }
}

/// <summary>
/// 解像度に合わせてメニューとパネルを配置し、選択状態を色へ反映する。
/// </summary>
void TitleScene::UpdateTitleMenuLayout()
{
    if (!ctx_.directXCommon || !menuSelection_) {
        return;
    }
    const float width = static_cast<float>(ctx_.directXCommon->GetRenderWidth()); // メニュー配置に使う描画幅
    const float height = static_cast<float>(ctx_.directXCommon->GetRenderHeight()); // メニュー配置に使う描画高さ
    const float labelWidth = (std::min)(width * 0.24f, height * 0.55f); // 右端からはみ出さない文字画像の幅
    const Vector4 selectedColor = { 0.2f, 0.94f, 0.8f, 1.0f }; // 選択中の項目と音量バーの色
    const float blinkRate = 0.5f + 0.5f * std::cos(menuBlinkTime_ * 6.28318531f / kSelectedMenuBlinkPeriod); // 選択文字の明暗を滑らかに変える率
    const Vector4 selectedTextColor = { selectedColor.x, selectedColor.y, selectedColor.z,
        kSelectedMenuMinimumAlpha + (1.0f - kSelectedMenuMinimumAlpha) * blinkRate }; // 完全には消えない選択文字の色
    for (size_t index = 0; index < menuLabels_.size(); ++index) {
        menuLabels_[index]->SetPosition({ width * 0.73f, height * (0.49f + static_cast<float>(index) * 0.1f) });
        menuLabels_[index]->SetSize({ labelWidth, labelWidth / 8.0f });
        menuLabels_[index]->SetColor(index == selectedMenuItem_ ? selectedTextColor : Vector4 { 0.8f, 0.84f, 0.87f, 1.0f });
    }
    menuSelection_->SetPosition({ width * 0.705f, height * (0.49f + static_cast<float>(selectedMenuItem_) * 0.1f) });
    menuSelection_->SetSize({ 3.0f, labelWidth * 0.055f });
    menuSelection_->SetColor(selectedColor);
    const float panelWidth = (std::min)(width * 0.8f, height * 1.18f); // 中央パネルの幅
    const float contentWidth = (std::min)(panelWidth * 0.92f, height * 0.94f); // 本文画像の幅
    panelDimmer_->SetPosition({ 0.0f, height * 0.5f });
    panelDimmer_->SetSize({ width, height });
    panelDimmer_->SetColor({ 0.0f, 0.0f, 0.0f, 0.7f });
    panelBackground_->SetPosition({ (width - panelWidth) * 0.5f, height * 0.5f });
    panelBackground_->SetSize({ panelWidth, height * 0.84f });
    panelBackground_->SetColor({ 0.08f, 0.09f, 0.10f, 1.0f });
    panelContent_->SetAnchorPoint({ 0.5f, 0.5f });
    panelContent_->SetPosition({ width * 0.5f, height * (menuView_ == MenuView::Controls ? 0.46f : 0.36f) });
    panelContent_->SetSize({ contentWidth, contentWidth * (menuView_ == MenuView::Controls ? 0.75f : 1.0f / 3.0f) });
    panelContent_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    panelBack_->SetPosition({ (width - contentWidth) * 0.5f + contentWidth / 15.0f, height * 0.86f });
    panelBack_->SetSize({ labelWidth, labelWidth / 8.0f });
    panelBack_->SetColor(menuView_ == MenuView::Controls || selectedSettingItem_ == 1 ? selectedTextColor : Vector4 { 0.8f, 0.84f, 0.87f, 1.0f });
    if (menuView_ == MenuView::Settings) {
        UpdateVolumeSettingsLayout(width, height);
    } else if (menuView_ == MenuView::Controls) {
        const float controlsLeft = (width - contentWidth) * 0.5f; // 二列の説明と戻る項目に共通の左端
        const float backWidth = contentWidth * 512.0f / 960.0f; // 本文と同じ文字倍率に揃える戻る画像の幅
        panelDimmer_->SetColor({ 0.0f, 0.0f, 0.0f, 0.82f });
        panelBackground_->SetColor({ 0.08f, 0.09f, 0.10f, 0.0f });
        panelContent_->SetAnchorPoint({ 0.0f, 0.0f });
        panelContent_->SetPosition({ controlsLeft, height * 0.10f });
        panelContent_->SetSize({ contentWidth, contentWidth * 720.0f / 960.0f });
        panelBack_->SetPosition({ controlsLeft, height * 0.88f });
        panelBack_->SetSize({ backWidth, backWidth / 8.0f });
        panelBack_->SetColor({ 0.80f, 0.84f, 0.87f, 1.0f });
    }
}

/// <summary>
/// 音量設定の文字とスライダーを共通の余白で配置し、選択状態を反映する。
/// </summary>
void TitleScene::UpdateVolumeSettingsLayout(float width, float height)
{
    const float panelWidth = (std::min)(width * 0.74f, height * 1.05f); // 音量設定領域の横幅
    const float panelHeight = panelWidth * 0.55f; // 各行の余白を一定比率に保つ高さ
    const Vector2 origin = { (width - panelWidth) * 0.5f, (height - panelHeight) * 0.5f }; // 中央パネルの左上位置
    const float inset = panelWidth * 0.1f; // 見出しと各項目に共通の左右余白
    const float innerWidth = panelWidth - inset * 2.0f; // 見出しとスライダーの共通幅
    const float textWidth = innerWidth * 0.56f; // 全体音量と戻る項目の文字画像幅
    const float valueWidth = textWidth * 192.0f / 512.0f; // 項目名と同じ倍率で描く音量数値領域
    const float rowY = origin.y + panelHeight * 0.40f; // 全体音量の項目名と数値の高さ
    const float backY = origin.y + panelHeight * 0.85f; // 戻る項目の高さ
    const float trackHeight = (std::max)(2.0f, panelWidth * 0.004f); // 解像度に追従する細い軌道の太さ
    const Vector4 accentColor = { 0.47f, 0.80f, 0.80f, 1.0f }; // 靄やステージ選択と合わせる落ち着いた青緑
    const Vector4 activeTextColor = { 0.90f, 0.93f, 0.94f, 1.0f }; // 点滅させず読みやすさを保つ選択文字色
    const Vector4 idleTextColor = { 0.60f, 0.65f, 0.68f, 1.0f }; // 選択されていない文字の表示色
    const Vector4 sliderColor = selectedSettingItem_ == 0 ? accentColor : Vector4 { 0.40f, 0.49f, 0.51f, 1.0f }; // 調整対象かどうかを示す軌道の色
    const Vector4 textColor = selectedSettingItem_ == 0 ? activeTextColor : idleTextColor; // 音量項目の選択状態を示す文字色
    panelDimmer_->SetColor({ 0.0f, 0.0f, 0.0f, 0.82f });
    panelBackground_->SetPosition({ origin.x, height * 0.5f });
    panelBackground_->SetSize({ panelWidth, panelHeight });
    panelBackground_->SetColor({ 0.055f, 0.065f, 0.07f, 0.0f });
    panelContent_->SetAnchorPoint({ 0.0f, 0.5f });
    panelContent_->SetPosition({ origin.x + inset, origin.y + panelHeight * 0.13f });
    panelContent_->SetSize({ innerWidth, innerWidth * 128.0f / 960.0f });
    panelContent_->SetColor(activeTextColor);
    panelBack_->SetPosition({ origin.x + inset, backY });
    panelBack_->SetSize({ textWidth, textWidth / 8.0f });
    panelBack_->SetColor(selectedSettingItem_ == 1 ? activeTextColor : idleTextColor);
    volumeLabel_->SetPosition({ origin.x + inset, rowY });
    volumeLabel_->SetSize({ textWidth, textWidth / 8.0f });
    volumeLabel_->SetColor(textColor);
    const int volumeStep = static_cast<int>(std::round(titleMasterVolume_ * 10.0f)); // 数値画像の切り出し行番号
    volumeValue_->SetTextureLeftTop({ 0.0f, static_cast<float>(volumeStep + 6) * 64.0f });
    volumeValue_->SetPosition({ origin.x + panelWidth - inset - valueWidth, rowY });
    volumeValue_->SetSize({ valueWidth, valueWidth / 3.0f });
    volumeValue_->SetColor(textColor);
    const Vector2 trackPosition = { origin.x + inset, origin.y + panelHeight * 0.56f }; // スライダー軌道の左端位置
    volumeTrack_->SetPosition(trackPosition);
    volumeTrack_->SetSize({ innerWidth, trackHeight });
    volumeTrack_->SetColor({ 0.18f, 0.22f, 0.24f, 1.0f });
    volumeFill_->SetPosition(trackPosition);
    volumeFill_->SetSize({ innerWidth * titleMasterVolume_, trackHeight });
    volumeFill_->SetColor(sliderColor);
    const Vector2 thumbPosition = { trackPosition.x + innerWidth * titleMasterVolume_, trackPosition.y }; // 実際の音量位置に対応するつまみの中心
    volumeThumb_->SetPosition(thumbPosition);
    volumeThumb_->SetSize({ trackHeight * 1.5f, trackHeight * 6.0f });
    volumeThumb_->SetColor(selectedSettingItem_ == 0 ? activeTextColor : idleTextColor);
    for (size_t index = 0; index < settingsPanelRules_.size(); ++index) {
        settingsPanelRules_[index]->SetPosition({ origin.x + inset, origin.y + panelHeight * (index == 0 ? 0.26f : 0.73f) });
        settingsPanelRules_[index]->SetSize({ innerWidth, 1.0f });
        settingsPanelRules_[index]->SetColor({ 0.18f, 0.23f, 0.25f, 1.0f });
    }
    settingsSelection_->SetPosition({ origin.x + inset - panelWidth * 0.025f, selectedSettingItem_ == 0 ? rowY : backY });
    settingsSelection_->SetSize({ trackHeight, textWidth * 0.06f });
    settingsSelection_->SetColor(accentColor);
}

/// <summary>
/// 現在開いているメニューだけを、デモより手前へ描画する。
/// </summary>
void TitleScene::DrawTitleMenu()
{
    if (!menuSelection_) {
        return;
    }
    if (menuView_ == MenuView::Main) {
        menuSelection_->Draw();
        for (Sprite* label : menuLabels_) {
            label->Draw();
        }
        return;
    }
    panelDimmer_->Draw();
    panelBackground_->Draw();
    if (menuView_ == MenuView::Settings) {
        for (Sprite* rule : settingsPanelRules_) {
            rule->Draw();
        }
        settingsSelection_->Draw();
    }
    panelContent_->Draw();
    panelBack_->Draw();
    if (menuView_ == MenuView::Settings) {
        volumeLabel_->Draw();
        volumeTrack_->Draw();
        if (titleMasterVolume_ > 0.0f) {
            volumeFill_->Draw();
        }
        volumeValue_->Draw();
        volumeThumb_->Draw();
    }
}

/// <summary>
/// Scene View用のオフスクリーン描画だけにするか設定する。
/// </summary>
void TitleScene::SetSceneViewOnly(bool enabled)
{
    sceneViewOnly_ = enabled;
}

/// <summary>
/// Scene Viewに表示するSRV番号を取得する。
/// </summary>
uint32_t TitleScene::GetSceneViewSrvIndex() const
{
    if (sceneRenderTarget_.HasColorSrv()) {
        return sceneRenderTarget_.GetColorSrvIndex();
    }
    return UINT32_MAX;
}

/// <summary>
/// ウィンドウリサイズ時にScene View用RenderTargetを追従させる。
/// </summary>
void TitleScene::OnWindowResize(uint32_t width, uint32_t height)
{
    if (sceneRenderTarget_.IsValid()) {
        sceneRenderTarget_.Resize(width, height);
    }
    if (demoRenderTarget_.IsValid()) {
        demoRenderTarget_.Resize(width, height);
    }
}

/// <summary>
/// 所有中のスプライトから参照用ビューを作り直す。
/// </summary>
void TitleScene::RebuildSpritePointerView()
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
/// 所有中の3Dオブジェクトから参照用ビューを作り直す。
/// </summary>
void TitleScene::RebuildObjectPointerView()
{
    objectPointerView_.clear();
    objectPointerView_.reserve(objects3d_.size());
    for (auto& object : objects3d_) {
        if (object) {
            objectPointerView_.push_back(object.get());
        }
    }
    for (auto& object : translucentObjects_) {
        objectPointerView_.push_back(object.get());
    }
}

/// <summary>
/// シーンが所有する3Dオブジェクトのポインタを収集する
/// </summary>
void TitleScene::FillObject3dPointers(std::vector<Object3d*>* out)
{
    if (!out) {
        return;
    }

    RebuildObjectPointerView();
    out->clear();
    out->reserve(objectPointerView_.size());
    out->insert(out->end(), objectPointerView_.begin(), objectPointerView_.end());
}

/// <summary>
/// シーンが所有するスプライトのポインタを収集する
/// </summary>
void TitleScene::FillSpritePointers(std::vector<Sprite*>* out)
{
    if (!out) {
        return;
    }

    RebuildSpritePointerView();
    out->clear();
    out->reserve(spritePointerView_.size());
    out->insert(out->end(), spritePointerView_.begin(), spritePointerView_.end());
}
