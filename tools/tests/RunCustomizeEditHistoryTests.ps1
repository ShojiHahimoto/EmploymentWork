$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Push-Location $root
try {
    Import-Module 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    Enter-VsDevShell -VsInstallPath 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise' -SkipAutomaticLocation -DevCmdArguments '-arch=x64' | Out-Null
    $output = Join-Path $root 'x64\CustomizeEditHistoryTests'
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $package = 'packages\directxtk_desktop_2019.2025.10.28.2'
    $executable = Join-Path $output 'CustomizeEditHistoryTests.exe'
    & cl /nologo /std:c++20 /EHsc /MDd /D_DEBUG /DNOMINMAX /utf-8 /I"DrectX11Sample\Source" /I"$package\include" `
        'tools\tests\CustomizeEditHistoryTests.cpp' 'DrectX11Sample\Source\Controller\CustomizeEditHistory.cpp' `
        /Fo"$output\\" /Fe"$executable" /link "$package\native\lib\x64\Debug\DirectXTK.lib"
    if ($LASTEXITCODE -ne 0) { throw 'History test build failed.' }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw 'History tests failed.' }
}
finally { Pop-Location }
