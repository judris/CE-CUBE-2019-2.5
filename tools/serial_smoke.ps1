[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$Port,

    [Parameter(Mandatory = $false)]
    [ValidateSet('manual-smoke', 'current-build-smoke', 'usb-build-smoke',
                 'automation-smoke', 'seed-minimal-protocol',
                 'eeprom-roundtrip', 'list-ports')]
    [string]$Mode = 'manual-smoke',

    [Parameter(Mandatory = $false)]
    [int]$BaudRate = 115200,

    [Parameter(Mandatory = $false)]
    [int]$OpenDelayMs = 3000,

    [Parameter(Mandatory = $false)]
    [int]$ImmediateTimeoutMs = 500,

    [Parameter(Mandatory = $false)]
    [int]$FollowupTimeoutMs = 2000,

    [Parameter(Mandatory = $false)]
    [int]$AutomationTimeoutMs = 3000,

    [Parameter(Mandatory = $false)]
    [int]$ExpectedFaults = -1,

    [Parameter(Mandatory = $false)]
    [string]$TranscriptPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:TranscriptPath = $null
$script:ProtocolVersion = 2
$script:ProtocolChunkDataBytes = 8
$script:ProtocolStoreFaultBit = 16
$script:AnalysisTimeToleranceMs = 25

$script:MessageKind = @{
    Telemetry = 1
    Command = 2
    Ack = 3
    Error = 4
    Event = 5
}

$script:AckCode = @{
    Accepted = 0
    Status = 1
}

$script:ErrorCode = @{
    InvalidJson = 1
    InvalidField = 2
    InvalidCommand = 3
    Busy = 4
    TransportOverflow = 5
    RfCrc = 6
    RfFragment = 7
    SensorIo = 8
    SensorConfig = 9
    NotReady = 10
    ProtocolInvalid = 11
    ProtocolCrc = 12
    ProtocolBounds = 13
    ProtocolOpcode = 14
    Storage = 15
    Unavailable = 16
}

$script:EventCode = @{
    RunStart = 1
    RunStop = 2
    SampleReady = 3
    ProtocolInfo = 4
}

$script:CommandId = @{
    SensorSetRate = 1
    SensorSetExcitation = 2
    SensorTempComp = 3
    SensorAutoZero = 4
    LiftMove = 5
    CarouselStep = 6
    CarouselGotoSlot = 7
    CarouselAdjust = 8
    HvSet = 9
    PumpSet = 10
    ValveSet = 11
    InjectionConfigure = 12
    RunStart = 13
    RunStop = 14
    RunStatus = 15
    ProtocolBegin = 16
    ProtocolChunk = 17
    ProtocolCommit = 18
    ProtocolInfo = 19
    ProtocolClear = 20
    StatusGet = 21
}

function New-TranscriptPath {
    param(
        [string]$SelectedMode,
        [string]$SerialPortName,
        [string]$RequestedPath
    )

    if ($RequestedPath -ne '') {
        return $RequestedPath
    }

    $scriptRoot = Split-Path -Parent $PSCommandPath
    $repoRoot = Split-Path -Parent $scriptRoot
    $artifactRoot = Join-Path $repoRoot 'artifacts\serial-smoke'
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $safePort = $SerialPortName
    if ([string]::IsNullOrWhiteSpace($safePort)) {
        $safePort = 'no-port'
    }
    return (Join-Path $artifactRoot "$SelectedMode-$safePort-$timestamp.log")
}

function Initialize-Transcript {
    param([string]$Path)

    $parent = Split-Path -Parent $Path
    if (($parent -ne '') -and -not (Test-Path -LiteralPath $parent)) {
        $null = New-Item -ItemType Directory -Force -Path $parent
    }

    $script:TranscriptPath = $Path
    $header = @(
        "CE-CUBE serial smoke transcript"
        "Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss K')"
        "Mode: $Mode"
        "Port: $Port"
        ''
    )
    Set-Content -LiteralPath $script:TranscriptPath -Value $header
}

function Write-Log {
    param([string]$Text)

    Write-Host $Text
    if ($null -ne $script:TranscriptPath) {
        Add-Content -LiteralPath $script:TranscriptPath -Value $Text
    }
}

function Fail-Test {
    param([string]$Message)

    throw $Message
}

function Get-AvailablePorts {
    return [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
}

function Get-ModeExpectedFaults {
    param(
        [string]$SelectedMode,
        [int]$RequestedFaults
    )

    if ($SelectedMode -eq 'current-build-smoke') {
        return 38
    }
    if ($SelectedMode -eq 'usb-build-smoke') {
        return 36
    }
    if ($SelectedMode -eq 'automation-smoke') {
        if ($RequestedFaults -ge 0) {
            return $RequestedFaults
        }
        return 36
    }
    if ($SelectedMode -eq 'seed-minimal-protocol') {
        if ($RequestedFaults -ge 0) {
            return $RequestedFaults
        }
        return 0
    }
    if ($SelectedMode -eq 'eeprom-roundtrip') {
        if ($RequestedFaults -ge 0) {
            return $RequestedFaults
        }
        return 4
    }
    if ($RequestedFaults -ge 0) {
        return $RequestedFaults
    }
    return 38
}

function Open-SerialPort {
    param(
        [string]$SerialPortName,
        [int]$SerialBaudRate
    )

    $serialPort = New-Object System.IO.Ports.SerialPort
    $serialPort.PortName = $SerialPortName
    $serialPort.BaudRate = $SerialBaudRate
    $serialPort.DataBits = 8
    $serialPort.Parity = [System.IO.Ports.Parity]::None
    $serialPort.StopBits = [System.IO.Ports.StopBits]::One
    $serialPort.Handshake = [System.IO.Ports.Handshake]::None
    $serialPort.NewLine = "`n"
    $serialPort.Encoding = [System.Text.Encoding]::ASCII
    $serialPort.DtrEnable = $true
    $serialPort.RtsEnable = $false
    $serialPort.Open()
    $serialPort.DiscardInBuffer()
    $serialPort.DiscardOutBuffer()
    return $serialPort
}

function Get-JsonFieldValue {
    param(
        [object]$JsonObject,
        [string]$Name
    )

    $property = $JsonObject.PSObject.Properties[$Name]
    if ($null -eq $property) {
        Fail-Test "Missing JSON field '$Name'"
    }
    return $property.Value
}

function Has-JsonField {
    param(
        [object]$JsonObject,
        [string]$Name
    )

    return $null -ne $JsonObject.PSObject.Properties[$Name]
}

function Assert-Equal {
    param(
        [object]$Actual,
        [object]$Expected,
        [string]$Label
    )

    if ($Actual -ne $Expected) {
        Fail-Test "$Label mismatch. Expected '$Expected', got '$Actual'"
    }
}

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Label
    )

    if (-not $Condition) {
        Fail-Test $Label
    }
}

function Assert-IntWithin {
    param(
        [int]$Actual,
        [int]$Expected,
        [int]$Tolerance,
        [string]$Label
    )

    $delta = [Math]::Abs($Actual - $Expected)
    if ($delta -gt $Tolerance) {
        Fail-Test "$Label mismatch. Expected '$Expected +/- $Tolerance', got '$Actual'"
    }
}

function Compute-Crc16Ccitt {
    param([byte[]]$Data)

    $crc = 0xFFFF
    for ($index = 0; $index -lt $Data.Length; ++$index) {
        $crc = ($crc -bxor (($Data[$index] -band 0xFF) -shl 8)) -band 0xFFFF
        for ($bit = 0; $bit -lt 8; ++$bit) {
            if (($crc -band 0x8000) -ne 0) {
                $crc = ((($crc -shl 1) -bxor 0x1021) -band 0xFFFF)
            } else {
                $crc = (($crc -shl 1) -band 0xFFFF)
            }
        }
    }

    return [uint16]$crc
}

function Get-MinimalProtocolDefinition {
    $metadata = [ordered]@{
        sc = 12
        b1 = 0
        b2 = 1
        s1 = 5
        sn = 1
        rr = 1
        im = 1
        cd = 1000
        jd = 1000
        dd = 1000
        w0 = 1000
        w1 = 0
        w2 = 0
        w3 = 0
        w4 = 0
        w5 = 0
        w6 = 0
        w7 = 0
    }
    $program = [byte[]]@(
        0x00,
        0x61,
        0x50,
        0x62,
        0x00
    )

    return @{
        Metadata = $metadata
        PrepareLength = 1
        Program = $program
    }
}

function Convert-ProgramChunkToHex {
    param(
        [byte[]]$Program,
        [int]$Offset
    )

    $chunk = New-Object byte[] $script:ProtocolChunkDataBytes
    for ($index = 0; $index -lt $script:ProtocolChunkDataBytes; ++$index) {
        $programIndex = $Offset + $index
        if ($programIndex -lt $Program.Length) {
            $chunk[$index] = $Program[$programIndex]
        } else {
            $chunk[$index] = 0
        }
    }

    return ([System.BitConverter]::ToString($chunk)).Replace('-', '')
}

function Receive-JsonMessage {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [int]$TimeoutMs
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    while ([DateTime]::UtcNow -lt $deadline) {
        $remainingMs = [int]($deadline - [DateTime]::UtcNow).TotalMilliseconds
        if ($remainingMs -le 0) {
            break
        }

        $serialPort.ReadTimeout = [Math]::Min($remainingMs, 200)
        try {
            $line = $serialPort.ReadLine()
        } catch [System.TimeoutException] {
            continue
        }

        $line = $line.Trim()
        if ($line.Length -eq 0) {
            continue
        }

        Write-Log "RX $line"
        try {
            $json = $line | ConvertFrom-Json
        } catch {
            Fail-Test "Failed to parse JSON line: $line"
        }

        return @{
            RawLine = $line
            Json = $json
        }
    }

    Fail-Test "Timed out waiting for JSON after $TimeoutMs ms"
}

function Receive-ExpectedMessage {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [int]$TimeoutMs,
        [int]$ExpectedKind,
        [bool]$MatchSeq = $false,
        [uint16]$ExpectedSeq = 0,
        [int[]]$SkippableKinds = @()
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    while ([DateTime]::UtcNow -lt $deadline) {
        $remainingMs = [int]($deadline - [DateTime]::UtcNow).TotalMilliseconds
        if ($remainingMs -le 0) {
            break
        }

        $message = Receive-JsonMessage -SerialPort $SerialPort -TimeoutMs $remainingMs
        $kind = [int](Get-JsonFieldValue $message.Json 'k')
        if ($kind -eq $ExpectedKind) {
            if ($MatchSeq) {
                Assert-Equal -Actual (Get-JsonFieldValue $message.Json 's') `
                    -Expected $ExpectedSeq -Label "kind $ExpectedKind seq"
            }
            return $message
        }

        $skip = $false
        foreach ($skippableKind in $SkippableKinds) {
            if ($kind -eq $skippableKind) {
                $skip = $true
                break
            }
        }

        if ($skip) {
            Write-Log "SKIP kind $kind while waiting for kind $ExpectedKind"
            continue
        }

        Fail-Test "Expected kind '$ExpectedKind', got '$kind'"
    }

    Fail-Test "Timed out waiting for kind $ExpectedKind after $TimeoutMs ms"
}

function New-CommandJson {
    param(
        [uint16]$Seq,
        [uint16]$Command,
        [System.Collections.IDictionary]$Fields
    )

    $body = [ordered]@{
        v = $script:ProtocolVersion
        k = $script:MessageKind.Command
        s = $Seq
        c = $Command
    }

    if ($null -ne $Fields) {
        foreach ($key in $Fields.Keys) {
            $body[$key] = $Fields[$key]
        }
    }

    return ($body | ConvertTo-Json -Compress)
}

function Send-CommandJson {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [string]$Json
    )

    Write-Log "TX $Json"
    $SerialPort.Write($Json + "`n")
}

function Invoke-CommandExpectAck {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq,
        [uint16]$Command,
        [System.Collections.IDictionary]$Fields,
        [string]$Label
    )

    $seq = [uint16]$NextSeq.Value
    $NextSeq.Value = [uint16]($NextSeq.Value + 1)
    $json = New-CommandJson -Seq $seq -Command $Command -Fields $Fields
    Send-CommandJson -SerialPort $SerialPort -Json $json
    $message = Receive-ExpectedMessage -SerialPort $SerialPort `
        -TimeoutMs $ImmediateTimeoutMs -ExpectedKind $script:MessageKind.Ack `
        -MatchSeq $true -ExpectedSeq $seq `
        -SkippableKinds @($script:MessageKind.Telemetry, $script:MessageKind.Event)
    Assert-Equal -Actual (Get-JsonFieldValue $message.Json 'a') `
        -Expected $script:AckCode.Accepted -Label "Ack code for $Label"
    return $message.Json
}

function Invoke-CommandExpectError {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq,
        [uint16]$Command,
        [System.Collections.IDictionary]$Fields,
        [int]$ExpectedCode,
        [string]$Label
    )

    $seq = [uint16]$NextSeq.Value
    $NextSeq.Value = [uint16]($NextSeq.Value + 1)
    $json = New-CommandJson -Seq $seq -Command $Command -Fields $Fields
    Send-CommandJson -SerialPort $SerialPort -Json $json
    $message = Receive-ExpectedMessage -SerialPort $SerialPort `
        -TimeoutMs $ImmediateTimeoutMs -ExpectedKind $script:MessageKind.Error `
        -MatchSeq $true -ExpectedSeq $seq `
        -SkippableKinds @($script:MessageKind.Telemetry, $script:MessageKind.Event)
    Assert-Equal -Actual (Get-JsonFieldValue $message.Json 'e') `
        -Expected $ExpectedCode -Label "Error code for $Label"
}

function Invoke-StatusRequest {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq
    )

    $seq = [uint16]$NextSeq.Value
    $NextSeq.Value = [uint16]($NextSeq.Value + 1)
    $json = New-CommandJson -Seq $seq -Command $script:CommandId.StatusGet -Fields $null
    Send-CommandJson -SerialPort $SerialPort -Json $json

    $ack = Receive-ExpectedMessage -SerialPort $SerialPort `
        -TimeoutMs $ImmediateTimeoutMs -ExpectedKind $script:MessageKind.Ack `
        -MatchSeq $true -ExpectedSeq $seq `
        -SkippableKinds @($script:MessageKind.Telemetry, $script:MessageKind.Event)
    Assert-Equal -Actual (Get-JsonFieldValue $ack.Json 'a') `
        -Expected $script:AckCode.Status -Label 'Ack code for status.get'

    $telemetry = Receive-ExpectedMessage -SerialPort $SerialPort `
        -TimeoutMs $FollowupTimeoutMs -ExpectedKind $script:MessageKind.Telemetry `
        -SkippableKinds @($script:MessageKind.Event)
    Assert-Equal -Actual (Get-JsonFieldValue $telemetry.Json 'v') `
        -Expected $script:ProtocolVersion -Label 'Telemetry protocol version'
    return $telemetry.Json
}

function Assert-BaselineTelemetry {
    param(
        [object]$Telemetry,
        [int]$Faults
    )

    $protocolValid = [int](Get-JsonFieldValue $Telemetry 'pv')
    Assert-True -Condition (($protocolValid -eq 0) -or ($protocolValid -eq 1)) `
        -Label 'pv should be 0 or 1 on an idle board'
    Assert-Equal -Actual (Get-JsonFieldValue $Telemetry 'hv') -Expected 0 `
        -Label 'hv'
    Assert-Equal -Actual (Get-JsonFieldValue $Telemetry 'pm') -Expected 0 `
        -Label 'pm'
    Assert-Equal -Actual (Get-JsonFieldValue $Telemetry 'v1') -Expected 0 `
        -Label 'v1'
    Assert-Equal -Actual (Get-JsonFieldValue $Telemetry 'v2') -Expected 0 `
        -Label 'v2'
    Assert-Equal -Actual (Get-JsonFieldValue $Telemetry 'rs') -Expected 0 `
        -Label 'rs'
    Assert-Equal -Actual (Get-JsonFieldValue $Telemetry 'cs') -Expected 0 `
        -Label 'cs'
    Assert-Equal -Actual (Get-JsonFieldValue $Telemetry 'ps') -Expected 1 `
        -Label 'ps'
    Assert-Equal -Actual (Get-JsonFieldValue $Telemetry 'ss') -Expected 2 `
        -Label 'ss'
    $expectedFaults = $Faults
    if ($protocolValid -eq 0) {
        $expectedFaults += $script:ProtocolStoreFaultBit
    }
    Assert-Equal -Actual (Get-JsonFieldValue $Telemetry 'ff') -Expected $expectedFaults `
        -Label 'ff'
    Assert-True -Condition (-not (Has-JsonField $Telemetry 'gt')) `
        -Label 'Idle telemetry should omit gt'
    Assert-True -Condition (-not (Has-JsonField $Telemetry 'at')) `
        -Label 'Idle telemetry should omit at'
}

function Wait-ForTelemetryField {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq,
        [string]$FieldName,
        [object]$ExpectedValue,
        [int]$TimeoutMs
    )

    $pollIntervalMs = 100
    $attemptCount = [Math]::Max(1, [int][Math]::Ceiling($TimeoutMs / $pollIntervalMs))
    for ($attempt = 0; $attempt -lt $attemptCount; ++$attempt) {
        if ($attempt -gt 0) {
            Start-Sleep -Milliseconds $pollIntervalMs
        }

        $telemetry = Invoke-StatusRequest -SerialPort $SerialPort -NextSeq $NextSeq
        if ((Get-JsonFieldValue $telemetry $FieldName) -eq $ExpectedValue) {
            return $telemetry
        }
    }

    Fail-Test "Timed out waiting for telemetry field '$FieldName' to become '$ExpectedValue'"
}

function Invoke-OutputLatchCheck {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq,
        [uint16]$Command,
        [System.Collections.IDictionary]$Fields,
        [string]$Label,
        [string]$TelemetryField,
        [int]$ExpectedValue
    )

    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $Command -Fields $Fields -Label $Label
    $telemetry = Wait-ForTelemetryField -SerialPort $SerialPort -NextSeq $NextSeq `
        -FieldName $TelemetryField -ExpectedValue $ExpectedValue `
        -TimeoutMs $FollowupTimeoutMs
    Assert-Equal -Actual (Get-JsonFieldValue $telemetry $TelemetryField) `
        -Expected $ExpectedValue -Label $TelemetryField
}

function Assert-EventMessage {
    param(
        [object]$EventJson,
        [int]$ExpectedEvent,
        [int]$ExpectedAnalysisTime = -1,
        [int]$ExpectedAnalysisTimeToleranceMs = 0
    )

    Assert-Equal -Actual (Get-JsonFieldValue $EventJson 'k') `
        -Expected $script:MessageKind.Event -Label 'Event kind'
    Assert-Equal -Actual (Get-JsonFieldValue $EventJson 'v') `
        -Expected $script:ProtocolVersion -Label 'Event protocol version'
    Assert-Equal -Actual (Get-JsonFieldValue $EventJson 'ev') `
        -Expected $ExpectedEvent -Label 'Event code'
    Assert-True -Condition (Has-JsonField $EventJson 'gt') `
        -Label 'Event should include gt'
    Assert-True -Condition ([int](Get-JsonFieldValue $EventJson 'gt') -ge 0) `
        -Label 'Event gt should be non-negative'
    if ($ExpectedAnalysisTime -ge 0) {
        Assert-True -Condition (Has-JsonField $EventJson 'at') `
            -Label 'Timed event should include at'
        if ($ExpectedAnalysisTimeToleranceMs -gt 0) {
            Assert-IntWithin -Actual ([int](Get-JsonFieldValue $EventJson 'at')) `
                -Expected $ExpectedAnalysisTime `
                -Tolerance $ExpectedAnalysisTimeToleranceMs `
                -Label 'Event analysis time'
        } else {
            Assert-Equal -Actual (Get-JsonFieldValue $EventJson 'at') `
                -Expected $ExpectedAnalysisTime -Label 'Event analysis time'
        }
    }
}

function Invoke-ProtocolInfoRequest {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq
    )

    $seq = [uint16]$NextSeq.Value
    $NextSeq.Value = [uint16]($NextSeq.Value + 1)
    $json = New-CommandJson -Seq $seq -Command $script:CommandId.ProtocolInfo -Fields $null
    Send-CommandJson -SerialPort $SerialPort -Json $json

    $ack = Receive-ExpectedMessage -SerialPort $SerialPort `
        -TimeoutMs $ImmediateTimeoutMs -ExpectedKind $script:MessageKind.Ack `
        -MatchSeq $true -ExpectedSeq $seq `
        -SkippableKinds @($script:MessageKind.Telemetry, $script:MessageKind.Event)
    Assert-Equal -Actual (Get-JsonFieldValue $ack.Json 'a') `
        -Expected $script:AckCode.Status -Label 'Ack code for protocol.info'

    $event = Receive-ExpectedMessage -SerialPort $SerialPort `
        -TimeoutMs $FollowupTimeoutMs -ExpectedKind $script:MessageKind.Event `
        -SkippableKinds @($script:MessageKind.Telemetry)
    Assert-EventMessage -EventJson $event.Json `
        -ExpectedEvent $script:EventCode.ProtocolInfo
    return $event.Json
}

function Assert-ProtocolInfoMatches {
    param(
        [object]$ProtocolInfoEvent,
        [int]$ExpectedValid,
        [int]$ExpectedPrepareLength,
        [int]$ExpectedProgramLength,
        [int]$ExpectedCrc,
        [System.Collections.IDictionary]$ExpectedMetadata = $null
    )

    Assert-Equal -Actual (Get-JsonFieldValue $ProtocolInfoEvent 'pv') `
        -Expected $ExpectedValid -Label 'protocol.info pv'
    Assert-Equal -Actual (Get-JsonFieldValue $ProtocolInfoEvent 'pl') `
        -Expected $ExpectedPrepareLength -Label 'protocol.info pl'
    Assert-Equal -Actual (Get-JsonFieldValue $ProtocolInfoEvent 'ln') `
        -Expected $ExpectedProgramLength -Label 'protocol.info ln'
    Assert-Equal -Actual (Get-JsonFieldValue $ProtocolInfoEvent 'cr') `
        -Expected $ExpectedCrc -Label 'protocol.info cr'

    if ($null -eq $ExpectedMetadata) {
        return
    }

    foreach ($fieldName in @('sc', 'b1', 'b2', 's1', 'sn', 'rr', 'im', 'cd', 'jd', 'dd')) {
        Assert-Equal -Actual (Get-JsonFieldValue $ProtocolInfoEvent $fieldName) `
            -Expected $ExpectedMetadata[$fieldName] -Label "protocol.info $fieldName"
    }

    foreach ($fieldName in @('w0', 'w1', 'w2', 'w3', 'w4', 'w5', 'w6', 'w7')) {
        Assert-Equal -Actual (Get-JsonFieldValue $ProtocolInfoEvent $fieldName) `
            -Expected $ExpectedMetadata[$fieldName] -Label "protocol.info $fieldName"
    }
}

function Invoke-ProtocolUpload {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq,
        [System.Collections.IDictionary]$Metadata,
        [byte[]]$Program,
        [int]$PrepareLength,
        [string]$LabelPrefix
    )

    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.ProtocolBegin -Fields $Metadata `
        -Label "$LabelPrefix.begin"

    for ($offset = 0; $offset -lt $Program.Length; $offset += $script:ProtocolChunkDataBytes) {
        $chunkFields = [ordered]@{
            of = $offset
            dt = Convert-ProgramChunkToHex -Program $Program -Offset $offset
        }
        $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
            -Command $script:CommandId.ProtocolChunk -Fields $chunkFields `
            -Label "$LabelPrefix.chunk@$offset"
    }

    $crc16 = Compute-Crc16Ccitt -Data $Program
    $commitFields = [ordered]@{
        pl = $PrepareLength
        ln = $Program.Length
        cr = $crc16
    }
    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.ProtocolCommit -Fields $commitFields `
        -Label "$LabelPrefix.commit"
}

function Invoke-MinimalProtocolSeed {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq
    )

    Write-Log 'Seeding minimal EEPROM protocol.'
    $baseline = Invoke-StatusRequest -SerialPort $SerialPort -NextSeq $NextSeq
    $baselineFaults = [int](Get-JsonFieldValue $baseline 'ff')
    $baselineProtocolValid = [int](Get-JsonFieldValue $baseline 'pv')
    Write-Log "Baseline protocol_valid=$baselineProtocolValid faults=$baselineFaults"

    $definition = Get-MinimalProtocolDefinition
    Invoke-ProtocolUpload -SerialPort $SerialPort -NextSeq $NextSeq `
        -Metadata $definition.Metadata -Program $definition.Program `
        -PrepareLength $definition.PrepareLength -LabelPrefix 'seed'

    $afterCommit = Invoke-StatusRequest -SerialPort $SerialPort -NextSeq $NextSeq
    Assert-Equal -Actual (Get-JsonFieldValue $afterCommit 'pv') -Expected 1 `
        -Label 'pv after seed'
    $expectedFaults = ($baselineFaults -band 0xFFEF)
    Assert-Equal -Actual (Get-JsonFieldValue $afterCommit 'ff') `
        -Expected $expectedFaults -Label 'ff after seed'
    Write-Log 'Minimal EEPROM protocol seeded successfully.'
}

function Assert-MinimalProtocolRun {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq,
        [int]$Faults
    )

    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.RunStart -Fields $null -Label 'run.start'

    $runStartMessage = Receive-ExpectedMessage -SerialPort $SerialPort `
        -TimeoutMs $ImmediateTimeoutMs -ExpectedKind $script:MessageKind.Event `
        -SkippableKinds @($script:MessageKind.Telemetry)
    Assert-EventMessage -EventJson $runStartMessage.Json `
        -ExpectedEvent $script:EventCode.RunStart -ExpectedAnalysisTime 0

    $runStopMessage = Receive-ExpectedMessage -SerialPort $SerialPort `
        -TimeoutMs $AutomationTimeoutMs -ExpectedKind $script:MessageKind.Event `
        -SkippableKinds @($script:MessageKind.Telemetry)
    Assert-EventMessage -EventJson $runStopMessage.Json `
        -ExpectedEvent $script:EventCode.RunStop -ExpectedAnalysisTime 1000 `
        -ExpectedAnalysisTimeToleranceMs $script:AnalysisTimeToleranceMs

    $completeTelemetry = Wait-ForTelemetryField -SerialPort $SerialPort `
        -NextSeq $NextSeq -FieldName 'rs' -ExpectedValue 2 `
        -TimeoutMs $AutomationTimeoutMs
    Assert-True -Condition (Has-JsonField $completeTelemetry 'at') `
        -Label 'Completed run telemetry should include at'
    Assert-IntWithin -Actual ([int](Get-JsonFieldValue $completeTelemetry 'at')) `
        -Expected 1000 -Tolerance $script:AnalysisTimeToleranceMs `
        -Label 'Completed run analysis time'
    Assert-Equal -Actual (Get-JsonFieldValue $completeTelemetry 'ff') `
        -Expected $Faults -Label 'ff after minimal protocol run'
}

function Invoke-EepromRoundTripTest {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq,
        [int]$Faults
    )

    Write-Log 'Running EEPROM round-trip test.'
    $baseline = Invoke-StatusRequest -SerialPort $SerialPort -NextSeq $NextSeq
    Assert-BaselineTelemetry -Telemetry $baseline -Faults $Faults

    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.ProtocolClear -Fields $null `
        -Label 'protocol.clear'
    $afterClear = Invoke-StatusRequest -SerialPort $SerialPort -NextSeq $NextSeq
    Assert-Equal -Actual (Get-JsonFieldValue $afterClear 'pv') -Expected 0 `
        -Label 'pv after clear'
    Assert-Equal -Actual (Get-JsonFieldValue $afterClear 'ff') `
        -Expected ($Faults + $script:ProtocolStoreFaultBit) `
        -Label 'ff after clear'

    $emptyInfo = Invoke-ProtocolInfoRequest -SerialPort $SerialPort -NextSeq $NextSeq
    Assert-ProtocolInfoMatches -ProtocolInfoEvent $emptyInfo `
        -ExpectedValid 0 -ExpectedPrepareLength 0 `
        -ExpectedProgramLength 0 -ExpectedCrc 0

    $definition = Get-MinimalProtocolDefinition
    Invoke-ProtocolUpload -SerialPort $SerialPort -NextSeq $NextSeq `
        -Metadata $definition.Metadata -Program $definition.Program `
        -PrepareLength $definition.PrepareLength -LabelPrefix 'eeprom'

    $expectedCrc = Compute-Crc16Ccitt -Data $definition.Program
    $afterCommit = Invoke-StatusRequest -SerialPort $SerialPort -NextSeq $NextSeq
    Assert-Equal -Actual (Get-JsonFieldValue $afterCommit 'pv') -Expected 1 `
        -Label 'pv after eeprom write'
    Assert-Equal -Actual (Get-JsonFieldValue $afterCommit 'ff') `
        -Expected $Faults -Label 'ff after eeprom write'

    $storedInfo = Invoke-ProtocolInfoRequest -SerialPort $SerialPort -NextSeq $NextSeq
    Assert-ProtocolInfoMatches -ProtocolInfoEvent $storedInfo `
        -ExpectedValid 1 -ExpectedPrepareLength $definition.PrepareLength `
        -ExpectedProgramLength $definition.Program.Length -ExpectedCrc $expectedCrc `
        -ExpectedMetadata $definition.Metadata

    Assert-MinimalProtocolRun -SerialPort $SerialPort -NextSeq $NextSeq `
        -Faults $Faults
    Write-Log 'EEPROM round-trip test passed.'
}

function Invoke-ManualSmoke {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq,
        [int]$Faults
    )

    Write-Log 'Running manual smoke sequence.'
    $baseline = Invoke-StatusRequest -SerialPort $SerialPort -NextSeq $NextSeq
    Assert-BaselineTelemetry -Telemetry $baseline -Faults $Faults

    Invoke-CommandExpectError -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command 99 -Fields $null -ExpectedCode $script:ErrorCode.InvalidCommand `
        -Label 'unknown command'
    Invoke-CommandExpectError -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.HvSet -Fields $null `
        -ExpectedCode $script:ErrorCode.InvalidField -Label 'hv.set'

    Invoke-OutputLatchCheck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.HvSet -Fields ([ordered]@{ en = 1 }) `
        -Label 'hv.set' -TelemetryField 'hv' -ExpectedValue 1
    Invoke-OutputLatchCheck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.PumpSet -Fields ([ordered]@{ en = 1 }) `
        -Label 'pump.set' -TelemetryField 'pm' -ExpectedValue 1
    Invoke-OutputLatchCheck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.ValveSet -Fields ([ordered]@{ vi = 1; en = 1 }) `
        -Label 'valve1.set' -TelemetryField 'v1' -ExpectedValue 1
    Invoke-OutputLatchCheck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.ValveSet -Fields ([ordered]@{ vi = 2; en = 1 }) `
        -Label 'valve2.set' -TelemetryField 'v2' -ExpectedValue 1

    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.HvSet -Fields ([ordered]@{ en = 0 }) `
        -Label 'hv.set off'
    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.PumpSet -Fields ([ordered]@{ en = 0 }) `
        -Label 'pump.set off'
    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.ValveSet -Fields ([ordered]@{ vi = 1; en = 0 }) `
        -Label 'valve1.set off'
    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.ValveSet -Fields ([ordered]@{ vi = 2; en = 0 }) `
        -Label 'valve2.set off'
    $outputsOff = Wait-ForTelemetryField -SerialPort $SerialPort -NextSeq $NextSeq `
        -FieldName 'v2' -ExpectedValue 0 -TimeoutMs $FollowupTimeoutMs
    Assert-Equal -Actual (Get-JsonFieldValue $outputsOff 'hv') -Expected 0 `
        -Label 'hv reset'
    Assert-Equal -Actual (Get-JsonFieldValue $outputsOff 'pm') -Expected 0 `
        -Label 'pm reset'
    Assert-Equal -Actual (Get-JsonFieldValue $outputsOff 'v1') -Expected 0 `
        -Label 'v1 reset'

    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.LiftMove -Fields ([ordered]@{ lp = 1 }) `
        -Label 'lift.move up'
    Invoke-CommandExpectError -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.LiftMove -Fields ([ordered]@{ lp = 0 }) `
        -ExpectedCode $script:ErrorCode.Busy -Label 'lift.move busy'
    $liftUp = Wait-ForTelemetryField -SerialPort $SerialPort -NextSeq $NextSeq `
        -FieldName 'lf' -ExpectedValue 2 -TimeoutMs $FollowupTimeoutMs
    Assert-Equal -Actual (Get-JsonFieldValue $liftUp 'lf') -Expected 2 `
        -Label 'lift up state'

    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.LiftMove -Fields ([ordered]@{ lp = 0 }) `
        -Label 'lift.move down'
    $liftDown = Wait-ForTelemetryField -SerialPort $SerialPort -NextSeq $NextSeq `
        -FieldName 'lf' -ExpectedValue 0 -TimeoutMs $FollowupTimeoutMs
    Assert-Equal -Actual (Get-JsonFieldValue $liftDown 'lf') -Expected 0 `
        -Label 'lift down state'

    $null = Invoke-CommandExpectAck -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.CarouselGotoSlot -Fields ([ordered]@{ sl = 1 }) `
        -Label 'carousel.goto_slot 1'
    Invoke-CommandExpectError -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.CarouselGotoSlot -Fields ([ordered]@{ sl = 2 }) `
        -ExpectedCode $script:ErrorCode.Busy -Label 'carousel.goto_slot busy'
    $carouselAtOne = Wait-ForTelemetryField -SerialPort $SerialPort -NextSeq $NextSeq `
        -FieldName 'cs' -ExpectedValue 1 -TimeoutMs $FollowupTimeoutMs
    Assert-Equal -Actual (Get-JsonFieldValue $carouselAtOne 'cs') -Expected 1 `
        -Label 'carousel final slot'

    Invoke-CommandExpectError -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.SensorAutoZero -Fields $null `
        -ExpectedCode $script:ErrorCode.NotReady -Label 'sensor.auto_zero'
    Invoke-CommandExpectError -SerialPort $SerialPort -NextSeq $NextSeq `
        -Command $script:CommandId.ProtocolInfo -Fields $null `
        -ExpectedCode $script:ErrorCode.Unavailable -Label 'protocol.info'

    Write-Log 'Manual smoke sequence passed.'
}

function Invoke-AutomationSmoke {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [ref]$NextSeq,
        [int]$Faults
    )

    Write-Log 'Running automation smoke sequence.'
    $baseline = Invoke-StatusRequest -SerialPort $SerialPort -NextSeq $NextSeq
    Assert-BaselineTelemetry -Telemetry $baseline -Faults $Faults

    $definition = Get-MinimalProtocolDefinition
    Invoke-ProtocolUpload -SerialPort $SerialPort -NextSeq $NextSeq `
        -Metadata $definition.Metadata -Program $definition.Program `
        -PrepareLength $definition.PrepareLength -LabelPrefix 'protocol'

    $afterCommit = Invoke-StatusRequest -SerialPort $SerialPort -NextSeq $NextSeq
    Assert-Equal -Actual (Get-JsonFieldValue $afterCommit 'pv') -Expected 1 `
        -Label 'pv after commit'
    Assert-Equal -Actual (Get-JsonFieldValue $afterCommit 'ff') `
        -Expected $Faults -Label 'ff after commit'
    Assert-MinimalProtocolRun -SerialPort $SerialPort -NextSeq $NextSeq `
        -Faults $Faults

    Write-Log 'WARNING: EEPROM now contains the temporary smoke-test protocol.'
    Write-Log 'Automation smoke sequence passed.'
}

$resolvedFaults = Get-ModeExpectedFaults -SelectedMode $Mode -RequestedFaults $ExpectedFaults
$resolvedTranscript = New-TranscriptPath -SelectedMode $Mode -SerialPortName $Port `
    -RequestedPath $TranscriptPath
Initialize-Transcript -Path $resolvedTranscript

$serialPort = $null
try {
    Write-Log 'Starting CE-CUBE serial smoke tester.'
    Write-Log "Transcript: $resolvedTranscript"

    if ($Mode -eq 'list-ports') {
        $ports = Get-AvailablePorts
        if ($ports.Count -eq 0) {
            Write-Log 'No serial ports found.'
        } else {
            foreach ($name in $ports) {
                Write-Log "PORT $name"
            }
        }
        exit 0
    }

    if ([string]::IsNullOrWhiteSpace($Port)) {
        Fail-Test 'Port is required for all smoke modes.'
    }

    $serialPort = Open-SerialPort -SerialPortName $Port -SerialBaudRate $BaudRate
    Write-Log "Opened $Port at $BaudRate baud."
    Write-Log "Waiting $OpenDelayMs ms for Nano reset and startup."
    Start-Sleep -Milliseconds $OpenDelayMs
    $serialPort.DiscardInBuffer()

    $nextSeq = [uint16]1
    if (($Mode -eq 'manual-smoke') -or ($Mode -eq 'current-build-smoke') -or
        ($Mode -eq 'usb-build-smoke')) {
        Invoke-ManualSmoke -SerialPort $serialPort -NextSeq ([ref]$nextSeq) `
            -Faults $resolvedFaults
    } elseif ($Mode -eq 'automation-smoke') {
        Invoke-AutomationSmoke -SerialPort $serialPort -NextSeq ([ref]$nextSeq) `
            -Faults $resolvedFaults
    } elseif ($Mode -eq 'seed-minimal-protocol') {
        Invoke-MinimalProtocolSeed -SerialPort $serialPort -NextSeq ([ref]$nextSeq)
    } elseif ($Mode -eq 'eeprom-roundtrip') {
        Invoke-EepromRoundTripTest -SerialPort $serialPort -NextSeq ([ref]$nextSeq) `
            -Faults $resolvedFaults
    } else {
        Fail-Test "Unsupported mode '$Mode'"
    }

    Write-Log 'RESULT PASS'
    exit 0
} catch {
    Write-Log "RESULT FAIL: $($_.Exception.Message)"
    exit 1
} finally {
    if (($null -ne $serialPort) -and $serialPort.IsOpen) {
        $serialPort.Close()
    }
}
