@echo off
ECHO Usage: Unpack.bat (src dir) [cp932] (dst dir) [utf-8]
SET src=..\Static\org
IF NOT "%~1" == "" (SET "src=%~1")
IF NOT EXIST "%src%\" (ECHO Directory not found "%src%" & GOTO :error)
SET dst=..\Static\dec
IF NOT "%~3" == "" (SET "dst=%~3")
IF NOT EXIST "%dst%\" (ECHO Directory not found "%dst%" & GOTO :error)
SET fenc=cp932
IF NOT "%~2" == "" (SET "fenc=%~2")
SET tenc=utf-8
IF NOT "%~4" == "" (SET "tenc=%~4")
ECHO Will unpack %src%\*.cd (%fenc%) to %dst%\*.cd.txt (%tenc%)
PAUSE

python Hpack0cd.py unpack "%src%\0.cd" "%dst%\0.cd.txt" %fenc% || GOTO :error
python EncConv.py %fenc% "%dst%\0.cd.txt" %tenc% "%dst%\0.cd.txt" || GOTO :error
python Hpack1cd.py unpack "%src%\1.cd" "%dst%\1.cd.txt" || GOTO :error
FOR %%i IN ("%src%\00??.cd", "%src%\01??.cd") DO (
python HpackScd.py unpack "%src%\%%~nxi" "%dst%\%%~nxi.txt" %fenc% || GOTO :error
python EncConv.py %fenc% "%dst%\%%~nxi.txt" %tenc% "%dst%\%%~nxi.txt" || GOTO :error
)
python HJFstBlk.py unpack "%dst%" %tenc% || GOTO :error
python HTextExtract.py unpack "%dst%" "%dst%\ExText.txt" %tenc% || GOTO :error

PAUSE
EXIT /b 0
:error
ECHO Error encountered, exiting
PAUSE
EXIT /b %ERRORLEVEL%