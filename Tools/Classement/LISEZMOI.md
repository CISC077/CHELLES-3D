# Classement des joueurs du DERNIER REFUGE (serveur du VPS)

Le jeu (plugin `ChellesSurvie`) envoie chaque partie terminée au serveur CHELLES 3D
du VPS, qui garde le meilleur score de chaque pseudo.

- `GET  http://51.178.54.208:8080/api/classement?limite=10` : les meilleurs (JSON `meilleurs`, `total`)
- `POST http://51.178.54.208:8080/api/classement` : `{pseudo, score, vague, kills, tetes, joueurs}`
  (une partie toutes les 15 s au plus par adresse, valeurs vérifiées)
- Chat admin : `/score-effacer pseudo` retire un tricheur du classement.
- Le 1er du classement est annoncé dans le chat global.

## Mettre à jour le serveur (une fois)

Les fichiers `serveur.js` et `base.sql` de `D:\SITE CHELLES3D\VPS\serveur-chat` ont été mis à jour
(les anciens sont dans `Tools\Classement\sauvegarde_avant`). Double-cliquez
`D:\SITE CHELLES3D\VPS\ENVOYER_LE_CHAT_SUR_LE_VPS.bat` : il envoie le serveur, crée la
table `scores_refuge` et redémarre le service `chelles3d-chat`.

Vérification dans un navigateur : http://51.178.54.208:8080/api/classement
doit afficher `{"meilleurs":[],"total":0}`.
