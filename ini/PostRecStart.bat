@echo off
rem _EDCBX_DIRECT_
rem _EDCBX_#HIDE_
set
if not defined Title set Title=x
if not "%RecMode%"=="4" (
echo "���^��J�n�F%ServiceName% %SDYYYY%/%SDM%/%SDD% %STH%:%STM% �` %ETH%:%ETM% %Title:"='%"
rem | C:\Windows\Sysnative\msg.exe console
)
pause
