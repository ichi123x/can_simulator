param([string]$Project, [string]$Prefix, [long]$DurationUs)

# ---- project: senders, periods, offsets, receive filters ----
$nodes = @{}; $frames = @{}; $cur = $null; $curFrame = $null
foreach ($line in [IO.File]::ReadAllLines($Project)) {
  $l = ($line -replace ';.*$', '').Trim()
  if ($l -match '^\[Node(\d+)\]$') { $cur = [int]$matches[1]; $nodes[$cur] = @{ Rx = $null }; $curFrame = $null; continue }
  if ($l -match '^\[Node(\d+)\.Frame(\d+)\]$') { $curFrame = @{ Node = [int]$matches[1] }; continue }
  if ($l -match '^([A-Za-z]+)=(.*)$') {
    $k = $matches[1]; $v = $matches[2]
    if ($curFrame -ne $null) {
      $curFrame[$k] = $v
      if ($k -eq 'InitialOffsetUs') { $frames[[Convert]::ToInt32($curFrame.CanId, 16)] = $curFrame }
    } elseif ($cur -ne $null -and $k -eq 'RxFilter') {
      $nodes[$cur].Rx = @($v.Split(',') | ForEach-Object { [Convert]::ToInt32($_.Trim(), 16) })
    }
  }
}
$normalUs = 22   # 11 bit times at 500 kbps (startup.csv)

$rows = Import-Csv "$Prefix.log.csv"
$problems = New-Object System.Collections.ArrayList
function Problem($msg) { if ($problems.Count -lt 20) { [void]$problems.Add($msg) } else { $script:more++ } }
$more = 0

# 1. statuses
$statuses = $rows | Group-Object Status | ForEach-Object { "$($_.Name)=$($_.Count)" }
foreach ($r in $rows) { if ($r.Status -ne 'SUCCESS' -and $r.Status -ne 'ARBITRATION_LOST_RETRY') { Problem "unexpected status $($r.Status) id=$($r.CAN_ID) t=$($r.Timestamp_us)" } }

# group rows into transmissions (one frame on the bus = same id + same Tx_Start)
$tx = $rows | Group-Object CAN_ID, Tx_Start
$byId = @{}
foreach ($g in $tx) {
  $r0 = $g.Group[0]; $id = [Convert]::ToInt32($r0.CAN_ID, 16)
  $f = $frames[$id]
  if ($f -eq $null) { Problem "frame $($r0.CAN_ID) is not in the project"; continue }
  # 2. sender and DLC
  if ([int]$r0.Tx_Node -ne $f.Node) { Problem "$($r0.CAN_ID) sent by $($r0.Tx_Node), expected $($f.Node)" }
  if ([int]$r0.DLC -ne [int]$f.Dlc) { Problem "$($r0.CAN_ID) DLC $($r0.DLC), expected $($f.Dlc)" }
  # 3. receivers = every other node whose filter accepts the id (no filter = all)
  $expected = @($nodes.Keys | Where-Object { $_ -ne $f.Node -and ($nodes[$_].Rx -eq $null -or $nodes[$_].Rx -contains $id) } | Sort-Object)
  $actual = @($g.Group | ForEach-Object { [int]$_.Rx_Node } | Sort-Object)
  if (($expected -join ',') -ne ($actual -join ',')) { Problem "$($r0.CAN_ID) at $($r0.Tx_Start): receivers [$($actual -join ',')] expected [$($expected -join ',')]" }
  # 4. reception timing: Rx_End = Tx_End, no receive delay
  foreach ($r in $g.Group) { if ($r.Rx_End -ne $r.Tx_End -or $r.Rx_Process -ne $r.Rx_End) { Problem "$($r.CAN_ID) rx timing differs at $($r.Tx_Start)"; break } }
  if (-not $byId.ContainsKey($id)) { $byId[$id] = New-Object System.Collections.ArrayList }
  [void]$byId[$id].Add([pscustomobject]@{ Id = $id; Req = [long]$r0.Tx_Request; Start = [long]$r0.Tx_Start; End = [long]$r0.Tx_End; Retry = [int]$r0.Retry_Count })
}

# 5. count and period of every frame
$countReport = @()
foreach ($id in ($frames.Keys | Sort-Object)) {
  $f = $frames[$id]; $period = [long]$f.PeriodUs; $first = $normalUs + [long]$f.InitialOffsetUs
  $list = @($byId[$id] | Sort-Object Req)
  $requested = [math]::Floor(($DurationUs - 1 - $first) / $period) + 1
  if ($list.Count -lt $requested - 1 -or $list.Count -gt $requested) { Problem ("0x{0:X3}: {1} sent, {2} requested" -f $id, $list.Count, $requested) }
  if ($list.Count -gt 0 -and $list[0].Req -ne $first) { Problem ("0x{0:X3}: first request {1}, expected {2}" -f $id, $list[0].Req, $first) }
  for ($i = 1; $i -lt $list.Count; $i++) { if ($list[$i].Req - $list[$i-1].Req -ne $period) { Problem ("0x{0:X3}: request interval {1} at {2}" -f $id, ($list[$i].Req - $list[$i-1].Req), $list[$i].Req); break } }
  $countReport += ("0x{0:X3} {1,5}/{2,-5}" -f $id, $list.Count, $requested)
}

# 6. bus: one frame at a time, 3-bit intermission (6 us) between frames
$all = @($byId.Values | ForEach-Object { $_ } | Sort-Object Start)
for ($i = 1; $i -lt $all.Count; $i++) { if ($all[$i].Start -lt $all[$i-1].End + 6) { Problem ("bus overlap: 0x{0:X3} starts {1}, 0x{2:X3} ended {3}" -f $all[$i].Id, $all[$i].Start, $all[$i-1].Id, $all[$i-1].End) } }

# 7. arbitration: a frame waiting when another starts must have a higher CAN ID
$arbChecked = 0
for ($i = 0; $i -lt $all.Count; $i++) {
  $w = $all[$i]
  for ($j = $i - 1; $j -ge 0 -and $all[$j].Start -gt $w.Req - 2000; $j--) {
    $s = $all[$j]
    if ($w.Req -lt $s.Start -and $w.Start -gt $s.Start) { $arbChecked++; if ($w.Id -lt $s.Id) { Problem ("priority: 0x{0:X3} waited while 0x{1:X3} won at {2}" -f $w.Id, $s.Id, $s.Start) } }
  }
}
$retries = @($all | Where-Object { $_.Retry -gt 0 })
$maxWait = ($all | ForEach-Object { $_.Start - $_.Req } | Measure-Object -Maximum).Maximum

"statuses: $($statuses -join ', ')"
"transmissions: $($all.Count)  rows: $($rows.Count)"
"frames sent/requested:"; $countReport | ForEach-Object { "  $_" }
"arbitration pairs checked: $arbChecked, frames that lost arbitration at least once: $($retries.Count), longest wait: $maxWait us"
if ($problems.Count -eq 0) { "RESULT: no problems found" } else { "RESULT: $($problems.Count + $more) problem(s)"; $problems }
