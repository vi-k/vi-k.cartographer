@echo off
del *.ilk 2>nul
del *.idb 2>nul
del *.pdb 2>nul
del *.ncb 2>nul
del /A:H *.suo 2>nul
rmdir /S /Q obj
del *.depend 2>nul
del *.layout 2>nul
del main.log 2>nul
rmdir /S /Q Debug
rmdir /S /Q Release
call :vc_clean bin\Debug %1
call :vc_clean bin\Release %1
goto :eof

:vc_clean
echo %1...
del %1\BuildLog.htm 2>nul
del %1\mt.dep 2>nul
del %1\*.obj 2>nul
del %1\*.idb 2>nul
del %1\*.pdb 2>nul
del %1\*.ilk 2>nul
del %1\*.manifest 2>nul
del %1\*.manifest.res 2>nul
del %1\*.map 2>nul
del %1\*.pch 2>nul
del %1\*.res 2>nul
if "%2"=="exe" del %1\*.exe 2>nul
goto :eof
