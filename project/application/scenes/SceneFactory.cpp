#include "SceneFactory.h"
#include "StageSelectionState.h"
#ifndef TRACE_SHIFT_PREVIEW_ONLY
#include "PlayScene.h"
#endif
#include "TitleScene.h"

using namespace GameApp;
using namespace MyEngine;

/// <summary>
/// シーンの名前からシーンオブジェクトを生成する。ここで新しいシーンを追加していく。
/// </summary>
std::unique_ptr<IScene> SceneFactory::Create(const std::string& name, std::shared_ptr<StageSelectionState> stageSelection)
{
    if (!IsSceneAvailable(name)) {
        return nullptr;
    }
    if (name == "Play" && stageSelection && !stageSelection->IsStageUnlocked(stageSelection->selectedStageIndex)) {
        return nullptr;
    }

    // シーンの名前に応じて対応するシーンオブジェクトを生成して返す
    if (name == "Title") {
        return std::make_unique<TitleScene>(stageSelection);
    } else if (name == "StageSelect") {
        return std::make_unique<TitleScene>(stageSelection, true);
    } else if (name == "Play") {
#ifndef TRACE_SHIFT_PREVIEW_ONLY
        return std::make_unique<PlayScene>(stageSelection);
#endif
    }

    // 存在しない名前の場合は nullptr を返す
    return nullptr;
}
