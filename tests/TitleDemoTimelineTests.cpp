#include "../project/application/scenes/TitleScene.cpp"
#include <stdexcept>

namespace {
/// <summary>
/// デモの条件を確認し、不成立なら理由を通知する。
/// </summary>
void CheckDemo(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/// <summary>
/// 実装と同じ姿勢生成処理で単独試行と役割分担後の通過を検証する。
/// </summary>
void TestDemoTimeline()
{
    constexpr float kStopX = kDoorX - 0.35f * 0.5f - 0.9f * 0.5f; // 現在の扉幅と本体幅から求める接触位置
    const PlayerState cloneStart = CreateDemoRecordState(0.0f); // 分身が現れる位置の状態
    const PlayerState onSwitch = CreateDemoPlayerState(kAttemptSwitchArrival, cloneStart, kStopX); // 最初の踏み込み完了状態
    CheckDemo(std::fabs(onSwitch.transform.translate.x - kSwitchX) < 0.001f,
        "Solo attempt must reach the switch");
    const PlayerState checkingSwitch = CreateDemoPlayerState(kAttemptSwitchApproachEnd + 0.1f, cloneStart, kStopX); // スイッチ手前で位置を確認する状態
    CheckDemo(!checkingSwitch.isMoving && checkingSwitch.transform.translate.x > kSwitchX + 0.1f,
        "Player must release movement before adjusting onto the switch");
    CheckDemo(kAttemptLeaveSwitch > kAttemptSwitchArrival + (kDoorEnd - kDoorBegin),
        "Solo attempt must wait long enough to show the open door");
    for (float time = 0.0f; time < kRecordBegin; time += 1.0f / 60.0f) { // 単独試行全体を60Hzで調べる時刻
        const PlayerState state = CreateDemoPlayerState(time, cloneStart, kStopX); // 検証時刻の本体状態
        CheckDemo(state.transform.translate.x <= kStopX + 0.001f,
            "Solo attempt must never cross the closed door");
    }
    const PlayerState stopped = CreateDemoPlayerState(kAttemptStepOffEnd + 0.05f, cloneStart, kStopX); // スイッチを離れた直後に止まっている状態
    CheckDemo(std::fabs(stopped.transform.translate.x - kSwitchX) > 0.1f && !stopped.isMoving,
        "Player must stop just outside the switch after the door closes");
    for (float time = kAttemptStepOffEnd; time < kAttemptReturnBegin; time += 1.0f / 60.0f) { // 閉鎖を確認する停止区間の検証時刻
        const PlayerState state = CreateDemoPlayerState(time, cloneStart, kStopX); // 停止区間内の本体状態
        CheckDemo(!state.isMoving && std::fabs(state.transform.translate.x - stopped.transform.translate.x) < 0.001f,
            "Player must not continue toward the closed door during the confirmation pause");
    }
    const PlayerState returned = CreateDemoPlayerState(kAttemptReturnEnd + 0.1f, cloneStart, kStopX); // 開始位置へ戻った状態
    CheckDemo(std::fabs(returned.transform.translate.x - kStartX) < 0.001f && !returned.isMoving,
        "Player must return continuously to the recording start");
    const PlayerState recordStart = CreateDemoPlayerState(kRecordBegin, cloneStart, kStopX); // 記録開始時の状態
    CheckDemo(std::fabs(returned.transform.translate.x - recordStart.transform.translate.x) < 0.001f,
        "Recording must not teleport the player");
    const PlayerState cloneHolding = CreateDemoRecordState(kRecordEnd - kRecordBegin); // 記録の終端でスイッチを押し続ける分身
    CheckDemo(std::fabs(cloneHolding.transform.translate.x - kSwitchX) < 0.001f,
        "Recorded clone must end on the switch");
    const PlayerState checkingDoor = CreateDemoPlayerState(kCrossCheckBegin + 0.08f, cloneHolding, kStopX); // 成功時に開いた扉を確認している状態
    CheckDemo(!checkingDoor.isMoving && checkingDoor.transform.translate.x < kStopX,
        "Player must briefly check the open door before passing");
    const PlayerState goal = CreateDemoPlayerState(kCrossEnd, cloneHolding, kStopX); // 役割分担後にゴールへ到達した状態
    CheckDemo(std::fabs(goal.transform.translate.x - kGoalX) < 0.001f,
        "Player must reach the goal while the clone holds the switch");
    CheckDemo(kColorRestoreTime < kCloneMoveBegin && kGoalHopEnd < kDemoDuration - 0.4f,
        "Color restoration and goal response must fit before the loop fade");
    float previousRate = 0.0f; // 入力風の移動率が逆行しないことを確認する前回値
    for (int sample = 0; sample <= 1000; ++sample) { // 加速と急停止の境界を含む移動率の検証点
        const float rate = DemoTravelProgress(static_cast<float>(sample) / 1000.0f, 0.0f, 1.0f); // 正規化した移動区間の進行率
        CheckDemo(rate >= previousRate && rate >= 0.0f && rate <= 1.0f,
            "Input-like acceleration must remain monotonic and within endpoints");
        previousRate = rate;
    }
}

/// <summary>
/// 記録した待機で扉通過まで分身が残り、その後に自然消失することを確認する。
/// </summary>
void TestDemoCloneFade()
{
    PastSelfRecorder recorder; // タイトルと同じ経路・待機を作る記録
    constexpr float step = 1.0f / 60.0f; // タイトル用記録の更新間隔
    const int samples = static_cast<int>(std::ceil((kRecordEnd - kRecordBegin) / step)); // 待機を含めた記録区間数
    recorder.Start();
    recorder.Update(0.0f, CreateDemoRecordState(0.0f));
    for (int sample = 1; sample <= samples; ++sample) { // タイトルデモと同じ記録時刻
        recorder.Update(step, CreateDemoRecordState(static_cast<float>(sample) * step));
    }
    recorder.Stop();
    PastSelfClone clone; // デバイスなしでデモ分身の寿命を確認する本体
    CheckDemo(clone.Start(recorder.GetFrames()), "Demo record must include the switch hold");
    clone.Update(kCrossEnd - kCloneMoveBegin, {});
    CheckDemo(clone.IsVisible() && std::fabs(clone.GetCurrentState().transform.translate.x - kSwitchX) < 0.001f,
        "Demo clone must keep holding the switch until the player reaches the goal");
    clone.Update(1.2f, {});
    CheckDemo(!clone.IsVisible(), "Demo clone must fade after its recorded hold ends");
    CheckDemo(CalculateDemoCloneDeltaTime(kReplayBegin, kColorRestoreTime) == 0.0f,
        "Clone appearance wait must not consume playback or fade time");
    CheckDemo(std::fabs(CalculateDemoCloneDeltaTime(kCrossEnd + 0.2f, kCrossEnd + 0.3f) - 0.1f) < 0.001f,
        "Finished demo clone must receive frame delta instead of accumulated time");
    CheckDemo(CalculateDemoCloneDeltaTime(kDemoDuration - 0.1f, 0.1f) == 0.0f,
        "Loop reset must not leak previous playback time into the next clone");
}
}

/// <summary>
/// ゲーム画面を起動せず、タイトルの攻略デモの位置と区間順序を確認する。
/// </summary>
int main()
{
    try {
        TestDemoTimeline();
        TestDemoCloneFade();
        std::cout << "Title demo tests passed: stop on release, confirmation pause, preparation, clone role and goal\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
