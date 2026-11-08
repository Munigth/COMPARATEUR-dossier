COMPARATEUR DE DOSSIERS - INTERFACE GRAPHIQUE WIN32

Fichiers :
- main.c : code C utilisant l'API Win32 et les contrôles communs Windows.
- compiler-et-lancer.bat : compilation GCC/MinGW et lancement.

Compilation :
gcc main.c -o ComparateurDossiers.exe -std=c11 -Wall -Wextra -municode -mwindows -lcomctl32 -lshell32 -lole32

Fonctions :
- sélection graphique des dossiers A et B ;
- parcours récursif ;
- comparaison par chemin relatif et contenu octet par octet ;
- affichage état, taille, date de modification et source ;
- filtre des résultats ;
- copie d'un fichier sélectionné vers un troisième dossier sans écrasement ;
- suppression d'un fichier après saisie exacte de OUI.

Limites :
- Windows uniquement ; GCC/MinGW recommandé.
- La copie/suppression de dossiers complets n'est pas activée.
- Les liens symboliques et points de jonction sont ignorés.
- Pour un très grand nombre de fichiers, l'analyse peut prendre du temps.
- Effectue les premiers essais sur des dossiers de test et garde une sauvegarde.
