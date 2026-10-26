# 素材の再生成

ゲーム本体で使用している文字画像と開始・落下音の生成元です。通常のビルドからは実行しません。生成すると `project/resources` の対応するPNG/WAVを上書きするため、変更後は画像・音声とGit差分を確認してください。

Windows PowerShellから、必要な処理だけ実行します。

```powershell
.\tools\GenerateTitleMenuTextures.ps1
.\tools\GenerateStageSelectTextures.ps1
.\tools\GeneratePauseMenuTextures.ps1
python .\tools\GenerateStageStartSound.py
python .\tools\GenerateStageFailureSound.py
```

画像生成はWindowsのSystem.Drawingを使用します。音声生成はPython 3の標準ライブラリを使用します。スクリプトの場所を基準に保存先を解決します。

文字画像の再生成に使うRajdhaniとZen Kaku Gothic New、およびステージ選択用のFira Monoは `project/resources/fonts` にあります。各フォントのOFL文書を同じ場所に保持します。

`font-comparison` は個人用の比較資料で、このリポジトリの共有対象には含めません。
