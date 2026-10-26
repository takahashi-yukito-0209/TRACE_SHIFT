#include "TitleScene.h"
#include "SceneFactory.h"
#include "TraceShiftStageJson.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/2d/TextureManager.h"
#include "../../engine/3d/Camera.h"
#include "../../engine/3d/Object3d.h"
#include "../../engine/3d/Object3dCommon.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/io/InputManager.h"
#include "../../engine/level/LevelWriter.h"
#include "../../engine/utility/JsonFileLoader.h"
#include "../../engine/utility/Logger.h"
#include "../../engine/utility/mathUtility.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr Vector4 kStageSelectAccent { 0.47f, 0.80f, 0.80f, 1.0f }; // 設定と操作説明に合わせた落ち着いた青緑
constexpr Vector4 kStageSelectActiveText { 0.90f, 0.93f, 0.94f, 1.0f }; // 見出しと選択項目の読みやすい白色
constexpr Vector4 kStageSelectIdleText { 0.60f, 0.65f, 0.68f, 1.0f }; // 非選択項目と補助情報の共通色
constexpr Vector4 kStageSelectBackground { 0.025f, 0.03f, 0.035f, 1.0f }; // 背景とプレビューの境界で共用するチャコール色
constexpr float kStageAtlasRowHeight = 64.0f; // 文字アトラスの1行の高さ
constexpr float kStageCursorMoveDuration = 0.15f; // 選択ラインが次の行へ移動する時間
constexpr float kStageFogSelectionDuration = 0.55f; // もやが集まって元の漂いへ戻る時間
constexpr float kStageLockedConfirmDuration = 0.45f; // 未公開ステージの決定に反応する時間
constexpr float kStageFeedbackPi = 3.14159265359f; // 一度だけ滑らかに強まって戻る演出の半周期

/// <summary>
/// ステージ一覧または戻る項目の高さ率を取得する。
/// </summary>
float GetStageSelectItemY(size_t item)
{
    return item < GameApp::kSelectableStageCount ? 0.34f + static_cast<float>(item) * 0.13f : 0.84f;
}

/// <summary>
/// 短い演出の開始と終了をゼロにする一山の反応強度を取得する。
/// </summary>
float GetStageFeedbackPulse(float time, float duration)
{
    const float rate = (std::clamp)(time / duration, 0.0f, 1.0f); // 演出内の進行率
    return (std::max)(std::sin(rate * kStageFeedbackPi), 0.0f);
}

/// <summary>
/// 文字領域から独立した右上のプレビュー描画範囲を取得する。
/// </summary>
D3D12_VIEWPORT GetStagePreviewViewport(float width, float height)
{
    return { width * 0.45f, height * 0.22f, width * 0.50f, height * 0.41f, 0.0f, 1.0f };
}

/// <summary>
/// 既存のルート別描画に合わせてプレビュー対象のギミックを判定する。
/// </summary>
bool IsPreviewGimmickEnabled(size_t stageIndex, const std::string& id)
{
    if (stageIndex == 0) {
        return false;
    }
    if (stageIndex == 1) {
        return id == "clone_switch" || id == "linked_door" || id == "timed_switch" || id == "timed_door";
    }
    return true;
}

/// <summary>
/// ギミックの役割をプレイ中と同系統の色で表示する。
/// </summary>
Vector4 GetPreviewGimmickColor(const std::string& id)
{
    if (id.find("toggle") != std::string::npos) {
        return { 1.0f, 0.42f, 0.05f, 1.0f };
    }
    if (id.find("timed") != std::string::npos) {
        return { 0.0f, 0.45f, 1.0f, 1.0f };
    }
    if (id == "clone_switch" || id == "linked_door") {
        return { 0.0f, 0.7f, 0.25f, 1.0f };
    }
    return { 0.85f, 0.72f, 0.25f, 1.0f };
}
} // namespace

/// <summary>
/// ステージ選択用の文字と実配置プレビューを作成する。
/// </summary>
void TitleScene::InitializeStageSelect()
{
    TextureManager* textures = ctx_.textureManager ? ctx_.textureManager : TextureManager::GetInstance(); // 選択画面の文字画像を登録する管理
    textures->LoadTexture("stage_select.png");
    textures->LoadTexture("stage_locked.png");
    textures->LoadTexture("stage_detail_locked.png");
    textures->LoadTexture("stage_preview_edges.png");
    for (const char* name : GameApp::kStageDetailTextures) { // 各ステージの名前と目的の画像
        textures->LoadTexture(name);
    }
    /// <summary>単色背景または文字行を作成し、選択画面の描画順へ登録する。</summary>
    const auto createSprite = [this](const char* texture, int row) {
        auto sprite = std::make_unique<Sprite>(); // 選択画面が所有する表示部品
        if (texture) {
            sprite->Initialize(ctx_.spriteCommon, texture, ctx_.imguiManager);
            if (row >= 0) {
                sprite->SetTextureLeftTop({ 0.0f, static_cast<float>(row) * kStageAtlasRowHeight });
                sprite->SetTextureSize({ 768.0f, kStageAtlasRowHeight });
            }
        } else {
            sprite->InitializeSolidColor(ctx_.spriteCommon, ctx_.srvManager);
        }
        sprite->SetAnchorPoint({ 0.0f, 0.5f });
        Sprite* result = sprite.get(); // 所有一覧へ移動後の参照
        sprites_.push_back(std::move(sprite));
        stageSelectSprites_.push_back(result);
        return result;
    };
    createSprite(nullptr, -1)->SetColor(kStageSelectBackground);
    createSprite("stage_select.png", 0);
    for (size_t index = 0; index < stageLabels_.size(); ++index) { // 左側のステージ一覧
        stageLabels_[index] = createSprite("stage_select.png", static_cast<int>(index + 1));
        stageStatuses_[index] = createSprite("stage_select.png", 5);
    }
    stageBack_ = createSprite("stage_select.png", 4);
    stageDetail_ = createSprite(GameApp::kStageDetailTextures[0], -1);
    stageCursor_ = createSprite(nullptr, -1);
    stageCursor_->SetColor(kStageSelectAccent);
    stagePreviewError_ = createSprite("stage_select.png", 7);
    stagePreviewEdges_ = createSprite("stage_preview_edges.png", -1);
    stagePreviewEdges_->SetColor(kStageSelectBackground);
    for (Sprite*& rule : stageSelectRules_) { // 背景へ控えめに引く区切り線
        rule = createSprite(nullptr, -1);
        rule->SetColor({ 0.18f, 0.23f, 0.25f, 1.0f });
    }
    if (ctx_.directXCommon) {
        RenderTargetDesc desc {}; // 細部を読ませない低解像度のプレビュー設定
        desc.width = 64;
        desc.height = 48;
        desc.format = ctx_.directXCommon->GetSwapChainFormat();
        desc.clearColor = { kStageSelectBackground.x, kStageSelectBackground.y, kStageSelectBackground.z, 1.0f };
        demoRenderTarget_.Initialize(ctx_.directXCommon, desc);
        desc.useDepth = false;
        stageBlurIntermediateTarget_.Initialize(ctx_.directXCommon, desc);
        demoPostProcess_.Initialize(ctx_.directXCommon);
        demoPostProcess_.SetGaussianKernelSize(7u);
        demoPostProcess_.SetGaussianSigma(4.0f);
        InitializeStageFog();
    }
    InitializeStagePreviews();
}

/// <summary>
/// 実ステージJSONからルート別のプレビューを構築する。
/// </summary>
void TitleScene::InitializeStagePreviews()
{
    if (!ctx_.object3dCommon) {
        return;
    }
    JsonDocument root; // プレビューと実プレイで共用するステージJSON
    TraceShiftStageJson::StageData data; // 検証済みのステージ配置
    std::string error; // 読み込みまたは検証の失敗理由
    const std::string path = LevelWriter::ResolveWritableLevelPath("levels/trace_shift_stage.json"); // プレイ側と同じ配置ファイルの解決先
    if (!JsonFileLoader::Load(path, root, nullptr, ResourceResolver::Type::Json, &error) ||
        !TraceShiftStageJson::Decode(root, data, error)) {
        Logger::Warn("StageSelect preview: " + error + "\n");
        return;
    }
    for (size_t index = 0; index < stagePreviews_.size(); ++index) { // ルートごとに生成するプレビュー
        /// <summary>実配置のブロックをプレビュー用の所有一覧へ追加する。</summary>
        const auto append = [this, index](const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Vector4& color) {
            auto object = std::make_unique<Object3d>(); // 実配置を縮尺変更せず表示するオブジェクト
            object->Initialize(ctx_.object3dCommon, ctx_.imguiManager);
            object->SetModel("block/block.obj");
            object->SetScale(scale);
            object->SetRotate(rotate);
            object->SetTranslate(translate);
            object->SetMaterialColor(color);
            object->SetUseTexture(false);
            object->SetEnableLighting(false);
            stagePreviews_[index].push_back(std::move(object));
        };
        for (const auto& block : data.blocks) { // 選択ルートに属する固定地形
            if ((block.routeMask & (1u << index)) != 0) {
                append(block.scale, block.rotate, block.translate, block.color);
            }
        }
        for (const auto& gimmick : data.gimmicks) { // 選択ルートが使用するギミックの初期配置
            if (IsPreviewGimmickEnabled(index, gimmick.id)) {
                append(gimmick.scale, gimmick.rotate, gimmick.translate, GetPreviewGimmickColor(gimmick.id));
            }
        }
    }
}

/// <summary>
/// 選択移動、プレイ開始、タイトルへ戻る入力を処理する。
/// </summary>
void TitleScene::UpdateStageSelectInput()
{
    InputManager* input = InputManager::GetInstance(); // 選択画面の入力状態
    if (!input || !ctx_.requestSceneChange) {
        return;
    }
    const bool up = input->IsKeyJustPressed(DIK_UP) || input->IsKeyJustPressed(DIK_W) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_DPAD_UP); // 上の項目へ移る入力
    const bool down = input->IsKeyJustPressed(DIK_DOWN) || input->IsKeyJustPressed(DIK_S) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_DPAD_DOWN); // 下の項目へ移る入力
    const bool confirm = input->IsKeyJustPressed(DIK_RETURN) || input->IsKeyJustPressed(DIK_SPACE) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_A); // 選択項目を実行する入力
    const bool back = input->IsKeyJustPressed(DIK_ESCAPE) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_B); // タイトルへ戻る入力
    if (back) {
        ctx_.requestSceneChange("Title");
        return;
    }
    if (up != down) {
        stageCursorStartY_ = stageCursorY_;
        stageCursorMoveTime_ = 0.0f;
        stageLockedConfirmTime_ = kStageLockedConfirmDuration;
        selectedStageItem_ = (selectedStageItem_ + (up ? GameApp::kSelectableStageCount : 1)) % (GameApp::kSelectableStageCount + 1);
        if (selectedStageItem_ < GameApp::kSelectableStageCount) {
            stageSelection_->selectedStageIndex = selectedStageItem_;
            stageFogSelectionTime_ = 0.0f;
        } else {
            stageFogSelectionTime_ = kStageFogSelectionDuration;
        }
    }
    if (confirm) {
        const std::string nextScene = selectedStageItem_ == GameApp::kSelectableStageCount ? "Title" : "Play"; // 決定した項目の遷移先
        if (GameApp::SceneFactory::IsSceneAvailable(nextScene) &&
            (nextScene != "Play" || stageSelection_->IsStageUnlocked(selectedStageItem_))) {
            ctx_.requestSceneChange(nextScene);
        } else if (stageLockedConfirmTime_ >= kStageLockedConfirmDuration) {
            // 連打中も現在の反応を最後まで見せ、演出を重ねない。
            stageLockedConfirmTime_ = 0.0f;
        }
    }
}

/// <summary>
/// 選択ラインの補間と選択・決定時の演出時間を更新する。
/// </summary>
void TitleScene::UpdateStageSelectFeedback(float dt)
{
    const float targetY = GetStageSelectItemY(selectedStageItem_); // 現在の選択行の高さ率
    const float deltaTime = (std::max)(dt, 0.0f); // 逆行しない演出更新時間
    if (deltaTime == 0.0f) {
        // 初期表示と切替準備では選択中の行へ直接配置する。
        stageCursorY_ = targetY;
        stageCursorStartY_ = targetY;
        stageCursorMoveTime_ = kStageCursorMoveDuration;
        return;
    }
    stageCursorMoveTime_ = (std::min)(stageCursorMoveTime_ + deltaTime, kStageCursorMoveDuration);
    const float rate = stageCursorMoveTime_ / kStageCursorMoveDuration; // 移動区間内の進行率
    const float easedRate = rate * rate * (3.0f - 2.0f * rate); // 始点と終点を緩やかにつなぐ移動率
    stageCursorY_ = stageCursorStartY_ + (targetY - stageCursorStartY_) * easedRate;
    stageFogSelectionTime_ = (std::min)(stageFogSelectionTime_ + deltaTime, kStageFogSelectionDuration);
    stageLockedConfirmTime_ = (std::min)(stageLockedConfirmTime_ + deltaTime, kStageLockedConfirmDuration);
}

/// <summary>
/// 選択文字とプレビューのカメラ・描画行列を更新する。
/// </summary>
void TitleScene::UpdateStageSelect(float dt)
{
    if (!ctx_.directXCommon || stageSelectSprites_.empty()) {
        return;
    }
    if (dt > 0.0f && (!ctx_.isSceneTransitionActive || !ctx_.isSceneTransitionActive())) {
        UpdateStageSelectInput();
    }
    UpdateStageSelectFeedback(dt);
    const float width = ctx_.directXCommon->GetRenderWidth(); // 選択画面の描画幅
    const float height = ctx_.directXCommon->GetRenderHeight(); // 選択画面の描画高さ
    const float textWidth = (std::min)(width * 0.32f, height * 0.74f); // 左列の文字を読みやすく保つ画像幅
    const float headingWidth = (std::min)(width * 0.48f, height * 0.95f); // 一覧より一段大きく表示する見出し画像幅
    const float contentLeft = width * 0.08f; // 見出し・一覧・戻る項目に共通の左端
    const size_t selected = (std::min)(stageSelection_->selectedStageIndex, GameApp::kSelectableStageCount - 1); // 戻る選択時にも維持するプレビュー番号
    const bool locked = !GameApp::SceneFactory::IsSceneAvailable("Play") || !stageSelection_->IsStageUnlocked(selected); // 選択ステージの名前と攻略内容を伏せるか
    const float confirmPulse = locked ? GetStageFeedbackPulse(stageLockedConfirmTime_, kStageLockedConfirmDuration) : 0.0f; // 決定時だけ文字とラインを強調する反応率
    stageSelectSprites_[0]->SetPosition({ 0.0f, height * 0.5f });
    stageSelectSprites_[0]->SetSize({ width, height });
    stageSelectSprites_[1]->SetPosition({ contentLeft, height * 0.15f });
    stageSelectSprites_[1]->SetSize({ headingWidth, headingWidth / 12.0f });
    stageSelectSprites_[1]->SetColor(kStageSelectActiveText);
    for (size_t index = 0; index < stageLabels_.size(); ++index) { // 各ステージ行の名前とクリア状態
        const float rowY = height * GetStageSelectItemY(index); // 選択ラインと共有する各行の中心位置
        const bool rowLocked = !GameApp::SceneFactory::IsSceneAvailable("Play") || !stageSelection_->IsStageUnlocked(index); // 各行自身の入場可否
        stageLabels_[index]->SetTexture(rowLocked ? "stage_locked.png" : "stage_select.png");
        stageLabels_[index]->SetTextureLeftTop({ 0.0f, static_cast<float>(index + (rowLocked ? 0 : 1)) * kStageAtlasRowHeight });
        stageLabels_[index]->SetTextureSize({ 768.0f, kStageAtlasRowHeight });
        stageLabels_[index]->SetPosition({ contentLeft, rowY });
        stageLabels_[index]->SetSize({ textWidth, textWidth / 12.0f });
        stageLabels_[index]->SetColor(selectedStageItem_ == index ? kStageSelectActiveText : kStageSelectIdleText);
        const float statusRow = rowLocked ? 8.0f
            : (stageSelection_->cleared[index] ? 6.0f : 5.0f); // プレビュー専用または攻略状態の表示行
        stageStatuses_[index]->SetTextureLeftTop({ 0.0f, statusRow * kStageAtlasRowHeight });
        stageStatuses_[index]->SetPosition({ contentLeft, rowY + height * 0.045f });
        stageStatuses_[index]->SetSize({ textWidth * 0.65f, textWidth * 0.65f / 12.0f });
        stageStatuses_[index]->SetColor(stageSelection_->cleared[index] ? kStageSelectAccent : kStageSelectIdleText);
    }
    stageBack_->SetPosition({ contentLeft, height * GetStageSelectItemY(GameApp::kSelectableStageCount) });
    stageBack_->SetSize({ textWidth * 0.9f, textWidth * 0.9f / 12.0f });
    stageBack_->SetColor(selectedStageItem_ == GameApp::kSelectableStageCount ? kStageSelectActiveText : kStageSelectIdleText);
    stageCursor_->SetPosition({ width * 0.06f, height * stageCursorY_ });
    stageCursor_->SetSize({ 3.0f + confirmPulse, height * 0.035f });
    stageCursor_->SetColor({ kStageSelectAccent.x + confirmPulse * 0.35f,
        kStageSelectAccent.y + confirmPulse * 0.06f, kStageSelectAccent.z + confirmPulse * 0.20f, 1.0f });
    stageDetail_->SetTexture(locked ? "stage_detail_locked.png" : GameApp::kStageDetailTextures[selected]);
    stageDetail_->SetAnchorPoint({ locked ? 0.5f : 0.0f, 0.5f });
    stageDetail_->SetPosition({ width * (locked ? 0.70f : 0.46f), height * (locked ? 0.70f : 0.81f) });
    const float detailWidth = (std::min)(width * (locked ? 0.38f : 0.47f), height * (locked ? 0.70f : 0.84f)); // 未公開の文字も読める大きさで専用領域へ収める幅
    stageDetail_->SetSize({ detailWidth, detailWidth / 3.0f });
    stageDetail_->SetColor(locked ? Vector4 { kStageSelectIdleText.x + confirmPulse * 0.30f,
        kStageSelectIdleText.y + confirmPulse * 0.28f, kStageSelectIdleText.z + confirmPulse * 0.26f, 1.0f }
        : kStageSelectActiveText);
    const D3D12_VIEWPORT preview = GetStagePreviewViewport(width, height); // 中央表示と境界マスクで共有するプレビュー範囲
    stagePreviewEdges_->SetPosition({ preview.TopLeftX, preview.TopLeftY + preview.Height * 0.5f });
    stagePreviewEdges_->SetSize({ preview.Width, preview.Height });
    stageSelectRules_[0]->SetPosition({ contentLeft, height * 0.23f });
    stageSelectRules_[0]->SetSize({ width * 0.84f, 1.0f });
    stageSelectRules_[1]->SetPosition({ width * 0.42f, height * 0.50f });
    stageSelectRules_[1]->SetSize({ 1.0f, height * 0.46f });
    stagePreviewError_->SetPosition({ width * 0.46f, height * 0.43f });
    stagePreviewError_->SetSize({ width * 0.46f, width * 0.46f / 12.0f });
    stagePreviewError_->SetColor(kStageSelectIdleText);
    UpdateStagePreviewCamera(selected);
    UpdateStageFog(dt);
    for (Sprite* sprite : stageSelectSprites_) { // 配置変更後のスプライトの描画行列
        sprite->Update();
    }
}

/// <summary>
/// 選択ルート全体を右側に収めるカメラと描画行列を設定する。
/// </summary>
void TitleScene::UpdateStagePreviewCamera(size_t stageIndex)
{
    const float width = ctx_.directXCommon->GetRenderWidth(); // プレビュー配置の描画幅
    const float height = ctx_.directXCommon->GetRenderHeight(); // プレビュー配置の描画高さ
    if (ctx_.camera && !stagePreviews_[stageIndex].empty()) {
        float minX = (std::numeric_limits<float>::max)(); // プレビュー全体の左端
        float maxX = (std::numeric_limits<float>::lowest)(); // プレビュー全体の右端
        float minY = minX; // プレビュー全体の下端
        float maxY = maxX; // プレビュー全体の上端
        float minZ = minX; // プレビュー全体の手前端
        float maxZ = maxX; // プレビュー全体の奥端
        for (const auto& object : stagePreviews_[stageIndex]) { // 全体が収まる表示範囲の計算対象
            const Matrix4x4 world = MathUtil::MakeAffineMatrix(object->GetScale(), object->GetRotate(), object->GetTranslate()); // 回転も反映するブロックの配置行列
            for (int corner = 0; corner < 8; ++corner) { // ブロックの8頂点から求める表示範囲
                const Vector3 position = MathUtil::Transform({ corner & 1 ? 0.5f : -0.5f,
                    corner & 2 ? 0.5f : -0.5f, corner & 4 ? 0.5f : -0.5f }, world); // ワールド座標の頂点
                minX = (std::min)(minX, position.x);
                maxX = (std::max)(maxX, position.x);
                minY = (std::min)(minY, position.y);
                maxY = (std::max)(maxY, position.y);
                minZ = (std::min)(minZ, position.z);
                maxZ = (std::max)(maxZ, position.z);
            }
        }
        const D3D12_VIEWPORT viewport = GetStagePreviewViewport(width, height); // カメラ計算と描画で共通の表示領域
        const float aspect = viewport.Width / (std::max)(viewport.Height, 1.0f); // プレビュー専用の描画縦横比
        const float tangent = std::tan(0.31f); // 視野角の半角から計算する表示倍率
        const float distance = (std::max)({ 20.0f, (maxX - minX + 3.0f) / (1.7f * tangent * aspect),
            (maxY - minY + 0.3f * (maxZ - minZ) + 3.0f) / (1.7f * tangent) }) + (maxZ - minZ) * 0.5f; // 奥行きと余白を含めて全体を収める距離
        ctx_.camera->SetFovY(0.62f);
        ctx_.camera->SetRotate({ -0.14f, 0.0f, 0.0f });
        const Vector3 focus = { (minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f }; // プレビュー範囲の中心
        const Vector3 rotatedFocus = MathUtil::Transform(focus, MathUtil::MakeRotateXMatrix(-0.14f)); // 既存カメラの回転後に平行移動するビュー計算に合わせた注視点
        ctx_.camera->SetTranslate({ rotatedFocus.x, rotatedFocus.y, rotatedFocus.z - distance });
        ctx_.camera->Update();
        const Matrix4x4 projection = MathUtil::MakePerspectiveFovMatrix(0.62f, aspect, 0.1f, 1000.0f); // 共通カメラの縦横比を変更しないプレビュー用射影
        for (auto& object : stagePreviews_[stageIndex]) { // 選択中プレビューの描画行列
            object->Update(ctx_.camera->GetViewMatrix(), projection);
        }
    }
}

/// <summary>
/// 入場不可のステージを低解像度で描画し、縦横のぼかしを適用する。
/// </summary>
bool TitleScene::DrawLockedStagePreview(size_t stageIndex)
{
    if (!ctx_.object3dCommon || !demoRenderTarget_.IsValid() || !demoRenderTarget_.HasColorSrv() ||
        !stageBlurIntermediateTarget_.IsValid() || !stageBlurIntermediateTarget_.HasColorSrv() ||
        !demoPostProcess_.IsReady() || !demoPostProcess_.IsGaussianFilterReady() || !demoPostProcess_.IsCopyReady()) {
        return false;
    }
    demoRenderTarget_.Begin(true);
    ctx_.object3dCommon->SetCommonDrawSetting();
    for (auto& object : stagePreviews_[stageIndex]) { // 鮮明な画面へ直接描かない入場不可ステージ
        object->Draw();
    }
    demoRenderTarget_.End();
    stageBlurIntermediateTarget_.Begin(true);
    demoPostProcess_.DrawGaussianPass(demoRenderTarget_.GetColorSrvIndex(), 0u);
    stageBlurIntermediateTarget_.End();
    demoRenderTarget_.Begin(true);
    demoPostProcess_.DrawGaussianPass(stageBlurIntermediateTarget_.GetColorSrvIndex(), 1u);
    demoRenderTarget_.End();
    return true;
}

/// <summary>
/// 入場不可プレビューを覆う半透明のもや粒子を初期化する。
/// </summary>
void TitleScene::InitializeStageFog()
{
    TextureManager* textures = ctx_.textureManager ? ctx_.textureManager : TextureManager::GetInstance(); // もや画像の登録先
    textures->LoadTexture("stage_fog.png");
    stageFogTime_ = 0.0f;
    for (auto& fog : stageFogSprites_) { // 最初から密度を保つ固定数のもや粒子
        fog = std::make_unique<Sprite>();
        fog->Initialize(ctx_.spriteCommon, "stage_fog.png", ctx_.imguiManager);
        fog->SetAnchorPoint({ 0.5f, 0.5f });
    }
}

/// <summary>
/// プレビュー内で漂うもやの位置・大きさ・透明度を更新する。
/// </summary>
void TitleScene::UpdateStageFog(float dt)
{
    if (!ctx_.directXCommon || (GameApp::SceneFactory::IsSceneAvailable("Play") &&
        stageSelection_->IsStageUnlocked(stageSelection_->selectedStageIndex))) {
        return;
    }
    stageFogTime_ += (std::max)(dt, 0.0f);
    const float selectionPulse = GetStageFeedbackPulse(stageFogSelectionTime_, kStageFogSelectionDuration); // 選択時にもやを中央へ集める反応率
    const float confirmPulse = GetStageFeedbackPulse(stageLockedConfirmTime_, kStageLockedConfirmDuration); // 決定時の小さな波の強度
    const float confirmRate = (std::clamp)(stageLockedConfirmTime_ / kStageLockedConfirmDuration, 0.0f, 1.0f); // 決定時の波が進む時間率
    const D3D12_VIEWPORT viewport = GetStagePreviewViewport(ctx_.directXCommon->GetRenderWidth(), ctx_.directXCommon->GetRenderHeight()); // 文字領域と分離したもやの配置範囲
    for (size_t index = 0; index < stageFogSprites_.size(); ++index) { // わずかに異なる速さで漂う各粒子
        Sprite* fog = stageFogSprites_[index].get(); // 今回更新するもや
        if (!fog) {
            continue;
        }
        const float seed = static_cast<float>(index); // 粒子ごとの配置と揺らぎを決める番号
        const float phase = seed * 2.399963f; // 周期が重ならない初期位相
        const float motion = stageFogTime_ * (0.13f + static_cast<float>(index % 4) * 0.017f); // 粒子ごとのゆっくりした移動時刻
        const float x = 0.12f + std::fmod(seed * 0.618034f, 1.0f) * 0.76f + std::sin(motion + phase) * 0.12f; // 横方向の配置と漂い
        const float y = 0.18f + std::fmod(seed * 0.381966f, 1.0f) * 0.64f + std::cos(motion * 0.7f + phase) * 0.055f; // 縦方向の配置と漂い
        const float breath = 0.5f + 0.5f * std::sin(motion * 1.3f + phase); // 大きさと透明度を滑らかに変える呼吸率
        const float gather = 1.0f - selectionPulse * 0.18f; // 選択した時に広がりを少し狭める倍率
        const float ripple = std::sin(phase - confirmRate * 6.28318531f) * confirmPulse * 0.035f; // もや内部を小さく伝わる決定時の波
        const float reactionX = 0.5f + (x - 0.5f) * gather + ripple; // 収束と波を反映した横位置
        const float reactionY = 0.5f + (y - 0.5f) * gather + ripple * 0.45f; // 横移動より控えめに揺らす縦位置
        const float reactionScale = 1.0f - selectionPulse * 0.08f + confirmPulse * 0.06f; // 収束時は少し縮み、決定時はわずかに膨らむ倍率
        const Vector4 tint = index % 3 == 0 ? Vector4 { 0.42f, 0.36f, 0.52f, 0.06f + breath * 0.14f }
            : Vector4 { 0.28f, 0.48f, 0.50f, 0.07f + breath * 0.15f }; // 水色を主体に少量の紫を混ぜる落ち着いた霧色
        fog->SetPosition({ viewport.TopLeftX + viewport.Width * reactionX, viewport.TopLeftY + viewport.Height * reactionY });
        fog->SetSize({ viewport.Width * (0.70f + breath * 0.28f) * reactionScale,
            viewport.Height * (0.95f + breath * 0.30f) * reactionScale });
        fog->SetRotation(std::sin(motion * 0.6f + phase) * 0.12f);
        fog->SetIsFlipX(index % 2 != 0);
        fog->SetIsFlipY(index % 3 == 0);
        fog->SetColor(tint);
        fog->Update();
    }
}

/// <summary>
/// 現在の切り取り範囲内へ半透明のもや粒子を重ねる。
/// </summary>
void TitleScene::DrawStageFog()
{
    const BlendMode previousBlend = ctx_.spriteCommon->GetBlendMode(); // もやを描く前のスプライト合成設定
    ctx_.spriteCommon->SetBlendMode(BlendMode::Alpha);
    ctx_.spriteCommon->SetCommonDrawSetting();
    for (auto& fog : stageFogSprites_) { // ぼかし結果の手前へ重ねるもや
        if (fog) {
            fog->Draw();
        }
    }
    ctx_.spriteCommon->SetBlendMode(previousBlend);
}

/// <summary>
/// 現在の選択ステージと開始項目を共通遷移付きで描画する。
/// </summary>
void TitleScene::DrawStageSelect()
{
    if (!ctx_.spriteCommon || stageSelectSprites_.empty()) {
        return;
    }
    const size_t selected = (std::min)(stageSelection_->selectedStageIndex, GameApp::kSelectableStageCount - 1); // 現在プレビューするルート
    const bool locked = !GameApp::SceneFactory::IsSceneAvailable("Play") || !stageSelection_->IsStageUnlocked(selected); // 未解放の選択プレビューをぼかすか
    const bool blurredPreviewReady = locked && DrawLockedStagePreview(selected); // 出力先を切り替える前にぼかし結果を準備する
    const bool useSceneView = sceneViewOnly_ && sceneRenderTarget_.IsValid(); // Scene Viewへ出力するか
    if (useSceneView) {
        sceneRenderTarget_.Begin(true);
    }
    if (ctx_.beginSceneTransition) {
        ctx_.beginSceneTransition(useSceneView ? sceneRenderTarget_.GetHandle() : -1);
    }
    ctx_.spriteCommon->SetCommonDrawSetting();
    stageSelectSprites_[0]->Draw();
    if (ctx_.object3dCommon) {
        ID3D12GraphicsCommandList* commandList = ctx_.directXCommon->GetCommandList(); // 描画範囲を切り替えるコマンドリスト
        const D3D12_VIEWPORT viewport = GetStagePreviewViewport(ctx_.directXCommon->GetRenderWidth(), ctx_.directXCommon->GetRenderHeight()); // 文字と重ならないプレビューの描画領域
        const D3D12_RECT clip { static_cast<LONG>(viewport.TopLeftX), static_cast<LONG>(viewport.TopLeftY),
            static_cast<LONG>(viewport.TopLeftX + viewport.Width), static_cast<LONG>(viewport.TopLeftY + viewport.Height) }; // 領域外への描画を防ぐ切り取り範囲
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &clip);
        if (locked) {
            if (blurredPreviewReady) {
                demoPostProcess_.DrawTexture(demoRenderTarget_.GetColorSrvIndex(), PostEffectType::Copy);
            }
        } else {
            ctx_.object3dCommon->SetCommonDrawSetting();
            for (auto& object : stagePreviews_[selected]) { // 入場可能なルートだけ鮮明に表示するプレビュー
                object->Draw();
            }
        }
        const D3D12_VIEWPORT fullViewport { 0.0f, 0.0f, ctx_.directXCommon->GetRenderWidth(), ctx_.directXCommon->GetRenderHeight(), 0.0f, 1.0f }; // UIと遷移を描く画面全体の領域
        const D3D12_RECT fullClip { 0, 0, static_cast<LONG>(fullViewport.Width), static_cast<LONG>(fullViewport.Height) }; // UI用の切り取り範囲
        commandList->RSSetViewports(1, &fullViewport);
        if (locked) {
            // Spriteは画面座標で配置し、プレビューの切り取り範囲だけ維持する。
            DrawStageFog();
        }
        commandList->RSSetScissorRects(1, &fullClip);
    }
    ctx_.spriteCommon->SetCommonDrawSetting();
    for (size_t index = 1; index < stageSelectSprites_.size(); ++index) { // 背景以外の文字と選択表示
        Sprite* sprite = stageSelectSprites_[index]; // 表示条件を判定する選択画面の部品
        const auto status = std::find(stageStatuses_.begin(), stageStatuses_.end(), sprite); // 各ステージ行に付く攻略状態の表示
        if (status != stageStatuses_.end() && (!GameApp::SceneFactory::IsSceneAvailable("Play") ||
            !stageSelection_->IsStageUnlocked(static_cast<size_t>(status - stageStatuses_.begin())))) {
            continue;
        }
        if (!locked && sprite == stagePreviewEdges_) {
            continue;
        }
        if (stageSelectSprites_[index] != stagePreviewError_ || stagePreviews_[selected].empty()) {
            stageSelectSprites_[index]->Draw();
        }
    }
    if (ctx_.drawSceneTransition) {
        ctx_.drawSceneTransition();
    }
    if (useSceneView) {
        sceneRenderTarget_.End();
    }
}
