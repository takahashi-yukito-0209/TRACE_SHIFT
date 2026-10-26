#include "PlayScene.h"
#include "ImGuiManager.h"
#include "../../engine/2d/Sprite.h"

#include <algorithm>
#include <array>
#include <string>

using namespace MyEngine;

namespace {
constexpr std::array<const char*, 5> kSceneSpriteCreateTextureNames = {
    "uvChecker.png",
    "monsterBall.png",
    "circle.png",
    "circle2.png",
    "gradationLine.png",
}; // ImGuiから生成できるスプライト用テクスチャ名

/// <summary>
/// パス文字列から表示用のファイル名部分だけを取得する。
/// </summary>
std::string GetDisplayFileName(const std::string& path)
{
    const size_t separatorPosition = path.find_last_of("/\\"); // 最後に見つかったパス区切り位置
    if (separatorPosition == std::string::npos) {
        return path;
    }

    return path.substr(separatorPosition + 1);
}
} // namespace

/// <summary>
/// ImGuiでシーン内スプライトの生成と削除を行う。
/// </summary>
void PlayScene::DrawSceneSpriteEditImGui()
{
#ifdef USE_IMGUI
    int& selectedCreateTextureIndex = objectEditorState_.selectedCreateTextureIndex; // このシーンの生成テクスチャ選択
    int& selectedDeleteSpriteIndex = objectEditorState_.selectedDeleteSpriteIndex; // このシーンの削除スプライト選択

    ImGui::SeparatorText("Create");
    const char* textureNames[kSceneSpriteCreateTextureNames.size()] = {}; // Combo表示用のテクスチャ名一覧
    for (size_t textureIndex = 0; textureIndex < kSceneSpriteCreateTextureNames.size(); ++textureIndex) {
        textureNames[textureIndex] = kSceneSpriteCreateTextureNames[textureIndex];
    }

    ImGui::Combo(
        "Texture",
        &selectedCreateTextureIndex,
        textureNames,
        static_cast<int>(kSceneSpriteCreateTextureNames.size()));
    selectedCreateTextureIndex = (std::clamp)(
        selectedCreateTextureIndex,
        0,
        static_cast<int>(kSceneSpriteCreateTextureNames.size()) - 1);
    if (ImGui::Button("Create Sprite")) {
        const std::string textureName = kSceneSpriteCreateTextureNames[static_cast<size_t>(selectedCreateTextureIndex)]; // 生成するスプライトのテクスチャ名
        CreateSceneSprite(textureName);
        selectedDeleteSpriteIndex = static_cast<int>(sprites_.size()) - 1;
    }

    ImGui::SeparatorText("Delete");
    if (!sprites_.empty()) {
        const int spriteCount = static_cast<int>(sprites_.size()); // 削除対象として選択できるスプライト数
        selectedDeleteSpriteIndex = (std::clamp)(selectedDeleteSpriteIndex, 0, spriteCount - 1);

        std::string preview = "Sprite " + std::to_string(selectedDeleteSpriteIndex); // Comboの現在表示名
        Sprite* previewSprite = sprites_[static_cast<size_t>(selectedDeleteSpriteIndex)].get(); // 現在選択中のスプライト
        if (previewSprite && !previewSprite->GetTextureFilePath().empty()) {
            preview += " : " + GetDisplayFileName(previewSprite->GetTextureFilePath());
        }

        if (ImGui::BeginCombo("Delete Target", preview.c_str())) {
            for (int spriteIndex = 0; spriteIndex < spriteCount; ++spriteIndex) {
                Sprite* sprite = sprites_[static_cast<size_t>(spriteIndex)].get(); // 表示名を作る対象のスプライト
                std::string label = "Sprite " + std::to_string(spriteIndex); // Comboに表示するスプライト名
                if (sprite && !sprite->GetTextureFilePath().empty()) {
                    label += " : " + GetDisplayFileName(sprite->GetTextureFilePath());
                }

                const bool isSelected = selectedDeleteSpriteIndex == spriteIndex; // 現在選択中かどうか
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedDeleteSpriteIndex = spriteIndex;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Delete Sprite")) {
            DeleteSceneSprite(static_cast<size_t>(selectedDeleteSpriteIndex));
            selectedDeleteSpriteIndex = (std::min)(selectedDeleteSpriteIndex, static_cast<int>(sprites_.size()) - 1);
        }
    } else {
        ImGui::Text("No sprites.");
    }
#endif
}
