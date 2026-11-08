COMPARATEUR DE DOSSIERS EN C — PROTOTYPE

Contenu :
- main.c : code C11
- compiler-et-lancer.bat : compilation GCC et lancement

Fonctions :
- parcours récursif des deux dossiers;
- comparaison des chemins relatifs et noms de fichiers;
- affichage des tailles en octets et dates de dernière modification;
- comparaison exacte du contenu octet par octet;
- liste des fichiers uniquement présents dans A ou B;
- copie vers un troisième dossier, sans écraser un fichier déjà présent;
- suppression d'un fichier seulement après avoir tapé OUI.

Compilation :
1. Installer GCC (par exemple MinGW-w64/MSYS2).
2. Ouvrir ce dossier dans l'Explorateur et lancer compiler-et-lancer.bat.
   Ou dans un terminal : gcc main.c -o ComparateurDossiers.exe -std=c11 -Wall -Wextra

Sécurité :
- Commence avec des dossiers de test et conserve une sauvegarde.
- La suppression est définitive.
- L'option de rassemblement COPIE les fichiers; elle ne déplace ni ne supprime les originaux.
- Si un fichier de même chemin existe dans la destination, il n'est pas écrasé.
- La date affichée est la date de dernière modification, pas nécessairement la date initiale d'ajout.
- Le prototype est en console. Une interface graphique avec cases à cocher et boutons peut être ajoutée ensuite.
- Les liens symboliques, très longs chemins et certains noms non ASCII sous Windows peuvent nécessiter des améliorations.
