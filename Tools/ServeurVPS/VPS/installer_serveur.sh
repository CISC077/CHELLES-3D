#!/usr/bin/env bash
# ------------------------------------------------------------------
#  CHELLES 3D - installe (ou met a jour) le serveur de jeu sur le VPS.
#  Utilisation (dans le terminal SSH, depuis /home/ubuntu) :
#     sudo bash installer_serveur.sh
#  Il cherche CHELLES3D_Serveur_Linux.tar.gz a cote de lui.
#
#  Ce que fait ce script :
#   - installe le serveur dans /opt/chelles3d/serveur (l'ancien est garde en /opt/chelles3d/ancien)
#   - le fait tourner sous un utilisateur dedie "chelles" (pas root)
#   - cree le service "chelles3d" : demarre tout seul au boot, redemarre s'il plante
#   - ouvre le port 7777 UDP dans le pare-feu ufw (sans fermer SSH ni le site web)
#  Il ne fait AUCUNE mise a niveau d'Ubuntu (on reste en 22.04).
# ------------------------------------------------------------------
set -euo pipefail

PORT=7777
CARTE=/Game/Maps/Rachid
BASE=/opt/chelles3d
SERVICE=chelles3d
ICI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCHIVE="${1:-$ICI/CHELLES3D_Serveur_Linux.tar.gz}"

vert()  { printf '\033[1;32m%s\033[0m\n' "$*"; }
rouge() { printf '\033[1;31m%s\033[0m\n' "$*"; }

if [ "$(id -u)" -ne 0 ]; then
  rouge "Lancez-le avec sudo :  sudo bash installer_serveur.sh"
  exit 1
fi
if [ ! -f "$ARCHIVE" ]; then
  rouge "Archive introuvable : $ARCHIVE"
  echo "Envoyez d'abord CHELLES3D_Serveur_Linux.tar.gz dans /home/ubuntu avec FileZilla."
  exit 1
fi

# Place libre (l'archive decompressee prend environ 2 fois sa taille)
BESOIN=$(( $(stat -c %s "$ARCHIVE") * 3 / 1024 / 1024 / 1024 + 1 ))
LIBRE=$(df -BG --output=avail /opt | tail -1 | tr -dc '0-9')
echo "Place libre : ${LIBRE} Go (il en faut environ ${BESOIN})"
if [ "$LIBRE" -lt "$BESOIN" ]; then
  rouge "Pas assez de place sur le VPS."
  exit 1
fi

# 1) Utilisateur dedie
if ! id chelles >/dev/null 2>&1; then
  useradd --system --home-dir "$BASE" --shell /usr/sbin/nologin chelles
  vert "Utilisateur 'chelles' cree."
fi
mkdir -p "$BASE"

# 2) Arret de l'ancien serveur + sauvegarde
if systemctl list-unit-files | grep -q "^${SERVICE}.service"; then
  systemctl stop "$SERVICE" || true
fi
if [ -d "$BASE/serveur" ]; then
  rm -rf "$BASE/ancien"
  mv "$BASE/serveur" "$BASE/ancien"
  echo "Ancienne version gardee dans $BASE/ancien"
fi

# 3) Decompression
echo "Decompression (quelques minutes)..."
mkdir -p "$BASE/serveur"
tar -xzf "$ARCHIVE" -C "$BASE/serveur" --strip-components=1
LANCEUR=$(find "$BASE/serveur" -maxdepth 1 -name '*Server.sh' | head -1)
if [ -z "$LANCEUR" ]; then
  rouge "Lanceur *Server.sh introuvable dans l'archive : ce n'est pas un serveur Linux."
  exit 1
fi
find "$BASE/serveur" -name '*.sh' -exec sed -i 's/\r$//' {} +
chmod +x "$LANCEUR"
find "$BASE/serveur" -path '*/Binaries/Linux/*' -type f -exec chmod +x {} +
chown -R chelles:chelles "$BASE"
vert "Serveur installe : $LANCEUR"

# 4) Service systemd
cat > /etc/systemd/system/${SERVICE}.service <<EOF
[Unit]
Description=Serveur de jeu CHELLES 3D (port ${PORT} UDP)
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=chelles
Group=chelles
WorkingDirectory=${BASE}/serveur
ExecStart=${LANCEUR} ${CARTE} -port=${PORT} -log -unattended
Restart=on-failure
RestartSec=15
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
EOF
systemctl daemon-reload
systemctl enable "$SERVICE" >/dev/null 2>&1
vert "Service '${SERVICE}' cree (demarre tout seul au boot du VPS)."

# 5) Pare-feu ufw : on garde SSH, le site (80/443) et on ouvre le jeu
if ! command -v ufw >/dev/null 2>&1; then
  apt-get install -y ufw
fi
PORT_SSH=$(sshd -T 2>/dev/null | awk '/^port /{print $2; exit}')
PORT_SSH=${PORT_SSH:-22}
ufw allow "${PORT_SSH}/tcp" comment 'SSH' >/dev/null
ufw allow 80/tcp comment 'Site web' >/dev/null
ufw allow 443/tcp comment 'Site web https' >/dev/null
ufw allow "${PORT}/udp" comment 'Serveur CHELLES 3D' >/dev/null
if ! ufw status | grep -q "Status: active"; then
  ufw --force enable >/dev/null
fi
vert "Pare-feu : SSH (${PORT_SSH}), 80, 443 et ${PORT}/udp ouverts."

# 6) Demarrage
systemctl start "$SERVICE"
echo "Demarrage du serveur (la carte Rachid met 30 s a 2 min a charger)..."
for i in $(seq 1 24); do
  sleep 5
  if ss -uln | grep -q ":${PORT} "; then
    break
  fi
  if ! systemctl is-active --quiet "$SERVICE"; then
    break
  fi
done

echo
if ss -uln | grep -q ":${PORT} "; then
  IP=$(hostname -I | awk '{print $1}')
  vert "=============================================="
  vert " SERVEUR CHELLES 3D EN LIGNE"
  vert " Dans le jeu : MULTI-PLAYER > ADRESSE IP SERVEUR > ${IP}"
  vert "=============================================="
else
  rouge "Le serveur n'ecoute pas encore sur le port ${PORT}."
  echo "Regardez le journal :  sudo journalctl -u ${SERVICE} -n 80 --no-pager"
fi
echo
echo "Commandes utiles :"
echo "  Etat          : sudo systemctl status ${SERVICE}"
echo "  Journal live  : sudo journalctl -u ${SERVICE} -f      (Ctrl+C pour quitter)"
echo "  Redemarrer    : sudo systemctl restart ${SERVICE}"
echo "  Arreter       : sudo systemctl stop ${SERVICE}"
echo "  Log Unreal    : ${BASE}/serveur/FPS_Chelles/Saved/Logs/FPS_Chelles.log"
