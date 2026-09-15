$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Push-Location $root
try {
    Import-Module 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    Enter-VsDevShell -VsInstallPath 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise' -SkipAutomaticLocation -DevCmdArguments '-arch=x64' | Out-Null
    $output = Join-Path $root 'x64\MotionPoseTests'
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $package = 'packages\directxtk_desktop_2019.2025.10.28.2'
    & cl /nologo /std:c++20 /EHsc /MDd /D_DEBUG /DNOMINMAX /utf-8 /I"DrectX11Sample\Source" /I"$package\include" `
        'tools\tests\MotionPoseTests.cpp' 'DrectX11Sample\Source\System\MotionPose.cpp' `
        'DrectX11Sample\Source\Data\MotionSkeletonDefinition.cpp' /Fo"$output\\" /Fe"$output\MotionPoseTests.exe" `
        /link "$package\native\lib\x64\Debug\DirectXTK.lib"
    if ($LASTEXITCODE -ne 0) { throw 'Test build failed.' }
    & "$output\MotionPoseTests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'MotionPose tests failed.' }
}
finally { Pop-Location }
