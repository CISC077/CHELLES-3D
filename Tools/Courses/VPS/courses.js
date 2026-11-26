/*
 * CHELLES 3D - Classement des COURSES DE RUE (plugin ChellesCourses du jeu)
 *
 *   GET  /api/courses?course=<id>&limite=10   -> { meilleurs: [{ rang, pseudo, temps_ms, mode, quand }], total }
 *   GET  /api/courses                          -> { courses: [{ course, nom, joueurs, record_ms, pseudo }] }
 *   POST /api/courses { pseudo, course, nom, temps_ms, portes, longueur_m, mode } -> { ok, rang, record, total }
 *
 * Branché dans serveur.js par installer_courses.sh (2 lignes ajoutées, une copie de serveur.js est gardée).
 * Le classement garde le meilleur temps de chaque pseudo, course par course.
 */
"use strict";

const LIMITES = {
  intervalleMs: 10000,        // un envoi toutes les 10 s par PC
  tempsMaxMs: 60 * 60 * 1000, // 1 heure
  vitesseMaxMs: 70,           // 70 m/s = 252 km/h de moyenne : au-delà c'est de la triche
  longueurMaxM: 50000
};

module.exports = function brancherCourses(o) {
  const { sql, json, ipDe, empreinte, lireCorps, entier, pseudoValide, diffuser, systeme, journal } = o;
  const envois = new Map();
  let tablePrete = null;

  function preparerTable() {
    if (!tablePrete) {
      tablePrete = sql(`CREATE TABLE IF NOT EXISTS courses_rue (
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
      )`).catch(e => { journal("Courses : table courses_rue", e.message); }); // créée aussi par installer_courses.sh
    }
    return tablePrete;
  }

  function idValide(v) {
    const id = String(v || "").trim();
    return /^[A-Za-z0-9-]{3,48}$/.test(id) ? id : null;
  }

  async function meilleurs(course) {
    // Meilleur temps de chaque pseudo sur cette course (à égalité : le plus ancien)
    const lignes = await sql(
      `SELECT pseudo, temps_ms, mode, quand FROM courses_rue
       WHERE course = ? AND efface = 0 ORDER BY temps_ms ASC, quand ASC LIMIT 2000`, [course]);
    const vus = new Set(); const liste = [];
    for (const l of lignes) {
      const cle = l.pseudo.toLowerCase();
      if (vus.has(cle)) continue;
      vus.add(cle);
      liste.push(l);
    }
    return liste;
  }

  async function lire(req, res, url) {
    await preparerTable();
    const course = idValide(url.searchParams.get("course"));
    if (!course) {
      // Sans course : la liste des courses jouées (pour le site)
      const lignes = await sql(
        `SELECT c.course, MAX(c.nom) AS nom, COUNT(DISTINCT LOWER(c.pseudo)) AS joueurs, MIN(c.temps_ms) AS record_ms
         FROM courses_rue c WHERE c.efface = 0 GROUP BY c.course ORDER BY joueurs DESC LIMIT 50`);
      return json(res, 200, { courses: lignes });
    }
    const limite = entier(url.searchParams.get("limite") || 10, 1, 100) || 10;
    const tous = await meilleurs(course);
    json(res, 200, {
      course,
      meilleurs: tous.slice(0, limite).map((l, i) => Object.assign({ rang: i + 1 }, l)),
      total: tous.length
    });
  }

  async function envoyer(req, res) {
    await preparerTable();
    const e = empreinte(ipDe(req), "courses");
    if (Date.now() - (envois.get(e) || 0) < LIMITES.intervalleMs) return json(res, 429, { erreur: "trop vite" });
    let m;
    try { m = JSON.parse(await lireCorps(req, 2048)); } catch (err) { return json(res, 400, { erreur: "format" }); }
    const course = idValide(m.course);
    const temps = entier(m.temps_ms, 1000, LIMITES.tempsMaxMs);
    const portes = entier(m.portes, 1, 500);
    const longueur = entier(m.longueur_m, 50, LIMITES.longueurMaxM);
    if (!course || temps === null || portes === null || longueur === null) return json(res, 400, { erreur: "valeurs" });
    if (temps < longueur * 1000 / LIMITES.vitesseMaxMs) return json(res, 400, { erreur: "trop rapide" });
    const nom = String(m.nom || "").replace(/[\u0000-\u001f\u007f]/g, " ").trim().slice(0, 48);
    const mode = String(m.mode || "").replace(/[^a-z ]/gi, "").slice(0, 12);
    const pseudo = pseudoValide(m.pseudo) || ("Joueur-" + e.slice(0, 4));
    envois.set(e, Date.now());
    await sql("INSERT INTO courses_rue (course, nom, pseudo, temps_ms, portes, longueur_m, mode, empreinte) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
      [course, nom, pseudo, temps, portes, longueur, mode, e]);
    const tous = await meilleurs(course);
    const rang = tous.findIndex(l => l.pseudo.toLowerCase() === pseudo.toLowerCase()) + 1;
    const record = rang > 0 && tous[rang - 1].temps_ms === temps;
    journal("Course", course, pseudo, temps, "ms rang", rang);
    if (rang === 1 && record && !pseudo.startsWith("Joueur-")) {
      const s = (temps / 1000).toFixed(2).replace(".", ",");
      diffuser(systeme("🏁 " + pseudo + " bat le record de la course " + (nom || course) + " : " + s + " s !"));
    }
    json(res, 200, { ok: true, rang, record, total: tous.length });
  }

  /** Rend une promesse si la requête est pour les courses, sinon null. */
  return function routeCourses(req, res, url) {
    if (url.pathname !== "/api/courses") return null;
    if (req.method === "GET") return lire(req, res, url);
    if (req.method === "POST") return envoyer(req, res);
    return null;
  };
};
