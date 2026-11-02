# Generates res\brightkeys.ico (16/24/32/48/256 px, PNG-compressed frames) and docs\images\icon.png.
# Only needed when changing the icon; both outputs are committed.
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent

function New-SunPng([int]$s) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $c = $s / 2.0
    $color = [System.Drawing.Color]::FromArgb(255, 255, 184, 28)
    $core = $s * 0.22
    $g.FillEllipse((New-Object System.Drawing.SolidBrush $color), [float]($c - $core), [float]($c - $core), [float](2 * $core), [float](2 * $core))
    $pen = New-Object System.Drawing.Pen $color, ([float][Math]::Max(1.0, $s * 0.09))
    $pen.StartCap = 'Round'; $pen.EndCap = 'Round'
    for ($i = 0; $i -lt 8; $i++) {
        $a = $i * [Math]::PI / 4
        $g.DrawLine($pen, [float]($c + [Math]::Cos($a) * $s * 0.335), [float]($c + [Math]::Sin($a) * $s * 0.335),
                          [float]($c + [Math]::Cos($a) * $s * 0.445), [float]($c + [Math]::Sin($a) * $s * 0.445))
    }
    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    , $ms.ToArray()
}

$sizes = 16, 24, 32, 48, 256
$frames = foreach ($s in $sizes) { , (New-SunPng $s) }

$ico = Join-Path $root 'res\brightkeys.ico'
$w = New-Object System.IO.BinaryWriter ([System.IO.File]::Create($ico))
$w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $len = $frames[$i].Length
    $w.Write([byte]($s % 256)); $w.Write([byte]($s % 256)); $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([UInt16]1); $w.Write([UInt16]32); $w.Write([UInt32]$len); $w.Write([UInt32]$offset)
    $offset += $len
}
foreach ($f in $frames) { $w.Write($f) }
$w.Close()

[System.IO.File]::WriteAllBytes((Join-Path $root 'docs\images\icon.png'), (New-SunPng 128))
Write-Host "Wrote $ico and docs\images\icon.png"
