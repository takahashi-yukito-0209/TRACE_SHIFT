#pragma once

#include <algorithm>
#include <cmath>

/// <summary>
/// 落下時の静止・消失・暗転中の復帰・再実体化を共通の時間で管理する。
/// </summary>
class StageFailureSequenceState {
public:
    inline static constexpr float kStopSeconds = 0.10f; // 失敗を区切る短い静止時間
    inline static constexpr float kFadeOutSeconds = 0.30f; // 本体を消して暗転する時間
    inline static constexpr float kBlackHoldSeconds = 0.05f; // 完全な暗転を保って復帰するまでの時間
    inline static constexpr float kRecoverySeconds = 0.35f; // 既存リトライと合わせる明転・再実体化の時間
    inline static constexpr float kRespawnTime = kStopSeconds + kFadeOutSeconds + kBlackHoldSeconds; // リセットを実行する時刻
    inline static constexpr float kDuration = kRespawnTime + kRecoverySeconds; // 失敗から再操作までの合計時間

    /// <summary>
    /// 落下失敗の演出を先頭から開始する。
    /// </summary>
    void Begin() { elapsedSeconds_ = 0.0f; active_ = true; respawned_ = false; }

    /// <summary>
    /// 演出を進め、暗転中にリセットする更新で一度だけtrueを返す。
    /// </summary>
    bool Update(float deltaTime)
    {
        if (!active_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f) {
            return false;
        }
        const float nextTime = (std::min)(elapsedSeconds_ + deltaTime, kDuration); // 今回更新する演出時刻
        if (!respawned_ && nextTime >= kRespawnTime) {
            // 長いフレームでも暗転した画面を一度描き、復帰位置への切り替えを隠す。
            elapsedSeconds_ = kRespawnTime;
            respawned_ = true;
            return true;
        }
        elapsedSeconds_ = nextTime;
        active_ = elapsedSeconds_ < kDuration;
        return false;
    }

    /// <summary>
    /// ゲーム進行と操作を停止する演出中か取得する。
    /// </summary>
    bool IsActive() const { return active_; }

    /// <summary>
    /// 暗転中の復帰を実行済みか取得する。
    /// </summary>
    bool HasRespawned() const { return respawned_; }

    /// <summary>
    /// 失敗地点を隠し、復帰地点へ明転する暗幕の透明度を取得する。
    /// </summary>
    float GetDimAlpha() const
    {
        if (!active_) {
            return 0.0f;
        }
        if (respawned_) {
            return 1.0f - (std::clamp)((elapsedSeconds_ - kRespawnTime) / kRecoverySeconds, 0.0f, 1.0f);
        }
        return (std::clamp)((elapsedSeconds_ - kStopSeconds) / kFadeOutSeconds, 0.0f, 1.0f);
    }

    /// <summary>
    /// 失敗地点での消失と復帰地点での実体化に使う本体の透明度を取得する。
    /// </summary>
    float GetBodyAlpha() const { return 1.0f - GetDimAlpha(); }

private:
    float elapsedSeconds_ = 0.0f; // 今回の失敗演出の経過時間
    bool active_ = false; // 失敗演出が進行中か
    bool respawned_ = false; // 暗転中の復帰通知を送信済みか
};
