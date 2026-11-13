$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root=Split-Path -Parent $PSScriptRoot
$folder=Join-Path $root 'DrectX11Sample/assets/texture/UI/Parts'
$data=Get-Content (Join-Path $folder 'assets.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$sheet=[System.Drawing.Bitmap]::new(1000,([int][Math]::Ceiling($data.assets.Count/4.0)*160))
$g=[System.Drawing.Graphics]::FromImage($sheet)
$g.Clear([System.Drawing.Color]::White)
$font=[System.Drawing.Font]::new('Segoe UI',10)
$i=0
try {
    foreach($asset in $data.assets) {
        $b=[System.Drawing.Bitmap]::FromFile((Join-Path $folder $asset.file))
        try {
            if($b.Width -ne $asset.width -or $b.Height -ne $asset.height){throw "Size mismatch: $($asset.name)"}
            if($asset.slice[0]+$asset.slice[2] -ge $b.Width -or $asset.slice[1]+$asset.slice[3] -ge $b.Height){throw "Invalid slice: $($asset.name)"}
            $x=($i%4)*250; $y=[int][Math]::Floor($i/4)*160
            $scale=[Math]::Min(220.0/$b.Width,115.0/$b.Height)
            $g.DrawImage($b,[System.Drawing.Rectangle]::new($x+15,$y+30,[int]($b.Width*$scale),[int]($b.Height*$scale)))
            $g.DrawString($asset.name,$font,[System.Drawing.Brushes]::Black,$x+8,$y+5)
            $i++
        } finally {$b.Dispose()}
    }
    $sheet.Save((Join-Path $folder 'inspection.png'),[System.Drawing.Imaging.ImageFormat]::Png)
    "Validated $i files and slice bounds."
} finally {$font.Dispose();$g.Dispose();$sheet.Dispose()}
