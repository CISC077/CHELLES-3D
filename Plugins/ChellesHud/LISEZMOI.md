# Chelles HUD (plugin ChellesHud)

Affichage clair pendant le jeu, sur chaque PC (solo et multijoueur) :

- **En bas à gauche** : VIE (barre verte → orange → rouge, chiffres), NIVEAU + titre + barre d'XP,
  POUVOIR choisi avec Y et son état (PRÊT • J / ACTIF 0:12 / RECHARGE 0:32 / BLOQUÉ).
- **En haut à droite**, seulement pendant une invasion : nombre de zombies restants + temps restant.
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
- Pour tester : `Chelles.XP 300` (XP), `Chelles.Zombies` (lance une invasion), `Chelles.ZombiesFin`.

Journal : lignes `[HUD]` dans `Saved\Logs\FPS_Chelles.log`.

Les paliers et les noms des pouvoirs sont recopiés de `ChellesPouvoirs\...\PouvoirsTypes.h`
(dans `SHudChelles.cpp`) : si on change l'un, changer l'autre.
