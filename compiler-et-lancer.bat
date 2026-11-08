@echo off
setlocal
title Comparateur de dossiers - Compilation
where gcc >nul 2>nul
if errorlevel 1 (
  echo GCC est introuvable. Installe MinGW-w64/MSYS2 et ajoute son dossier bin au PATH.
  pause
  exit /b 1
)
gcc main.c -o ComparateurDossiers.exe -std=c11 -Wall -Wextra -municode -mwindows -lcomctl32 -lshell32 -lole32
if errorlevel 1 (
  echo Echec de compilation. Corrige les erreurs affichees ci-dessus.
  pause
  exit /b 1
)
echo Compilation reussie.
start "" ComparateurDossiers.exe
endlocal
