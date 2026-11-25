#!/usr/bin/env bash
# =====================================================================
#  CHELLES 3D : installation de l'IA des PNJ sur le VPS (Ubuntu 22.04)
#  À lancer avec :  sudo bash installer_pnj.sh
#
#  - service "chelles3d-pnj" (port 8081), séparé du chat : ne touche pas au chat.
#  - les clés API sont lues dans  ~/serveur-pnj/cles.env  (fichier envoyé à part),
#    copiées dans /opt/chelles3d/pnj/.env (chmod 600, propriétaire chelles),
#    puis le fichier envoyé est EFFACÉ.
#  - peut être relancé sans danger (garde les clés et le cache).
# =====================================================================
set -euo pipefail

SOURCE="$(cd "$(dirname "$0")" && pwd)"
DOSSIER="/opt/chelles3d/pnj"
UTILISATEUR="chelles"
SERVICE="chelles3d-pnj"
PORT=8081

vert()  { echo -e "\e[1;32m$*\e[0m"; }
rouge() { echo -e "\e[1;31m$*\e[0m"; }

[ "$(id -u)" -eq 0 ] || { rouge "Lancez avec sudo :  sudo bash installer_pnj.sh"; exit 1; }
command -v node >/dev/null || { rouge "Node.js absent (il est installé avec le chat)."; exit 1; }
[ "$(node -v | sed 's/^v//' | cut -d. -f1)" -ge 18 ] || { rouge "Node.js 18 ou plus requis."; exit 1; }
id "$UTILISATEUR" >/dev/null 2>&1 || useradd --system --create-home --shell /usr/sbin/nologin "$UTILISATEUR"

vert "==> 1/4  Fichiers dans $DOSSIER"
mkdir -p "$DOSSIER"
install -m 644 "$SOURCE/pnj.js" "$DOSSIER/pnj.js"

vert "==> 2/4  Clés API (.env, lisible seulement par le service)"
if [ -f "$SOURCE/cles.env" ]; then
  sed -i 's/\r$//' "$SOURCE/cles.env"
  install -m 600 -o "$UTILISATEUR" -g "$UTILISATEUR" "$SOURCE/cles.env" "$DOSSIER/.env"
  shred -u "$SOURCE/cles.env" 2>/dev/null || rm -f "$SOURCE/cles.env"
  echo "Clés installées, fichier envoyé effacé."
elif [ -f "$DOSSIER/.env" ]; then
  echo "Clés déjà en place (gardées)."
else
  rouge "Pas de clés : envoyez cles.env avec le .bat."; exit 1
fi
grep -q '^PORT_PNJ=' "$DOSSIER/.env" || echo "PORT_PNJ=$PORT" >> "$DOSSIER/.env"
chown "$UTILISATEUR":"$UTILISATEUR" "$DOSSIER" "$DOSSIER/pnj.js" "$DOSSIER/.env"
chmod 700 "$DOSSIER"
chmod 600 "$DOSSIER/.env"

vert "==> 3/4  Service $SERVICE"
cat > "/etc/systemd/system/$SERVICE.service" <<UNIT
[Unit]
Description=CHELLES 3D - IA des PNJ (repliques des passants)
After=network-online.target
Wants=network-online.target

[Service]
User=$UTILISATEUR
WorkingDirectory=$DOSSIER
ExecStart=$(command -v node) $DOSSIER/pnj.js
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

vert "==> 4/4  Test des clés (OK/KO, les clés ne s'affichent jamais)"
sudo -u "$UTILISATEUR" bash -c "cd $DOSSIER && node pnj.js --tester" || true
sleep 2
if curl -fs "http://127.0.0.1:$PORT/api/pnj/sante" >/dev/null; then
  vert "IA des PNJ EN LIGNE : http://51.178.54.208:$PORT/api/pnj?categorie=Salut"
  curl -s "http://127.0.0.1:$PORT/api/pnj?categorie=Salut&nb=3"; echo
else
  rouge "Le service ne répond pas :  sudo journalctl -u $SERVICE -n 40"
fi
