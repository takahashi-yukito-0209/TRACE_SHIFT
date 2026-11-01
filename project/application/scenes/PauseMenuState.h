#pragma once

#include <cstddef>

/// <summary>
/// ポーズ画面の選択と確認を管理し、ゲーム側へ実行する操作だけを返す。
/// </summary>
class PauseMenuState {
public:
    enum class View { Closed, Menu, Controls, ConfirmRetry, ConfirmExit }; // 現在表示しているページ
    enum class Action { None, Resume, Retry, Exit }; // 確認後にシーンへ渡す操作
    inline static constexpr size_t kMenuItemCount = 4; // 続行・操作方法・やり直し・退出の項目数

    /// <summary>
    /// 最初の項目を選択してポーズを開く。
    /// </summary>
    void Open() { view_ = View::Menu; selection_ = 0; }

    /// <summary>
    /// ページと選択を初期状態へ戻して閉じる。
    /// </summary>
    void Close() { view_ = View::Closed; selection_ = 0; }

    /// <summary>
    /// メニューと確認画面の項目を上下へ循環移動する。
    /// </summary>
    void Move(int direction)
    {
        if (view_ == View::Closed || view_ == View::Controls || direction == 0) {
            return;
        }
        const size_t count = view_ == View::Menu ? kMenuItemCount : 2; // 現在ページの項目数
        selection_ = (selection_ + (direction < 0 ? count - 1 : 1)) % count;
    }

    /// <summary>
    /// 選択を決定し、やり直しと退出は確認画面で実行を選んだときだけ通知する。
    /// </summary>
    Action Confirm()
    {
        if (view_ == View::Controls) {
            Back();
        } else if (view_ == View::Menu) {
            if (selection_ == 0) {
                Close();
                return Action::Resume;
            }
            view_ = selection_ == 1 ? View::Controls : (selection_ == 2 ? View::ConfirmRetry : View::ConfirmExit);
            selection_ = 0;
        } else if (view_ == View::ConfirmRetry || view_ == View::ConfirmExit) {
            if (selection_ == 0) {
                Back();
            } else {
                const Action action = view_ == View::ConfirmRetry ? Action::Retry : Action::Exit; // 確認済みの操作
                Close();
                return action;
            }
        }
        return Action::None;
    }

    /// <summary>
    /// サブページから元の項目へ戻り、メニューではポーズを閉じる。
    /// </summary>
    void Back()
    {
        if (view_ == View::Menu) {
            Close();
        } else if (view_ != View::Closed) {
            selection_ = view_ == View::Controls ? 1 : (view_ == View::ConfirmRetry ? 2 : 3);
            view_ = View::Menu;
        }
    }

    /// <summary>
    /// ゲームの時間更新を停止する必要があるか取得する。
    /// </summary>
    bool IsOpen() const { return view_ != View::Closed; }

    /// <summary>
    /// 描画するページを取得する。
    /// </summary>
    View GetView() const { return view_; }

    /// <summary>
    /// 現在選択している項目番号を取得する。
    /// </summary>
    size_t GetSelection() const { return selection_; }

private:
    View view_ = View::Closed; // 現在のページ
    size_t selection_ = 0; // ページ内の選択番号。確認画面ではキャンセルが初期値
};
