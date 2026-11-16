# Language of the mod, independent from original game resources.
function Get-ModIniValue([string]$Path,[string]$Section,[string]$Key,[string]$Fallback) {
 $current=''
 if(Test-Path -LiteralPath $Path){foreach($line in Get-Content -LiteralPath $Path){
  if($line -match '^\s*\[([^]]+)\]'){$current=$matches[1]}
  elseif($current -eq $Section -and $line -match ('^\s*'+[regex]::Escape($Key)+'\s*=\s*(.*?)\s*$')){return $matches[1]}
 }}
 return $Fallback
}
function Get-ModText([string]$German,[string]$English) {
 $root=Split-Path -Parent $PSScriptRoot
 $language=$env:HP2VR_LANGUAGE
 if(-not $language){$language=Get-ModIniValue (Join-Path $root 'system/Default.ini') 'Engine.Engine' 'Language' 'int'}
 $choice=Get-ModIniValue (Join-Path $root 'config/vr-preferences.ini') 'VR' 'Language' '0'
 if($choice -eq '2' -or ($choice -ne '1' -and $language -notin @('de','ger'))){return $English}
 return $German
}
