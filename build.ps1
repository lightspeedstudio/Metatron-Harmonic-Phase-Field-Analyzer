$ErrorActionPreference = "Stop"
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)
New-Item -ItemType Directory -Force -Path build | Out-Null
gcc -std=c11 -Wall -Wextra -Werror -I include -o build\test_firmware_layout.exe tests\test_firmware_layout.c src\firmware_client.c src\serial_win.c
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& .\build\test_firmware_layout.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
gcc -std=c11 -Wall -Wextra -Werror -shared -I include -o build\metatron.dll src\firmware_client.c src\serial_win.c
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
gcc -std=c11 -Wall -Wextra -Werror -I include -o build\read_pillar.exe examples\read_pillar.c src\firmware_client.c src\serial_win.c
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
python python\metatron.py --self-test
exit $LASTEXITCODE
