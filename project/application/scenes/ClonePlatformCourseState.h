#pragma once

#include "../../engine/utility/MathTypes.h"
#include <array>
#include <cstddef>

/// <summary>
/// 分身足場コースの現在区間と、区間ごとの再挑戦位置を管理する。
/// </summary>
class ClonePlatformCourseState {
public:
    inline static constexpr std::array<Math::Vector3, 3> kStartPositions = { {
        { -8.7f, 0.5f, 0.0f }, { 15.3f, 0.5f, 0.0f }, { 39.3f, 0.5f, 0.0f },
    } }; // 足場・移動・再合流の各区間を地上から始める位置
    inline static constexpr size_t kSectionCount = kStartPositions.size(); // コース内の区間数

    /// <summary>
    /// コースを最初の区間へ戻す。区間内のリトライでは呼び出さない。
    /// </summary>
    void Reset() { sectionIndex_ = 0; }

    /// <summary>
    /// 次の区間へ進み、最終区間では位置を変えずfalseを返す。
    /// </summary>
    bool TryAdvance()
    {
        if (sectionIndex_ + 1 >= kSectionCount) {
            return false;
        }
        ++sectionIndex_;
        return true;
    }

    /// <summary>
    /// 現在区間の番号を0始まりで取得する。
    /// </summary>
    size_t GetSectionIndex() const { return sectionIndex_; }

    /// <summary>
    /// 現在区間の記録準備・リトライに使用する開始位置を取得する。
    /// </summary>
    const Math::Vector3& GetStartPosition() const { return kStartPositions[sectionIndex_]; }

private:
    size_t sectionIndex_ = 0; // 現在挑戦中の区間番号
};
