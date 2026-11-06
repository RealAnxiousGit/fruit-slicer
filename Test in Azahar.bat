@echo off
rem Double-click to play Fruit Slicer 3DS in a window. The mouse is the stylus.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\run-azahar.ps1"
if errorlevel 1 pause
