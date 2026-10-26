# ステージ選択・開始・クリアの文字画像を英数字と日本語の階層を分けて生成する。
Add-Type -AssemblyName System.Drawing
$outputDirectory = Join-Path $PSScriptRoot '../project/resources/textures' # 文字画像の保存先
$latinCollection = [System.Drawing.Text.PrivateFontCollection]::new() # プロジェクト同梱の英数字フォントを読み込む管理
$latinCollection.AddFontFile((Join-Path $PSScriptRoot '../project/resources/fonts/FiraMono-Regular.ttf'))
$headingCollection = [System.Drawing.Text.PrivateFontCollection]::new() # 英語見出しの書体管理
$headingCollection.AddFontFile((Join-Path $PSScriptRoot '../project/resources/fonts/Rajdhani-SemiBold.ttf'))
$japaneseCollection = [System.Drawing.Text.PrivateFontCollection]::new() # 日本語の書体管理
$japaneseCollection.AddFontFile((Join-Path $PSScriptRoot '../project/resources/fonts/ZenKakuGothicNew-Medium.ttf'))
$latinFont = [System.Drawing.Font]::new($headingCollection.Families[0], 42, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # 英語見出しの直線的な書体
$titleFont = [System.Drawing.Font]::new($japaneseCollection.Families[0], 44, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # 日本語のステージ名を表示する書体
$bodyFont = [System.Drawing.Font]::new($japaneseCollection.Families[0], 30, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # 説明文を表示する書体
$numberFont = [System.Drawing.Font]::new($latinCollection.Families[0], 52, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # 未公開一覧で主役にするステージ番号

# <summary>透明な文字画像を作成し、行ごとの表示文字を描き込む。</summary>
function Write-StageTextTexture {
    param([string]$Name, [int]$Height, [int]$LineHeight, [string[]]$Lines, [switch]$Atlas)
    $bitmap = [System.Drawing.Bitmap]::new(768, $Height) # スプライトに渡す透過画像
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap) # 文字画像の描画先
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        for ($index = 0; $index -lt $Lines.Count; ++$index) { # 画像内の文字行番号
            $lineY = [single]($index * $LineHeight) # 文字行の上端
            if ($Atlas -and (($index -ge 1 -and $index -le 3) -or $Name -eq 'stage_locked.png')) {
                if ($Name -eq 'stage_locked.png') {
                    $graphics.DrawString($Lines[$index].Substring(0, 2), $numberFont, [System.Drawing.Brushes]::White, 0, $lineY)
                    $graphics.DrawString($Lines[$index].Substring(4), $bodyFont, [System.Drawing.Brushes]::White, 112, ($lineY + 12))
                } else {
                    $graphics.DrawString($Lines[$index].Substring(0, 2), $numberFont, [System.Drawing.Brushes]::White, 0, $lineY)
                    $graphics.DrawString($Lines[$index].Substring(4), $titleFont, [System.Drawing.Brushes]::White, 112, $lineY)
                }
            } else {
                $lineFont = if ($Atlas) { # 英語見出しと状態表示、日本語本文の書体選択
                    if ($index -eq 0 -or $index -eq 6) { $latinFont }
                    elseif ($index -eq 4) { $titleFont }
                    else { $bodyFont }
                } else {
                    if ($index -eq 0) { $latinFont }
                    elseif ($index -eq 1) { $titleFont }
                    else { $bodyFont }
                }
                $graphics.DrawString($Lines[$index], $lineFont, [System.Drawing.Brushes]::White, 0, $lineY)
            }
        }
        $bitmap.Save((Join-Path $outputDirectory $Name), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

# <summary>指定した文字を画像中央へ配置する。</summary>
function Write-StageCenteredTexture {
    param([string]$Name, [int]$Width, [int]$Height, [string]$Text, [System.Drawing.Font]$Font)
    $bitmap = [System.Drawing.Bitmap]::new($Width, $Height) # 中央表示用の透明な文字画像
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap) # 中央文字の描画先
    $format = [System.Drawing.StringFormat]::new() # 横・縦の中央に揃える配置形式
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        $format.Alignment = [System.Drawing.StringAlignment]::Center
        $format.LineAlignment = [System.Drawing.StringAlignment]::Center
        $graphics.DrawString($Text, $Font, [System.Drawing.Brushes]::White,
            [System.Drawing.RectangleF]::new(0, 0, $Width, $Height), $format)
        $bitmap.Save((Join-Path $outputDirectory $Name), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $format.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

# <summary>プレビューの四辺を背景色へなじませる透明度マスクを生成する。</summary>
function Write-PreviewEdgeTexture {
    $bitmap = [System.Drawing.Bitmap]::new(256, 256) # 白色に透明度だけを付けた境界用画像
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap) # 境界マスクの描画先
    $transparentWhite = [System.Drawing.Color]::FromArgb(0, 255, 255, 255) # 中央側の透明色
    $edges = @(
        @{ Rectangle = [System.Drawing.Rectangle]::new(0, 0, 72, 256); Start = [System.Drawing.Point]::new(0, 0); End = [System.Drawing.Point]::new(72, 0) },
        @{ Rectangle = [System.Drawing.Rectangle]::new(184, 0, 72, 256); Start = [System.Drawing.Point]::new(256, 0); End = [System.Drawing.Point]::new(184, 0) },
        @{ Rectangle = [System.Drawing.Rectangle]::new(0, 0, 256, 72); Start = [System.Drawing.Point]::new(0, 0); End = [System.Drawing.Point]::new(0, 72) },
        @{ Rectangle = [System.Drawing.Rectangle]::new(0, 184, 256, 72); Start = [System.Drawing.Point]::new(0, 256); End = [System.Drawing.Point]::new(0, 184) }
    ) # 四辺から中央へ透明になる描画範囲
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        foreach ($edge in $edges) { # 端で不透明、中央側で透明になる各辺
            $brush = [System.Drawing.Drawing2D.LinearGradientBrush]::new($edge.Start, $edge.End,
                [System.Drawing.Color]::White, $transparentWhite) # 背景色へのフェード率を作るブラシ
            try {
                $graphics.FillRectangle($brush, $edge.Rectangle)
            } finally {
                $brush.Dispose()
            }
        }
        $bitmap.Save((Join-Path $outputDirectory 'stage_preview_edges.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

try {
    Write-StageTextTexture 'stage_select.png' 576 64 -Atlas -Lines @(
        'STAGE SELECT',
        '01  過去の自分を足場に',
        '02  二つの役割',
        '03  総合課題',
        'タイトルへ戻る',
        '挑戦可能',
        'CLEAR',
        'プレビューを読み込めませんでした',
        '未公開'
    )
    Write-StageTextTexture 'stage_detail_01.png' 256 72 @(
        'STAGE 01', '過去の自分を足場に', '足場・移動・再合流の三つの課題'
    )
    Write-StageTextTexture 'stage_section_01.png' 256 72 @(
        'SECTION 01 / 03', '足場にする', '自分の記録を足場として使う'
    )
    Write-StageTextTexture 'stage_section_02.png' 256 72 @(
        'SECTION 02 / 03', '運んでもらう', '記録をリセット・新しい区間から開始'
    )
    Write-StageTextTexture 'stage_section_03.png' 256 72 @(
        'SECTION 03 / 03', '別れて再合流する', '記録をリセット・新しい区間から開始'
    )
    Write-StageTextTexture 'stage_detail_02.png' 256 72 @(
        'STAGE 02', '二つの役割', '分身2体でスイッチを連携する'
    )
    Write-StageTextTexture 'stage_detail_03.png' 256 72 @(
        'STAGE 03', '総合課題', '分身と時間差を使いゴールへ'
    )
    Write-StageTextTexture 'stage_locked.png' 192 64 -Atlas -Lines @(
        '01  ？？？？？', '02  ？？？？？', '03  ？？？？？'
    )
    Write-StageCenteredTexture 'stage_detail_locked.png' 768 256 '未公開' $bodyFont
    Write-PreviewEdgeTexture
} finally {
    $latinFont.Dispose()
    $titleFont.Dispose()
    $bodyFont.Dispose()
    $numberFont.Dispose()
    $headingCollection.Dispose()
    $japaneseCollection.Dispose()
    $latinCollection.Dispose()
}
