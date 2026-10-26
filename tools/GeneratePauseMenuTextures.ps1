# ポーズ用文字画像を同梱書体から生成する。
Add-Type -AssemblyName System.Drawing
$outputDirectory = Join-Path $PSScriptRoot '../project/resources/textures' # 文字画像の保存先
$fontCollection = [System.Drawing.Text.PrivateFontCollection]::new() # 同梱日本語書体の管理
$fontCollection.AddFontFile((Join-Path $PSScriptRoot '../project/resources/fonts/ZenKakuGothicNew-Medium.ttf'))
$font = [System.Drawing.Font]::new($fontCollection.Families[0], 40, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # 項目用書体
$smallFont = [System.Drawing.Font]::new($fontCollection.Families[0], 30, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # 補足と入力説明用書体
$labels = @(
    'ポーズ', 'つづける', '操作方法', '現在区間をやり直す', 'ステージ選択へ戻る',
    '現在区間をやり直しますか？', '現在区間の記録と分身は削除されます',
    'ステージ選択へ戻りますか？', '再入場時はステージの最初からになります',
    'キャンセル', '実行する',
    '↑↓ / W S / 十字キー：選択　 Enter / SPACE / A：決定',
    'Esc / B：戻る　 START：ポーズを閉じる',
    'Enter / SPACE / A / Esc / B：メニューへ戻る',
    'Esc / START：ポーズ・操作方法'
) # 64ピクセル単位で切り出す文字の行
$bitmap = [System.Drawing.Bitmap]::new(1024, 64 * $labels.Count) # 透明背景の文字一覧
$graphics = [System.Drawing.Graphics]::FromImage($bitmap) # 文字の描画先
try {
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    for ($row = 0; $row -lt $labels.Count; ++$row) { # 描画する文字行番号
        $rowFont = if ($row -ge 11 -or $row -eq 6 -or $row -eq 8) { $smallFont } else { $font } # 説明行だけ小さくする書体
        $graphics.DrawString($labels[$row], $rowFont, [System.Drawing.Brushes]::White, [single]0, [single]($row * 64))
    }
    $bitmap.Save((Join-Path $outputDirectory 'pause_menu.png'), [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $graphics.Dispose()
    $bitmap.Dispose()
    $font.Dispose()
    $smallFont.Dispose()
    $fontCollection.Dispose()
}
