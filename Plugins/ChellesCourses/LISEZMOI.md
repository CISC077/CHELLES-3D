# ChellesCourses : courses de rue chronométrées

Des courses dans les rues du quartier (carte Rachid), à pied ou en véhicule, avec chrono, record de ce PC et classement de tous les joueurs sur le VPS.

## Installer

1. Fermez Unreal.
2. Double-cliquez sur `C:\Users\CISCO\Documents\Unreal Projects\CHELLES 3D\Tools\Courses\INSTALLER_COURSES.bat`
   (compile le plugin, puis envoie le classement sur le VPS).

## Jouer

- Les **colonnes cyan** de 60 m sont les départs.
- Sur un départ : touche **DÉBUT** (Home, la flèche en diagonale) : 3, 2, 1, PARTEZ !
- Passez les **portes dorées** dans l'ordre ; l'**arrivée** est rose. Un losange à l'écran montre la porte suivante.
- En haut au centre : chrono, portes, votre record, et l'écart avec votre record à chaque porte (vert = en avance).
- Pendant une course, **DÉBUT** = abandonner.
- Ailleurs, **DÉBUT** ouvre l'écran **COURSES** : records, 5 meilleurs joueurs, bouton « MONTRER LE DÉPART ».
- Arrivée : +50 XP des pouvoirs, +50 XP de plus si c'est votre record. Le 1er du classement est annoncé dans le chat.
- Pas de course pendant une mission.

## Les parcours

Au premier lancement, le plugin fabrique 3 courses à partir des rues de la circulation (ChellesCirculation) :
SPRINT DU QUARTIER (1 km), TOUR DE CHELLES (2 km), GRANDE COURSE (3,5 km). Ils sont enregistrés dans
`Courses_Parcours_Rachid.txt` et sont les mêmes sur tous les PC (le classement compare les mêmes courses).

Console (touche ²) :

| Commande | Effet |
|---|---|
| `Chelles.Course 2` | montre le départ de la course 2 |
| `Chelles.CourseFin` | abandonne la course |
| `Chelles.CoursesRefaire` | refait les courses automatiques |
| `Chelles.CourseCreer MA COURSE` | enregistre votre propre course : roulez, une porte est posée tous les 120 m |
| `Chelles.CourseCreerFin` | l'arrivée = là où vous êtes |
| `Chelles.Courses` | liste des courses et records dans le journal |

## Réglages

`Courses.txt` (Bloc-notes, sans recompiler) : touche, longueur des courses, écart et taille des portes,
compte à rebours, XP, classement en ligne. Records de ce PC : `Records_Courses.txt`.

## Classement en ligne (VPS)

`GET /api/courses?course=<id>` et `POST /api/courses` sur le serveur du chat (port 8080), fichier `courses.js`.
Table `courses_rue`. Un temps à plus de 252 km/h de moyenne est refusé. Retirer un tricheur :
`sudo mariadb chelles3d -e "UPDATE courses_rue SET efface=1 WHERE pseudo='NOM'"`.
