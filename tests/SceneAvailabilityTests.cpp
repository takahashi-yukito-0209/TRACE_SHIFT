#include "application/scenes/SceneFactory.h"
#include "application/scenes/StageSelectionState.h"
#include <iostream>
#include <stdexcept>

namespace {
/// <summary>
/// シーン名とステージ解放条件の判定結果を確認する。
/// </summary>
void Check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
}

/// <summary>
/// 登録済みシーンの利用可否とステージの段階解放を検証する。
/// </summary>
int main()
{
    Check(GameApp::SceneFactory::IsSceneAvailable("Title"), "Title must remain available");
    Check(GameApp::SceneFactory::IsSceneAvailable("StageSelect"), "Stage preview must remain available");
    Check(!GameApp::SceneFactory::IsSceneAvailable(""), "Empty scene must be rejected");
    Check(!GameApp::SceneFactory::IsSceneAvailable("Unknown"), "Unknown scene must be rejected");
    GameApp::StageSelectionState selection; // 段階解放と確認用の全解放を調べる共有状態
    Check(selection.IsStageUnlocked(0) && !selection.IsStageUnlocked(1) && !selection.IsStageUnlocked(2),
        "Only the first stage must be unlocked by default");
    selection.cleared[0] = true;
    Check(selection.IsStageUnlocked(1) && !selection.IsStageUnlocked(2), "First clear must unlock only the second stage");
    selection.cleared[1] = true;
    Check(selection.IsStageUnlocked(2), "Second clear must unlock the final stage");
    selection.cleared = {};
    selection.unlockAllStages = true;
    Check(selection.IsStageUnlocked(0) && selection.IsStageUnlocked(1) && selection.IsStageUnlocked(2),
        "Testing override must allow all stages without clearing them");
    Check(!selection.IsStageUnlocked(GameApp::kSelectableStageCount), "Override must still reject invalid stage indices");
    selection.cleared[2] = true;
    selection.unlockAllStages = false;
    Check(!selection.IsStageUnlocked(1) && !selection.IsStageUnlocked(2) && selection.cleared[2],
        "Disabling override must restore prerequisites without modifying clear history");
    Check(GameApp::SceneFactory::IsSceneAvailable("Play"), "Normal build must allow gameplay");
#ifdef USE_IMGUI
    std::cout << "Development scene availability tests passed\n";
#else
    std::cout << "Release scene availability tests passed\n";
#endif
}
