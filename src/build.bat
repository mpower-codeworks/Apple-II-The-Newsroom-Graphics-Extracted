cls
@echo off
cl /nologo /O2 /W4 savethewhales.c
set "BUILD_RESULT=%ERRORLEVEL%"
if exist savethewhales.obj del /q savethewhales.obj
exit /b %BUILD_RESULT%