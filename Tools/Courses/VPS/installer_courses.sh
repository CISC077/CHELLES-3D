#!/bin/bash
# CHELLES 3D - installe le classement des COURSES DE RUE sur le VPS (a lancer avec sudo).
# Ajoute courses.js a cote du chat (/opt/chelles3d/chat), branche 2 lignes dans serveur.js
# (copie gardee : serveur.js.avant_courses_DATE), cree la table courses_rue et redemarre le chat.
set -u
SOURCE="$(cd "$(dirname "$0")" && pwd)"
DOSSIER="/opt/chelles3d/chat"
SERVICE="chelles3d-chat"
BASE="chelles3d"
UTILISATEUR="chelles"
DATE="$(date +%Y-%m-%d_%Hh%M)"

vert() { echo -e "\e[32m$*\e[0m"; }
rouge() { echo -e "\e[31m$*\e[0m"; }

if [ "$(id -u)" != "0" ]; then rouge "Lancez avec sudo."; exit 1; fi
if [ ! -f "$DOSSIER/serveur.js" ]; then rouge "Chat introuvable dans $DOSSIER : installez d'abord le chat."; exit 1; fi

echo "1/4  Copie de courses.js"
cp "$DOSSIER/serveur.js" "$DOSSIER/serveur.js.avant_courses_$DATE"
cp "$SOURCE/courses.js" "$DOSSIER/courses.js"
chown "$UTILISATEUR":"$UTILISATEUR" "$DOSSIER/courses.js" 2>/dev/null || true

echo "2/4  Branchement dans serveur.js"
if ! node "$SOURCE/brancher_courses.js" "$DOSSIER/serveur.js" || ! node --check "$DOSSIER/serveur.js"; then
  rouge "Probleme : serveur.js est remis comme avant."
  cp "$DOSSIER/serveur.js.avant_courses_$DATE" "$DOSSIER/serveur.js"
  exit 1
fi

echo "3/4  Table courses_rue dans la base"
mariadb "$BASE" <<'SQL'
CREATE TABLE IF NOT EXISTS courses_rue (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  course     VARCHAR(48) NOT NULL,
  nom        VARCHAR(48) NOT NULL DEFAULT '',
  pseudo     VARCHAR(24) NOT NULL,
  temps_ms   INT         NOT NULL,
  portes     SMALLINT    NOT NULL DEFAULT 0,
  longueur_m INT         NOT NULL DEFAULT 0,
  mode       VARCHAR(12) NOT NULL DEFAULT '',
  empreinte  CHAR(16)    NOT NULL,
  quand      DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
  efface     TINYINT     NOT NULL DEFAULT 0,
  INDEX (course, temps_ms),
  INDEX (pseudo)
);
SQL

echo "4/4  Redemarrage du chat"
systemctl restart "$SERVICE"
sleep 3
if ! systemctl is-active --quiet "$SERVICE"; then
  rouge "Le chat ne redemarre pas : on remet l'ancien serveur.js."
  journalctl -u "$SERVICE" -n 20 --no-pager
  cp "$DOSSIER/serveur.js.avant_courses_$DATE" "$DOSSIER/serveur.js"
  systemctl restart "$SERVICE"
  exit 1
fi
REPONSE="$(curl -s "http://127.0.0.1:8080/api/courses?course=TEST-00000000")"
echo "Test : $REPONSE"
if echo "$REPONSE" | grep -q '"meilleurs"'; then
  vert "TERMINE : le classement des courses de rue est en ligne (http://51.178.54.208:8080/api/courses)."
else
  rouge "Le chat tourne mais /api/courses ne repond pas comme prevu. Envoyez ce message a Claude."
fi
