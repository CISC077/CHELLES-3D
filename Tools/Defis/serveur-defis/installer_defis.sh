#!/usr/bin/env bash
# =====================================================================
#  CHELLES 3D : installation des DÉFIS DE LA SEMAINE sur le VPS (Ubuntu 22.04)
#  Lancé par ENVOYER_LES_DEFIS_SUR_LE_VPS.bat (sudo bash installer_defis.sh)
#
#  - service "chelles3d-defis" (port 8082), séparé du chat : ne touche pas au chat.
#  - lit le classement du DERNIER REFUGE avec un compte MariaDB en LECTURE SEULE
#    (chelles3d_defis, seulement la table scores_refuge).
#  - l'adresse du webhook Discord arrive dans ~/serveur-defis/discord.env (fichier envoyé
#    à part), elle est rangée dans /opt/chelles3d/defis/.env (chmod 600), puis le fichier
#    envoyé est EFFACÉ.
#  - peut être relancé sans danger (garde les réglages et ce qui a déjà été annoncé).
# =====================================================================
set -euo pipefail

SOURCE="$(cd "$(dirname "$0")" && pwd)"
DOSSIER="/opt/chelles3d/defis"
UTILISATEUR="chelles"
SERVICE="chelles3d-defis"
PORT=8082
ENV_CHAT="/opt/chelles3d/chat/.env"

vert()  { echo -e "\e[1;32m$*\e[0m"; }
jaune() { echo -e "\e[1;33m$*\e[0m"; }
rouge() { echo -e "\e[1;31m$*\e[0m"; }

[ "$(id -u)" -eq 0 ] || { rouge "Lancez avec sudo :  sudo bash installer_defis.sh"; exit 1; }
command -v node >/dev/null || { rouge "Node.js absent (il est installé avec le chat)."; exit 1; }
[ "$(node -v | sed 's/^v//' | cut -d. -f1)" -ge 18 ] || { rouge "Node.js 18 ou plus requis."; exit 1; }
command -v npm >/dev/null || { rouge "npm absent (il est installé avec le chat)."; exit 1; }
command -v mariadb >/dev/null || { rouge "MariaDB absent (il est installé avec le chat)."; exit 1; }
id "$UTILISATEUR" >/dev/null 2>&1 || useradd --system --create-home --shell /usr/sbin/nologin "$UTILISATEUR"

vert "==> 1/5  Fichiers dans $DOSSIER"
mkdir -p "$DOSSIER"
install -m 644 "$SOURCE/defis.js" "$DOSSIER/defis.js"
install -m 644 "$SOURCE/defis.json" "$DOSSIER/defis.json"
install -m 644 "$SOURCE/package.json" "$DOSSIER/package.json"
( cd "$DOSSIER" && npm install --omit=dev --no-audit --no-fund --loglevel=error )

vert "==> 2/5  Accès en lecture seule au classement"
touch "$DOSSIER/.env"
BASE="chelles3d"
if [ -f "$ENV_CHAT" ] && grep -q '^DB_NOM=' "$ENV_CHAT"; then BASE="$(grep '^DB_NOM=' "$ENV_CHAT" | cut -d= -f2-)"; fi
MDP="$(grep '^DB_MOT_DE_PASSE=' "$DOSSIER/.env" | cut -d= -f2- || true)"
if [ -z "$MDP" ]; then
  MDP="$(head -c 32 /dev/urandom | base64 | tr -dc 'A-Za-z0-9' | head -c 28)"
  {
    echo "DB_HOTE=127.0.0.1"
    echo "DB_PORT=3306"
    echo "DB_UTILISATEUR=chelles3d_defis"
    echo "DB_MOT_DE_PASSE=$MDP"
    echo "DB_NOM=$BASE"
  } >> "$DOSSIER/.env"
fi
mariadb <<SQL
CREATE USER IF NOT EXISTS 'chelles3d_defis'@'localhost' IDENTIFIED BY '$MDP';
CREATE USER IF NOT EXISTS 'chelles3d_defis'@'127.0.0.1' IDENTIFIED BY '$MDP';
ALTER USER 'chelles3d_defis'@'localhost' IDENTIFIED BY '$MDP';
ALTER USER 'chelles3d_defis'@'127.0.0.1' IDENTIFIED BY '$MDP';
GRANT SELECT ON \`$BASE\`.scores_refuge TO 'chelles3d_defis'@'localhost';
GRANT SELECT ON \`$BASE\`.scores_refuge TO 'chelles3d_defis'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL
echo "Compte chelles3d_defis prêt (lecture seule de $BASE.scores_refuge)."

vert "==> 3/5  Discord (l'adresse du webhook n'est jamais affichée)"
if [ -f "$SOURCE/discord.env" ]; then
  sed -i 's/\r$//' "$SOURCE/discord.env"
  for v in $(cut -d= -f1 "$SOURCE/discord.env" | grep -E '^[A-Z0-9_]+$'); do sed -i "/^$v=/d" "$DOSSIER/.env"; done
  cat "$SOURCE/discord.env" >> "$DOSSIER/.env"
  shred -u "$SOURCE/discord.env" 2>/dev/null || rm -f "$SOURCE/discord.env"
  echo "Webhook Discord installé, fichier envoyé effacé."
elif grep -q '^DISCORD_WEBHOOK=' "$DOSSIER/.env"; then
  echo "Webhook Discord déjà en place (gardé)."
else
  jaune "Pas de webhook Discord : le jeu aura le défi, mais rien ne sera annoncé sur le Discord."
fi
grep -q '^PORT_DEFIS=' "$DOSSIER/.env" || echo "PORT_DEFIS=$PORT" >> "$DOSSIER/.env"
chown -R "$UTILISATEUR":"$UTILISATEUR" "$DOSSIER"
chmod 700 "$DOSSIER"
chmod 600 "$DOSSIER/.env"

vert "==> 4/5  Service $SERVICE"
cat > "/etc/systemd/system/$SERVICE.service" <<UNIT
[Unit]
Description=CHELLES 3D - Defis de la semaine (Discord + jeu)
After=network-online.target mariadb.service
Wants=network-online.target mariadb.service

[Service]
User=$UTILISATEUR
WorkingDirectory=$DOSSIER
ExecStart=$(command -v node) $DOSSIER/defis.js
Restart=always
RestartSec=5
Environment=NODE_ENV=production
NoNewPrivileges=true
ProtectSystem=full
ProtectHome=true

[Install]
WantedBy=multi-user.target
UNIT
systemctl daemon-reload
systemctl enable "$SERVICE" >/dev/null
systemctl restart "$SERVICE"
ufw allow $PORT/tcp >/dev/null 2>&1 || true

vert "==> 5/5  Test"
sudo -u "$UTILISATEUR" bash -c "cd $DOSSIER && node defis.js --tester" || true
sleep 8
if curl -fs "http://127.0.0.1:$PORT/api/defis/sante" >/dev/null; then
  vert "DÉFIS EN LIGNE : http://51.178.54.208:$PORT/api/defis/semaine"
  journalctl -u "$SERVICE" -n 5 --no-pager -o cat || true
else
  rouge "Le service ne répond pas :  sudo journalctl -u $SERVICE -n 40"
fi
