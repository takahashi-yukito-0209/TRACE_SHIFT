#include "PlayScene.h"
#include "../effects/EffectImGuiUtility.h"
#include "ImGuiManager.h"
#include "../../engine/3d/Object3d.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstring>
#include <string>
#include <vector>

using namespace MyEngine;
using namespace Math;

namespace {
constexpr const char* kSceneEditorWindowName = "Scene Editor"; // シーン編集用ImGuiウィンドウ名
constexpr const char* kEffectTypeComboLabel = "Effect Type"; // エフェクト種別選択のImGuiラベル
constexpr const char* kPlayEffectButtonLabel = "Play Effect"; // エフェクト再生ボタンの表示文
constexpr std::array<const char*, 3> kEffectNames = {
    "Dimensional Shatter", "Time Reversal", "Time Stop",
}; // ImGuiで選択できるエフェクト名

#ifdef USE_IMGUI
/// <summary>
/// std::stringをImGuiで編集する。
/// </summary>
bool EditStringText(const char* label, std::string& value)
{
    std::array<char, 256> textBuffer {}; // 編集中の文字列バッファ
    const size_t copyLength = (std::min)(value.size(), textBuffer.size() - 1); // バッファへコピーする文字数
    std::memcpy(textBuffer.data(), value.data(), copyLength);

    if (!ImGui::InputText(label, textBuffer.data(), textBuffer.size())) {
        return false;
    }

    value = textBuffer.data();
    return true;
}

/// <summary>
/// Vector3の各成分が同じ値か判定する。
/// </summary>
bool IsSameVector3Value(const Vector3& lhs, const Vector3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

/// <summary>
/// Vector4の各成分が同じ値か判定する。
/// </summary>
bool IsSameVector4Value(const Vector4& lhs, const Vector4& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

#endif
} // namespace

/// <summary>
/// エフェクト選択と再生操作用のImGuiを描画する。
/// </summary>
void PlayScene::DrawEffectControllerImGui()
{
#ifdef USE_IMGUI
    int selectedEffectIndex = static_cast<int>(selectedEffectType_); // ImGuiで編集中のエフェクト番号
    const bool isPlaying = IsAnyEffectPlaying(); // 再生中の選択変更と重複再生を防ぐ
    EffectImGuiUtility::DrawPropertyLabel(kEffectTypeComboLabel);
    ImGui::BeginDisabled(isPlaying);
    if (ImGui::Combo(
            "##EffectType",
            &selectedEffectIndex,
            kEffectNames.data(),
            static_cast<int>(kEffectNames.size()))) {
        selectedEffectType_ = static_cast<EffectType>(selectedEffectIndex);
    }

    if (ImGui::Button(kPlayEffectButtonLabel, ImVec2(-FLT_MIN, 0.0f))) {
        StartSelectedEffect();
    }
    ImGui::EndDisabled();
#endif
}

/// <summary>
/// 選択中エフェクトの詳細ImGuiを描画する。
/// </summary>
void PlayScene::DrawSelectedEffectImGui()
{
#ifdef USE_IMGUI
    switch (selectedEffectType_) {
    case EffectType::DimensionalShatter:
        temporalRiftEffect_.DrawImGui();
        break;
    case EffectType::TimeStop:
        timeStopEffect_.DrawImGui();
        break;
    case EffectType::TimeReversal:
        timeReversalEffect_.DrawImGui();
        break;
    }
#endif
}

/// <summary>
/// 実ステージで使用中のオブジェクト編集ImGuiを描画する。
/// </summary>
void PlayScene::DrawPastSelfTutorialStageEditorImGui()
{
#ifdef USE_IMGUI
    constexpr uint32_t kOneCloneRouteMask = 1u << 0; // 1体ルートの編集用フラグ
    constexpr uint32_t kTwoCloneRouteMask = 1u << 1; // 2体ルートの編集用フラグ
    constexpr uint32_t kFinalRouteMask = 1u << 2; // 最終ルートの編集用フラグ
    int& lastAutoOpenedObjectIndex = lastAutoOpenedStageObjectIndex_; // このシーンの自動展開状態
    std::vector<PastSelfTutorialEditorObject> editorObjects; // 選択中ルートで使用するステージ編集対象
    BuildPastSelfTutorialEditorObjects(&editorObjects);

    int selectedObjectIndex = GetSelectedSceneObjectIndex(); // このフレーム開始時の選択オブジェクト番号
    const bool hasSelectedEditorObject = selectedObjectIndex >= 0 && selectedObjectIndex < static_cast<int>(editorObjects.size()); // 有効な編集対象を選択しているか
    const bool selectedStageBlock = hasSelectedEditorObject && editorObjects[static_cast<size_t>(selectedObjectIndex)].type == PastSelfTutorialEditorObjectType::StageBlock; // 固定ブロックを選択しているか
    const size_t selectedStageBlockIndex = selectedStageBlock ? editorObjects[static_cast<size_t>(selectedObjectIndex)].stageBlockIndex : pastSelfTutorialStageBlocks_.size(); // 選択中の固定ブロック番号

    const auto selectStageBlock = [this, &editorObjects](size_t stageBlockIndex) {
        BuildPastSelfTutorialEditorObjects(&editorObjects);
        for (size_t editorIndex = 0; editorIndex < editorObjects.size(); ++editorIndex) {
            if (editorObjects[editorIndex].type == PastSelfTutorialEditorObjectType::StageBlock && editorObjects[editorIndex].stageBlockIndex == stageBlockIndex) {
                SelectSceneObjectForEditor(editorIndex);
                return;
            }
        }
    }; // ステージブロック番号に対応する編集対象を選択する処理

    if (ImGui::Checkbox("Auto Camera Follow", &pastSelfTutorialAutoCameraFollow_)) {
        if (pastSelfTutorialAutoCameraFollow_) {
            ResetPastSelfTutorialCameraFrame();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Camera")) {
        ResetPastSelfTutorialCameraFrame();
    }

    EditStringText("Stage File", pastSelfTutorialStageFileName_);
    if (ImGui::Button("Load Hazard Trial")) {
        pastSelfTutorialStageFileName_ = "levels/trace_shift_hazard_trial.json";
        if (ReloadPastSelfTutorialStage()) {
            SelectPastSelfTutorialRoute(PastSelfTutorialRoute::FinalChallenge);
            BuildPastSelfTutorialEditorObjects(&editorObjects);
            selectedObjectIndex = -1;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Main Stage")) {
        pastSelfTutorialStageFileName_ = "levels/trace_shift_stage.json";
        if (ReloadPastSelfTutorialStage()) {
            SelectPastSelfTutorialRoute(PastSelfTutorialRoute::OneCloneBasics);
            BuildPastSelfTutorialEditorObjects(&editorObjects);
            selectedObjectIndex = -1;
        }
    }
    if (ImGui::Button("New Block")) {
        const size_t createdBlockIndex = CreatePastSelfTutorialStageBlock(); // 新規作成した固定ブロック番号
        selectStageBlock(createdBlockIndex);
        selectedObjectIndex = GetSelectedSceneObjectIndex();
    }
    ImGui::SameLine();
    if (!selectedStageBlock) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Duplicate")) {
        const size_t duplicatedBlockIndex = DuplicatePastSelfTutorialStageBlock(selectedStageBlockIndex); // 複製した固定ブロック番号
        if (duplicatedBlockIndex < pastSelfTutorialStageBlocks_.size()) {
            selectStageBlock(duplicatedBlockIndex);
            selectedObjectIndex = GetSelectedSceneObjectIndex();
        }
    }
    ImGui::SameLine();
    const bool selectedGoalMarker = selectedStageBlock && selectedStageBlockIndex < pastSelfTutorialStageBlocks_.size() && pastSelfTutorialStageBlocks_[selectedStageBlockIndex].goalMarker; // 削除不可のゴール表示か
    if (selectedGoalMarker) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Delete")) {
        if (DeletePastSelfTutorialStageBlock(selectedStageBlockIndex)) {
            BuildPastSelfTutorialEditorObjects(&editorObjects);
            SelectSceneObjectForEditor(0);
            selectedObjectIndex = GetSelectedSceneObjectIndex();
        }
    }
    if (selectedGoalMarker) {
        ImGui::EndDisabled();
    }
    if (!selectedStageBlock) {
        ImGui::EndDisabled();
    }

    if (ImGui::Button("Save Stage")) {
        SavePastSelfTutorialStage();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Stage") && ReloadPastSelfTutorialStage()) {
        BuildPastSelfTutorialEditorObjects(&editorObjects);
        SelectSceneObjectForEditor(0);
        selectedObjectIndex = GetSelectedSceneObjectIndex();
        lastAutoOpenedObjectIndex = -1;
    }
    if (!pastSelfTutorialStageFileMessage_.empty()) {
        const ImVec4 statusColor = pastSelfTutorialStageFileSucceeded_
            ? ImVec4(0.35f, 0.9f, 0.45f, 1.0f)
            : ImVec4(1.0f, 0.45f, 0.3f, 1.0f); // 読み書き結果に応じた表示色
        ImGui::TextColored(statusColor, "%s", pastSelfTutorialStageFileMessage_.c_str());
    }
    ImGui::Separator();
    ImGui::Text("Route Objects: %zu", editorObjects.size());
    for (size_t objectIndex = 0; objectIndex < editorObjects.size(); ++objectIndex) {
        PastSelfTutorialEditorObject& editorObject = editorObjects[objectIndex]; // 一覧へ表示する実ステージオブジェクト
        if (!editorObject.object) {
            continue;
        }

        ImGui::PushID(static_cast<int>(objectIndex));
        const bool isSelected = selectedObjectIndex == static_cast<int>(objectIndex); // ギズモ操作対象として選択中か
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth; // ステージオブジェクト表示用フラグ
        if (isSelected) {
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
            if (lastAutoOpenedObjectIndex != selectedObjectIndex) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            }
        }

        const bool nodeOpen = ImGui::TreeNodeEx(editorObject.label, nodeFlags); // Transform詳細を表示するか
        if (ImGui::IsItemClicked()) {
            SelectSceneObjectForEditor(objectIndex);
        }
        if (nodeOpen) {
            const Vector3 beforeScale = editorObject.object->GetScale(); // 編集前の表示スケール
            const Vector3 beforeRotate = editorObject.object->GetRotate(); // 編集前の表示回転
            const Vector3 beforeTranslate = editorObject.object->GetTranslate(); // 編集前の表示座標
            const Vector4 beforeColor = editorObject.object->GetMaterialColor(); // 編集前の表示色
            editorObject.object->DrawImGui(static_cast<int>(objectIndex));
            if (editorObject.type == PastSelfTutorialEditorObjectType::StageBlock && editorObject.stageBlockIndex < pastSelfTutorialStageBlocks_.size()) {
                PastSelfTutorialStageBlock& stageBlock = pastSelfTutorialStageBlocks_[editorObject.stageBlockIndex]; // 追加設定を編集する固定ブロック
                if (!IsSameVector4Value(beforeColor, editorObject.object->GetMaterialColor())) {
                    stageBlock.baseColor = editorObject.object->GetMaterialColor();
                }
                ImGui::SeparatorText("Stage Block");
                EditStringText("Name", stageBlock.name);
                ImGui::BeginDisabled(stageBlock.goalMarker || stageBlock.oneCloneGoalPlatform);
                if (ImGui::Checkbox("Hazard Area", &stageBlock.hazard) && stageBlock.hazard) {
                    stageBlock.collider.enabled = false;
                }
                ImGui::EndDisabled();
                if (stageBlock.hazard) {
                    editorObject.object->SetRotate({});
                    ImGui::TextDisabled("Axis-aligned hazard; green switch stops it.");
                }
                ImGui::BeginDisabled(stageBlock.hazard);
                ImGui::Checkbox("Collidable", &stageBlock.collider.enabled);
                ImGui::EndDisabled();
                bool useOneCloneRoute = (stageBlock.routeMask & kOneCloneRouteMask) != 0; // 1体ルートで使用するか
                bool useTwoCloneRoute = (stageBlock.routeMask & kTwoCloneRouteMask) != 0; // 2体ルートで使用するか
                bool useFinalRoute = (stageBlock.routeMask & kFinalRouteMask) != 0; // 最終ルートで使用するか
                if (ImGui::Checkbox("One Clone Route", &useOneCloneRoute)) {
                    stageBlock.routeMask = useOneCloneRoute ? stageBlock.routeMask | kOneCloneRouteMask : stageBlock.routeMask & ~kOneCloneRouteMask;
                }
                if (ImGui::Checkbox("Two Clone Route", &useTwoCloneRoute)) {
                    stageBlock.routeMask = useTwoCloneRoute ? stageBlock.routeMask | kTwoCloneRouteMask : stageBlock.routeMask & ~kTwoCloneRouteMask;
                }
                if (ImGui::Checkbox("Final Route", &useFinalRoute)) {
                    stageBlock.routeMask = useFinalRoute ? stageBlock.routeMask | kFinalRouteMask : stageBlock.routeMask & ~kFinalRouteMask;
                }
                if (stageBlock.routeMask == 0) {
                    if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::OneCloneBasics) {
                        stageBlock.routeMask = kOneCloneRouteMask;
                    } else if (pastSelfTutorialRoute_ == PastSelfTutorialRoute::TwoCloneCooperation) {
                        stageBlock.routeMask = kTwoCloneRouteMask;
                    } else {
                        stageBlock.routeMask = kFinalRouteMask;
                    }
                }
                if (stageBlock.goalMarker) {
                    stageBlock.routeMask |= kFinalRouteMask;
                    ImGui::TextDisabled("Goal marker cannot be deleted.");
                }
            }
            const bool transformEdited = !IsSameVector3Value(beforeScale, editorObject.object->GetScale()) ||
                !IsSameVector3Value(beforeRotate, editorObject.object->GetRotate()) ||
                !IsSameVector3Value(beforeTranslate, editorObject.object->GetTranslate()); // Transformが変更されたか
            if (transformEdited) {
                ApplyPastSelfTutorialEditorTransform(editorObject);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    lastAutoOpenedObjectIndex = selectedObjectIndex;
#endif
}

/// <summary>
/// シーン編集用のImGuiを描画する。
/// </summary>
void PlayScene::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin(kSceneEditorWindowName);

    if (ImGui::BeginTabBar("SceneEditorTabs")) {
        if (ImGui::BeginTabItem("Stage")) {
            DrawPastSelfTutorialStageEditorImGui();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Player")) {
            DrawPastSelfTutorialRouteSelector();
            DrawPastSelfTutorialFixedStatusHud();
            ImGui::Checkbox("Verification Details", &pastSelfTutorialShowVerificationDetails_);
            if (ImGui::BeginChild("PlayerTabDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None)) {
                DrawPastSelfTutorialImGui();
                if (pastSelfTutorialShowVerificationDetails_) {
                    ImGui::Separator();
                    DrawPastSelfTutorialStatusHud();
                }
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Sprites")) {
            DrawSceneSpriteEditImGui();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Effects")) {
            DrawEffectControllerImGui();
            ImGui::Separator();
            if (ImGui::BeginChild("EffectSettings", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None)) {
                DrawSelectedEffectImGui();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
#endif
}
