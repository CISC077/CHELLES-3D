-- CHELLES 3D : tables de la base (créées automatiquement par installer_chat.sh)
CREATE TABLE IF NOT EXISTS compteurs (
  nom    VARCHAR(32) PRIMARY KEY,
  valeur BIGINT NOT NULL DEFAULT 0
);
INSERT IGNORE INTO compteurs (nom, valeur) VALUES ('vues', 0);

CREATE TABLE IF NOT EXISTS visites (
  jour      DATE     NOT NULL,
  empreinte CHAR(16) NOT NULL,
  PRIMARY KEY (jour, empreinte)
);

CREATE TABLE IF NOT EXISTS messages (
  id      BIGINT AUTO_INCREMENT PRIMARY KEY,
  pseudo  VARCHAR(24)  NOT NULL,
  texte   VARCHAR(400) NOT NULL,
  source  VARCHAR(8)   NOT NULL DEFAULT 'site',
  quand   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  efface  TINYINT      NOT NULL DEFAULT 0,
  INDEX (quand)
);

CREATE TABLE IF NOT EXISTS bannis (
  empreinte CHAR(16)     PRIMARY KEY,
  pseudo    VARCHAR(24)  NOT NULL,
  raison    VARCHAR(120) NOT NULL DEFAULT '',
  quand     DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Classement du DERNIER REFUGE (vagues de zombies du jeu) : une ligne par partie terminée
CREATE TABLE IF NOT EXISTS scores_refuge (
  id        BIGINT AUTO_INCREMENT PRIMARY KEY,
  pseudo    VARCHAR(24) NOT NULL,
  score     INT         NOT NULL,
  vague     INT         NOT NULL,
  kills     INT         NOT NULL DEFAULT 0,
  tetes     INT         NOT NULL DEFAULT 0,
  joueurs   TINYINT     NOT NULL DEFAULT 1,
  empreinte CHAR(16)    NOT NULL,
  quand     DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
  efface    TINYINT     NOT NULL DEFAULT 0,
  INDEX (score),
  INDEX (pseudo)
);
