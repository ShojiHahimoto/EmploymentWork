$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$folder = Join-Path $root 'DrectX11Sample/assets/texture/UI'
$names = @('undo','redo','rotate','translate','add','delete','copy','paste','mirror','preset_save','preset_load','select_all','deselect_all','keyframe_movement')
$sheet = [System.Drawing.Bitmap]::FromFile((Join-Path $folder 'Source/motion_editor_icons_v1.png'))
$entries = @()
try {
    for ($i=0; $i -lt $names.Count; $i++) {
        $x0=[int][Math]::Floor(($i%4)*$sheet.Width/4)
        $y0=[int][Math]::Floor([Math]::Floor($i/4)*$sheet.Height/4)
        $x1=[int][Math]::Floor((($i%4)+1)*$sheet.Width/4)
        $y1=[int][Math]::Floor(([Math]::Floor($i/4)+1)*$sheet.Height/4)
        $l=$x1; $t=$y1; $r=$x0; $b=$y0
        for($y=$y0;$y -lt $y1;$y++){for($x=$x0;$x -lt $x1;$x++){
            if($sheet.GetPixel($x,$y).A -gt 16){
                $l=[Math]::Min($l,$x);$t=[Math]::Min($t,$y)
                $r=[Math]::Max($r,$x+1);$b=[Math]::Max($b,$y+1)
            }
        }}
        if($r -le $l -or $b -le $t){throw "Empty icon: $($names[$i])"}
        $rect=[System.Drawing.Rectangle]::new($l,$t,$r-$l,$b-$t)
        $icon=$sheet.Clone($rect,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $icon.Save((Join-Path $folder ('Parts/'+$names[$i]+'.png')),[System.Drawing.Imaging.ImageFormat]::Png)
            $entries+=@{name=$names[$i];file=$names[$i]+'.png';width=$icon.Width;height=$icon.Height;slice=@(0,0,0,0)}
        } finally {$icon.Dispose()}
    }
} finally {$sheet.Dispose()}
# Separate manifest: existing runtime assets remain unchanged until UI integration.
$json=@{assets=$entries;sliceOrder=@('left','top','right','bottom')} | ConvertTo-Json -Depth 6
[IO.File]::WriteAllText((Join-Path $folder 'Parts/motion_assets.json'),$json,[Text.UTF8Encoding]::new($true))
Write-Output "Exported $($entries.Count) transparent icons. Existing UI assets unchanged."
