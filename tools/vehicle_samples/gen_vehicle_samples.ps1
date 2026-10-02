# Generates sample_vehicle_{2,4,8,16}ecu.canprj from one vehicle model.
param([string]$OutDir)

# ECU catalogue: key, label (<= 10 chars), name, frames (id, dlc, period ms, name), receives (CAN IDs; $null = all)
$ecus = [ordered]@{
  Engine  = @{ Label='エンジン';    Name='ECM Engine Control Module';
    Frames=@('0x0C0,8,10,EngineRPM_ThrottlePos', '0x0C8,8,20,EngineTorque_Actual',
             '0x1A0,8,100,Coolant_OilTemp', '0x1A8,4,200,FuelLevel_Consumption');
    Rx=@(0x0A8,0x0E0,0x380,0x3E8) }
  Trans   = @{ Label='トランスミッション';     Name='TCM Transmission Control';
    Frames=@('0x0E0,4,20,GearPosition_ShiftReq', '0x0E8,8,100,ATF_Temp_ClutchStatus');
    Rx=@(0x0C0,0x0C8,0x0A0) }
  Brake   = @{ Label='ブレーキ/ESC'; Name='ESC Brake and Stability Control';
    Frames=@('0x0A0,8,10,WheelSpeed_FL_FR_RL_RR', '0x0A8,8,20,BrakePressure_TCS_Req',
             '0x0B0,8,10,YawRate_LateralAccel');
    Rx=@(0x0C0,0x0C8,0x0E0,0x0F0,0x128) }
  Steer   = @{ Label='ステアリング';  Name='EPS Electric Power Steering';
    Frames=@('0x0F0,8,10,SteeringAngle_Torque', '0x0F8,4,100,EPS_Status');
    Rx=@(0x0A0,0x110) }
  Airbag  = @{ Label='エアバッグ';    Name='SRS Airbag Control Unit';
    Frames=@('0x050,4,100,SRS_Status_SeatBelt');
    Rx=@(0x0A0,0x0B0) }
  Gateway = @{ Label='ゲートウェイ';   Name='CGW Central Gateway';
    Frames=@('0x200,8,50,VehicleState_Relay', '0x208,8,1000,Diag_SessionStatus');
    Rx=$null }
  Body    = @{ Label='ボデー';      Name='BCM Body Control Module';
    Frames=@('0x300,4,100,DoorLock_Command', '0x308,4,100,Lighting_TurnSignal',
             '0x310,2,200,Wiper_Washer');
    Rx=@(0x050,0x0A0,0x320,0x328,0x340) }
  Meter   = @{ Label='メーター';     Name='IPC Instrument Cluster';
    Frames=@('0x3E0,8,500,Odometer_TripMeter', '0x3E8,2,200,Meter_DriveModeSwitch');
    Rx=@(0x050,0x0A0,0x0C0,0x0E0,0x1A0,0x1A8,0x1C0,0x200,0x208,0x300,0x308,0x360) }
  Climate = @{ Label='エアコン';   Name='HVAC Climate Control';
    Frames=@('0x380,8,200,AC_Compressor_Request', '0x388,4,500,CabinTemp_FanLevel');
    Rx=@(0x0C8,0x1A0) }
  Camera  = @{ Label='前方カメラ';  Name='ADAS Front Camera';
    Frames=@('0x110,8,50,LaneMarking_Info', '0x118,8,50,TrafficSign_Object');
    Rx=@(0x0A0,0x0B0,0x0F0) }
  Radar   = @{ Label='レーダー';     Name='ACC Front Radar';
    Frames=@('0x120,8,50,RadarObject_List', '0x128,8,20,ACC_DecelRequest');
    Rx=@(0x0A0,0x0B0,0x110) }
  DoorFL  = @{ Label='左前ドア';   Name='DCU Door Control Front Left';
    Frames=@('0x320,4,100,DoorFL_Window_Mirror');
    Rx=@(0x300,0x308) }
  DoorFR  = @{ Label='右前ドア';   Name='DCU Door Control Front Right';
    Frames=@('0x328,4,100,DoorFR_Window_Mirror');
    Rx=@(0x300,0x308) }
  Battery = @{ Label='バッテリー';   Name='BMS 12V Battery Sensor';
    Frames=@('0x1C0,8,100,BatteryVoltage_SOC');
    Rx=@(0x0C0) }
  TPMS    = @{ Label='タイヤ空気圧';      Name='TPMS Tire Pressure Monitor';
    Frames=@('0x360,8,1000,TirePressure_Temp');
    Rx=@(0x0A0) }
  Audio   = @{ Label='ナビ/オーディオ'; Name='IVI Infotainment Head Unit';
    Frames=@('0x400,8,200,Audio_Volume_Source', '0x408,8,500,Navi_Guidance');
    Rx=@(0x0A0,0x200,0x380,0x3E0) }
}

$samples = [ordered]@{
  'sample_vehicle_2ecu'  = @{ Ecus=@('Engine','Meter');
    Title='2 ECU: engine ECU and instrument cluster (minimum powertrain-to-meter link)' }
  'sample_vehicle_4ecu'  = @{ Ecus=@('Engine','Brake','Body','Meter');
    Title='4 ECU: engine, brake/ESC, body control and instrument cluster' }
  'sample_vehicle_8ecu'  = @{ Ecus=@('Engine','Trans','Brake','Steer','Airbag','Gateway','Body','Meter');
    Title='8 ECU: powertrain and chassis, airbag, gateway, body and meter' }
  'sample_vehicle_16ecu' = @{ Ecus=@('Engine','Trans','Brake','Steer','Airbag','Gateway','Body','Meter',
                                     'Climate','Camera','Radar','DoorFL','DoorFR','Battery','TPMS','Audio');
    Title='16 ECU: full vehicle with ADAS, comfort, door, battery, TPMS and infotainment ECUs' }
}

function Hex3([int]$v) { '0x{0:X3}' -f $v }

foreach ($sampleName in $samples.Keys) {
  $s = $samples[$sampleName]
  $present = @{}
  foreach ($k in $s.Ecus) {
    foreach ($fs in $ecus[$k].Frames) {
      $id = [Convert]::ToInt32(($fs -split ',')[0], 16)
      # keep a frame only when some other ECU of this sample receives it
      $heard = @($s.Ecus | Where-Object { $_ -ne $k -and ($ecus[$_].Rx -eq $null -or $ecus[$_].Rx -contains $id) }).Count -gt 0
      if ($heard) { $present[$id] = $true }
    }
  }
  $sb = New-Object Text.StringBuilder
  $L = { param($t) [void]$sb.Append($t + "`r`n") }
  & $L "; Vehicle-like sample project: $($s.Title)."
  & $L "; 500 kbps high-speed CAN. Lower CAN IDs have higher priority: airbag, chassis and"
  & $L "; powertrain (10-20 ms) before body and comfort (100-1000 ms). RxFilter lists the frames"
  if ($s.Ecus -contains 'Gateway') { & $L "; each ECU really uses, so the frame display shows realistic receivers; the gateway"; & $L "; receives every frame. Start offsets are staggered as on a real bus; some frames still" } else { & $L "; each ECU really uses, so the frame display shows realistic receivers."; & $L "; Start offsets are staggered as on a real bus; some frames still" }
  & $L "; meet and show arbitration. Speed -3 (1/8 of real time) makes the traffic easy to follow."
  & $L ""
  & $L "[Project]"
  & $L "EcuCount=$($s.Ecus.Count)"
  & $L "DurationSec=5"
  & $L "FrameDisplay=1"
  & $L "Speed=-3"
  & $L ""
  & $L "[Network]"
  & $L "BitRateBps=500000"
  $n = 0
  foreach ($k in $s.Ecus) {
    $n++
    $e = $ecus[$k]
    & $L ""
    & $L "[Node$n]"
    & $L "NodeId=$n"
    & $L "Name=$($e.Name)"
    & $L "Label=$($e.Label)"
    if ($e.Rx -ne $null) {
      $rx = @($e.Rx | Where-Object { $present.ContainsKey([int]$_) } | Sort-Object)
      if ($rx.Count -eq 0) { throw "$sampleName $k receives nothing" }
      & $L ("RxFilter=" + (($rx | ForEach-Object { Hex3 $_ }) -join ','))
    }
    $m = 0
    foreach ($fs in $e.Frames) {
      $f = $fs -split ','   # "id,dlc,period ms,name"
      if (-not $present.ContainsKey([Convert]::ToInt32($f[0], 16))) { continue }   # nobody in this sample receives it
      $f[0] = [Convert]::ToInt32($f[0], 16)
      $m++
      $period = [int]$f[2]
      $offsetMs = ($n * 3 + $m * 2) % $period    # staggered, always < period
      & $L ""
      & $L "[Node$n.Frame$m]"
      & $L "Name=$($f[3])"
      & $L ("CanId=" + (Hex3 $f[0]))
      & $L "Dlc=$($f[1])"
      & $L ("PeriodUs=" + ($period * 1000))
      & $L ("InitialOffsetUs=" + ($offsetMs * 1000))
    }
  }
  $path = Join-Path $OutDir "$sampleName.canprj"
  [IO.File]::WriteAllText($path, $sb.ToString(), (New-Object Text.UTF8Encoding($false)))
  "$path"
}
