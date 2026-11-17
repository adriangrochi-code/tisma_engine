@echo off
rem Makes the project key on a USB stick when it is plugged in (docs/FIRMAR_VERSIONES.md).
python "%~dp0sign_release.py" create-key %*
pause
