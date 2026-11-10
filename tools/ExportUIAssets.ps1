param()
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'DrectX11Sample/assets/texture/UI/Source'
$output = Join-Path $root 'DrectX11Sample/assets/texture/UI/Parts'
New-Item -ItemType Directory -Force $output | Out-Null
$entries = @()

# 生成原稿は変更せず、セル内の実際のアルファ領域を切り出す。
function Export-Sheet($file, $columns, $rows, $names) {
    $bitmap = [System.Drawing.Bitmap]::FromFile((Join-Path $source $file))
    try {
        for ($i = 0; $i -lt $names.Count; $i++) {
            if (!$names[$i]) { continue }
            $cx = $i % $columns
            $cy = [Math]::Floor($i / $columns)
            $x0 = [int]($cx * $bitmap.Width / $columns)
            $x1 = [int](($cx + 1) * $bitmap.Width / $columns)
            $y0 = [int]($cy * $bitmap.Height / $rows)
            $y1 = [int](($cy + 1) * $bitmap.Height / $rows)
            $left = $x1; $top = $y1; $right = $x0; $bottom = $y0
            for ($y = $y0; $y -lt $y1; $y++) {
                for ($x = $x0; $x -lt $x1; $x++) {
                    if ($bitmap.GetPixel($x,$y).A -gt 8) {
                        $left = [Math]::Min($left,$x); $top = [Math]::Min($top,$y)
                        $right = [Math]::Max($right,$x+1); $bottom = [Math]::Max($bottom,$y+1)
                    }
                }
            }
            if ($right -le $left) { throw "Empty asset: $($names[$i])" }
            $rect = [System.Drawing.Rectangle]::new($left,$top,$right-$left,$bottom-$top)
            $part = $bitmap.Clone($rect,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                for ($y=0; $y -lt $part.Height; $y++) {
                    for ($x=0; $x -lt $part.Width; $x++) {
                        $p=$part.GetPixel($x,$y)
                        if ($p.A -le 8) { $part.SetPixel($x,$y,[System.Drawing.Color]::Transparent) }
                    }
                }
                $part.Save((Join-Path $output ($names[$i]+'.png')),[System.Drawing.Imaging.ImageFormat]::Png)
                $script:entries += @{name=$names[$i]; file=($names[$i]+'.png'); width=$part.Width; height=$part.Height; source=$file; sourceRect=@($left,$top,$right,$bottom); slice=@(0,0,0,0)}
            } finally { $part.Dispose() }
        }
    } finally { $bitmap.Dispose() }
}

Export-Sheet 'ui_parts_base_v1.png' 4 4 @('panel','button','button_primary','number_field','checkbox_off','checkbox_on','label_background','', 'back','reset','play','pause','previous','next','camera','keyframe')
# 追加シートの丸い棒と古い区間帯は採用しない。
Export-Sheet 'ui_parts_extra_v1.png' 4 3 @('','','','','','','header','playback_panel','','','','playhead')
Export-Sheet 'ui_icons_v2.png' 4 1 @('pencil','up','down','motion')
Export-Sheet 'header_accent_v1.png' 1 1 @('header_accent')

# 白背景の確認原稿から色境界で分割する。外形だけをアルファマスク化し、内側の境界は直線を維持する。
$proof=[System.Drawing.Bitmap]::FromFile((Join-Path $source 'ui_geometry_v2_proof.png'))
try {
    $regions=@(
        @{name='phase_startup'; x=59; y=82; w=550; h=148; side='left'},
        @{name='phase_active'; x=609; y=82; w=539; h=148; side='none'},
        @{name='phase_recovery'; x=1148; y=82; w=547; h=148; side='right'},
        @{name='section_accent'; x=237; y=497; w=50; h=223; side='none'}
    )
    foreach($r in $regions) {
        $part=[System.Drawing.Bitmap]::new($r.w,$r.h,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            for($y=0;$y -lt $r.h;$y++) { for($x=0;$x -lt $r.w;$x++) {
                $alpha=1.0
                $radius=40.0
                $edge=if($r.side -eq 'left'){$x+0.5}else{$r.w-$x-0.5}
                $vertical=[Math]::Min($y+0.5,$r.h-$y-0.5)
                if($r.side -ne 'none' -and $edge -lt $radius -and $vertical -lt $radius) {
                    $distance=[Math]::Sqrt([Math]::Pow($radius-$edge,2)+[Math]::Pow($radius-$vertical,2))
                    $alpha=[Math]::Min(1.0,[Math]::Max(0.0,$radius-$distance+0.5))
                }
                # 元画像の白い角を混入させないよう、境界付近の色は同じ行の内側から取得する。
                $sx=if($r.side -eq 'left'){[Math]::Max(42,$x)}elseif($r.side -eq 'right'){[Math]::Min($r.w-43,$x)}else{$x}
                $p=$proof.GetPixel($r.x+$sx,$r.y+$y)
                $part.SetPixel($x,$y,[System.Drawing.Color]::FromArgb([int]($alpha*255),$p.R,$p.G,$p.B))
            }}
            $part.Save((Join-Path $output ($r.name+'.png')),[System.Drawing.Imaging.ImageFormat]::Png)
            $entries+=@{name=$r.name; file=($r.name+'.png'); width=$r.w; height=$r.h; source='ui_geometry_v2_proof.png'; slice=@(0,0,0,0)}
        } finally {$part.Dispose()}
    }
} finally {$proof.Dispose()}
foreach($e in $entries) {
    if($e.name -in @('panel','button','button_primary','number_field','label_background','header','playback_panel')) {$e.slice=@(28,28,28,28)}
    if($e.name -eq 'phase_startup') {$e.slice=@(42,42,0,42)}
    if($e.name -eq 'phase_recovery') {$e.slice=@(0,42,42,42)}
}
# 単体で生成した影なし素材も再出力時に保持する。
foreach($asset in @(
    @{name='panel_flat_white'; source='panel_flat_white_rounded.png'; slice=@(110,110,110,110)},
    @{name='button_primary_clean'; source='button_primary_clean.png'; slice=@(160,160,160,160)}
)) {
    $file=$asset.name+'.png'
    Copy-Item -LiteralPath (Join-Path $source $asset.source) -Destination (Join-Path $output $file)
    $bitmap=[System.Drawing.Bitmap]::FromFile((Join-Path $output $file))
    try {
        $entries+=@{name=$asset.name; file=$file; source=$asset.source; width=$bitmap.Width; height=$bitmap.Height; slice=$asset.slice}
    } finally {$bitmap.Dispose()}
}
# モーション編集と共有する透過アイコンも再出力で欠落させない。
$motionManifest = Join-Path $output 'motion_assets.json'
if (Test-Path -LiteralPath $motionManifest) {
    $motionEntries = (Get-Content -LiteralPath $motionManifest -Raw | ConvertFrom-Json).assets
    foreach ($entry in $motionEntries) { if ($entry.name -notin $entries.name) { $entries += $entry } }
}
$json=@{version=1; sliceOrder=@('left','top','right','bottom'); assets=$entries} | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText((Join-Path $output 'assets.json'),$json,[Text.UTF8Encoding]::new($true))
Write-Output "Exported $($entries.Count) assets to $output"
