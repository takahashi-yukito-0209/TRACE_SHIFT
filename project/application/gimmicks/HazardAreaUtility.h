#pragma once

#include "../player/PlayerState.h"
#include <algorithm>
#include <array>
#include <cmath>

/// <summary>
/// 固定危険エリアの本体接触判定と稼働表示を扱う。
/// </summary>
namespace HazardAreaUtility {
/// <summary>
/// 稼働中の危険エリアへ最初に接触する移動割合を取得する。既に重なる場合は0を返す。
/// </summary>
inline bool TryGetContactFraction(const PlayerState& previous, const PlayerState& current,
    const SolidCollider& area, bool switchPressed, float& contactFraction)
{
    if (switchPressed || !area.enabled || area.halfSize.x <= 0.0f || area.halfSize.y <= 0.0f || area.halfSize.z <= 0.0f) {
        return false;
    }
    const std::array<float, 3> start { previous.transform.translate.x, previous.transform.translate.y, previous.transform.translate.z }; // 移動前の本体中心
    const std::array<float, 3> finish { current.transform.translate.x, current.transform.translate.y, current.transform.translate.z }; // 移動後の本体中心
    const std::array<float, 3> center { area.center.x, area.center.y, area.center.z }; // 危険エリアの中心
    const std::array<float, 3> extent { // 本体の半サイズで膨らませた接触範囲
        area.halfSize.x + (std::max)(std::fabs(previous.transform.scale.x), std::fabs(current.transform.scale.x)) * 0.5f,
        area.halfSize.y + (std::max)(std::fabs(previous.transform.scale.y), std::fabs(current.transform.scale.y)) * 0.5f,
        area.halfSize.z + (std::max)(std::fabs(previous.transform.scale.z), std::fabs(current.transform.scale.z)) * 0.5f,
    };
    float entry = 0.0f; // 全軸で接触範囲へ入る移動割合
    float exit = 1.0f; // 全軸で接触範囲から出る移動割合
    for (size_t axis = 0; axis < start.size(); ++axis) { // X・Y・Zの判定軸
        if (!std::isfinite(start[axis]) || !std::isfinite(finish[axis]) || !std::isfinite(center[axis]) || !std::isfinite(extent[axis])) {
            return false;
        }
        const float offset = start[axis] - center[axis]; // エリア中心を基準にした移動開始位置
        const float movement = finish[axis] - start[axis]; // この軸の移動距離
        if (std::fabs(movement) <= 0.000001f) {
            if (std::fabs(offset) > extent[axis]) {
                return false;
            }
            continue;
        }
        const float first = (-extent[axis] - offset) / movement; // 片側の境界へ達する移動割合
        const float second = (extent[axis] - offset) / movement; // 反対側の境界へ達する移動割合
        entry = (std::max)(entry, (std::min)(first, second));
        exit = (std::min)(exit, (std::max)(first, second));
        if (entry > exit) {
            return false;
        }
    }
    contactFraction = entry;
    return true;
}

/// <summary>
/// スイッチで停止していない危険エリアと、本体の移動区間が接触するか確認する。
/// </summary>
inline bool TouchesPlayer(const PlayerState& previous, const PlayerState& current,
    const SolidCollider& area, bool switchPressed)
{
    float contactFraction = 0.0f; // 接触有無だけを確認するための移動割合
    return TryGetContactFraction(previous, current, area, switchPressed, contactFraction);
}

/// <summary>
/// 稼働中は明滅させ、接触時は暖色の白へ発光させる。停止中は静止した薄い表示にする。
/// </summary>
inline Math::Vector4 CalculateColor(const Math::Vector4& baseColor, bool switchPressed, float elapsedTime, float impactStrength = 0.0f)
{
    if (switchPressed) {
        return { 0.08f, 0.55f, 0.65f, 0.12f };
    }
    const float time = std::isfinite(elapsedTime) ? elapsedTime : 0.0f; // 明滅に使う有限の経過時間
    const float brightness = 0.75f + 0.25f * std::sin(time * 10.0f); // 稼働を示す明滅の明るさ
    const float impact = std::isfinite(impactStrength) ? (std::clamp)(impactStrength, 0.0f, 1.0f) : 0.0f; // 接触時だけ適用する発光の強さ
    const Math::Vector4 normal { baseColor.x * brightness, baseColor.y * brightness, baseColor.z * brightness, baseColor.w }; // 通常の稼働色
    return {
        normal.x + (1.0f - normal.x) * impact,
        normal.y + (0.85f - normal.y) * impact,
        normal.z + (0.65f - normal.z) * impact,
        normal.w + (1.0f - normal.w) * impact,
    };
}
}
