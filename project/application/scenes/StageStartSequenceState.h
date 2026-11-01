#pragma once

#include <algorithm>
#include <cmath>

/// <summary>
/// 開始表示・操作解禁・解禁直後の光を共通の時間で管理する。
/// </summary>
class StageStartSequenceState {
public:
    inline static constexpr float kInitialHoldSeconds = 2.0f; // 初回と新区間でテーマを読む時間
    inline static constexpr float kRetryHoldSeconds = 0.35f; // 通常リトライで区切りを作る時間
    inline static constexpr float kPulseSeconds = 0.55f; // 操作解禁後にリングを広げる時間
    inline static constexpr float kCameraMoveSeconds = 1.65f; // 操作解禁前にカメラを落ち着かせる移動時間
    inline static constexpr float kCameraStartDistanceScale = 1.08f; // 開始時に通常より少し引く距離倍率

    /// <summary>
    /// 初回またはリトライ用の演出を先頭から開始する。
    /// </summary>
    void Begin(bool retry)
    {
        holdSeconds_ = retry ? kRetryHoldSeconds : kInitialHoldSeconds;
        elapsedSeconds_ = 0.0f;
        retry_ = retry;
        started_ = true;
    }

    /// <summary>
    /// 時間を進め、操作解禁をまたいだ更新で一度だけtrueを返す。
    /// </summary>
    bool Update(float deltaTime)
    {
        if (!started_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f) {
            return false;
        }
        const bool wasHolding = IsHolding(); // 更新前に操作を止めていたか
        const float nextTime = (std::min)(elapsedSeconds_ + deltaTime, holdSeconds_ + kPulseSeconds); // 今回の更新後の時刻
        if (wasHolding && nextTime >= holdSeconds_) {
            // 長いフレームでも解禁直後の光を飛ばさず、同じ更新から表示する。
            elapsedSeconds_ = holdSeconds_;
            return true;
        }
        elapsedSeconds_ = nextTime;
        return false;
    }

    /// <summary>
    /// 操作とゲーム進行を止める開始表示中か取得する。
    /// </summary>
    bool IsHolding() const { return started_ && elapsedSeconds_ < holdSeconds_; }

    /// <summary>
    /// 初回本文を出現開始より遅らせて表示し、操作解禁前にフェードする透明度を取得する。
    /// </summary>
    float GetIntroAlpha() const
    {
        return IsArrivalVisible() ? (std::clamp)((elapsedSeconds_ - 0.4f) / 0.25f, 0.0f, 1.0f) *
            (std::min)((holdSeconds_ - elapsedSeconds_) / 0.3f, 1.0f) : 0.0f;
    }

    /// <summary>
    /// 初回と新区間だけ、本体を実体化させる表示中か取得する。
    /// </summary>
    bool IsArrivalVisible() const { return IsHolding() && !retry_; }

    /// <summary>
    /// 本体の実体化に使う進行率を取得する。
    /// </summary>
    float GetArrivalProgress() const
    {
        return IsArrivalVisible() ? (std::clamp)(elapsedSeconds_ / holdSeconds_, 0.0f, 1.0f) : 1.0f;
    }

    /// <summary>
    /// 出現中の画面を薄く暗くし、操作解禁までに通常へ戻す強度を取得する。
    /// </summary>
    float GetArrivalDimAlpha() const
    {
        return IsArrivalVisible() ? 0.32f * (1.0f - GetArrivalProgress()) : 0.0f;
    }

    /// <summary>
    /// 初回だけ少し引いた画角から通常へ寄せ、操作解禁前に静止する距離倍率を取得する。
    /// </summary>
    float GetCameraDistanceScale() const
    {
        if (!IsArrivalVisible()) {
            return 1.0f;
        }
        const float progress = (std::clamp)(elapsedSeconds_ / kCameraMoveSeconds, 0.0f, 1.0f); // カメラ移動の進行率
        const float eased = progress * progress * (3.0f - 2.0f * progress); // 開始と停止の速度変化を抑える補間率
        return 1.0f + (kCameraStartDistanceScale - 1.0f) * (1.0f - eased);
    }

    /// <summary>
    /// 到達床を一度明るくして元へ戻す強度を取得する。
    /// </summary>
    float GetGoalGlow() const
    {
        return IsHolding() ? std::sin(elapsedSeconds_ / holdSeconds_ * 3.14159265f) * (retry_ ? 0.25f : 0.75f) : 0.0f;
    }

    /// <summary>
    /// 操作解禁直後のリングを描画する期間か取得する。
    /// </summary>
    bool IsPulseVisible() const
    {
        return started_ && !IsHolding() && elapsedSeconds_ < holdSeconds_ + kPulseSeconds;
    }

    /// <summary>
    /// リングの拡大とフェードに使う0から1の進行率を取得する。
    /// </summary>
    float GetPulseProgress() const
    {
        return (std::clamp)((elapsedSeconds_ - holdSeconds_) / kPulseSeconds, 0.0f, 1.0f);
    }

private:
    float elapsedSeconds_ = 0.0f; // 演出開始からの経過時間
    float holdSeconds_ = 0.0f; // 今回の操作停止時間
    bool retry_ = false; // 短いリトライ演出か
    bool started_ = false; // 演出が開始済みか
};
