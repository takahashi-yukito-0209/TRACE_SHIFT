#include "engine/3d/Object3d.h"
#include "application/player/Player.h"
#include "application/player/PastSelfClone.h"
#include "application/scenes/TraceShiftStageJson.h"
#include "application/scenes/ClonePlatformCourseState.h"
#include "application/scenes/StageStartSequenceState.h"
#include "application/scenes/StageFailureSequenceState.h"
#include "application/scenes/PauseMenuState.h"
#include "engine/particle/ParticleManager.h"
#include "engine/sound/Sound.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>

namespace {
/// <summary>
/// 条件を満たさない場合に検証を失敗させる。
/// </summary>
void Check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/// <summary>
/// 三つの課題の配置順、開始位置、到達床と再合流用の迂回路を確認する。
/// </summary>
float TestTrialLayout()
{
    std::ifstream file("project/resources/levels/trace_shift_stage.json"); // 実配置を読み取る入力
    nlohmann::json root; // 保存されている実ステージ
    file >> root;
    TraceShiftStageJson::StageData stage; // 検証済みの配置
    std::string error; // 読み込みの失敗理由
    Check(TraceShiftStageJson::Decode(root, stage, error), "Stage JSON failed validation");
    size_t trialBlockCount = 0; // 第一ステージで使う地形数
    std::vector<const TraceShiftStageJson::BlockData*> goals; // 保存順を維持した各区間の到達床
    const TraceShiftStageJson::BlockData* roof = nullptr; // 最後の課題で本体が通る迂回路
    for (const auto& block : stage.blocks) { // 保存配置の各ブロック
        if ((block.routeMask & 1u) == 0) {
            continue;
        }
        ++trialBlockCount;
        Check(block.name != "Clone Trial Recording Guide", "Recording position guide must be removed");
        if (block.name == "Course 03 Bypass Roof") {
            roof = &block;
        }
        if (block.oneCloneGoalPlatform) {
            Check(block.collidable, "Goal platform must support the player");
            goals.push_back(&block);
        }
    }
    Check(trialBlockCount == 9 && goals.size() == ClonePlatformCourseState::kSectionCount,
        "Course must contain nine blocks and exactly three ordered exits");
    const float jumpHeight = 6.5f * 6.5f / 36.0f; // 現行プレイヤーの理論ジャンプ上昇量
    for (size_t sectionIndex = 0; sectionIndex < goals.size(); ++sectionIndex) { // 各区間の配置番号
        const auto& goal = *goals[sectionIndex]; // 現在区間の到達床
        const float top = goal.translate.y + goal.scale.y * 0.5f; // 到達床上面
        const auto& start = ClonePlatformCourseState::kStartPositions[sectionIndex]; // 区間内の復帰位置
        Check(std::fabs(top - 2.65f) < 0.001f && top > 1.0f + jumpHeight && top < 1.0f + 2.0f * jumpHeight - 0.2f,
            "Each exit must keep the tested rising-clone height");
        Check(goal.translate.x > start.x && (sectionIndex == 0 || goal.translate.x > goals[sectionIndex - 1]->translate.x),
            "Course exit order must match the left-to-right section order");
        const bool startsOnFloor = std::any_of(stage.blocks.begin(), stage.blocks.end(), [&start](const auto& block) { // 復帰位置を支える床の判定
            return block.routeMask == 1 && block.collidable && !block.oneCloneGoalPlatform &&
                std::fabs(block.translate.y + block.scale.y * 0.5f - (start.y - 0.5f)) < 0.001f &&
                std::fabs(start.x - block.translate.x) + 0.5f < block.scale.x * 0.5f;
        });
        Check(startsOnFloor, "Every section must restart safely on its own floor");
    }
    Check(roof && roof->collidable, "Last section must include a bypass roof");
    const float roofBottom = roof->translate.y - roof->scale.y * 0.5f; // 下の通路高さ
    const float goalLeft = goals.back()->translate.x - goals.back()->scale.x * 0.5f; // 最後の到達床の左端
    const float roofRight = roof->translate.x + roof->scale.x * 0.5f; // 迂回路の右端
    Check(roofBottom > 1.0f && roofBottom < 2.0f, "Tunnel must fit one actor but not stacked actors");
    Check(goalLeft - roofRight > 3.0f * 2.0f * 6.5f / 18.0f + 1.0f, "Bypass must not allow a direct jump to exit");
    return goals.back()->translate.y + goals.back()->scale.y * 0.5f;
}

/// <summary>
/// 区間の順送り、最終区間の維持、コース再入場時の先頭復帰を確認する。
/// </summary>
void TestCourseProgression()
{
    ClonePlatformCourseState course; // 描画を持たない区間進捗
    for (size_t sectionIndex = 0; sectionIndex < ClonePlatformCourseState::kSectionCount; ++sectionIndex) { // 期待する区間番号
        Check(course.GetSectionIndex() == sectionIndex, "Course skipped or repeated a section");
        Check(course.GetStartPosition().x == ClonePlatformCourseState::kStartPositions[sectionIndex].x,
            "Checkpoint start must follow the current section");
        Check(course.TryAdvance() == (sectionIndex + 1 < ClonePlatformCourseState::kSectionCount),
            "Only the last section may finish the course");
    }
    Check(!course.TryAdvance() && course.GetSectionIndex() == 2, "Final section must not advance past available checkpoints");
    course.Reset();
    Check(course.GetSectionIndex() == 0 && course.GetStartPosition().x == ClonePlatformCourseState::kStartPositions[0].x,
        "Course reentry must restore the first section");
}

/// <summary>
/// 各区間で記録する待機・移動・ジャンプへの追従を複数の更新間隔で確認する。
/// </summary>
void TestRidingJump(float step, size_t sectionIndex, float targetTop)
{
    std::vector<PastSelfFrame> frames; // 60Hzで作成する待機とジャンプの記録
    constexpr float kJumpVelocity = 6.5f; // 現行プレイヤーと同じ初速
    constexpr float kGravity = -18.0f; // 現行プレイヤーと同じ重力
    constexpr std::array<float, 3> kRecordStarts { -4.8f, 14.6f, 38.6f }; // 各課題の記録開始位置
    constexpr std::array<float, 3> kRecordEnds { -4.8f, 19.2f, 46.7f }; // 各課題のジャンプ位置
    const float positionX = kRecordStarts[sectionIndex]; // 今回の記録開始位置
    const float distance = kRecordEnds[sectionIndex] - positionX; // 本体を運ぶ距離
    const float waitSeconds = 1.0f + distance / 3.0f + (sectionIndex == 2 ? 2.0f : 0.3f); // 再合流区間では出口でも待機する時刻
    for (int frameIndex = 0; frameIndex <= 420; ++frameIndex) { // 入口待機・通過・出口待機・着地までの記録番号
        const float time = static_cast<float>(frameIndex) / 60.0f; // 記録時刻
        const float jumpTime = (std::max)(time - waitSeconds, 0.0f); // 待機後のジャンプ経過時間
        const float height = (std::max)(kJumpVelocity * jumpTime + 0.5f * kGravity * jumpTime * jumpTime, 0.0f); // 床からの上昇量
        const float movement = (std::clamp)((time - 1.0f) * 3.0f, 0.0f, distance); // 記録した横移動の量
        PlayerState state; // 記録する分身の状態
        state.transform.translate = { positionX + movement, 0.5f + height, 0.0f };
        state.isGrounded = height == 0.0f;
        frames.push_back({ time, state });
    }
    PastSelfClone clone; // GPUを作らず再生する分身
    Check(clone.Start(frames), "Jump recording must start");
    Player player; // 分身に乗る本体
    PlayerState rider; // 分身上面から開始する本体の状態
    rider.transform.translate = { positionX, 1.5f, 0.0f };
    player.SetInitialState(rider);
    float highestFoot = 0.0f; // 分身に運ばれた本体の最高足元位置
    for (float time = 0.0f; time < 7.5f; time += step) { // 再生終了後まで確認する時刻
        const StandablePlatform previous = clone.GetStandablePlatform(); // 移動前の分身上面
        const std::vector<StandablePlatform> playerPlatform { player.GetStandablePlatform() }; // 本番と同じ分身側の着地対象
        clone.Update(step, playerPlatform);
        const StandablePlatform current = clone.GetStandablePlatform(); // 移動後の分身上面
        Check(player.ResolveMovingStandablePlatform(previous, current), "Rider lost the moving clone");
        player.Update(step, false, {}, { current });
        const float foot = player.GetState().transform.translate.y - 0.5f; // 更新後の本体足元
        Check(player.GetState().isGrounded && std::fabs(foot - current.center.y - current.halfSize.y) < 0.001f,
            "Rider must stay on the clone during ascent and descent");
        Check(std::fabs(player.GetState().transform.translate.x - current.center.x) < 0.001f,
            "Rider must follow horizontal movement without steering");
        highestFoot = (std::max)(highestFoot, foot);
    }
    const float jumpHeight = kJumpVelocity * kJumpVelocity / (-2.0f * kGravity); // 通常ジャンプの理論上昇量
    Check(jumpHeight < targetTop && 1.0f + jumpHeight < targetTop, "Ordinary jump and stationary clone must not reach next platform");
    Check(highestFoot < targetTop && highestFoot + jumpHeight > targetTop + 0.2f,
        "Rising clone must provide enough height for a second jump");
    Check(clone.IsVisible() && !clone.IsPlaying(), "Finished clone must stay visible");
    Check(std::fabs(player.GetState().transform.translate.x - kRecordEnds[sectionIndex]) < 0.001f,
        "Clone must transport rider to its recorded endpoint");
}

/// <summary>
/// 空中・側面・非表示の分身に本体が吸着せず、到達床には通常着地できることを確認する。
/// </summary>
void TestContactAndLanding(float goalTop)
{
    const StandablePlatform previous { { 0.0f, 0.5f, 0.0f }, { 0.5f, 0.5f, 0.5f }, true }; // 移動前の足場
    const StandablePlatform current { { 0.2f, 0.7f, 0.0f }, { 0.5f, 0.5f, 0.5f }, true }; // 移動後の足場
    Player player; // 接触条件を調べる本体
    for (const PlayerState state : { PlayerState {}, PlayerState { { { 1, 1, 1 }, {}, { 0, 1.5f, 0 } }, false, false } }) { // 側面と空中の入力
        player.SetInitialState(state);
        Check(!player.ResolveMovingStandablePlatform(previous, current), "Side or airborne actor must not be carried");
        Check(player.GetState().transform.translate.y == state.transform.translate.y, "Rejected contact changed the actor");
    }
    PlayerState rider; // 分身の上に立つ本体
    rider.transform.translate = { 0.0f, 1.5f, 0.0f };
    player.SetInitialState(rider);
    StandablePlatform hidden = current; // 再生停止で無効になる足場
    hidden.enabled = false;
    Check(!player.ResolveMovingStandablePlatform(previous, hidden), "Hidden clone must not carry the actor");
    const SolidCollider goal { { 50.0f, goalTop - 0.15f, 0.0f }, { 2.0f, 0.15f, 2.0f }, true }; // 実配置と同じ到達床
    PlayerState falling; // 到達床の上から落下する本体
    falling.transform.translate = { 50.0f, goalTop + 0.9f, 0.0f };
    falling.isGrounded = false;
    player.SetInitialState(falling);
    for (int frameIndex = 0; frameIndex < 60; ++frameIndex) { // 着地まで進める更新番号
        player.Update(1.0f / 60.0f, false, { goal }, {});
    }
    Check(player.GetState().isGrounded && std::fabs(player.GetState().transform.translate.y - 0.5f - goalTop) < 0.001f,
        "Player must land on the high goal platform");
}

/// <summary>
/// 第三区間の出口で待つ分身へ乗り直し、後続ジャンプに追従することを確認する。
/// </summary>
void TestRejoinAtExit()
{
    std::ifstream file("project/resources/levels/trace_shift_stage.json"); // 実ステージの地形入力
    nlohmann::json root; // 保存配置
    file >> root;
    TraceShiftStageJson::StageData stage; // 検証済みの地形
    std::string error; // 読み込み結果
    Check(TraceShiftStageJson::Decode(root, stage, error), "Rejoin stage failed validation");
    std::vector<SolidCollider> colliders; // 第一ステージの全面衝突地形
    for (const auto& block : stage.blocks) { // 本番と同じ配置から作成する地形
        if ((block.routeMask & 1u) != 0 && block.collidable) {
            colliders.push_back({ block.translate, { block.scale.x * 0.5f, block.scale.y * 0.5f, block.scale.z * 0.5f }, true });
        }
    }
    std::vector<PastSelfFrame> frames; // 出口待機とその後のジャンプ記録
    for (int frameIndex = 0; frameIndex <= 180; ++frameIndex) { // 60Hzで3秒間の記録を作る番号
        const float time = static_cast<float>(frameIndex) / 60.0f; // 記録時刻
        const float jumpTime = (std::max)(time - 2.0f, 0.0f); // 接地確認用の待機後の経過時間
        const float height = (std::max)(6.5f * jumpTime - 9.0f * jumpTime * jumpTime, 0.0f); // 記録ジャンプの高さ
        PlayerState state; // 出口に置く分身の状態
        state.transform.translate = { 46.7f, 0.5f + height, 0.0f };
        state.isGrounded = height == 0.0f;
        frames.push_back({ time, state });
    }
    PastSelfClone clone; // 出口で待つ分身
    Check(clone.Start(frames), "Exit recording must start");
    Player player; // 上の経路から降りてくる本体
    PlayerState airborne; // 再合流直前の空中状態
    airborne.transform.translate = { 46.7f, 2.4f, 0.0f };
    airborne.isGrounded = false;
    player.SetInitialState(airborne);
    bool landedOnClone = false; // 再合流が成立したか
    float highestFootAfterLanding = 0.0f; // 再合流後に分身で持ち上がった足元高さ
    for (int frameIndex = 0; frameIndex < 210; ++frameIndex) { // 再合流と後続ジャンプまで進める番号
        const StandablePlatform previous = clone.GetStandablePlatform(); // 更新前の分身足場
        clone.Update(1.0f / 60.0f, { player.GetStandablePlatform() });
        const StandablePlatform current = clone.GetStandablePlatform(); // 更新後の分身足場
        player.ResolveMovingStandablePlatform(previous, current);
        const SolidCollider cloneCollider { current.center, current.halfSize, current.enabled }; // 本番と同じ外部移動後の衝突解決用上面
        player.ResolveExternalSolidCollisions(colliders, cloneCollider);
        player.Update(1.0f / 60.0f, false, colliders, { current });
        const float foot = player.GetState().transform.translate.y - 0.5f; // 更新後の本体足元
        if (player.GetState().isGrounded && std::fabs(foot - current.center.y - current.halfSize.y) < 0.001f) {
            landedOnClone = true;
        }
        if (landedOnClone) {
            highestFootAfterLanding = (std::max)(highestFootAfterLanding, foot);
        }
    }
    Check(landedOnClone && highestFootAfterLanding > 1.9f, "Actor must rejoin the waiting clone and ride its next jump");
}

/// <summary>
/// 地上・空中とも終端位置で薄くなり、消失時に足場もなくなることを確認する。
/// </summary>
void TestCloneFinishFade(bool grounded)
{
    PlayerState state; // 地上または空中で記録を終える分身
    state.transform.translate = { 0.0f, grounded ? 0.5f : 3.0f, 0.0f };
    state.isGrounded = grounded;
    state.isMoving = true;
    const std::vector<PastSelfFrame> frames { { 0.0f, state }, { 0.2f, state } }; // 終端までの短い記録
    PastSelfClone clone; // デバイスなしで寿命と判定を調べる分身
    Check(clone.Start(frames), "Both grounded and airborne recordings must remain playable");
    clone.Update(0.1f, {});
    Check(clone.IsPlaying() && clone.GetFadeProgress() == 0.0f, "Replay must not fade before its endpoint");
    clone.Update(0.1f, {});
    Check(!clone.IsPlaying() && clone.IsVisible() && clone.GetStandablePlatform().enabled,
        "Finished clone must remain visible and standable during its grace period");
    clone.Update(0.25f, {});
    Check(std::fabs(clone.GetFadeProgress() - 0.25f) < 0.001f &&
        clone.GetCurrentState().transform.translate.y == state.transform.translate.y && !clone.GetCurrentState().isMoving,
        "Finished clone must fade in place instead of falling or continuing movement");
    Player rider; // 残像の上に立つ本体
    PlayerState riderState; // 分身の上面に足元を合わせた本体状態
    riderState.transform.translate = { 0.0f, state.transform.translate.y + 1.0f, 0.0f };
    rider.SetInitialState(riderState);
    rider.Update(1.0f / 60.0f, false, {}, { clone.GetStandablePlatform() });
    Check(rider.GetState().isGrounded, "Fading clone must still support its rider");
    clone.Update(0.0f, {});
    Check(std::fabs(clone.GetFadeProgress() - 0.25f) < 0.001f, "Paused time must not consume fade duration");
    clone.Update(0.74f, {});
    Check(clone.IsVisible() && clone.GetStandablePlatform().enabled && clone.GetFadeProgress() > 0.98f,
        "Platform and visible actor must remain active until the grace period expires");
    clone.Update(0.02f, {});
    Check(!clone.IsVisible() && !clone.GetStandablePlatform().enabled && clone.GetFadeProgress() == 1.0f,
        "Visibility and platform must disappear together at expiry");
    rider.Update(1.0f / 60.0f, false, {}, { clone.GetStandablePlatform() });
    Check(!rider.GetState().isGrounded && rider.GetState().transform.translate.y < riderState.transform.translate.y,
        "Rider must fall when the clone disappears");
    Check(clone.Start() && clone.IsVisible() && clone.GetFadeProgress() == 0.0f,
        "Replay must preserve the record and reset its fade state");
    clone.Update(0.5f, {});
    Check(std::fabs(clone.GetFadeProgress() - 0.3f) < 0.001f, "Frame crossing endpoint must apply only leftover time to fade");
    clone.Pause();
    clone.Update(2.0f, {});
    Check(clone.IsVisible() && std::fabs(clone.GetFadeProgress() - 0.3f) < 0.001f,
        "Explicit pause must freeze a fading clone too");
    clone.Start();
    clone.Update(2.0f, {});
    Check(!clone.IsVisible(), "Long frame must expire the clone without leaving a platform behind");
    Check(clone.Start() && clone.GetFadeProgress() == 0.0f, "Expired clone must be replayable again");
    clone.Stop();
    Check(!clone.IsVisible() && !clone.GetStandablePlatform().enabled, "Stop must hide immediately without a grace period");
}

/// <summary>
/// フレームレートが違っても終端後に約1秒だけ残り、消失まで単調に薄くなることを確認する。
/// </summary>
void TestCloneFadeTiming(float step)
{
    const std::vector<PastSelfFrame> frames { { 0.0f, PlayerState {} }, { 0.2f, PlayerState {} } }; // 時刻境界だけを確認する記録
    PastSelfClone clone; // 更新間隔ごとの消失時刻を調べる分身
    clone.Start(frames);
    clone.Update(0.2f, {});
    float previousProgress = 0.0f; // フェードの逆行を検出する前回値
    float elapsed = 0.0f; // 終端後に更新した時間
    while (elapsed < 1.0f + step * 2.0f) {
        clone.Update(step, {});
        elapsed += step;
        Check(clone.GetFadeProgress() >= previousProgress, "Fade progress must be monotonic");
        previousProgress = clone.GetFadeProgress();
        if (elapsed < 1.0f - step) {
            Check(clone.IsVisible() && clone.GetStandablePlatform().enabled, "Clone must not disappear before its grace period");
        }
    }
    Check(!clone.IsVisible() && !clone.GetStandablePlatform().enabled, "Fade must finish near one second at every frame rate");
}
/// <summary>
/// 初回とリトライの待機時間、操作解禁の単発通知、長いフレームでの光の保持を確認する。
/// </summary>
void TestStageStartSequence()
{
    StageStartSequenceState sequence; // 描画・音声・入力で共有する演出状態
    Check(!sequence.IsHolding() && !sequence.IsPulseVisible(), "Unstarted sequence must not block input or draw a ring");
    sequence.Begin(false);
    Check(sequence.IsHolding() && sequence.IsArrivalVisible() && sequence.GetIntroAlpha() == 0.0f &&
        sequence.GetArrivalProgress() == 0.0f && sequence.GetArrivalDimAlpha() > 0.3f,
        "Initial sequence must begin with a dim arrival before the theme");
    Check(!sequence.Update(1.0f) && sequence.GetGoalGlow() > 0.7f, "Goal must brighten during the initial hold");
    Check(sequence.GetIntroAlpha() == 1.0f && std::fabs(sequence.GetArrivalProgress() - 0.5f) < 0.001f &&
        sequence.GetArrivalDimAlpha() > 0.0f && sequence.GetArrivalDimAlpha() < 0.2f,
        "Theme must appear during convergence while the screen returns to normal brightness");
    Check(!sequence.Update(0.8f) && sequence.GetIntroAlpha() > 0.0f && sequence.GetIntroAlpha() < 1.0f,
        "Theme must fade before input is released");
    Check(sequence.Update(0.21f) && !sequence.IsHolding() && sequence.IsPulseVisible() && sequence.GetPulseProgress() == 0.0f,
        "Release event, input unlock and ring onset must occur on the same update");
    Check(!sequence.IsArrivalVisible() && sequence.GetArrivalDimAlpha() == 0.0f && sequence.GetArrivalProgress() == 1.0f,
        "Arrival must finish completely before normal play");
    Check(!sequence.Update(0.2f) && sequence.GetPulseProgress() > 0.0f, "Start event must not repeat during ring expansion");
    Check(!sequence.Update(1.0f) && !sequence.IsPulseVisible() && sequence.GetGoalGlow() == 0.0f,
        "Ring and goal glow must finish without retriggering the start event");
    sequence.Begin(true);
    Check(!sequence.IsArrivalVisible() && sequence.GetArrivalDimAlpha() == 0.0f,
        "Retry must omit convergence and full screen dimming");
    Check(sequence.GetIntroAlpha() == 0.0f && !sequence.Update(0.34f), "Retry must omit the theme and retain only a short hold");
    Check(sequence.Update(0.02f) && sequence.IsPulseVisible(), "Retry must unlock near 0.35 seconds with the same start cue");
    sequence.Begin(false);
    Check(!sequence.Update(-1.0f) && !sequence.Update(std::numeric_limits<float>::quiet_NaN()) && sequence.IsHolding(),
        "Invalid time steps must not release input");
    Check(sequence.Update(20.0f) && sequence.IsPulseVisible(), "Long frames must not skip the release ring");
    Check(!sequence.Update(20.0f), "Long frames must emit only one start event");
    for (float step : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f }) { // 異なるフレームレートでの更新間隔
        sequence.Begin(false);
        int releaseCount = 0; // 操作解禁通知の回数
        for (float time = 0.0f; time < 3.0f; time += step) { // 演出が終わるまでの時刻
            releaseCount += sequence.Update(step) ? 1 : 0;
        }
        Check(releaseCount == 1 && !sequence.IsHolding() && !sequence.IsPulseVisible(),
            "Every frame rate must produce exactly one release event and finish the effect");
    }
}

/// <summary>
/// 開始カメラが解禁前に通常距離へ戻り、リトライや停止中には動かないことを確認する。
/// </summary>
void TestStageStartCamera()
{
    StageStartSequenceState sequence; // 開始演出とカメラで共有する時刻
    Check(sequence.GetCameraDistanceScale() == 1.0f, "Inactive sequence must preserve the gameplay camera");
    sequence.Begin(false);
    Check(std::fabs(sequence.GetCameraDistanceScale() - 1.08f) < 0.0001f, "Initial camera must start eight percent farther away");
    sequence.Update(0.8f);
    const float pausedScale = sequence.GetCameraDistanceScale(); // 停止直前の画角
    sequence.Update(0.0f);
    Check(sequence.GetCameraDistanceScale() == pausedScale, "Paused time must preserve the current camera distance");
    sequence.Update(0.86f);
    Check(sequence.IsHolding() && sequence.GetCameraDistanceScale() == 1.0f,
        "Camera must settle at the normal distance before input unlocks");
    sequence.Update(1.0f);
    Check(!sequence.IsHolding() && sequence.GetCameraDistanceScale() == 1.0f, "Release must not leave a camera offset");
    sequence.Begin(true);
    Check(sequence.GetCameraDistanceScale() == 1.0f, "Retry must omit camera movement");
    sequence.Update(0.2f);
    Check(sequence.GetCameraDistanceScale() == 1.0f, "Retry hold must retain the gameplay camera");
    for (float step : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f }) { // フレームレートごとの確認間隔
        sequence.Begin(false);
        float previousScale = sequence.GetCameraDistanceScale(); // 逆向きの移動と行き過ぎを検出する前回値
        while (sequence.IsHolding()) {
            sequence.Update(step);
            const float scale = sequence.GetCameraDistanceScale(); // 今回の距離倍率
            Check(scale >= 1.0f && scale <= previousScale, "Camera must approach without overshoot at every frame rate");
            previousScale = scale;
        }
        Check(previousScale == 1.0f, "Every frame rate must restore normal distance before gameplay");
    }
}

/// <summary>
/// 失敗の暗転中に一度だけ復帰し、リトライと明転が同時に終わることを確認する。
/// </summary>
void TestStageFailureSequence()
{
    StageFailureSequenceState failure; // 失敗の表示・復帰と入力停止で共有する状態
    Check(!failure.IsActive() && failure.GetDimAlpha() == 0.0f, "Inactive failure must not darken the game");
    failure.Begin();
    failure.Update(0.05f);
    Check(failure.IsActive() && failure.GetBodyAlpha() == 1.0f && failure.GetDimAlpha() == 0.0f,
        "Failure must begin with a short readable stop");
    failure.Update(0.2f);
    Check(failure.GetBodyAlpha() > 0.0f && failure.GetBodyAlpha() < 1.0f && failure.GetDimAlpha() > 0.0f,
        "Body disappearance and screen dimming must progress together");
    Check(failure.Update(2.0f) && failure.HasRespawned() && failure.IsActive() && failure.GetDimAlpha() == 1.0f,
        "Even a long frame must reset behind a fully black frame");
    Check(!failure.Update(2.0f) && !failure.IsActive() && failure.GetDimAlpha() == 0.0f,
        "Failure must finish without a duplicate respawn event");
    for (float step : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f }) { // フレームレートごとの演出進行間隔
        failure.Begin();
        StageStartSequenceState retry; // 暗転中の復帰から始める既存の短い開始演出
        int respawnCount = 0; // 現在区間へ戻す回数
        int startCount = 0; // 操作再開の開始合図の回数
        while (failure.IsActive()) {
            const bool wasRespawned = failure.HasRespawned(); // 復帰と同じフレームでリトライ時間を消費しないための状態
            if (failure.Update(step)) {
                ++respawnCount;
                retry.Begin(true);
                Check(failure.GetDimAlpha() == 1.0f, "Respawn must always happen behind black");
            } else if (wasRespawned) {
                startCount += retry.Update(step) ? 1 : 0;
            }
        }
        Check(respawnCount == 1 && startCount == 1 && !retry.IsHolding(),
            "Every frame rate must reset once and finish recovery with the short retry cue");
    }
}

/// <summary>
/// ポーズの操作説明・キャンセル優先の確認・戻り先と確定操作を確認する。
/// </summary>
void TestPauseMenu()
{
    PauseMenuState menu; // 入力と描画に依存しないポーズ状態
    Check(!menu.IsOpen() && menu.Confirm() == PauseMenuState::Action::None, "Closed menu must not execute an action");
    menu.Open();
    Check(menu.IsOpen() && menu.GetSelection() == 0, "Opening must select resume");
    menu.Move(-1);
    Check(menu.GetSelection() == 3, "Selection must wrap to exit");
    Check(menu.Confirm() == PauseMenuState::Action::None && menu.GetView() == PauseMenuState::View::ConfirmExit && menu.GetSelection() == 0,
        "Exit must open confirmation with cancel selected");
    Check(menu.Confirm() == PauseMenuState::Action::None && menu.GetView() == PauseMenuState::View::Menu && menu.GetSelection() == 3,
        "Repeated confirm must cancel exit and restore selection");
    menu.Move(-1);
    menu.Confirm();
    Check(menu.GetView() == PauseMenuState::View::ConfirmRetry && menu.GetSelection() == 0, "Retry must require confirmation");
    menu.Back();
    Check(menu.GetSelection() == 2, "Cancel must return to retry without executing it");
    menu.Confirm();
    menu.Move(1);
    Check(menu.Confirm() == PauseMenuState::Action::Retry && !menu.IsOpen(), "Retry must execute only after explicit confirmation");
    menu.Open();
    menu.Move(1);
    menu.Confirm();
    Check(menu.IsOpen() && menu.GetView() == PauseMenuState::View::Controls, "Controls must keep gameplay paused");
    menu.Move(1);
    Check(menu.GetSelection() == 0, "Controls page must ignore menu navigation");
    menu.Confirm();
    Check(menu.GetView() == PauseMenuState::View::Menu && menu.GetSelection() == 1, "Controls must return to the original item");
    menu.Move(-1);
    Check(menu.Confirm() == PauseMenuState::Action::Resume && !menu.IsOpen(), "Resume must close the menu");
    menu.Open();
    menu.Move(-1);
    menu.Confirm();
    menu.Move(1);
    Check(menu.Confirm() == PauseMenuState::Action::Exit && !menu.IsOpen(), "Confirmed exit must return exactly one action");
    Check(menu.Confirm() == PauseMenuState::Action::None, "Closed menu must not repeat the exit action");
}

/// <summary>
/// 停止時間のGPU発生抑制と、保留した手動発生要求が再開後に実行されることを確認する。
/// </summary>
void TestPausedParticleEmission()
{
    auto* manager = MyEngine::ParticleManager::GetInstance(); // デバイスを初期化せず時間と発生フラグだけを調べる管理
    auto* emitter = manager->GetMutableGpuEmitterState(); // 停止中の発生条件を作る設定
    emitter->count = 1;
    emitter->frequency = 0.0f;
    emitter->frequencyTime = 0.25f;
    emitter->emit = 1;
    manager->RequestGpuEmitterEmit();
    manager->Update(0.0f);
    Check(emitter->emit == 0 && emitter->frequencyTime == 0.25f, "Paused particle frame must not emit or advance time");
    manager->Update(0.0f);
    Check(emitter->emit == 0, "Zero-frequency emitter must not emit repeatedly while paused");
    manager->Update(0.01f);
    Check(emitter->emit == 1 && emitter->frequencyTime == 0.25f, "Pending manual emission must survive pause until resumed");
}

/// <summary>
/// 音声デバイスなしで、開始チャイムを既存のWAV読み込み処理へ通す。
/// </summary>
void TestStageStartSound()
{
    MyEngine::SoundSystem sound; // 初期化せずWAV読み込みだけに使う音声管理
    const auto clip = sound.LoadFromFile("project/resources/sounds/se/stage_start.wav"); // 生成済みの開始音
    Check(clip && clip->IsValid(), "Start chime must load through the existing sound system");
    Check(clip->wfex.wFormatTag == WAVE_FORMAT_PCM && clip->wfex.nChannels == 1 &&
        clip->wfex.nSamplesPerSec == 48000 && clip->wfex.wBitsPerSample == 16 && clip->GetSize() == 23040,
        "Start chime must be a 0.24 second mono PCM clip");
    const auto failureClip = sound.LoadFromFile("project/resources/sounds/se/stage_failure.wav"); // 開始音と同じ音量規約で使う下降音
    Check(failureClip && failureClip->IsValid() && failureClip->wfex.wFormatTag == WAVE_FORMAT_PCM &&
        failureClip->wfex.nChannels == 1 && failureClip->wfex.nSamplesPerSec == 48000 &&
        failureClip->wfex.wBitsPerSample == 16 && failureClip->GetSize() == 23040,
        "Failure chime must load as a matching 0.24 second PCM clip");
}
}

/// <summary>
/// DirectXを初期化せず、検証部屋と開始演出の時間管理・音声読み込みを確認する。
/// </summary>
int main()
{
    try {
        const float goalTop = TestTrialLayout(); // 保存配置から得た到達高さ
        TestCourseProgression();
        for (float step : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 120.0f }) { // 検証する更新間隔
            for (size_t sectionIndex = 0; sectionIndex < ClonePlatformCourseState::kSectionCount; ++sectionIndex) { // 検証する課題番号
                TestRidingJump(step, sectionIndex, goalTop);
            }
        }
        TestContactAndLanding(goalTop);
        TestRejoinAtExit();
        for (float step : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 120.0f }) { // 終端後の消失時刻を確認する更新間隔
            TestCloneFadeTiming(step);
        }
        TestCloneFinishFade(false);
        TestCloneFinishFade(true);
        TestStageStartSequence();
        TestStageStartCamera();
        TestStageFailureSequence();
        TestStageStartSound();
        TestPauseMenu();
        TestPausedParticleEmission();
        std::cout << "Clone platform course tests passed: section progression, layout, riding at 30/60/120Hz, contact and rejoin\n";
        std::cout << "Stage start tests passed: synchronized release, short retry, long frames and PCM loading\n";
        std::cout << "Pause tests passed: controls, safe confirmation, resume and paused GPU emission\n";
        std::cout << "Clone finish tests passed: fixed endpoint, fade lifetime, platform expiry, pause and replay\n";
        std::cout << "Failure tests passed: single respawn, black reset, short recovery and PCM loading\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
