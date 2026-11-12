# Chelles HUD (plugin ChellesHud)

Affichage clair pendant le jeu, sur chaque PC (solo et multijoueur) :

- **En bas à gauche** : VIE (barre verte → orange → rouge, chiffres), NIVEAU + titre + barre d'XP,
  POUVOIR choisi avec Y et son état (PRÊT • J / ACTIF 0:12 / RECHARGE 0:32 / BLOQUÉ).
- **En haut à droite : la MINI-CARTE.** Le quartier vu du ciel autour de toi (80 m jusqu'au bord), qui tourne
  avec la caméra : ce qui est devant toi est en haut. Flèche dorée = toi, losanges roses = les 5 téléporteurs
  (collés au bord quand ils sont loin, pour montrer la direction), points rouges = zombies, points cyan = les
  autres joueurs en multijoueur, « N » = le nord. La vue du ciel montre les couleurs des matériaux sans lumière :
  elle reste lisible la nuit et sous la neige.
- **Sous la mini-carte**, seulement pendant une invasion : nombre de zombies restants + temps restant.
- Les bords de l'écran clignotent en rouge quand tu prends un coup, et pulsent quand la vie passe sous 25 %.
- Caché automatiquement pendant le menu, la pause, le chat et les options (quand la souris est visible).

Le HUD ne fait que **lire** les autres plugins (ChellesPouvoirs, ChellesZombies) : s'il en manque un,
sa partie ne s'affiche pas, rien ne plante. La vie est lue dans les variables `HealthCurrent` / `HealthMax`
du personnage joué (BP_CharacterBase et ses enfants). Un personnage sans vie (cheval, motoneige) affiche « — ».

## Installer

1. Fermer Unreal.
2. Double-cliquer `Plugins\ChellesHud\Compiler_le_plugin.bat`.
3. Double-cliquer `Plugins\ChellesPouvoirs\Compiler_le_plugin.bat` (petite modification : le HUD peut lire
   l'XP et le pouvoir choisi tout de suite). Sans cette étape le HUD marche quand même, mais l'XP arrive
   avec 1 s de retard et le pouvoir affiché est le dernier débloqué au lieu de celui choisi.
4. Ouvrir le projet, Play.

## Console (touche ²)

- `Chelles.Hud 0` cache le HUD, `Chelles.Hud 1` le remontre.
- `Chelles.MiniCarte 0` cache la mini-carte, `Chelles.MiniCarte 1` = points seuls sur fond quadrillé (plus léger
  si le jeu rame), `Chelles.MiniCarte 2` = avec le quartier vu du ciel (par défaut).
- `Chelles.MiniCarteZoom 150` voit plus loin (20 à 500 mètres, 80 par défaut).
- `Chelles.MiniCarteNord 90` si le « N » n'est pas du bon côté (-90 par défaut ; essayer 90, 0 ou 180).
- Pour tester : `Chelles.XP 300` (XP), `Chelles.Zombies` (lance une invasion), `Chelles.ZombiesFin`.

Pour garder un réglage à chaque lancement, ajouter par exemple `Chelles.MiniCarteZoom=120` sous `[SystemSettings]`
dans `Config\DefaultEngine.ini`.

Les téléporteurs sont trouvés tout seuls (classes dont le nom contient « Teleport », ex. BP_Teleporter) ; les zombies
aussi (personnages dont le nom contient « Zombie », plus la liste de l'invasion). Un téléporteur ajouté plus tard
apparaît donc sans rien changer.

Journal : lignes `[HUD]` dans `Saved\Logs\FPS_Chelles.log`.

Les paliers et les noms des pouvoirs sont recopiés de `ChellesPouvoirs\...\PouvoirsTypes.h`
(dans `SHudChelles.cpp`) : si on change l'un, changer l'autre.
