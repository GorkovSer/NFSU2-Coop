param([switch]$CheckOnly,[switch]$CheckUI,[switch]$CheckDiscovery,[switch]$ListGames)
# A patch launcher for both existing game copies. No game/installation starts
# until the user clicks Play. CheckOnly never opens UI or modifies files.
$ErrorActionPreference = 'Stop'
function Get-CoopHash([string]$Path) {
    $stream=$null;$sha=$null
    try{
        $stream=[IO.File]::OpenRead($Path);$sha=[Security.Cryptography.SHA256]::Create()
        return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant()
    }finally{if($stream){$stream.Dispose()};if($sha){$sha.Dispose()}}
}
function Get-LauncherPathFile {
    if($env:LOCALAPPDATA){return (Join-Path $env:LOCALAPPDATA 'NFSU2-Coop\launcher-path.json')}
    return $null
}
function Get-GameCandidates {
    # Read-only, bounded search. No whole-disk recursion or shortcut execution.
    $stateFile=Get-LauncherPathFile
    if($stateFile -and (Test-Path -LiteralPath $stateFile)){
        try{(Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json).GameFolder}catch{}
    }
    $PSScriptRoot
    Split-Path -Parent $PSScriptRoot
    foreach($key in @('HKLM:\SOFTWARE\EA GAMES\Need for Speed Underground 2','HKLM:\SOFTWARE\WOW6432Node\EA GAMES\Need for Speed Underground 2','HKCU:\SOFTWARE\EA GAMES\Need for Speed Underground 2')){
        $entry=Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
        foreach($property in @('Install Dir','InstallDir','InstallPath','Path')){if($entry){$entry.$property}}
    }
    foreach($branch in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall','HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall','HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall')){
        foreach($key in @(Get-ChildItem -LiteralPath $branch -ErrorAction SilentlyContinue)){
            $entry=Get-ItemProperty -LiteralPath $key.PSPath -ErrorAction SilentlyContinue
            if($entry -and $entry.DisplayName -match '(?i)(Need.*Speed|NFS).*Underground\s*2'){$entry.InstallLocation}
        }
    }
    $locations=@($env:ProgramFiles,${env:ProgramFiles(x86)},$env:ProgramW6432)
    foreach($drive in @(Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue)){
        if($drive.Root -notmatch '^[A-Za-z]:\\$' -or $drive.DisplayRoot -like '\\*'){continue}
        $locations+=Join-Path $drive.Root 'Games'
        $locations+=Join-Path $drive.Root 'Игры'
        foreach($dir in @(Get-ChildItem -LiteralPath $drive.Root -Directory -ErrorAction SilentlyContinue)){
            if($dir.Name -match '(?i)^(NFS|Need[ _-]*for[ _-]*Speed)'){$dir.FullName}
        }
    }
    foreach($location in $locations){
        if(-not $location){continue}
        foreach($name in @('Need for Speed Underground 2','NFS Underground 2','NFSU2','NFSU2-FreeRoam-Test')){Join-Path $location $name}
        foreach($vendor in @('EA Games','EA GAMES','Electronic Arts')){Join-Path (Join-Path $location $vendor) 'Need for Speed Underground 2'}
    }
    # Typical unpacked distributions, at most 400 directories and two levels
    # below each root. Do not traverse junctions into unrelated folder trees.
    $roots=@([Environment]::GetFolderPath('Desktop'),[Environment]::GetFolderPath('CommonDesktopDirectory'))
    if($env:USERPROFILE){$roots+=Join-Path $env:USERPROFILE 'Downloads'}
    $roots+=@($locations | Where-Object {$_ -and ([IO.Path]::GetFileName($_) -in @('Games','Игры'))})
    $queue=New-Object 'System.Collections.Generic.Queue[object]'
    foreach($root in $roots){if($root){$queue.Enqueue(@{Path=$root;Depth=0})}}
    $visited=0
    while($queue.Count -and $visited -lt 400){
        $node=$queue.Dequeue();$visited++;$node.Path
        if($node.Depth -ge 2){continue}
        foreach($dir in @(Get-ChildItem -LiteralPath $node.Path -Directory -ErrorAction SilentlyContinue)){
            if(($dir.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){continue}
            if($dir.Name -match '(?i)^(CoopBackups|\.git|node_modules|Windows|AppData)$'){continue}
            if($queue.Count+$visited -ge 400){break}
            $queue.Enqueue(@{Path=$dir.FullName;Depth=$node.Depth+1})
        }
    }
}
function Find-SupportedGames([object[]]$Candidates,[string]$ExpectedHash='fa940fe56c8af619a62e6c9cba5c552a01832424a7ee2189af9814d3c531a38f') {
    $seen=@{}
    foreach($candidate in $Candidates){
        if(-not ($candidate -is [string]) -or -not $candidate.Trim()){continue}
        try{
            $path=[Environment]::ExpandEnvironmentVariables($candidate.Trim().Trim('"'))
            if([IO.Path]::GetFileName($path) -ieq 'SPEED2.EXE'){$path=[IO.Path]::GetDirectoryName($path)}
            $path=[IO.Path]::GetFullPath($path)
            if($path -ne [IO.Path]::GetPathRoot($path)){$path=$path.TrimEnd('\','/')}
            if($seen.ContainsKey($path)){continue};$seen[$path]=$true
            $exe=Join-Path $path 'SPEED2.EXE'
            if((Test-Path -LiteralPath $exe -PathType Leaf) -and (Get-CoopHash $exe) -ieq $ExpectedHash){$path}
        }catch{continue} # Missing registry paths, access denied and stale saves.
    }
}
function Save-GameFolder([string]$GameFolder) {
    $stateFile=Get-LauncherPathFile
    if(-not $stateFile){return}
    try{
        $null=New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($stateFile)) -Force
        $content=@{GameFolder=$GameFolder} | ConvertTo-Json
        [IO.File]::WriteAllText($stateFile,$content,[Text.Encoding]::UTF8)
    }catch{} # Installation remains successful if preferences cannot be saved.
}
if($ListGames){@(Find-SupportedGames @(Get-GameCandidates)) | ConvertTo-Json;exit 0}
if($CheckDiscovery){
    # Only synthetic files under TEMP; no installation, process start or UI.
    $testRoot=Join-Path ([IO.Path]::GetTempPath()) ('NFSU2-Discovery-'+[guid]::NewGuid().ToString('N'))
    $null=New-Item -ItemType Directory -Path $testRoot
    try{
        $good=Join-Path $testRoot 'Игра [1]';$other=Join-Path $testRoot 'Другая копия';$bad=Join-Path $testRoot 'Wrong'
        foreach($path in @($good,$other,$bad)){$null=New-Item -ItemType Directory -Path $path}
        [IO.File]::WriteAllText((Join-Path $good 'SPEED2.EXE'),'supported fixture')
        [IO.File]::WriteAllText((Join-Path $other 'SPEED2.EXE'),'supported fixture')
        [IO.File]::WriteAllText((Join-Path $bad 'SPEED2.EXE'),'wrong exe')
        $hash=Get-CoopHash (Join-Path $good 'SPEED2.EXE')
        $found=@(Find-SupportedGames @('',$null,'does-not-exist',$bad,$good,($good+'\'),$good.ToUpperInvariant(),(Join-Path $good 'SPEED2.EXE'),$other) $hash)
        if($found.Count -ne 2 -or $found[0] -ne $good -or $found[1] -ne $other){throw 'Discovery filtering/order/deduplication regression'}
        $priorLocalData=$env:LOCALAPPDATA
        try{
            $env:LOCALAPPDATA=$testRoot;Save-GameFolder $other
            $saved=Get-Content -LiteralPath (Get-LauncherPathFile) -Raw | ConvertFrom-Json
            if($saved.GameFolder -ne $other){throw 'Saved path roundtrip regression'}
        }finally{$env:LOCALAPPDATA=$priorLocalData}
        Write-Output 'PASS game discovery: supported hash only, missing/wrong EXE rejection, case-insensitive deduplication, EXE/folder paths, multiple copies, priority order, Cyrillic/brackets and saved path; synthetic files only.'
    }finally{
        # This exact GUID-named directory was created above under TEMP.
        $resolved=[IO.Path]::GetFullPath($testRoot);$tempPrefix=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
        if($resolved.StartsWith($tempPrefix,[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($resolved).StartsWith('NFSU2-Discovery-')){Remove-Item -LiteralPath $resolved -Recurse -Force}
    }
    exit 0
}
function New-CoopConfig([string]$PlayerName,[string]$PeerAddress,[bool]$IsHost,[bool]$Traffic,[bool]$Outruns,[bool]$ExtraModes,[bool]$Contacts=$false,[bool]$RacePause=$true,[bool]$TrafficFeedback=$false,[bool]$QuickStart=$true,[string]$Profile='') {
    $PlayerName=$PlayerName.Trim();$PeerAddress=$PeerAddress.Trim()
    if($PlayerName -notmatch '^[A-Za-z0-9 _-]{1,16}$'){throw 'Ник: 1–16 латинских букв, цифр, пробелов, _ или -.'}
    $peerLine=''
    if($PeerAddress){
        $address=$null
        if($PeerAddress -notmatch '^\d{1,3}(\.\d{1,3}){3}$' -or -not [Net.IPAddress]::TryParse($PeerAddress,[ref]$address)){
            throw 'Укажите IPv4 другого компьютера, например 192.168.1.17.'
        }
        $octets=$address.GetAddressBytes()
        if($octets.Length -ne 4 -or -not ($octets[0] -eq 26 -or $octets[0] -eq 10 -or ($octets[0] -eq 172 -and $octets[1] -ge 16 -and $octets[1] -le 31) -or ($octets[0] -eq 192 -and $octets[1] -eq 168) -or ($octets[0] -eq 169 -and $octets[1] -eq 254))){
            throw 'Укажите адрес локальной сети или Radmin VPN (26.x.x.x). Для Radmin адреса друг друга нужны на обоих ПК, включая хост.'
        }
        $peerLine=$address.ToString()+':47210'
    }elseif(-not $IsHost){throw 'На клиенте нужен IPv4 компьютера-хоста.'}
    $role=[int]$IsHost;$trafficFlag=[int]$Traffic;$outrunFlag=[int]$Outruns;$contactFlag=[int]$Contacts;$pauseFlag=[int]$RacePause;$modes=if($ExtraModes){63}else{1}
    $feedbackFlag=[int]($Traffic -and $TrafficFeedback);$quickFlag=[int]$QuickStart
    if($Profile -and $Profile -notmatch '^[A-Za-z0-9 _-]{1,31}$'){throw 'Недопустимое имя профиля карьеры.'}
    return @"
[FreeRoam]
Enable=1
QuickStart=$quickFlag
Profile=$Profile
Name=$PlayerName
Port=47210
Peers=$peerLine
Discovery=0
Room=nfsu2-coop-03
NetThread=1
MaxPlayers=2
MaxCars=1
CarPoolKB=48000
Collide=0
PlayerContact=$contactFlag
RacePause=$pauseFlag
Coop=1
CoopHost=$role
WorldHost=$role
TrafficSync=$trafficFlag
TrafficFeedback=$feedbackFlag
Outruns=$outrunFlag
CoopRaceModes=$modes
NativeSelfTest=0
Log=1
TestMode=0
"@
}
function Read-CoopSettings([string]$GameFolder) {
    # Read only the selected copy's configuration; never execute INI text.
    $values=@{};$inSection=$false
    try{
        $iniPath=Join-Path $GameFolder 'scripts\NFSU2FreeRoam.ini'
        foreach($line in [IO.File]::ReadAllLines($iniPath)){
            if($line -match '^\s*\[([^\]]+)\]\s*$'){$inSection=$Matches[1] -ieq 'FreeRoam';continue}
            if($inSection -and $line -match '^\s*([A-Za-z]+)\s*=(.*?)\s*$'){$values[$Matches[1]]=$Matches[2]}
        }
    }catch{return $null}
    if(-not $values.Count){return $null}
    return $values
}
function Restore-CoopSettings([string]$GameFolder) {
    $settings=Read-CoopSettings $GameFolder
    Update-CareerProfiles $GameFolder
    if(-not $settings){return}
    if($settings.QuickStart -match '^[01]$'){$quickStart.Checked=$settings.QuickStart -eq '1'}
    if($settings.Profile -and $profile.Items.Contains($settings.Profile)){$profile.SelectedItem=$settings.Profile}
    if($settings.Name -match '^[A-Za-z0-9 _-]{1,16}$'){$name.Text=$settings.Name}
    if($settings.CoopHost -match '^[01]$'){$hostRole.Checked=$settings.CoopHost -eq '1';$clientRole.Checked=-not $hostRole.Checked}
    if($settings.ContainsKey('Peers')){
        if($settings.Peers -eq ''){$peer.Text=''}
        elseif($settings.Peers -match '^(\d{1,3}(?:\.\d{1,3}){3}):47210$'){$peer.Text=$Matches[1]}
    }
    foreach($item in @(@('TrafficSync',$traffic),@('Outruns',$outruns),@('PlayerContact',$contacts),@('RacePause',$racePause),@('TrafficFeedback',$trafficFeedback))){
        if($settings[$item[0]] -match '^[01]$'){$item[1].Checked=$settings[$item[0]] -eq '1'}
    }
    if($settings.CoopRaceModes -in @('1','63')){$extraModes.Checked=$settings.CoopRaceModes -eq '63'}
    $trafficFeedback.Enabled=$traffic.Checked
    if(-not $traffic.Checked){$trafficFeedback.Checked=$false}
}
if($CheckOnly){
    $h=New-CoopConfig 'Host' '' $true $true $true $false
    $c=New-CoopConfig 'Guest' '192.168.1.17' $false $true $true $true
    $vpnHost=New-CoopConfig 'Host' '26.12.34.56' $true $true $true $true
    $vpnClient=New-CoopConfig 'Guest' '26.141.106.52' $false $true $true $true
    if($vpnHost -notmatch 'Peers=26.12.34.56:47210' -or $vpnHost -notmatch 'CoopHost=1' -or $vpnClient -notmatch 'Peers=26.141.106.52:47210' -or $vpnClient -notmatch 'CoopHost=0'){throw 'Radmin explicit-peer regression'}
    $f=New-CoopConfig 'Host' '' $true $true $false $false $false $true $true
    $playerOnly=New-CoopConfig 'Host' '' $true $false $false $false $true
    if($playerOnly -notmatch 'PlayerContact=1' -or $playerOnly -notmatch 'WorldHost=1' -or $playerOnly -notmatch 'TrafficSync=0'){throw 'Player contact without traffic regression'}
    $noTraffic=New-CoopConfig 'Host' '' $true $false $false $false $false $true $true
    if($h -notmatch 'TrafficFeedback=0' -or $f -notmatch 'TrafficFeedback=1' -or $noTraffic -notmatch 'TrafficFeedback=0'){throw 'Feedback configuration regression'}
    if($h -notmatch 'CoopHost=1' -or $h -notmatch 'WorldHost=1' -or $h -notmatch 'NetThread=1' -or $h -notmatch 'RacePause=1' -or $c -notmatch 'Peers=192.168.1.17:47210' -or $c -notmatch 'CoopRaceModes=63'){throw 'Configuration regression'}
    foreach($bad in @('','127.0.0.1','224.1.1.1','192.168.1.999','8.8.8.8','26.1.1.999',"26.1.1.2`nEnable=0","192.168.1.2`nEnable=0")){
        $rejected=$false;try{$null=New-CoopConfig 'Guest' $bad $false $false $false $false}catch{$rejected=$true}
        if(-not $rejected){throw "Invalid address accepted: $bad"}
    }
    Write-Output 'PASS launcher configuration: host/client, common roles, LAN and explicit Radmin peers on both PCs, input validation; no writes or game launch.'
    exit 0
}
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$form=New-Object Windows.Forms.Form
$form.Text='NFSU2 — кооператив 0.5 Alpha'
$form.ClientSize=New-Object Drawing.Size(650,700)
$form.StartPosition='CenterScreen';$form.FormBorderStyle='FixedDialog';$form.MaximizeBox=$false
$form.Font=New-Object Drawing.Font('Segoe UI',10)
function Label([string]$Text,[int]$Y,[int]$Height=28,[int]$Width=614){
    $control=New-Object Windows.Forms.Label;$control.Text=$Text;$control.Location=New-Object Drawing.Point(18,$Y);$control.Size=New-Object Drawing.Size($Width,$Height);$form.Controls.Add($control)
}
function TextBox([int]$X,[int]$Y,[int]$Width,[string]$Text){
    # PowerShell variable names are case-insensitive: $x would overwrite
    # the typed integer parameter $X before the control can be returned.
    $control=New-Object Windows.Forms.TextBox;$control.Location=New-Object Drawing.Point($X,$Y);$control.Width=$Width;$control.Text=$Text;$form.Controls.Add($control);return $control
}
function CheckBox([string]$Text,[int]$Y,[bool]$Checked){
    $x=New-Object Windows.Forms.CheckBox;$x.Text=$Text;$x.Location=New-Object Drawing.Point(18,$Y);$x.Size=New-Object Drawing.Size(614,28);$x.Checked=$Checked;$form.Controls.Add($x);return $x
}
$foundGames=if($CheckUI){@()}else{@(Find-SupportedGames @(Get-GameCandidates))}
$folderCaption=if($foundGames.Count -gt 1){'Найдено несколько копий — выберите папку игры:'}elseif($foundGames.Count -eq 1){'Папка игры найдена автоматически:'}else{'Игра не найдена автоматически — укажите папку:'}
Label $folderCaption 14
$folder=New-Object Windows.Forms.ComboBox;$folder.Location=New-Object Drawing.Point(18,44);$folder.Width=505;$folder.DropDownStyle='DropDown';$folder.DropDownWidth=750;$form.Controls.Add($folder)
foreach($foundGame in $foundGames){$null=$folder.Items.Add($foundGame)}
if($foundGames.Count){$folder.SelectedIndex=0}
$browse=New-Object Windows.Forms.Button;$browse.Text='Выбрать';$browse.Location=New-Object Drawing.Point(535,42);$browse.Size=New-Object Drawing.Size(95,30);$form.Controls.Add($browse)
$browse.Add_Click({
    $picker=New-Object Windows.Forms.FolderBrowserDialog;$picker.Description='Папка, в которой находится SPEED2.EXE'
    if($picker.ShowDialog() -eq 'OK'){$folder.Text=$picker.SelectedPath;Restore-CoopSettings $folder.Text}
    $picker.Dispose()
})
$hostRole=New-Object Windows.Forms.RadioButton;$hostRole.Text='Хост';$hostRole.Location=New-Object Drawing.Point(18,85);$hostRole.Checked=$true;$form.Controls.Add($hostRole)
$clientRole=New-Object Windows.Forms.RadioButton;$clientRole.Text='Клиент';$clientRole.Location=New-Object Drawing.Point(150,85);$form.Controls.Add($clientRole)
Label 'Ваш ник:' 122 28 125
$name=TextBox 155 120 475 'Player';$name.MaxLength=16
Label 'IPv4 другого ПК:' 163 28 190
$peer=TextBox 220 160 410 ''
Label 'Radmin: ОБА вводят VPN-IP другого игрока. LAN: хост может оставить IP пустым.' 199
$addresses=@(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object {$_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*'} | Select-Object -ExpandProperty IPAddress)
Label ('Адреса этого ПК: '+($addresses -join ', ')) 230 42
$traffic=CheckBox 'Общий трафик' 278 $true
$outruns=CheckBox 'Вызовы игроков на Outrun через Engage' 310 $true
$extraModes=CheckBox 'Sprint, StreetX, URL, Drag и Drift (эксперимент)' 342 $false
$contacts=CheckBox 'Общие столкновения игроков (эксперимент; на обоих ПК)' 374 $false
$racePause=CheckBox 'Общая пауза гонки при сворачивании или меню (эксперимент)' 406 $true
$trafficFeedback=CheckBox 'Удары по общему трафику (эксперимент; включить на обоих ПК)' 438 $false
$traffic.Add_CheckedChanged({$trafficFeedback.Enabled=$traffic.Checked;if(-not $traffic.Checked){$trafficFeedback.Checked=$false}})
Label 'Перед установкой сохраняются прежний мод, настройки и профили. На обоих ПК нужен этот пакет. Дополнительные режимы гонок экспериментальные.' 476 56
$quickStart=CheckBox 'Сразу в город: пропуск интро и автоматическая загрузка карьеры' 532 $true
Label 'Профиль карьеры:' 568 28 140
$profile=New-Object Windows.Forms.ComboBox;$profile.Location=New-Object Drawing.Point(165,565);$profile.Width=465;$profile.DropDownStyle='DropDownList';$form.Controls.Add($profile)
function Get-CareerRoot([string]$GameFolder) {
    $defaultRoot=Join-Path $env:LOCALAPPDATA 'NFS Underground 2'
    try{
        $wideIni=Join-Path $GameFolder 'scripts\NFSUnderground2.WidescreenFix.ini'
        foreach($line in [IO.File]::ReadAllLines($wideIni)){
            if($line -match '^\s*CustomUserFilesDirectoryInGameDir\s*=\s*([^;\r\n]+)'){
                $custom=($Matches[1] -split '//',2)[0].Trim()
                if($custom -and $custom -ne '0'){
                    $baseRoot=if([IO.Path]::IsPathRooted($custom)){$custom}else{Join-Path $GameFolder $custom}
                    return (Join-Path ([IO.Path]::GetFullPath($baseRoot)) 'NFS Underground 2')
                }
            }
        }
    }catch{}
    return $defaultRoot
}
function Update-CareerProfiles([string]$GameFolder) {
    $previous=$profile.Text;$profile.Items.Clear()
    $root=Get-CareerRoot $GameFolder
    $candidates=@(Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue | Where-Object {-not ($_.Attributes -band [IO.FileAttributes]::ReparsePoint)} | ForEach-Object {
        if($_.Name -notmatch '^[A-Za-z0-9 _-]{1,31}$'){return}
        $file=Join-Path $_.FullName $_.Name
        try{
            $item=Get-Item -LiteralPath $file
            if($item.Length -ne 54966 -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){return}
            $bytes=[IO.File]::ReadAllBytes($file)
            if([Text.Encoding]::ASCII.GetString($bytes,0,4) -ne '20CM' -or $bytes[0xad7d] -ne 1){return}
            $inside=[Text.Encoding]::ASCII.GetString($bytes,0xd225,32).TrimEnd([char]0)
            if($inside -ne $_.Name){return}
            $item
        }catch{}
    } | Sort-Object LastWriteTimeUtc -Descending)
    foreach($item in $candidates){$null=$profile.Items.Add($item.Name)}
    if($profile.Items.Contains($previous)){$profile.SelectedItem=$previous}
    elseif($profile.Items.Count){$profile.SelectedIndex=0}
}
$status=New-Object Windows.Forms.Label;$status.Location=New-Object Drawing.Point(18,660);$status.Size=New-Object Drawing.Size(470,35);$form.Controls.Add($status)
$reports=New-Object Windows.Forms.Button;$reports.Text='Отчёты ошибок';$reports.Location=New-Object Drawing.Point(495,660);$reports.Size=New-Object Drawing.Size(137,30);$form.Controls.Add($reports)
$reports.Add_Click({
    try{
        $reportFolder=Join-Path $env:LOCALAPPDATA 'NFSU2-Coop\Crashes'
        $null=New-Item -ItemType Directory -Path $reportFolder -Force
        Start-Process -FilePath explorer.exe -ArgumentList ('"'+$reportFolder+'"')
    }catch{[Windows.Forms.MessageBox]::Show($_.Exception.Message,'NFSU2')|Out-Null}
})
function Install-Coop {
    $game=[IO.Path]::GetFullPath($folder.Text.Trim());$exe=Join-Path $game 'SPEED2.EXE'
    $config=New-CoopConfig $name.Text $peer.Text $hostRole.Checked $traffic.Checked $outruns.Checked $extraModes.Checked $contacts.Checked $racePause.Checked $trafficFeedback.Checked $quickStart.Checked $profile.Text
    if($quickStart.Checked -and -not $profile.Text){throw 'Для быстрого запуска нужен существующий профиль карьеры. Выберите профиль или отключите «Сразу в город».'}
    if($addresses -contains $peer.Text.Trim()){throw 'Укажите адрес другого компьютера, не этого.'}
    if(-not (Test-Path -LiteralPath $exe)){throw 'В выбранной папке нет SPEED2.EXE.'}
    if((Get-CoopHash $exe) -ne 'fa940fe56c8af619a62e6c9cba5c552a01832424a7ee2189af9814d3c531a38f'){throw 'Эта версия SPEED2.EXE не поддерживается.'}
    if(Get-Process -Name SPEED2 -ErrorAction SilentlyContinue){throw 'Сначала закройте NFSU2, чтобы обновить мод.'}
    if(-not (Test-Path -LiteralPath (Join-Path $game 'dinput8.dll'))){throw 'В копии игры нужен ASI Loader (dinput8.dll). Используйте ту же полную копию игры, на которой уже работал наш мод.'}
    $manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build.json') -Raw | ConvertFrom-Json
    $payload=Join-Path $PSScriptRoot 'payload\NFSU2FreeRoam.asi'
    if((Get-CoopHash $payload) -ne $manifest.plugin_sha256){throw 'Файл мода повреждён. Распакуйте пакет заново.'}
    $scripts=Join-Path $game 'scripts';$target=Join-Path $scripts 'NFSU2FreeRoam.asi';$ini=Join-Path $scripts 'NFSU2FreeRoam.ini'
    if(Test-Path -LiteralPath (Join-Path $game 'NFSU2FreeRoam.asi')){throw 'В корне игры найдена вторая NFSU2FreeRoam.asi. Оставьте мод только в scripts, чтобы он не загружался дважды.'}
    $backup=Join-Path $game ('CoopBackups\'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff'))
    $null=New-Item -ItemType Directory -Path $backup -Force
    foreach($path in @($target,$ini)){if(Test-Path -LiteralPath $path){Copy-Item -LiteralPath $path -Destination $backup}}
    $saves=Get-CareerRoot $game
    if(Test-Path -LiteralPath $saves){Copy-Item -LiteralPath $saves -Destination (Join-Path $backup 'profiles') -Recurse}
    $null=New-Item -ItemType Directory -Path $scripts -Force
    $staged=Join-Path $scripts ('NFSU2FreeRoam.'+[guid]::NewGuid().ToString('N')+'.tmp')
    try{
        Copy-Item -LiteralPath $payload -Destination $staged
        Move-Item -LiteralPath $staged -Destination $target -Force
        [IO.File]::WriteAllText($ini,$config,[Text.Encoding]::ASCII)
        if((Get-CoopHash $target) -ne $manifest.plugin_sha256){throw 'Не удалось проверить установленный мод.'}
    }catch{
        # Restore only the two files this action owns; never touch user saves.
        foreach($path in @($target,$ini)){
            $old=Join-Path $backup ([IO.Path]::GetFileName($path))
            if(Test-Path -LiteralPath $old){Copy-Item -LiteralPath $old -Destination $path -Force}
            elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}
        }
        throw
    }finally{if(Test-Path -LiteralPath $staged){Remove-Item -LiteralPath $staged}}
    Save-GameFolder $game
    return $exe
}
$save=New-Object Windows.Forms.Button;$save.Text='Установить и сохранить';$save.Location=New-Object Drawing.Point(18,625);$save.Size=New-Object Drawing.Size(265,30);$form.Controls.Add($save)
$play=New-Object Windows.Forms.Button;$play.Text='Установить и играть';$play.Location=New-Object Drawing.Point(365,625);$play.Size=New-Object Drawing.Size(265,30);$form.Controls.Add($play)
$save.Add_Click({try{$null=Install-Coop;$status.Text='Готово. Резервная копия — в CoopBackups рядом с игрой.'}catch{[Windows.Forms.MessageBox]::Show($_.Exception.Message,'NFSU2')|Out-Null}})
$play.Add_Click({try{
    $exe=Install-Coop
    $launch=New-Object Diagnostics.ProcessStartInfo;$launch.FileName=$exe;$launch.WorkingDirectory=[IO.Path]::GetDirectoryName($exe);$launch.UseShellExecute=$false
    $launch.EnvironmentVariables['NFSU2_COOP_QUICKSTART']=[string][int]$quickStart.Checked
    $launch.EnvironmentVariables['NFSU2_COOP_PROFILE']=if($quickStart.Checked){Join-Path (Join-Path (Get-CareerRoot $launch.WorkingDirectory) $profile.Text) $profile.Text}else{''}
    $null=[Diagnostics.Process]::Start($launch);$form.Close()
}catch{[Windows.Forms.MessageBox]::Show($_.Exception.Message,'NFSU2')|Out-Null}})
if(-not $CheckUI){
    Restore-CoopSettings $folder.Text
    $folder.Add_SelectedIndexChanged({Restore-CoopSettings $folder.Text})
    $folder.Add_Validated({Restore-CoopSettings $folder.Text})
}
if($CheckUI){
    try{
        if($folder -isnot [Windows.Forms.ComboBox] -or $folder.DropDownStyle -ne 'DropDown' -or $name -isnot [Windows.Forms.TextBox] -or $peer -isnot [Windows.Forms.TextBox]){throw 'Input construction regression'}
        if($folder.Left -ne 18 -or $folder.Top -ne 44 -or $name.Left -ne 155 -or $peer.Left -ne 220 -or $name.Text -ne 'Player' -or $name.MaxLength -ne 16){throw 'Text box layout regression'}
        foreach($inputControl in @($folder,$name,$peer)){
            if(-not $inputControl.Enabled -or ($inputControl -is [Windows.Forms.TextBox] -and $inputControl.ReadOnly) -or -not $inputControl.TabStop){throw 'Input is not editable/focusable'}
            foreach($otherControl in $form.Controls){
                if(-not [object]::ReferenceEquals($inputControl,$otherControl) -and $inputControl.Bounds.IntersectsWith($otherControl.Bounds)){throw ('Input overlaps '+$otherControl.GetType().Name+': '+$otherControl.Text)}
            }
        }
        $null=$folder.Items.Add('C:\Games\NFSU2');$folder.SelectedIndex=0
        if($folder.Text -ne 'C:\Games\NFSU2'){throw 'Discovered folder selection regression'}
        $folder.Text='D:\Custom\NFSU2'
        if($folder.Text -ne 'D:\Custom\NFSU2'){throw 'Manual folder override regression'}
        $peer.Text='192.168.1.17';$name.Text='Guest'
        $clientRole.Checked=$true;$extraModes.Checked=$true
        $config=New-CoopConfig $name.Text $peer.Text $hostRole.Checked $traffic.Checked $outruns.Checked $extraModes.Checked $contacts.Checked $racePause.Checked $trafficFeedback.Checked
        if($hostRole.Checked -or $config -notmatch 'CoopHost=0' -or $config -notmatch 'CoopRaceModes=63'){throw 'Client control binding regression'}
        $trafficFeedback.Checked=$true;$traffic.Checked=$false
        if($trafficFeedback.Enabled -or $trafficFeedback.Checked){throw 'Traffic checkbox dependency regression'}
        $fixtureRoot=Join-Path ([IO.Path]::GetTempPath()) ('NFSU2-Settings-'+[guid]::NewGuid().ToString('N'))
        try{
            $fixtureScripts=Join-Path $fixtureRoot 'scripts';$null=New-Item -ItemType Directory -Path $fixtureScripts
            $fixtureConfig=New-CoopConfig 'SavedGuest' '26.1.2.3' $false $true $false $true $true $false $true
            [IO.File]::WriteAllText((Join-Path $fixtureScripts 'NFSU2FreeRoam.ini'),$fixtureConfig)
            Restore-CoopSettings $fixtureRoot
            if($name.Text -ne 'SavedGuest' -or $peer.Text -ne '26.1.2.3' -or $hostRole.Checked -or -not $clientRole.Checked -or -not $traffic.Checked -or $outruns.Checked -or -not $extraModes.Checked -or -not $contacts.Checked -or $racePause.Checked -or -not $trafficFeedback.Checked){throw 'Saved settings restoration regression'}
        }finally{
            # Only the two explicitly created files/directories; no recursive removal.
            if(Test-Path -LiteralPath (Join-Path $fixtureScripts 'NFSU2FreeRoam.ini')){Remove-Item -LiteralPath (Join-Path $fixtureScripts 'NFSU2FreeRoam.ini')}
            if(Test-Path -LiteralPath $fixtureScripts){Remove-Item -LiteralPath $fixtureScripts}
            if(Test-Path -LiteralPath $fixtureRoot){Remove-Item -LiteralPath $fixtureRoot}
        }
        Write-Output 'PASS launcher UI: form and controls constructed, enabled/read-write/tabbable inputs, no overlapping controls, layout, client role/configuration and checkbox events; no window shown, installation or game launch.'
    }finally{$form.Dispose()}
    exit 0
}
$form.ShowDialog()|Out-Null;$form.Dispose()
