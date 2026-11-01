#pragma once

#include <algorithm>
#include <cmath>

/// <summary>
/// 落下・危険エリア接触時の静止・のけぞり・暗転中の復帰・再実体化を管理する。
/// </summary>
class StageFailureSequenceState {
public:
    /// <summary>
    /// 接触と落下で失敗直後の反応を切り替える原因。
    /// </summary>
    enum class Cause {
        Contact, // 危険エリアへの接触
        Fall, // 復帰できない高さへの落下
    };
    /// <summary>
    /// 比較用の演出時間と表示だけののけぞり量をまとめる。
    /// </summary>
    struct Timing {
        float stopSeconds; // 静止・明滅の時間
        float reactionSeconds; // 暗転前に反応を見せる時間
        float fadeOutSeconds; // 画面を暗転させる時間
        float blackHoldSeconds; // 完全な暗転を保つ時間
        float recoilDistance; // 表示だけの後退距離
        float recoilAngle; // 表示だけの傾きの角度
    };
    inline static constexpr Timing kDetailedTiming { 0.18f, 0.40f, 0.45f, 0.12f, 0.30f, 0.32f }; // 従来の約1.5秒の演出
    inline static constexpr Timing kQuickTiming { 0.12f, 0.23f, 0.23f, 0.07f, 0.16f, 0.16f }; // 再試行を優先した約1秒の演出
    inline static constexpr Timing kFallTiming { 0.0f, 0.18f, 0.22f, 0.05f, 0.0f, 0.0f }; // 落ち抜ける反応を見せて約0.8秒で復帰する演出
    inline static constexpr float kRecoverySeconds = 0.35f; // 既存リトライと合わせる明転・再実体化の時間

    /// <summary>
    /// 失敗演出を先頭から開始する。
    /// </summary>
    void Begin(bool quick = false, Cause cause = Cause::Contact)
    {
        cause_ = cause;
        timing_ = IsFall() ? kFallTiming : quick ? kQuickTiming : kDetailedTiming;
        elapsedSeconds_ = 0.0f;
        active_ = true;
        respawned_ = false;
    }

    /// <summary>
    /// 演出開始時に固定した時間と反応量を取得する。
    /// </summary>
    const Timing& GetTiming() const { return timing_; }

    /// <summary>
    /// 今回の失敗が接触反応を使わない落下か取得する。
    /// </summary>
    bool IsFall() const { return cause_ == Cause::Fall; }

    /// <summary>
    /// 完全な暗転中に復帰する時刻を取得する。
    /// </summary>
    float GetRespawnTime() const
    {
        return timing_.stopSeconds + timing_.reactionSeconds + timing_.fadeOutSeconds + timing_.blackHoldSeconds;
    }

    /// <summary>
    /// 操作再開までの合計時間を取得する。
    /// </summary>
    float GetDuration() const { return GetRespawnTime() + kRecoverySeconds; }

    /// <summary>
    /// 演出を進め、暗転中にリセットする更新で一度だけtrueを返す。
    /// </summary>
    bool Update(float deltaTime)
    {
        if (!active_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f) {
            return false;
        }
        const float nextTime = (std::min)(elapsedSeconds_ + deltaTime, GetDuration()); // 今回更新する演出時刻
        if (!respawned_ && nextTime >= GetRespawnTime()) {
            // 長いフレームでも暗転した画面を一度描き、復帰位置への切り替えを隠す。
            elapsedSeconds_ = GetRespawnTime();
            respawned_ = true;
            return true;
        }
        elapsedSeconds_ = nextTime;
        active_ = elapsedSeconds_ < GetDuration();
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
            return 1.0f - (std::clamp)((elapsedSeconds_ - GetRespawnTime()) / kRecoverySeconds, 0.0f, 1.0f);
        }
        return (std::clamp)((elapsedSeconds_ - timing_.stopSeconds - timing_.reactionSeconds) / timing_.fadeOutSeconds, 0.0f, 1.0f);
    }

    /// <summary>
    /// 接触した危険エリアの発光の強さを取得する。復帰時には発光させない。
    /// </summary>
    float GetImpactStrength() const
    {
        return active_ && !respawned_ && timing_.stopSeconds > 0.0f
            ? 1.0f - (std::clamp)(elapsedSeconds_ / timing_.stopSeconds, 0.0f, 1.0f) : 0.0f;
    }

    /// <summary>
    /// 接触直後だけ一往復して減衰するカメラ反応の強さを取得する。
    /// </summary>
    float GetCameraKickStrength() const
    {
        const float strength = GetImpactStrength(); // 静止時間内で減衰する接触反応の強さ
        return strength * std::cos((1.0f - strength) * 6.28318531f);
    }

    /// <summary>
    /// 失敗直後だけ通常色の本体を二度暗く明滅させる明るさを取得する。
    /// </summary>
    float GetHitBrightness() const
    {
        if (!active_ || respawned_ || elapsedSeconds_ >= timing_.stopSeconds) {
            return 1.0f;
        }
        const float phase = elapsedSeconds_ / timing_.stopSeconds; // 静止時間内の明滅の進行率
        return 0.65f + 0.35f * std::cos(phase * 12.56637061f);
    }

    /// <summary>
    /// 静止後ののけぞりの進行率を取得し、復帰地点では姿勢を戻す。
    /// </summary>
    float GetRecoilProgress() const
    {
        return active_ && !respawned_ && !IsFall() && timing_.reactionSeconds > 0.0f
            ? (std::clamp)((elapsedSeconds_ - timing_.stopSeconds) / timing_.reactionSeconds, 0.0f, 1.0f) : 0.0f;
    }

    /// <summary>
    /// 失敗確定後に本体が落ち続ける表示用の経過時間を取得する。
    /// </summary>
    float GetFallMotionSeconds() const
    {
        return active_ && !respawned_ && IsFall()
            ? (std::min)(elapsedSeconds_, timing_.reactionSeconds + timing_.fadeOutSeconds) : 0.0f;
    }

    /// <summary>
    /// 暗転前に落下姿勢を崩す進行率を取得する。
    /// </summary>
    float GetFallReactionProgress() const
    {
        return active_ && !respawned_ && IsFall()
            ? (std::clamp)(elapsedSeconds_ / timing_.reactionSeconds, 0.0f, 1.0f) : 0.0f;
    }

private:
    Cause cause_ = Cause::Contact; // 演出開始時に固定する失敗原因
    Timing timing_ = kDetailedTiming; // 演出中に切り替わらない開始時の設定
    float elapsedSeconds_ = 0.0f; // 今回の失敗演出の経過時間
    bool active_ = false; // 失敗演出が進行中か
    bool respawned_ = false; // 暗転中の復帰通知を送信済みか
};
