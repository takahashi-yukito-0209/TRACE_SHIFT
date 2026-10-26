#pragma once

/// <summary>
/// シーンごとに所有するスプライトの生成・削除選択。
/// </summary>
struct PlaySceneObjectEditorState {
    int selectedCreateTextureIndex = 0; // 生成に使用するテクスチャ番号
    int selectedDeleteSpriteIndex = 0; // 削除対象のスプライト番号
};
