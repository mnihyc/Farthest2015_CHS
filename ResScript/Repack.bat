@echo off
ECHO Usage: Repack.bat (src dir) [utf-8] (dst dir) [gbk]
SET src=..\Static\dec-chs
IF NOT "%~1" == "" (SET "src=%~1")
IF NOT EXIST "%src%\" (ECHO Directory not found "%src%" & GOTO :error)
SET dst=..\Static\rep
IF NOT "%~3" == "" (SET "dst=%~3")
IF NOT EXIST "%dst%\" (ECHO Directory not found "%dst%" & GOTO :error)
SET fenc=utf-8
IF NOT "%~2" == "" (SET "fenc=%~2")
SET tenc=gbk
IF NOT "%~4" == "" (SET "tenc=%~4")
ECHO Will unpack %src%\*.cd.txt (%fenc%) to %dst%\*.cd (%tenc%)
PAUSE
IF EXIST "%dst%\tmp\" (RMDIR /S /Q "%dst%\tmp\" || GOTO :error)
MKDIR "%dst%\tmp\" || GOTO :error
XCOPY /Q "%src%\" "%dst%\tmp\"

python HTextExtract.py pack "%dst%\tmp" "%src%\ExText.txt" %fenc% || GOTO :error
python HJFstBlk.py pack "%dst%\tmp" %fenc% || GOTO :error
python EncConv.py %fenc% "%src%\0.cd.txt" %tenc% "%dst%\0.cd" || GOTO :error
python Hpack0cd.py pack "%dst%\0.cd" "%dst%\0.cd" %tenc% || GOTO :error
python Hpack1cd.py pack "%src%\1.cd.txt" "%dst%\1.cd" || GOTO :error
FOR %%i IN ("%dst%\tmp\00??.cd.txt", "%dst%\tmp\01??.cd.txt") DO (
python EncConv.py %fenc% "%dst%\tmp\%%~ni.txt" %tenc% "%dst%\%%~ni" || GOTO :error
python HpackScd.py pack "%dst%\%%~ni" "%dst%\%%~ni" %tenc% || GOTO :error
)

RMDIR /S /Q "%dst%\tmp\" || GOTO :error
PAUSE
EXIT /b 0
:error
ECHO Error encountered, exiting
PAUSE
RMDIR /S /Q "%dst%\tmp\"
EXIT /b %ERRORLEVEL%