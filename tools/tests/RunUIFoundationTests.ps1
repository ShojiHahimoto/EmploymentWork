$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Push-Location $root
try {
    Import-Module 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    Enter-VsDevShell -VsInstallPath 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise' -SkipAutomaticLocation -DevCmdArguments '-arch=x64' | Out-Null
    $output = Join-Path $root 'x64\UIFoundationTests'
    $testExecutable = Join-Path $output 'UIFoundationTests-Images.exe'
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $package = 'packages\directxtk_desktop_2019.2025.10.28.2'
    & cl /nologo /std:c++20 /EHsc /MDd /D_DEBUG /DNOMINMAX /utf-8 /I"DrectX11Sample\Source" /I"$package\include" `
        'tools\tests\UIFoundationTests.cpp' 'DrectX11Sample\Source\UI\UIRoot.cpp' `
        'DrectX11Sample\Source\UI\UIRenderer.cpp' 'DrectX11Sample\Source\UI\UIInputSystem.cpp' `
        'DrectX11Sample\Source\Controller\TitleMenuController.cpp' `
        'DrectX11Sample\Source\Data\JsonValue.cpp' 'DrectX11Sample\Source\System\Debugger.cpp' `
        /Fo"$output\\" /Fe"$testExecutable" `
        /link "$package\native\lib\x64\Debug\DirectXTK.lib" user32.lib ole32.lib
    if ($LASTEXITCODE -ne 0) { throw 'UI test build failed.' }
    & $testExecutable
    if ($LASTEXITCODE -ne 0) { throw 'UI foundation tests failed.' }
}
finally { Pop-Location }
