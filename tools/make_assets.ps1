param(
    [string]$OutDir = 'assets'
)

# Placeholder art generator v2:
# - static title bg, classroom / street scenes
# - full-body school uniform sprites WITHOUT face (body PNG)
# - separate expression PNGs (head + face), same 480x900 canvas
# - two full-screen CG images

Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'

function New-Canvas([int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    return @{ bmp = $bmp; g = $g }
}

function Save-Canvas($canvas, [string]$path) {
    $dir = Split-Path -Parent $path
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $canvas.bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $canvas.g.Dispose()
    $canvas.bmp.Dispose()
    Write-Host "  wrote $path"
}

function Brush([string]$hex) {
    return New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml($hex))
}

function Pen([string]$hex, [float]$width) {
    return New-Object System.Drawing.Pen([System.Drawing.ColorTranslator]::FromHtml($hex), $width)
}

function Grad-Brush($rect, [string]$c1, [string]$c2, [float]$angle) {
    $r = New-Object System.Drawing.Rectangle(0, 0, $rect.Width, $rect.Height)
    return New-Object System.Drawing.Drawing2D.LinearGradientBrush($r,
        [System.Drawing.ColorTranslator]::FromHtml($c1),
        [System.Drawing.ColorTranslator]::FromHtml($c2), $angle)
}

function Fill-Ellipse($g, [float]$cx, [float]$cy, [float]$rx, [float]$ry, $brush) {
    if ($null -eq $brush) { return }
    $g.FillEllipse($brush, $cx - $rx, $cy - $ry, $rx * 2, $ry * 2)
}

function Fill-Rect($g, [float]$x, [float]$y, [float]$wd, [float]$ht, $brush) {
    if ($null -eq $brush) { return }
    $g.FillRectangle($brush, $x, $y, $wd, $ht)
}

function Draw-Arc($g, [float]$x, [float]$y, [float]$wd, [float]$ht, [float]$a1, [float]$a2, $pen) {
    if ($null -eq $pen) { return }
    $g.DrawArc($pen, $x, $y, $wd, $ht, $a1, $a2)
}

function New-BgTitle {
    $c = New-Canvas 1280 720
    $g = $c.g
    $g.FillRectangle((Grad-Brush (New-Object System.Drawing.Rectangle(0, 0, 1280, 720)) '#8FBFFF' '#E9F4FF' 90.0), 0, 0, 1280, 720)
    $sun = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(90, 255, 255, 240))
    Fill-Ellipse $g 980 170 130 130 $sun
    $sun.Dispose()
    $sunCore = Brush('#FFF6D8')
    Fill-Ellipse $g 980 170 74 74 $sunCore
    $sunCore.Dispose()
    $cloud = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(215, 255, 255, 255))
    Fill-Ellipse $g 260 150 95 38 $cloud
    Fill-Ellipse $g 340 130 70 34 $cloud
    Fill-Ellipse $g 200 130 60 30 $cloud
    Fill-Ellipse $g 640 90 110 40 $cloud
    Fill-Ellipse $g 730 70 70 32 $cloud
    $cloud.Dispose()
    $bld = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(160, 255, 255, 255))
    $g.FillRectangle($bld, 0, 470, 180, 250)
    $g.FillRectangle($bld, 170, 520, 240, 200)
    $g.FillRectangle($bld, 400, 480, 160, 240)
    $g.FillRectangle($bld, 550, 540, 260, 180)
    $g.FillRectangle($bld, 800, 490, 200, 230)
    $g.FillRectangle($bld, 990, 530, 290, 190)
    $bld.Dispose()
    $gnd = Brush('#DCEBFA')
    $g.FillRectangle($gnd, 0, 650, 1280, 70)
    $gnd.Dispose()
    Save-Canvas $c (Join-Path $OutDir 'bg\title.png')
}

function New-BgClassroom {
    $c = New-Canvas 1280 720
    $g = $c.g
    $g.FillRectangle((Grad-Brush (New-Object System.Drawing.Rectangle(0, 0, 1280, 720)) '#CFE0F2' '#93A9C8' 90.0), 0, 0, 1280, 720)
    # big windows
    $win = Brush('#F4F9FF')
    $g.FillRectangle($win, 120, 90, 360, 320)
    $win.Dispose()
    $frame = Brush('#5C6F8E')
    $g.FillRectangle($frame, 110, 80, 380, 24)
    $g.FillRectangle($frame, 110, 396, 380, 24)
    $g.FillRectangle($frame, 110, 80, 24, 340)
    $g.FillRectangle($frame, 466, 80, 24, 340)
    $g.FillRectangle($frame, 288, 80, 24, 340)
    $g.FillRectangle($frame, 110, 234, 380, 18)
    $frame.Dispose()
    $sky = Brush('#C9E3FF')
    $g.FillRectangle($sky, 134, 104, 332, 118)
    $sky.Dispose()
    # lockers on the right
    $locker = Brush('#6E7F9B')
    $g.FillRectangle($locker, 1020, 150, 220, 340)
    $locker.Dispose()
    $lockLine = Pen '#8FA0BC' 3.0
    $g.DrawLine($lockLine, 1020, 263, 1240, 263)
    $g.DrawLine($lockLine, 1130, 150, 1130, 490)
    $g.DrawLine($lockLine, 1020, 376, 1240, 376)
    $lockLine.Dispose()
    $handle = Pen '#C9D6EA' 4.0
    $g.DrawLine($handle, 1080, 200, 1080, 230)
    $g.DrawLine($handle, 1180, 200, 1180, 230)
    $g.DrawLine($handle, 1080, 310, 1080, 340)
    $g.DrawLine($handle, 1180, 310, 1180, 340)
    $handle.Dispose()
    # wall + floor
    $wall = Brush('#E6D8C2')
    $g.FillRectangle($wall, 0, 430, 1280, 70)
    $wall.Dispose()
    $floor = Brush('#B89573')
    $g.FillRectangle($floor, 0, 500, 1280, 220)
    $floor.Dispose()
    # floor perspective lines
    $floorLine = Pen '#A07E60' 3.0
    for ($i = 1; $i -lt 6; $i++) {
        $g.DrawLine($floorLine, 0, 500 + $i * 36, 1280, 500 + $i * 36)
    }
    $floorLine.Dispose()
    # desks
    $desk = Brush('#6E5C48')
    for ($i = 0; $i -lt 3; $i++) {
        $x = 560 + $i * 230
        $g.FillRectangle($desk, $x, 540, 170, 16)
        $g.FillRectangle($desk, $x + 20, 556, 12, 92)
        $g.FillRectangle($desk, $x + 138, 556, 12, 92)
    }
    $desk.Dispose()
    Save-Canvas $c (Join-Path $OutDir 'bg\classroom.png')
}

function New-BgStreet {
    $c = New-Canvas 1280 720
    $g = $c.g
    $g.FillRectangle((Grad-Brush (New-Object System.Drawing.Rectangle(0, 0, 1280, 720)) '#FFC48A' '#8D5A9E' 90.0), 0, 0, 1280, 720)
    $sun = Brush('#FFF2C8')
    Fill-Ellipse $g 640 300 90 90 $sun
    $sun.Dispose()
    $bld = Brush('#55405F')
    $g.FillRectangle($bld, 0, 380, 300, 340)
    $g.FillRectangle($bld, 280, 430, 260, 290)
    $g.FillRectangle($bld, 520, 360, 240, 360)
    $g.FillRectangle($bld, 740, 410, 260, 310)
    $g.FillRectangle($bld, 980, 380, 300, 340)
    $bld.Dispose()
    $lamp = Brush('#44314D')
    $g.FillRectangle($lamp, 340, 420, 12, 240)
    $g.FillEllipse($lamp, 332, 400, 26, 18)
    $g.FillRectangle($lamp, 920, 420, 12, 240)
    $g.FillEllipse($lamp, 912, 400, 26, 18)
    $lamp.Dispose()
    $gnd = Brush('#38243F')
    $g.FillRectangle($gnd, 0, 640, 1280, 80)
    $gnd.Dispose()
    Save-Canvas $c (Join-Path $OutDir 'bg\street.png')
}

# ---- Placeholder body: head + body in ONE image (blocky) ----
function New-Body([string]$name, [string]$jacket, [string]$accent, [string]$skirt) {
    $c = New-Canvas 480 900
    $g = $c.g
    $skin = Brush('#FFE0C4')

    # head (square)
    Fill-Rect $g 185 150 110 130 $skin
    # neck
    Fill-Rect $g 224 280 32 34 $skin
    $skin.Dispose()

    # torso (block)
    $jacketBrush = Brush($jacket)
    Fill-Rect $g 150 314 180 220 $jacketBrush
    $jacketBrush.Dispose()
    # accent collar strip
    $collar = Brush($accent)
    Fill-Rect $g 214 314 52 34 $collar
    $collar.Dispose()
    # arms
    $arm = Brush($jacket)
    Fill-Rect $g 122 314 40 210 $arm
    Fill-Rect $g 318 314 40 210 $arm
    $arm.Dispose()
    $hand = Brush('#FFE0C4')
    Fill-Rect $g 130 522 26 24 $hand
    Fill-Rect $g 324 522 26 24 $hand
    $hand.Dispose()

    # skirt / trousers
    if ($skirt) {
        $skirtBrush = Brush($skirt)
        Fill-Rect $g 170 534 140 130 $skirtBrush
        $skirtBrush.Dispose()
    } else {
        $pant = Brush('#31313F')
        Fill-Rect $g 176 534 56 200 $pant
        Fill-Rect $g 248 534 56 200 $pant
        $pant.Dispose()
    }
    # legs
    $leg = Brush('#2E2E3C')
    Fill-Rect $g 185 664 45 150 $leg
    Fill-Rect $g 250 664 45 150 $leg
    $leg.Dispose()
    # shoes
    $shoe = Brush('#3A3A4A')
    Fill-Rect $g 178 812 58 22 $shoe
    Fill-Rect $g 244 812 58 22 $shoe
    $shoe.Dispose()
    Save-Canvas $c (Join-Path $OutDir "char\$name`_body.png")
}

# ---- Expression: colored square overlay on the head ----
function New-Face([string]$name, [string]$hair, [string]$eye, [string]$kind) {
    $c = New-Canvas 480 900
    $g = $c.g
    $color = switch ($kind) {
        'happy' { '#FFD35C' }
        'sad'   { '#6FA8FF' }
        'angry' { '#FF6B6B' }
        'blush' { '#FF9EC4' }
        default { '#C9D4E8' }
    }
    $sq = Brush($color)
    Fill-Rect $g 185 150 110 130 $sq
    $sq.Dispose()
    Save-Canvas $c (Join-Path $OutDir "char\$name`_face\$kind.png")
}

function New-CgSchool {
    $c = New-Canvas 1280 720
    $g = $c.g
    $g.FillRectangle((Grad-Brush (New-Object System.Drawing.Rectangle(0, 0, 1280, 720)) '#FFB877' '#7A4B8F' 90.0), 0, 0, 1280, 720)
    $sun = Brush('#FFF3C8')
    Fill-Ellipse $g 640 280 110 110 $sun
    $sun.Dispose()
    $frame = Brush('#33203C')
    $g.FillRectangle($frame, 80, 120, 1120, 420)
    $frame.Dispose()
    $glow = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(90, 255, 220, 160))
    $g.FillRectangle($glow, 90, 130, 1100, 400)
    $glow.Dispose()
    $mull = Brush('#4A3356')
    $g.FillRectangle($mull, 0, 320, 1280, 18)
    $g.FillRectangle($mull, 420, 120, 18, 420)
    $g.FillRectangle($mull, 850, 120, 18, 420)
    $mull.Dispose()
    $gnd = Brush('#2B1A33')
    $g.FillRectangle($gnd, 0, 560, 1280, 160)
    $gnd.Dispose()
    $tbl = Brush('#3D2A46')
    $g.FillRectangle($tbl, 300, 600, 680, 22)
    $g.FillRectangle($tbl, 340, 622, 14, 98)
    $g.FillRectangle($tbl, 926, 622, 14, 98)
    $tbl.Dispose()
    Save-Canvas $c (Join-Path $OutDir 'cg\school.png')
}

function New-CgSky {
    $c = New-Canvas 1280 720
    $g = $c.g
    $g.FillRectangle((Grad-Brush (New-Object System.Drawing.Rectangle(0, 0, 1280, 720)) '#070B20' '#1B2347' 90.0), 0, 0, 1280, 720)
    $aurora = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(65, 90, 255, 190))
    $g.FillEllipse($aurora, 60, 120, 520, 120)
    $aurora.Dispose()
    $aurora2 = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(55, 170, 120, 255))
    $g.FillEllipse($aurora2, 420, 80, 520, 130)
    $aurora2.Dispose()
    $star = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(210, 255, 255, 255))
    $rand = New-Object System.Random(7)
    for ($i = 0; $i -lt 260; $i++) {
        $x = $rand.Next(0, 1280); $y = $rand.Next(0, 430)
        $r = 0.6 + $rand.NextDouble() * 1.6
        $g.FillEllipse($star, $x - $r, $y - $r, $r * 2, $r * 2)
    }
    $star.Dispose()
    $moon = Brush('#F4F0DA')
    Fill-Ellipse $g 1030 140 62 62 $moon
    $moon.Dispose()
    $cover = Brush('#0B1030')
    Fill-Ellipse $g 1012 124 50 50 $cover
    $cover.Dispose()
    $hill = Brush('#141A36')
    $g.FillPolygon($hill, [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF(0, 720)),
        (New-Object System.Drawing.PointF(0, 560)),
        (New-Object System.Drawing.PointF(300, 500)),
        (New-Object System.Drawing.PointF(640, 580)),
        (New-Object System.Drawing.PointF(980, 510)),
        (New-Object System.Drawing.PointF(1280, 560)),
        (New-Object System.Drawing.PointF(1280, 720))
    ))
    $hill.Dispose()
    Save-Canvas $c (Join-Path $OutDir 'cg\sky.png')
}

# ---- Placeholder audio: pure synthetic sine WAVs (no external assets) ----
function Write-Wav([string]$path, [int]$sampleRate, [float]$seconds,
                   [System.Func[float, float, float]]$sample) {
    $n = [int]($sampleRate * $seconds)
    $data = New-Object byte[] ($n * 2)
    for ($i = 0; $i -lt $n; $i++) {
        $t = $i / $sampleRate
        $v = $sample.Invoke($t, $seconds)
        $s = [int16]([Math]::Max(-1.0, [Math]::Min(1.0, $v)) * 32767)
        [BitConverter]::GetBytes($s).CopyTo($data, $i * 2)
    }
    $bytes = New-Object byte[] (44 + $data.Length)
    [Text.Encoding]::ASCII.GetBytes('RIFF').CopyTo($bytes, 0)
    [BitConverter]::GetBytes([int](36 + $data.Length)).CopyTo($bytes, 4)
    [Text.Encoding]::ASCII.GetBytes('WAVE').CopyTo($bytes, 8)
    [Text.Encoding]::ASCII.GetBytes('fmt ').CopyTo($bytes, 12)
    [BitConverter]::GetBytes([int]16).CopyTo($bytes, 16)
    [BitConverter]::GetBytes([int16]1).CopyTo($bytes, 20)
    [BitConverter]::GetBytes([int16]1).CopyTo($bytes, 22)
    [BitConverter]::GetBytes([int]$sampleRate).CopyTo($bytes, 24)
    [BitConverter]::GetBytes([int]($sampleRate * 2)).CopyTo($bytes, 28)
    [BitConverter]::GetBytes([int16]2).CopyTo($bytes, 32)
    [BitConverter]::GetBytes([int16]16).CopyTo($bytes, 34)
    [Text.Encoding]::ASCII.GetBytes('data').CopyTo($bytes, 36)
    [BitConverter]::GetBytes([int]$data.Length).CopyTo($bytes, 40)
    $data.CopyTo($bytes, 44)
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
    [IO.File]::WriteAllBytes($path, $bytes)
    Write-Host "  wrote $path"
}

function New-BgmDemo {
    # 8 秒 Am 和弦 + 轻微颤音，带淡入淡出，可循环
    $freqs = @(220.0, 277.18, 329.63, 440.0)
    $fn = {
        param([float]$t, [float]$dur)
        $fade = [Math]::Min(1.0, $t / 1.0) * [Math]::Min(1.0, ($dur - $t) / 1.0)
        $v = 0.0
        foreach ($f in $freqs) { $v += [Math]::Sin(2 * [Math]::PI * $f * $t) }
        $v *= 0.09 * (1.0 + 0.25 * [Math]::Sin(2 * [Math]::PI * 0.25 * $t)) * $fade
        return $v
    }
    Write-Wav (Join-Path $OutDir 'audio\bgm_demo.wav') 44100 8.0 $fn
}

function New-SeClick {
    # 0.16 秒短促提示音
    $fn = {
        param([float]$t, [float]$dur)
        $fade = [Math]::Min(1.0, $t / 0.02) * [Math]::Min(1.0, ($dur - $t) / 0.1)
        return [Math]::Sin(2 * [Math]::PI * 880 * $t) * 0.5 * $fade
    }
    Write-Wav (Join-Path $OutDir 'audio\se_click.wav') 44100 0.16 $fn
}

New-BgTitle
New-BgClassroom
New-BgStreet

New-Body 'hero' '#3E5C8F' '#8FAEFF' $null
New-Body 'heroine' '#F0D3DE' '#FFB9C8' '#C97B8E'

foreach ($kind in @('normal', 'happy', 'sad', 'angry', 'blush')) {
    New-Face 'hero' '#2B2B33' '#3A4A6B' $kind
    New-Face 'heroine' '#8A4B5E' '#6B3A4E' $kind
}

New-CgSchool
New-CgSky
New-BgmDemo
New-SeClick

Write-Host "assets generated under $OutDir"
