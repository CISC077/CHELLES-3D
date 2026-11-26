/*
 * CHELLES 3D : les DÉFIS DE LA SEMAINE, depuis le VPS.
 *
 *  - Un défi par semaine (lundi 0 h -> dimanche 23 h 59, heure de Paris), pris dans defis.json.
 *  - Lundi à 18 h : annonce sur le Discord (bilan de la semaine passée + nouveau défi).
 *  - Vendredi à 18 h : rappel avec les meilleurs joueurs du moment.
 *  - Le jeu lit le défi : GET /api/defis/semaine?pseudo=Rachid
 *  - Les défis du DERNIER REFUGE sont vérifiés tout seuls avec les parties envoyées
 *    au classement (table scores_refuge, lecture seule).
 *
 *  L'adresse du webhook Discord reste UNIQUEMENT dans le fichier .env de ce dossier (chmod 600).
 *  Elle n'est jamais envoyée au jeu, jamais écrite dans le journal.
 *
 *  Commandes (sur le VPS) :
 *    sudo -u chelles bash -c "cd /opt/chelles3d/defis && node defis.js --tester"    (base + Discord + défi du moment)
 *    sudo -u chelles bash -c "cd /opt/chelles3d/defis && node defis.js --apercu"    (message Discord, sans l'envoyer)
 *    sudo -u chelles bash -c "cd /opt/chelles3d/defis && node defis.js --annoncer"  (envoie l'annonce maintenant)
 */
"use strict";
const http = require("http");
const fs = require("fs");
const path = require("path");

// ---------- Réglages (.env) ----------
function lireEnv(fichier) {
  const env = {};
  if (!fs.existsSync(fichier)) return env;
  for (const ligne of fs.readFileSync(fichier, "utf8").split(/\r?\n/)) {
    const m = /^\s*([A-Z0-9_]+)\s*=\s*(.*?)\s*$/.exec(ligne);
    if (m) env[m[1]] = m[2].replace(/^["']|["']$/g, "");
  }
  return env;
}
const ENV = Object.assign(lireEnv(path.join(__dirname, ".env")), process.env);
const PORT = parseInt(ENV.PORT_DEFIS || "8082", 10);
const WEBHOOK = /^https:\/\/(discord\.com|discordapp\.com)\/api\/webhooks\/\d+\/[\w-]+$/.test(ENV.DISCORD_WEBHOOK || "") ? ENV.DISCORD_WEBHOOK : "";
const SALON_PREUVES = /^\d{15,25}$/.test(ENV.SALON_PREUVES || "") ? ENV.SALON_PREUVES : "";
const HEURE_ANNONCE = Math.min(23, Math.max(0, parseInt(ENV.HEURE_ANNONCE || "18", 10)));
const HEURE_RAPPEL = Math.min(23, Math.max(0, parseInt(ENV.HEURE_RAPPEL || "18", 10)));
const RAPPEL = !/^(non|no|0|false)$/i.test(ENV.RAPPEL || "oui");
const FICHIER_DEFIS = path.join(__dirname, "defis.json");
const FICHIER_ETAT = path.join(__dirname, "etat.json");
const JOUR_MS = 864e5;
const NOM_DISCORD = "Défis CHELLES 3D";
const PIED = "CHELLES 3D • Défis de la semaine";

// ---------- Journal (jamais l'adresse du webhook) ----------
function masquer(texte) {
  let t = String(texte);
  if (WEBHOOK) t = t.split(WEBHOOK).join("***");
  return t.replace(/api\/webhooks\/\d+\/[\w-]+/g, "api/webhooks/***");
}
function journal(...m) { console.log(new Date().toISOString(), masquer(m.join(" "))); }

// ---------- Heure de Paris ----------
const FORMAT_PARIS = new Intl.DateTimeFormat("en-GB", {
  timeZone: "Europe/Paris", year: "numeric", month: "2-digit", day: "2-digit",
  hour: "2-digit", minute: "2-digit", hour12: false, weekday: "short"
});
function paris(d) {
  const o = {};
  for (const p of FORMAT_PARIS.formatToParts(d)) o[p.type] = p.value;
  return {
    an: +o.year, mois: +o.month, jour: +o.day, heure: (+o.hour) % 24, minute: +o.minute,
    jourSemaine: ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"].indexOf(o.weekday) + 1   // 1 = lundi
  };
}
/** Minutes d'avance de Paris sur l'heure UTC à cet instant (60 l'hiver, 120 l'été). */
function decalageParis(ms) {
  const minute = Math.floor(ms / 60000) * 60000;
  const p = paris(new Date(minute));
  return Math.round((Date.UTC(p.an, p.mois - 1, p.jour, p.heure, p.minute) - minute) / 60000);
}
/** Instant (ms UTC) de minuit à Paris pour ce jour du calendrier. */
function minuitParis(an, mois, jour) {
  const base = Date.UTC(an, mois - 1, jour);
  let t = base - decalageParis(base) * 60000;
  t = base - decalageParis(t) * 60000;
  return t;
}
/** La semaine (lundi -> dimanche, Paris) qui contient cet instant. */
function semaineDe(ms) {
  const p = paris(new Date(ms));
  const lundi = Date.UTC(p.an, p.mois - 1, p.jour - (p.jourSemaine - 1));   // date du lundi (calendrier)
  const L = new Date(lundi), S = new Date(lundi + 7 * JOUR_MS);
  const jeudi = new Date(lundi + 3 * JOUR_MS);
  const anIso = jeudi.getUTCFullYear();
  const numero = 1 + Math.floor((jeudi - Date.UTC(anIso, 0, 1)) / JOUR_MS / 7);
  return {
    cle: anIso + "-S" + String(numero).padStart(2, "0"),
    numero,
    index: Math.round(lundi / (7 * JOUR_MS)),
    debut: minuitParis(L.getUTCFullYear(), L.getUTCMonth() + 1, L.getUTCDate()),
    fin: minuitParis(S.getUTCFullYear(), S.getUTCMonth() + 1, S.getUTCDate())
  };
}
function dateFr(ms, options) {
  return new Date(ms).toLocaleDateString("fr-FR", Object.assign({ timeZone: "Europe/Paris" }, options));
}

// ---------- État (ce qui a déjà été annoncé) ----------
function lireEtat() {
  try { return JSON.parse(fs.readFileSync(FICHIER_ETAT, "utf8")); } catch (e) { return {}; }
}
function sauverEtat(etat) {
  const tmp = FICHIER_ETAT + ".tmp";
  fs.writeFileSync(tmp, JSON.stringify(etat, null, 2));
  fs.renameSync(tmp, FICHIER_ETAT);
}
let ETAT = {};
/** Relit etat.json (une commande --annoncer lancée à côté du service peut l'avoir changé). */
function rechargerEtat() {
  ETAT = lireEtat();
  ETAT.semaines = ETAT.semaines || {};
}
rechargerEtat();

// ---------- Les défis ----------
const TYPES = {
  score:         { verifiable: true, unite: v => v.toLocaleString("fr-FR") + " points", colonne: "score" },
  vague:         { verifiable: true, unite: v => "vague " + v, colonne: "vague" },
  kills:         { verifiable: true, unite: v => v + " zombies", colonne: "kills" },
  kills_semaine: { verifiable: true, unite: v => v + " zombies", colonne: "kills_semaine" },
  tetes:         { verifiable: true, unite: v => v + " tirs dans la tête", colonne: "tetes" },
  parties:       { verifiable: true, unite: v => v + (v > 1 ? " parties" : " partie"), colonne: "parties" },
  coop:          { verifiable: true, unite: v => "vague " + v + " en équipe", colonne: "coop" },
  libre:         { verifiable: false, unite: v => String(v), colonne: null }
};

function lireDefis() {
  let d;
  try { d = JSON.parse(fs.readFileSync(FICHIER_DEFIS, "utf8")); }
  catch (e) { journal("defis.json illisible :", e.message); d = {}; }
  const defis = (Array.isArray(d.defis) ? d.defis : []).filter(x => x && x.id && x.titre && TYPES[x.type]);
  if (!defis.length) defis.push({ id: "photo-quartier", type: "libre", objectif: 0, xp: 0, titre: "La plus belle photo du quartier", texte: "Poste ta plus belle capture de Chelles 3D sur le Discord." });
  return { defis, planning: d.planning && typeof d.planning === "object" ? d.planning : {} };
}

/** Le défi d'une semaine : celui du planning, sinon le suivant dans la liste (la 1re semaine = le 1er défi). */
function defiDeLaSemaine(s) {
  const { defis, planning } = lireDefis();
  const memo = ETAT.semaines[s.cle];
  if (memo && memo.id) {
    const deja = defis.find(x => x.id === memo.id);
    if (deja) return deja;                       // on ne change jamais le défi d'une semaine commencée
  }
  if (planning[s.cle]) {
    const impose = defis.find(x => x.id === planning[s.cle]);
    if (impose) return impose;
  }
  if (typeof ETAT.origine !== "number") { ETAT.origine = s.index; sauverEtat(ETAT); }
  const rang = ((s.index - ETAT.origine) % defis.length + defis.length) % defis.length;
  return defis[rang];
}
function retenirDefi(s, defi) {
  const e = ETAT.semaines[s.cle] = ETAT.semaines[s.cle] || {};
  if (e.id !== defi.id) { e.id = defi.id; sauverEtat(ETAT); }
}

// ---------- Base (lecture seule de scores_refuge) ----------
let base = null;
let decalageBaseMin = null;                      // heure de la base - heure UTC, en minutes
function ouvrirBase() {
  if (base) return base;
  if (!ENV.DB_MOT_DE_PASSE) return null;
  const mariadb = require("mariadb");
  base = mariadb.createPool({
    host: ENV.DB_HOTE || "127.0.0.1", port: parseInt(ENV.DB_PORT || "3306", 10),
    user: ENV.DB_UTILISATEUR || "chelles3d_defis", password: ENV.DB_MOT_DE_PASSE,
    database: ENV.DB_NOM || "chelles3d", connectionLimit: 2, acquireTimeout: 8000
  });
  return base;
}
async function sql(requete, valeurs) {
  const b = ouvrirBase();
  if (!b) throw new Error("base non réglée (.env)");
  const c = await b.getConnection();
  try { return await c.query(requete, valeurs); } finally { c.release(); }
}
function versHeureBase(ms) {
  const d = new Date(ms + decalageBaseMin * 60000);
  return d.toISOString().slice(0, 19).replace("T", " ");
}
const cacheStats = new Map();                    // cle semaine -> { quand, lignes }
/** Résultats de chaque joueur pour une semaine (meilleure partie, totaux). */
async function statsSemaine(s) {
  const memo = cacheStats.get(s.cle);
  if (memo && Date.now() - memo.quand < 30000) return memo.lignes;
  if (decalageBaseMin === null) {
    const r = await sql("SELECT TIMESTAMPDIFF(MINUTE, UTC_TIMESTAMP(), NOW()) AS d");
    decalageBaseMin = Number(r[0].d) || 0;
  }
  const lignes = await sql(
    `SELECT MIN(pseudo) AS pseudo, MAX(score) AS score, MAX(vague) AS vague, MAX(kills) AS kills,
            SUM(kills) AS kills_semaine, MAX(tetes) AS tetes, COUNT(*) AS parties,
            MAX(CASE WHEN joueurs >= 2 THEN vague ELSE 0 END) AS coop
     FROM scores_refuge
     WHERE efface = 0 AND quand >= ? AND quand < ? AND pseudo NOT LIKE 'Joueur-%'
     GROUP BY LOWER(pseudo)`,
    [versHeureBase(s.debut), versHeureBase(s.fin)]);
  const propres = lignes.map(l => ({
    pseudo: String(l.pseudo), score: Number(l.score), vague: Number(l.vague), kills: Number(l.kills),
    kills_semaine: Number(l.kills_semaine), tetes: Number(l.tetes), parties: Number(l.parties), coop: Number(l.coop)
  }));
  cacheStats.set(s.cle, { quand: Date.now(), lignes: propres });
  if (cacheStats.size > 6) cacheStats.delete(cacheStats.keys().next().value);
  return propres;
}
/** Classement d'un défi : [{ pseudo, valeur, reussi }] du meilleur au moins bon. */
async function resultats(s, defi) {
  const t = TYPES[defi.type];
  if (!t.verifiable) return [];
  const lignes = await statsSemaine(s);
  return lignes
    .map(l => ({ pseudo: l.pseudo, valeur: l[t.colonne] || 0 }))
    .filter(l => l.valeur > 0)
    .sort((a, b) => b.valeur - a.valeur || a.pseudo.localeCompare(b.pseudo))
    .map(l => Object.assign(l, { reussi: l.valeur >= defi.objectif }));
}

// ---------- Messages Discord ----------
function propre(pseudo) {
  return String(pseudo).replace(/([*_`~|>\\\[\]()])/g, "\\$1").replace(/@/g, "@\u200b").slice(0, 40);
}
function finTexte(s) {
  return dateFr(s.fin - 60000, { weekday: "long", day: "numeric", month: "long" }) + " à 23 h 59";
}
function salonPreuves() { return SALON_PREUVES ? "<#" + SALON_PREUVES + ">" : "le salon **captures-écran**"; }
const MEDAILLES = ["🥇", "🥈", "🥉"];

function embedDefi(s, defi) {
  const t = TYPES[defi.type];
  const lignes = [defi.texte || "", ""];
  if (t.verifiable) {
    lignes.push("🎯 **Objectif : " + t.unite(defi.objectif) + "**");
    if (defi.xp > 0) lignes.push("🎁 Récompense : **+" + defi.xp + " XP** dans le jeu (pouvoirs)");
    lignes.push("✅ Vérifié automatiquement : joue au **DERNIER REFUGE** avec ton pseudo du chat global (touche B).");
  } else {
    lignes.push("📸 Poste ta participation dans " + salonPreuves() + " avant la fin de la semaine.");
  }
  lignes.push("📅 Jusqu'au **" + finTexte(s) + "**");
  lignes.push("🎮 Le défi s'affiche aussi dans le jeu au début de la partie.");
  return {
    title: "🏆 Défi de la semaine " + s.numero + " : " + defi.titre,
    color: 0xF1C40F,
    description: lignes.join("\n"),
    footer: { text: PIED }
  };
}
function listeJoueurs(liste, defi, max) {
  const t = TYPES[defi.type];
  return liste.slice(0, max).map((l, i) => (MEDAILLES[i] || "▫️") + " **" + propre(l.pseudo) + "** : " + t.unite(l.valeur)).join("\n");
}
async function embedBilan(s, defi) {
  const t = TYPES[defi.type];
  let description;
  if (!t.verifiable) {
    description = "Merci à tous ceux qui ont participé dans " + salonPreuves() + " ! Les plus belles participations seront partagées sur nos réseaux. 💜";
  } else {
    const liste = await resultats(s, defi);
    const gagnants = liste.filter(l => l.reussi);
    if (!liste.length) description = "Personne n'a tenté le défi cette fois… À vous de jouer cette semaine ! 💪";
    else {
      description = (gagnants.length
        ? "**" + gagnants.length + (gagnants.length > 1 ? " joueurs ont" : " joueur a") + " réussi le défi** (" + t.unite(defi.objectif) + ") ! 🎉"
        : "Personne n'a atteint l'objectif (" + t.unite(defi.objectif) + "), mais bravo aux plus courageux :") + "\n\n" + listeJoueurs(liste, defi, 3);
      const autres = gagnants.slice(3, 25).map(l => propre(l.pseudo));
      if (autres.length) description += "\n\nBravo aussi à : " + autres.join(", ") + (gagnants.length > 25 ? "…" : "");
    }
  }
  return {
    title: "📊 Bilan de la semaine " + s.numero + " : " + defi.titre,
    color: 0x9B59B6,
    description,
    footer: { text: PIED }
  };
}
async function embedRappel(s, defi) {
  const t = TYPES[defi.type];
  let description = "Il reste jusqu'au **" + finTexte(s) + "** pour réussir : **" + defi.titre + "**";
  if (t.verifiable) {
    description += "\n🎯 Objectif : " + t.unite(defi.objectif);
    const liste = await resultats(s, defi);
    description += liste.length ? "\n\n**En tête pour l'instant :**\n" + listeJoueurs(liste, defi, 3) : "\n\nPersonne n'a encore réussi… la place est libre ! 👀";
  } else {
    description += "\n📸 Participations dans " + salonPreuves();
  }
  return { title: "⏰ Plus que quelques jours pour le défi !", color: 0xE67E22, description, footer: { text: PIED } };
}

async function envoyerDiscord(embeds) {
  if (!WEBHOOK) throw new Error("pas de webhook Discord (DISCORD_WEBHOOK dans .env)");
  for (let essai = 1; essai <= 3; essai++) {
    const ctrl = new AbortController();
    const minuterie = setTimeout(() => ctrl.abort(), 15000);
    try {
      const rep = await fetch(WEBHOOK + "?wait=true", {
        method: "POST", signal: ctrl.signal,
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ username: NOM_DISCORD, embeds, allowed_mentions: { parse: [] } })
      });
      if (rep.ok) return true;
      if (rep.status === 429) { await new Promise(r => setTimeout(r, 3000 * essai)); continue; }
      throw new Error("Discord a répondu " + rep.status + " " + (await rep.text()).slice(0, 200));
    } finally { clearTimeout(minuterie); }
  }
  throw new Error("Discord limite trop les envois");
}

/** Annonce de la semaine : bilan de la semaine passée (si elle avait été annoncée) + nouveau défi. */
async function messageAnnonce(s) {
  const defi = defiDeLaSemaine(s);
  const embeds = [];
  const avant = semaineDe(s.debut - 3600e3);
  const e0 = ETAT.semaines[avant.cle];
  let bilanFait = false;
  if (e0 && e0.annonce && !e0.bilan && e0.id) {
    const ancien = lireDefis().defis.find(x => x.id === e0.id);
    if (ancien) {
      try { embeds.push(await embedBilan(avant, ancien)); bilanFait = true; }
      catch (err) { journal("Bilan impossible (base) :", err.message); }
    }
  }
  embeds.push(embedDefi(s, defi));
  return { defi, embeds, avant, bilanFait };
}
async function annoncer(s) {
  const { defi, embeds, avant, bilanFait } = await messageAnnonce(s);
  await envoyerDiscord(embeds);
  retenirDefi(s, defi);
  ETAT.semaines[s.cle].annonce = Date.now();
  if (bilanFait) ETAT.semaines[avant.cle].bilan = Date.now();
  // on garde seulement les 12 dernières semaines
  const cles = Object.keys(ETAT.semaines).sort();
  while (cles.length > 12) delete ETAT.semaines[cles.shift()];
  sauverEtat(ETAT);
  journal("Annonce Discord envoyée :", s.cle, defi.id, bilanFait ? "(avec bilan " + avant.cle + ")" : "");
}
async function rappeler(s) {
  const defi = defiDeLaSemaine(s);
  await envoyerDiscord([await embedRappel(s, defi)]);
  ETAT.semaines[s.cle].rappel = Date.now();
  sauverEtat(ETAT);
  journal("Rappel Discord envoyé :", s.cle, defi.id);
}

// ---------- Horloge : vérifie chaque minute s'il faut annoncer ----------
let dernierEssai = 0, avertiSansWebhook = false;
async function horloge() {
  rechargerEtat();
  const maintenant = Date.now();
  const s = semaineDe(maintenant);
  retenirDefi(s, defiDeLaSemaine(s));
  if (!WEBHOOK) {
    if (!avertiSansWebhook) { journal("Pas de webhook Discord : les annonces sont en pause (le jeu reçoit quand même le défi)."); avertiSansWebhook = true; }
    return;
  }
  if (maintenant - dernierEssai < 10 * 60000) return;        // après un échec, on réessaie toutes les 10 min
  const p = paris(new Date(maintenant));
  const e = ETAT.semaines[s.cle];
  try {
    if (!e.annonce && (p.jourSemaine > 1 || p.heure >= HEURE_ANNONCE)) { dernierEssai = maintenant; await annoncer(s); dernierEssai = 0; return; }
    if (RAPPEL && e.annonce && !e.rappel && maintenant - e.annonce > JOUR_MS &&
        (p.jourSemaine > 5 || (p.jourSemaine === 5 && p.heure >= HEURE_RAPPEL))) {
      dernierEssai = maintenant; await rappeler(s); dernierEssai = 0;
    }
  } catch (err) {
    journal("Envoi Discord raté (nouvel essai dans 10 min) :", err.message);
  }
}

// ---------- API pour le jeu (et le site) ----------
const requetesParIp = new Map();
function tropDeRequetes(ip) {
  const minute = Math.floor(Date.now() / 60000);
  const e = requetesParIp.get(ip);
  if (!e || e.minute !== minute) { requetesParIp.set(ip, { minute, n: 1 }); return false; }
  e.n++;
  return e.n > 30;
}
setInterval(() => requetesParIp.clear(), 10 * 60000).unref();

function json(res, code, objet) {
  res.writeHead(code, { "Content-Type": "application/json; charset=utf-8", "Cache-Control": "no-store", "Access-Control-Allow-Origin": "*" });
  res.end(JSON.stringify(objet));
}
async function apiSemaine(url) {
  const s = semaineDe(Date.now());
  const defi = defiDeLaSemaine(s);
  const t = TYPES[defi.type];
  const reponse = {
    ok: true, semaine: s.cle, numero: s.numero,
    id: defi.id, titre: defi.titre, texte: defi.texte || "", type: defi.type,
    objectif: defi.objectif || 0, objectifTexte: t.verifiable ? t.unite(defi.objectif) : "",
    xp: t.verifiable ? (defi.xp || 0) : 0, verifiable: t.verifiable,
    debut: new Date(s.debut).toISOString(), fin: new Date(s.fin).toISOString(),
    resteSecondes: Math.max(0, Math.round((s.fin - Date.now()) / 1000)),
    finTexte: finTexte(s),
    reussis: 0, top: [], moi: null, base: true
  };
  if (t.verifiable) {
    try {
      const liste = await resultats(s, defi);
      reponse.reussis = liste.filter(l => l.reussi).length;
      reponse.top = liste.slice(0, 5).map(l => ({ pseudo: l.pseudo, valeur: l.valeur, texte: t.unite(l.valeur), reussi: l.reussi }));
      const pseudo = String(url.searchParams.get("pseudo") || "").trim().slice(0, 24);
      if (pseudo) {
        const rang = liste.findIndex(l => l.pseudo.toLowerCase() === pseudo.toLowerCase());
        const l = rang >= 0 ? liste[rang] : { valeur: 0, reussi: false };
        reponse.moi = { pseudo, valeur: l.valeur, texte: t.unite(l.valeur), reussi: l.reussi, rang: rang + 1 };
      }
    } catch (err) {
      reponse.base = false;
      journal("Lecture des scores impossible :", err.message);
    }
  }
  return reponse;
}

function demarrerServeur() {
  const serveur = http.createServer(async (req, res) => {
    const url = new URL(req.url, "http://localhost");
    const ip = (req.socket.remoteAddress || "").replace(/^::ffff:/, "");
    try {
      if (req.method === "OPTIONS") { res.writeHead(204, { "Access-Control-Allow-Origin": "*" }); return res.end(); }
      if (req.method !== "GET") return json(res, 405, { erreur: "GET seulement" });
      if (url.pathname === "/api/defis/sante") return json(res, 200, { ok: true, discord: !!WEBHOOK, semaine: semaineDe(Date.now()).cle });
      if (url.pathname === "/api/defis/semaine") {
        if (tropDeRequetes(ip)) return json(res, 429, { erreur: "trop vite" });
        return json(res, 200, await apiSemaine(url));
      }
      json(res, 404, { erreur: "inconnu" });
    } catch (err) {
      journal("ERREUR API", url.pathname, err.message);
      json(res, 500, { erreur: "serveur" });
    }
  });
  serveur.listen(PORT, "0.0.0.0", () => journal("Défis de la semaine prêts sur le port", PORT, WEBHOOK ? "(Discord OK)" : "(sans Discord)"));
  setTimeout(horloge, 5000);
  setInterval(horloge, 60000);
}

// ---------- Commandes ----------
async function tester() {
  const s = semaineDe(Date.now());
  const defi = defiDeLaSemaine(s);
  console.log("Semaine        : " + s.cle + "  (du " + dateFr(s.debut, { day: "numeric", month: "long" }) + " au " + finTexte(s) + ")");
  console.log("Défi           : " + defi.titre + "  [" + defi.type + (TYPES[defi.type].verifiable ? ", objectif " + TYPES[defi.type].unite(defi.objectif) : "") + "]");
  try {
    const liste = await resultats(s, Object.assign({}, defi, { type: TYPES[defi.type].verifiable ? defi.type : "parties", objectif: defi.objectif || 1 }));
    console.log("Base de données: OK (" + liste.length + " joueur(s) classé(s) cette semaine)");
  } catch (err) { console.log("Base de données: KO (" + err.message + ")"); }
  if (!WEBHOOK) console.log("Discord        : KO (pas de webhook dans .env)");
  else {
    try {
      const rep = await fetch(WEBHOOK);
      const info = await rep.json().catch(() => ({}));
      console.log("Discord        : " + (rep.ok ? "OK (webhook « " + info.name + " »)" : "KO (code " + rep.status + ")"));
    } catch (err) { console.log("Discord        : KO (" + err.message + ")"); }
  }
  const e = ETAT.semaines[s.cle] || {};
  console.log("Annonce        : " + (e.annonce ? "déjà envoyée le " + dateFr(e.annonce, { day: "numeric", month: "long", hour: "2-digit", minute: "2-digit" }) : "pas encore envoyée"));
}

async function principal() {
  const arg = process.argv[2] || "";
  if (arg === "--tester") { await tester(); return process.exit(0); }
  if (arg === "--apercu") {
    const { embeds } = await messageAnnonce(semaineDe(Date.now()));
    console.log(JSON.stringify(embeds, null, 2));
    return process.exit(0);
  }
  if (arg === "--annoncer") {
    try { await annoncer(semaineDe(Date.now())); console.log("Annonce envoyée sur le Discord."); return process.exit(0); }
    catch (err) { console.log("Échec : " + masquer(err.message)); return process.exit(1); }
  }
  demarrerServeur();
}

if (require.main === module) principal();
module.exports = { semaineDe, paris, minuitParis, defiDeLaSemaine, TYPES, embedDefi };
