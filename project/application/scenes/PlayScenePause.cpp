#include "PlayScene.h"
#include "../../engine/2d/Sprite.h"
#include "../../engine/2d/SpriteCommon.h"
#include "../../engine/2d/TextureManager.h"
#include "../../engine/base/DirectXCommon.h"
#include "../../engine/io/InputManager.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

using namespace MyEngine;

/// <summary>
/// ポーズと操作説明に使う表示部品を初期化する。
/// </summary>
void PlayScene::InitializePauseMenu()
{
    pauseMenu_.Close();
    if (!ctx_.spriteCommon || !ctx_.textureManager) {
        return;
    }
    ctx_.textureManager->LoadTexture("pause_menu.png");
    ctx_.textureManager->LoadTexture("title_controls.png");
    const auto createSprite = [this](const char* texture) { // 表示用途に応じた画像または単色の部品を作る処理
        auto sprite = std::make_unique<Sprite>(); // 今回作成する表示部品
        if (texture) {
            sprite->Initialize(ctx_.spriteCommon, texture, ctx_.imguiManager);
        } else {
            sprite->InitializeSolidColor(ctx_.spriteCommon, ctx_.srvManager);
        }
        return sprite;
    };
    pauseDimmer_ = createSprite(nullptr);
    pausePanel_ = createSprite(nullptr);
    pauseCursor_ = createSprite(nullptr);
    pauseControls_ = createSprite("title_controls.png");
    pauseTexts_.clear();
    for (size_t textIndex = 0; textIndex < 9; ++textIndex) { // 同時表示する見出し・項目・補足の部品番号
        pauseTexts_.push_back(createSprite("pause_menu.png"));
    }
}

/// <summary>
/// ポーズの入力と確認済み操作を処理し、そのフレームをゲームへ渡さないか返す。
/// </summary>
bool PlayScene::HandlePauseMenuInput()
{
    InputManager* input = InputManager::GetInstance(); // ポーズとゲームが共用する入力管理
    if (!input || !pauseDimmer_) {
        return pauseMenu_.IsOpen();
    }
#ifdef USE_IMGUI
    if (ImGui::GetCurrentContext() && (ImGui::GetIO().WantCaptureKeyboard || ImGui::IsAnyItemActive())) {
        return pauseMenu_.IsOpen();
    }
#endif
    const bool toggle = input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_START); // どのページでもポーズを開閉する入力
    const bool back = input->IsKeyJustPressed(DIK_ESCAPE) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_B) ||
        input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_BACK); // サブページから戻る入力
    if (!pauseMenu_.IsOpen()) {
        if (toggle || input->IsKeyJustPressed(DIK_ESCAPE) || input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_BACK)) {
            pauseMenu_.Open();
            return true;
        }
        return false;
    }
    if (toggle) {
        pauseMenu_.Close();
        return true;
    }
    if (back) {
        pauseMenu_.Back();
        return true;
    }
    const bool up = input->IsKeyJustPressed(DIK_UP) || input->IsKeyJustPressed(DIK_W) ||
        input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_DPAD_UP); // 前の項目を選ぶ入力
    const bool down = input->IsKeyJustPressed(DIK_DOWN) || input->IsKeyJustPressed(DIK_S) ||
        input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_DPAD_DOWN); // 次の項目を選ぶ入力
    if (up != down) {
        pauseMenu_.Move(up ? -1 : 1);
    }
    const bool confirm = input->IsKeyJustPressed(DIK_RETURN) || input->IsKeyJustPressed(DIK_SPACE) ||
        input->IsGamePadButtonJustPressed(XINPUT_GAMEPAD_A); // 項目を決定する入力
    if (confirm) {
        const PauseMenuState::Action action = pauseMenu_.Confirm(); // 確認画面を経て許可された操作
        if (action == PauseMenuState::Action::Retry) {
            ResetPastSelfTutorialState();
        } else if (action == PauseMenuState::Action::Exit && ctx_.requestSceneChange) {
            ctx_.requestSceneChange("StageSelect");
        }
    }
    return true;
}

/// <summary>
/// ポーズの背景・項目・操作説明をゲーム画面へ重ねる。
/// </summary>
void PlayScene::DrawPauseMenu()
{
    if (!ctx_.directXCommon || !ctx_.spriteCommon || !pauseDimmer_) {
        return;
    }
    const float width = ctx_.directXCommon->GetRenderWidth(); // メニューを配置する画面幅
    const float height = ctx_.directXCommon->GetRenderHeight(); // メニューを配置する画面高さ
    if (!pauseMenu_.IsOpen()) {
        Sprite& hint = *pauseTexts_[8]; // ゲーム画面でポーズの開き方を示す小さな表示
        const float hintWidth = (std::min)(width * 0.50f, height * 0.75f); // ゲーム画面を妨げない説明画像幅
        ctx_.spriteCommon->SetCommonDrawSetting();
        hint.SetTextureLeftTop({ 0.0f, 14.0f * 64.0f });
        hint.SetTextureSize({ 1024.0f, 64.0f });
        hint.SetPosition({ width * 0.025f, height * 0.94f });
        hint.SetSize({ hintWidth, hintWidth / 16.0f });
        hint.SetColor({ 0.75f, 0.85f, 0.88f, 0.65f });
        hint.Update();
        hint.Draw();
        return;
    }
    const float panelWidth = (std::min)(width * 0.88f, height * 1.55f); // 文字を収めるパネル幅
    const float left = (width - panelWidth) * 0.5f; // 中央パネルの左端
    const float textWidth = panelWidth * 0.90f; // 文字画像の縦横比を保つ表示幅
    const float textLeft = left + panelWidth * 0.06f; // 項目と見出しに共通の左端
    const bool controls = pauseMenu_.GetView() == PauseMenuState::View::Controls; // 操作説明のページか
    ctx_.spriteCommon->SetCommonDrawSetting();
    pauseDimmer_->SetPosition({ 0.0f, 0.0f });
    pauseDimmer_->SetSize({ width, height });
    pauseDimmer_->SetColor({ 0.005f, 0.015f, 0.025f, 0.72f });
    pauseDimmer_->Update();
    pauseDimmer_->Draw();
    pausePanel_->SetPosition({ left, height * 0.08f });
    pausePanel_->SetSize({ panelWidth, height * 0.84f });
    pausePanel_->SetColor({ 0.025f, 0.045f, 0.06f, 0.96f });
    pausePanel_->Update();
    pausePanel_->Draw();
    const auto drawText = [this, textWidth, textLeft](size_t index, size_t row, float y, bool selected) { // 各部品へ文字行と選択色を設定する処理
        Sprite& text = *pauseTexts_[index]; // 独立した描画定数を持つ今回の文字
        text.SetTextureLeftTop({ 0.0f, static_cast<float>(row) * 64.0f });
        text.SetTextureSize({ 1024.0f, 64.0f });
        text.SetPosition({ textLeft, y });
        text.SetSize({ textWidth, textWidth / 16.0f });
        text.SetColor(selected ? Math::Vector4 { 0.48f, 0.90f, 0.92f, 1.0f } : Math::Vector4 { 0.87f, 0.92f, 0.94f, 1.0f });
        text.Update();
        text.Draw();
    };
    if (controls) {
        const float controlsHeight = (std::min)(height * 0.65f, panelWidth * 0.90f * 0.75f); // 横長と縦長の画面でパネル内へ収める高さ
        pauseControls_->SetPosition({ width * 0.5f - controlsHeight * (4.0f / 3.0f) * 0.5f, height * 0.11f });
        pauseControls_->SetSize({ controlsHeight * (4.0f / 3.0f), controlsHeight });
        pauseControls_->Update();
        pauseControls_->Draw();
        drawText(7, 13, height * 0.81f, false);
        return;
    }
    drawText(0, 0, height * 0.15f, true);
    const bool menu = pauseMenu_.GetView() == PauseMenuState::View::Menu; // 通常項目を表示するページか
    const float firstY = height * (menu ? 0.31f : 0.52f); // 項目一覧の先頭位置
    const size_t count = menu ? PauseMenuState::kMenuItemCount : 2; // 今回表示する項目数
    const float rowStep = height * 0.10f; // 項目間の縦方向の余白
    pauseCursor_->SetPosition({ textLeft - panelWidth * 0.025f, firstY + static_cast<float>(pauseMenu_.GetSelection()) * rowStep });
    pauseCursor_->SetSize({ 3.0f, textWidth / 16.0f });
    pauseCursor_->SetColor({ 0.48f, 0.90f, 0.92f, 1.0f });
    pauseCursor_->Update();
    pauseCursor_->Draw();
    for (size_t index = 0; index < count; ++index) { // 現在ページの選択項目番号
        drawText(index + 1, menu ? index + 1 : index + 9,
            firstY + static_cast<float>(index) * rowStep, index == pauseMenu_.GetSelection());
    }
    if (!menu) {
        const bool retry = pauseMenu_.GetView() == PauseMenuState::View::ConfirmRetry; // 記録を消す確認か退出の確認か
        drawText(5, retry ? 5 : 7, height * 0.30f, false);
        drawText(6, retry ? 6 : 8, height * 0.39f, false);
    }
    drawText(7, 11, height * 0.77f, false);
    drawText(8, 12, height * 0.84f, false);
}
