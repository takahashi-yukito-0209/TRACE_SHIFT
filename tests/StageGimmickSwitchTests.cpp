#include "engine/3d/Object3d.h"
#include "application/gimmicks/StageGimmicks.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
/// <summary>
/// 条件を満たさない場合にテストの失敗理由を通知する。
/// </summary>
void Check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/// <summary>
/// 既定判定範囲の内外に置くプレイヤーまたは分身の状態を作成する。
/// </summary>
PlayerState CreateState(bool onSwitch)
{
    PlayerState state {}; // スイッチへ渡す入力状態
    state.transform.translate = { onSwitch ? 0.0f : 2.0f, 0.0f, 0.0f };
    return state;
}

/// <summary>
/// 本体と分身の全入力組み合わせ、入力の引き継ぎ、最後の退出での閉鎖を確認する。
/// </summary>
void TestMomentarySwitchAndDoor()
{
    BoxSwitchGimmick pressureSwitch; // 描画を作らず既定範囲で入力を検証するスイッチ
    LinkedDoorGimmick door; // スイッチ入力へ直接連動する扉
    for (bool playerOn : { false, true }) { // 本体の内外の組み合わせ
        for (bool cloneOn : { false, true }) { // 分身の内外の組み合わせ
            const std::array<PlayerState, 1> clones { CreateState(cloneOn) }; // 今回の分身入力
            pressureSwitch.Update(CreateState(playerOn), clones);
            door.Update(pressureSwitch.IsActive());
            Check(pressureSwitch.IsPlayerOnSwitch() == playerOn, "Player switch input is incorrect");
            Check(pressureSwitch.IsCloneOnSwitch() == cloneOn, "Clone switch input is incorrect");
            Check(pressureSwitch.IsActive() == (playerOn || cloneOn), "Either actor must activate the switch");
            Check(door.IsOpen() == (playerOn || cloneOn), "Door must follow the current switch input");
        }
    }
    pressureSwitch.Update(CreateState(true), {});
    door.Update(pressureSwitch.IsActive());
    Check(door.IsOpen(), "Player alone must open the door");
    const std::array<PlayerState, 1> holdingClone { CreateState(true) }; // 本体の役を引き継ぐ分身
    pressureSwitch.Update(CreateState(true), holdingClone);
    pressureSwitch.Update(CreateState(false), holdingClone);
    door.Update(pressureSwitch.IsActive());
    Check(door.IsOpen(), "Clone must keep the door open after the player leaves");
    pressureSwitch.Update(CreateState(false), {});
    door.Update(pressureSwitch.IsActive());
    Check(!door.IsOpen(), "Door must close after the last actor leaves");
    pressureSwitch.Update(CreateState(true), holdingClone);
    pressureSwitch.Reset();
    door.Update(pressureSwitch.IsActive());
    Check(!pressureSwitch.IsPlayerOnSwitch() && !pressureSwitch.IsCloneOnSwitch() && !door.IsOpen(),
        "Reset must clear switch inputs and close the linked door");
}

/// <summary>
/// 本体と複数分身の重量合算、必要重量の変更、不正設定、リセットを検証する。
/// </summary>
void TestWeightSwitch()
{
    WeightSwitchGimmick weightSwitch; // 描画を作らず重量判定を検証するスイッチ
    const std::array<PlayerState, 1> oneClone { CreateState(true) }; // 判定内の分身1体
    const std::array<PlayerState, 2> twoClones { CreateState(true), CreateState(true) }; // 判定内の分身2体
    const std::array<PlayerState, 3> mixedClones { CreateState(true), CreateState(false), CreateState(true) }; // 範囲外の分身も含む入力
    Check(weightSwitch.GetRequiredWeight() == 2.0f && weightSwitch.GetCurrentWeight() == 0.0f && !weightSwitch.IsActive(),
        "Default weight switch must require weight two and start inactive");
    weightSwitch.Update(CreateState(true), {});
    Check(weightSwitch.GetCurrentWeight() == 1.0f && !weightSwitch.IsActive(), "Player alone must be insufficient");
    weightSwitch.Update(CreateState(false), oneClone);
    Check(weightSwitch.GetCurrentWeight() == 1.0f && !weightSwitch.IsActive(), "One clone alone must be insufficient");
    weightSwitch.Update(CreateState(true), oneClone);
    Check(weightSwitch.GetCurrentWeight() == 2.0f && weightSwitch.IsActive(), "Player and clone must reach required weight");
    weightSwitch.Update(CreateState(false), twoClones);
    Check(weightSwitch.GetCurrentWeight() == 2.0f && weightSwitch.IsActive()
            && !weightSwitch.IsPlayerOnSwitch() && weightSwitch.IsCloneOnSwitch(),
        "Two clones must activate without the player");
    weightSwitch.Update(CreateState(true), twoClones);
    Check(weightSwitch.GetCurrentWeight() == 3.0f && weightSwitch.IsActive(), "Weight above the requirement must activate");
    weightSwitch.Update(CreateState(false), mixedClones);
    Check(weightSwitch.GetCurrentWeight() == 2.0f && weightSwitch.IsActive(), "Outside clones must not contribute weight");
    weightSwitch.Update(CreateState(false), oneClone);
    Check(weightSwitch.GetCurrentWeight() == 1.0f && !weightSwitch.IsActive(), "Leaving must deactivate below required weight");
    weightSwitch.Update(CreateState(false), {});
    Check(weightSwitch.GetCurrentWeight() == 0.0f && !weightSwitch.IsActive()
            && !weightSwitch.IsPlayerOnSwitch() && !weightSwitch.IsCloneOnSwitch(),
        "No visible actors must clear all weight inputs");

    weightSwitch.Update(CreateState(false), twoClones);
    Check(weightSwitch.SetRequiredWeight(3.0f) && !weightSwitch.IsActive(), "Changing required weight must reevaluate current input");
    weightSwitch.Update(CreateState(true), twoClones);
    Check(weightSwitch.IsActive(), "Three actors must meet requirement three");
    Check(weightSwitch.SetRequiredWeight(1.0f), "Positive weight setting must be accepted");
    weightSwitch.Update(CreateState(false), oneClone);
    Check(weightSwitch.IsActive(), "Requirement one must accept one clone");
    Check(weightSwitch.SetRequiredWeight(2.5f) && !weightSwitch.IsActive(), "Fractional requirement must use total weight comparison");
    weightSwitch.Update(CreateState(false), twoClones);
    Check(!weightSwitch.IsActive(), "Weight two must not meet requirement two point five");
    weightSwitch.Update(CreateState(true), twoClones);
    Check(weightSwitch.IsActive(), "Weight three must meet requirement two point five");
    for (float invalidWeight : { 0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) { // 拒否すべき設定値
        Check(!weightSwitch.SetRequiredWeight(invalidWeight), "Invalid required weight must be rejected");
        Check(weightSwitch.GetRequiredWeight() == 2.5f && weightSwitch.GetCurrentWeight() == 3.0f && weightSwitch.IsActive(),
            "Rejected settings must preserve existing configuration and state");
    }
    weightSwitch.Reset();
    Check(weightSwitch.GetRequiredWeight() == 2.5f && weightSwitch.GetCurrentWeight() == 0.0f
            && !weightSwitch.IsActive() && !weightSwitch.IsPlayerOnSwitch() && !weightSwitch.IsCloneOnSwitch(),
        "Reset must clear inputs but preserve required weight");
    WeightSwitchGimmick anotherSwitch; // 必要重量がインスタンスごとに独立していることを確認するスイッチ
    Check(anotherSwitch.GetRequiredWeight() == 2.0f, "Required weight must be independent per switch");
}
}

/// <summary>
/// 描画やDirectX初期化なしで通常スイッチ、扉、重さスイッチの判定を検証する。
/// </summary>
int main()
{
    try {
        TestMomentarySwitchAndDoor();
        TestWeightSwitch();
        std::cout << "Stage switch tests passed: input combinations, handoff, total weight, settings and reset\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
