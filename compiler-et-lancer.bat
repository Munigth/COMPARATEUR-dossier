@echo off
chcp 65001 >nul
where gcc >nul 2>nul
if errorlevel 1 (
 echo GCC introuvable. Installe MinGW-w64/MSYS2 puis ajoute gcc au PATH.
 pause
 exit /b 1
)
gcc main.c -o ComparateurDossiers.exe -std=c11 -Wall -Wextra
if errorlevel 1 (echo Erreur de compilation.&pause&exit /b 1)
ComparateurDossiers.exe
pause
