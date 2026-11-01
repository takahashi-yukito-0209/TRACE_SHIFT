#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace GameApp {
inline constexpr size_t kSelectableStageCount = 4; // 危険エリアと既存の1体・2体・総合ルートの選択数
/// <summary>
/// 選択順と実際の配置ファイル・使用ルートの対応を保持する。
/// </summary>
struct SelectableStage {
    const char* fileName; // 読み込む配置JSON
    uint32_t routeMask; // 配置内で使用する既存ルートのフラグ
    bool allowCompletion = true; // ゴール判定とクリア遷移を許可するか
};
inline constexpr std::array<SelectableStage, kSelectableStageCount> kSelectableStages {{
    { "levels/trace_shift_hazard_trial.json", 4u, false },
    { "levels/trace_shift_stage.json", 1u },
    { "levels/trace_shift_stage.json", 2u },
    { "levels/trace_shift_stage.json", 4u },
}}; // 既存の地形を置き換えず危険エリアを先頭に追加した選択順
inline constexpr std::array<const char*, kSelectableStageCount> kStageDetailTextures {
    "stage_detail_01.png", "stage_detail_02.png", "stage_detail_03.png", "stage_detail_04.png"
}; // 各ルートの名前と攻略テーマを表示する画像

/// <summary>
/// 起動中のシーン間で選択位置とクリア履歴を共有する。
/// </summary>
struct StageSelectionState {
    size_t selectedStageIndex = 0; // 最後に選んだステージ番号
    std::array<bool, kSelectableStageCount> cleared {}; // 起動中にクリアしたステージ
    bool unlockAllStages = false; // 開発確認用にクリア条件を無視して全ステージへ入場するか

    /// <summary>
    /// 先行ステージのクリア履歴と確認用設定から、指定ステージの解放状態を取得する。
    /// </summary>
    bool IsStageUnlocked(size_t stageIndex) const
    {
        if (stageIndex >= kSelectableStageCount) {
            return false;
        }
        if (unlockAllStages) {
            return true;
        }
        for (size_t previousIndex = 0; previousIndex < stageIndex; ++previousIndex) { // 入場前にクリアが必要なステージ番号
            if (!cleared[previousIndex]) {
                return false;
            }
        }
        return true;
    }
};
} // namespace GameApp
