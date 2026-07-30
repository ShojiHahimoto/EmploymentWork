param(
	[string[]]$Paths = @()
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$utf8Bom = New-Object System.Text.UTF8Encoding($true)

if ($Paths.Count -eq 0)
{
	$extensions = @(".cpp", ".h", ".hpp", ".sln", ".vcxproj", ".filters", ".config", ".md", ".json")
	$files = Get-ChildItem -LiteralPath $root -Recurse -File |
		Where-Object {
			$extensions -contains $_.Extension -and
			$_.FullName -notmatch "\\.git\\" -and
			$_.FullName -notmatch "\\x64\\" -and
			$_.FullName -notmatch "\\Debug\\" -and
			$_.FullName -notmatch "\\Release\\" -and
			$_.FullName -notmatch "\\Source\\System\\imgui-docking\\"
		}
}
else
{
	$files = foreach ($path in $Paths)
	{
		$resolvedPath = if ([System.IO.Path]::IsPathRooted($path)) { $path } else { Join-Path $root $path }
		Get-Item -LiteralPath $resolvedPath
	}
}

foreach ($file in $files)
{
	$text = [System.IO.File]::ReadAllText($file.FullName)
	$text = $text -replace "`r?`n", "`r`n"
	[System.IO.File]::WriteAllText($file.FullName, $text, $utf8Bom)
}
