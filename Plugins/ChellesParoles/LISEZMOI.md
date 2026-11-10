# Chelles Paroles des passants

Les passants parlent **en français**, comme dans un jeu en monde ouvert :

| Situation | Catégorie dans `Repliques.txt` | Exemple |
|---|---|---|
| Vous passez près d'un passant (1 fois sur 3 environ) | `[Salut]` | « Bonjour ! », « Le bus est encore en retard. » |
| Vous le bousculez en courant (il est aussi un peu poussé) | `[Bousculade]` | « Eh ! Regarde où tu marches ! » |
| Vous foncez près de lui en motoneige / auto | `[Vehicule]` | « Ralentis, espèce de fou ! » |
| Vous restez planté devant lui 4 secondes | `[Remarque]` | « Qu'est-ce que tu regardes ? » |

La voix sort de la bouche du passant (son 3D, elle baisse avec la distance). Chaque passant garde sa voix ; une voix d'homme ou de femme est choisie d'après le nom du personnage 3D.

Rien n'est modifié dans le projet : pas de niveau, pas de Config, pas de Blueprint. L'acteur « Paroles des passants (Chelles) » s'ajoute tout seul au lancement de la partie. Pour retirer : supprimer le dossier `Plugins\ChellesParoles`.

## Installation (une seule fois)

1. Fermez Unreal.
2. Double-cliquez sur `Compiler_le_plugin.bat` (1 à 5 minutes).
3. Double-cliquez sur `Generer_les_voix.bat` : il fabrique les voix avec les voix françaises de Windows (gratuit, sans clé API, sans Internet). Il crée le dossier `Voix`.
4. Ouvrez le projet et lancez la partie (Play).

Quand un passant parle, sa phrase apparaît dans une **bulle noire avec le texte en blanc** au-dessus de sa tête (même sans l'étape 3). Réglages de la bulle (portée, taille du texte) dans l'acteur, catégorie « Paroles > Bulles ».

## Changer les phrases

Ouvrez `Repliques.txt` avec le Bloc-notes, ajoutez ou modifiez des lignes, enregistrez, puis relancez `Generer_les_voix.bat`. Pas besoin de recompiler.

## Avoir plus de voix

Windows 10/11 : Paramètres > Heure et langue > Voix > Ajouter des voix > Français (France). `Generer_les_voix.bat` utilise toutes les voix françaises trouvées (ex. Hortense, Julie, Paul).

## Réglages

Posez vous-même l'acteur « Paroles des passants (Chelles) » dans le niveau pour changer : distance, chance de parler, volume, portée de la voix, sous-titres toujours affichés, délai entre deux phrases…
