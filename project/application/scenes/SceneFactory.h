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
    /// 指定されたシーン名が生成対象として登録されているか判定する。
    /// </summary>
    static bool IsSceneAvailable(const std::string& name)
    {
        if (name == "Title" || name == "StageSelect") {
            return true;
        }
        return name == "Play";
    }

    /// <summary>
    /// 指定されたシーン名に対応するシーンを生成する
    /// </summary>
    static std::unique_ptr<MyEngine::IScene> Create(const std::string& name, std::shared_ptr<StageSelectionState> stageSelection = nullptr);
};

} // namespace GameApp
