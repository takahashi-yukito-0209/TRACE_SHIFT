# タイトル表示用の文字画像を再生成する。実行場所に依存しない。
Add-Type -AssemblyName System.Drawing
$outputDirectory = Join-Path $PSScriptRoot '../project/resources/textures' # 文字画像の保存先
$japaneseCollection = [System.Drawing.Text.PrivateFontCollection]::new() # 同梱日本語書体の管理
$japaneseCollection.AddFontFile((Join-Path $PSScriptRoot '../project/resources/fonts/ZenKakuGothicNew-Medium.ttf'))
$latinCollection = [System.Drawing.Text.PrivateFontCollection]::new() # 同梱英字書体の管理
$latinCollection.AddFontFile((Join-Path $PSScriptRoot '../project/resources/fonts/Rajdhani-SemiBold.ttf'))
$font = [System.Drawing.Font]::new($japaneseCollection.Families[0], 40, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # 日本語メニューの表示書体
$latinFont = [System.Drawing.Font]::new($latinCollection.Families[0], 40, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # 操作キーと数値の表示書体
$logoFont = [System.Drawing.Font]::new($latinCollection.Families[0], 240, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # タイトルロゴの表示書体
$headingFont = [System.Drawing.Font]::new($japaneseCollection.Families[0], 48, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # 設定画面の見出し書体
$groupFont = [System.Drawing.Font]::new($japaneseCollection.Families[0], 28, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel) # 操作説明のグループ見出し書体
$brush = [System.Drawing.Brushes]::White # 色変更できる白い文字
$accentBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(120, 204, 204)) # グループ見出しの落ち着いた青緑

# <summary>指定サイズの透過画像へ文字を描いて保存する。</summary>
function Write-TextTexture {
    param([string]$Name, [int]$Width, [int]$Height, [object[]]$Lines)
    $bitmap = [System.Drawing.Bitmap]::new($Width, $Height) # 透過文字画像
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap) # 文字の描画先
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        foreach ($line in $Lines) { # 各文字列の配置情報
            $lineFont = if ([string]$line.Text -match '^[\x20-\x7E]+$') { $latinFont } else { $font } # 英数字だけの行に適用する書体
            if ($line.Heading) {
                $lineFont = $headingFont;
            }
            $lineBrush = $brush # 通常行の文字色
            if ($line.Group) {
                $lineFont = $groupFont
                $lineBrush = $accentBrush
            }
            if ($line.AlignRight) {
                $format = [System.Drawing.StringFormat]::new() # 数値欄の右端へ揃える描画形式
                try {
                    $format.Alignment = [System.Drawing.StringAlignment]::Far
                    $format.FormatFlags = [System.Drawing.StringFormatFlags]::NoWrap
                    $rectangle = [System.Drawing.RectangleF]::new([single]$line.X, [single]$line.Y, [single]$line.Width, 64) # 数値欄の切り出し範囲
                    $graphics.DrawString([string]$line.Text, $lineFont, $lineBrush, $rectangle, $format)
                } finally {
                    $format.Dispose()
                }
            } else {
                $graphics.DrawString([string]$line.Text, $lineFont, $lineBrush, [single]$line.X, [single]$line.Y)
            }
        }
        $bitmap.Save((Join-Path $outputDirectory $Name), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

# <summary>既存の画像サイズを保ち、タイトルロゴを中央に描画する。</summary>
function Write-TitleLogoTexture {
    $bitmap = [System.Drawing.Bitmap]::new(1600, 320) # 既存サイズのロゴ画像
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap) # ロゴの描画先
    $format = [System.Drawing.StringFormat]::new() # 中央揃えの描画形式
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        $format.Alignment = [System.Drawing.StringAlignment]::Center
        $format.LineAlignment = [System.Drawing.StringAlignment]::Center
        $format.FormatFlags = [System.Drawing.StringFormatFlags]::NoWrap
        $graphics.DrawString('TRACE SHIFT', $logoFont, $brush, [System.Drawing.RectangleF]::new(0, 0, 1600, 320), $format)
        $bitmap.Save((Join-Path $outputDirectory 'title_logo.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $format.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

try {
    Write-TitleLogoTexture
    $labels = @('ゲーム開始', '操作説明', '設定', '戻る', '全体音量', '設定') # メニューと設定の文字
    $atlasLines = @() # 64ピクセル単位で切り出す文字一覧
    for ($index = 0; $index -lt $labels.Count; ++$index) { # 固定項目の行番号
        $atlasLines += @{ Text = $labels[$index]; X = 0; Y = $index * 64 }
    }
    for ($index = 0; $index -le 10; ++$index) { # 10パーセント刻みの表示行
        $atlasLines += @{ Text = "$($index * 10)%"; X = 0; Y = ($index + 6) * 64; Width = 192; AlignRight = $true }
    }
    Write-TextTexture 'title_menu.png' 512 1088 $atlasLines
    $controls = @(@{ Text = '操作説明'; X = 0; Y = 12; Heading = $true }) # 操作説明の見出し
    $groups = @(
        @{ Title = '基本操作'; Y = 104; Bindings = @(
            @('A / D', '左右移動'), @('SPACE', 'ジャンプ')
        ) },
        @{ Title = '記録と分身'; Y = 272; Bindings = @(
            @('C', '記録の開始 / 停止'), @('T', '記録を残して開始位置へ'),
            @('V', '分身の再生'), @('B', '分身の停止'), @('X', '最後の分身を削除')
        ) },
        @{ Title = 'やり直し'; Y = 584; Bindings = @(,@('R', 'リセット')) }
    ) # 既存のキー割り当てを操作の目的ごとにまとめた一覧
    foreach ($group in $groups) { # 操作説明の各グループ
        $controls += @{ Text = $group.Title; X = 0; Y = $group.Y; Group = $true }
        for ($index = 0; $index -lt $group.Bindings.Count; ++$index) { # グループ内の操作行番号
            $rowY = $group.Y + 46 + $index * 48 # グループ見出しからの行位置
            $controls += @{ Text = $group.Bindings[$index][0]; X = 0; Y = $rowY + 6 }
            $controls += @{ Text = $group.Bindings[$index][1]; X = 270; Y = $rowY }
        }
    }
    Write-TextTexture 'title_controls.png' 960 720 $controls
    Write-TextTexture 'title_settings.png' 960 128 @(
        @{ Text = '設定'; X = 0; Y = 24; Heading = $true }
    )
    Write-TextTexture 'title_volume_limits.png' 384 64 @(
        @{ Text = '0%'; X = 0; Y = 0 }
        @{ Text = '100%'; X = 192; Y = 0; Width = 192; AlignRight = $true }
    )
} finally {
    $font.Dispose()
    $latinFont.Dispose()
    $logoFont.Dispose()
    $headingFont.Dispose()
    $groupFont.Dispose()
    $accentBrush.Dispose()
    $japaneseCollection.Dispose()
    $latinCollection.Dispose()
}
