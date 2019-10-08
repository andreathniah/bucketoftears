function GetTheFlagOrNot
{
    Param(
        [Parameter(Mandatory = $true, Position = 0)]
        [int]$EncryptionKey
    )

    Get-ChildItem .\flag.txt | % { 
        $EncryptedData = "";
        (Get-Content -Encoding ASCII $_.FullName).ToCharArray() | % {
            $EncryptedData += [char]($_ -bxor $EncryptionKey)
   
        }
        Set-Content -Path $_.FullName -Value $EncryptedData
        Write-Output "All Your Files Are Belong To Me Now!"
    }
}


