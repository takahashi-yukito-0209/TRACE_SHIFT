#pragma once
#include "../../engine/base/IScene.h"
#include <memory>
#include <string>

namespace GameApp {
struct StageSelectionState;

/// <summary>
/// シーン名から対応するシーンを生成するファクトリクラス
/// </summary>
class SceneFactory {
public:
    /// <summary>
    /// ビルド設定に応じてシーンへの入場を許可するか判定する。
    /// </summary>
    static bool IsSceneAvailable(const std::string& name)
    {
        if (name == "Title" || name == "StageSelect") {
            return true;
        }
#ifdef TRACE_SHIFT_PREVIEW_ONLY
        return false;
#else
        return name == "Play";
#endif
    }

    /// <summary>
    /// 指定されたシーン名に対応するシーンを生成する
    /// </summary>
    static std::unique_ptr<MyEngine::IScene> Create(const std::string& name, std::shared_ptr<StageSelectionState> stageSelection = nullptr);
};

} // namespace GameApp
