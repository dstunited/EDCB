@echo off
rem _EDCBX_DIRECT_
rem _EDCBX_#HIDE_
set
if not defined ReserveComment set ReserveComment=x
if not defined Title set Title=x
if "%ReserveComment:~0,3%"=="EPG" (
echo "���\��ǉ��F%ServiceName% %SDYYYY%/%SDM%/%SDD% %STH%:%STM% �` %ETH%:%ETM% %Title:"='%"
rem | C:\Windows\Sysnative\msg.exe console
)
pause
