/*
 * CHELLES 3D – serveur du VPS
 *   - Chat global : joueurs du jeu + visiteurs du site, dans un seul salon (WebSocket /chat)
 *   - Compteur de visiteurs du site (GET /api/compteur)
 *   - Vidéos du site : dossier "/Videos site" (GET /api/videos et /videos/nom.mp4)
 *   - Monuments : dossier "/Monuments site" (GET /api/monuments)
 *   - Le foutoir 3D : dossier "/Foutoir site" (GET /api/foutoire), vignettes faites par ImageMagick
 *   - Classement du DERNIER REFUGE (jeu) : GET /api/classement, POST /api/classement (fin de partie)
 *   - Base de données MariaDB (réglages dans le fichier .env)
 *
 * Démarré par le service systemd "chelles3d-chat". Journal : sudo journalctl -u chelles3d-chat -n 80
 */
"use strict";
const http = require("http");
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");
const { execFile } = require("child_process");
const { WebSocketServer } = require("ws");
const mariadb = require("mariadb");

// ---------- Réglages (.env) ----------
function lireEnv(fichier) {
  const env = {};
  if (!fs.existsSync(fichier)) return env;
  for (const ligne of fs.readFileSync(fichier, "utf8").split(/\r?\n/)) {
    const m = /^\s*([A-Z0-9_]+)\s*=\s*(.*)\s*$/.exec(ligne);
    if (m) env[m[1]] = m[2].replace(/^["']|["']$/g, "");
  }
  return env;
}
const ENV = Object.assign(lireEnv(path.join(__dirname, ".env")), process.env);
const PORT = parseInt(ENV.PORT || "8080", 10);
const CLE_ADMIN = ENV.CLE_ADMIN || "";
const SEL = ENV.SEL || "chelles3d";            // sert à brouiller les adresses IP
const ORIGINES = (ENV.ORIGINES || "*").split(",").map(s => s.trim()).filter(Boolean);

const LIMITES = {
  texteMax: 200,          // caractères par message
  pseudoMin: 3, pseudoMax: 20,
  rafale: 3,              // messages d'affilée autorisés
  recharge: 1000,         // 1 message de plus par seconde
  historique: 50,         // messages envoyés à l'arrivée
  enLigneMs: 5 * 60 * 1000
};

// Petit filtre de gros mots (les mots sont remplacés par des étoiles). À compléter dans gros_mots.txt
let GROS_MOTS = ["connard", "connasse", "salope", "pute", "enculé", "encule", "fdp", "ntm", "batard", "bâtard", "pd", "nique", "niquer", "enfoiré", "enfoire", "abruti", "débile"];
try {
  const extra = fs.readFileSync(path.join(__dirname, "gros_mots.txt"), "utf8").split(/\r?\n/).map(s => s.trim().toLowerCase()).filter(s => s && !s.startsWith("#"));
  GROS_MOTS = GROS_MOTS.concat(extra);
} catch (e) { /* fichier facultatif */ }
const RE_GROS_MOTS = new RegExp("(^|[^\\p{L}])(" + GROS_MOTS.map(m => m.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")).join("|") + ")(?=$|[^\\p{L}])", "giu");
function filtrer(texte) { return texte.replace(RE_GROS_MOTS, (t, avant, mot) => avant + "*".repeat(mot.length)); }

// ---------- Base de données ----------
const base = mariadb.createPool({
  host: ENV.DB_HOTE || "127.0.0.1",
  port: parseInt(ENV.DB_PORT || "3306", 10),
  user: ENV.DB_UTILISATEUR || "chelles3d",
  password: ENV.DB_MOT_DE_PASSE || "",
  database: ENV.DB_NOM || "chelles3d",
  connectionLimit: 5,
  bigIntAsNumber: true,
  dateStrings: true
});
async function sql(requete, valeurs) {
  let c;
  try { c = await base.getConnection(); return await c.query(requete, valeurs); }
  finally { if (c) c.release(); }
}

// ---------- Outils ----------
function ipDe(req) {
  const fwd = req.headers["x-forwarded-for"];
  return (fwd ? String(fwd).split(",")[0] : req.socket.remoteAddress || "").trim();
}
function empreinte(...morceaux) {
  return crypto.createHash("sha256").update(SEL + "|" + morceaux.join("|")).digest("hex").slice(0, 16);
}
function aujourdhui() {
  return new Date().toLocaleDateString("sv-SE", { timeZone: "Europe/Paris" }); // AAAA-MM-JJ
}
function nettoyer(texte, max) {
  return String(texte || "").replace(/[\u0000-\u001f\u007f]/g, " ").replace(/\s+/g, " ").trim().slice(0, max);
}
function journal(...m) { console.log(new Date().toISOString(), ...m); }

// ---------- Présence (site + chat) ----------
const vusRecemment = new Map();            // empreinte -> dernière visite (ms)
function marquerPresent(e) { vusRecemment.set(e, Date.now()); }
function nombreEnLigne() {
  const limite = Date.now() - LIMITES.enLigneMs;
  for (const [e, t] of vusRecemment) if (t < limite) vusRecemment.delete(e);
  const ids = new Set(vusRecemment.keys());
  for (const c of clients) ids.add(c.empreinte);
  return Math.max(1, ids.size);
}

// ---------- API HTTP ----------
function entetesCors(req, res) {
  const o = req.headers.origin;
  const autorise = ORIGINES.includes("*") ? "*" : (o && ORIGINES.includes(o) ? o : ORIGINES[0]);
  res.setHeader("Access-Control-Allow-Origin", autorise);
  res.setHeader("Vary", "Origin");
  res.setHeader("Access-Control-Allow-Methods", "GET, HEAD, POST, OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "Range, Content-Type");
  res.setHeader("Access-Control-Expose-Headers", "Content-Range, Content-Length, Accept-Ranges");
  res.setHeader("Cache-Control", "no-store");
}
function json(res, code, donnees) {
  res.writeHead(code, { "Content-Type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(donnees));
}

async function apiCompteur(req, res, url) {
  const ip = ipDe(req);
  const ua = req.headers["user-agent"] || "";
  const jour = aujourdhui();
  const eJour = empreinte(jour, ip, ua);
  marquerPresent(empreinte(ip, ua));
  if (url.searchParams.has("visite")) {
    await sql("UPDATE compteurs SET valeur = valeur + 1 WHERE nom = 'vues'");
    await sql("INSERT IGNORE INTO visites (jour, empreinte) VALUES (?, ?)", [jour, eJour]);
  }
  const [v] = await sql("SELECT valeur FROM compteurs WHERE nom = 'vues'");
  const [t] = await sql("SELECT COUNT(*) AS n FROM visites");
  const [a] = await sql("SELECT COUNT(*) AS n FROM visites WHERE jour = ?", [jour]);
  json(res, 200, { vues: v ? v.valeur : 0, visiteurs: t.n, aujourdhui: a.n, en_ligne: nombreEnLigne(), chat: clients.size });
}

async function apiHistorique(req, res) {
  json(res, 200, { messages: await historique() });
}

async function historique() {
  const lignes = await sql(
    "SELECT id, pseudo, texte, source, quand FROM messages WHERE efface = 0 ORDER BY id DESC LIMIT ?",
    [LIMITES.historique]);
  return lignes.reverse().map(l => ({ type: "message", id: l.id, pseudo: l.pseudo, texte: l.texte, source: l.source, quand: String(l.quand).replace(" ", "T") + "Z" }));
}


// ---------- Vidéos (dossier VIDEOS du .env, par défaut "/Videos site") ----------
// Toute vidéo .mp4 / .webm / .mov déposée dans ce dossier apparaît sur le site.
// Vignette facultative : une image du même nom (ex. demo.mp4 + demo.jpg).
const DOSSIER_VIDEOS = ENV.VIDEOS || "/Videos site";
const DOSSIER_SONS = ENV.SONS || "/Sons site";          // musique du site (01.mp3 à 20.mp3)
const DOSSIER_MONUMENTS = ENV.MONUMENTS || "/Monuments site";   // photos des monuments, nommées par leur nom
const DOSSIER_ATELIER = ENV.ATELIER || "/Atelier site";         // "L'Atelier Bâtiments" : créations Blender / UE4 (même principe)
const DOSSIER_FOUTOIRE = ENV.FOUTOIRE || "/Foutoir site";      // "Le foutoir 3D de 2019 à 2025" : photos en vrac
const TYPES = { ".mp4": "video/mp4", ".m4v": "video/mp4", ".webm": "video/webm", ".mov": "video/quicktime",
  ".mp3": "audio/mpeg", ".ogg": "audio/ogg", ".wav": "audio/wav", ".m4a": "audio/mp4",
  ".jpg": "image/jpeg", ".jpeg": "image/jpeg", ".png": "image/png", ".webp": "image/webp" };
const EXT_VIDEO = [".mp4", ".m4v", ".webm", ".mov"];
const EXT_IMAGE = [".jpg", ".jpeg", ".png", ".webp"];

function titreDe(nom) {
  const m = /^vid[eé]o[\s._-]*0*(\d+)\.[^.]+$/i.exec(nom);
  if (m) return "Vidéo " + m[1];
  return nom.replace(/\.[^.]+$/, "").replace(/^\d+[\s._-]+/, "").replace(/[_]+/g, " ").replace(/\s+/g, " ").trim() || nom;
}
function apiVideos(req, res) {
  let fichiers = [];
  try { fichiers = fs.readdirSync(DOSSIER_VIDEOS); } catch (e) { return json(res, 200, { videos: [] }); }
  const images = new Map();
  for (const f of fichiers) { const ext = path.extname(f).toLowerCase(); if (EXT_IMAGE.includes(ext)) images.set(f.slice(0, -ext.length).toLowerCase(), f); }
  const videos = fichiers
    .filter(f => EXT_VIDEO.includes(path.extname(f).toLowerCase()) && !f.startsWith("."))
    .sort((a, b) => a.localeCompare(b, "fr", { numeric: true }))
    .map(f => {
      const base = f.slice(0, -path.extname(f).length);
      let taille = 0, date = null;
      try { const st = fs.statSync(path.join(DOSSIER_VIDEOS, f)); taille = st.size; date = st.mtime.toISOString(); } catch (e) {}
      const img = images.get(base.toLowerCase());                       // image du même nom que la vidéo
      const defaut = images.get("vignette");                           // sinon "vignette.jpg" du dossier (affiche du lecteur)
      return { fichier: f, titre: titreDe(f), url: "/videos/" + encodeURIComponent(f),
               vignette: img ? "/videos/" + encodeURIComponent(img) : null,
               vignette_defaut: defaut ? "/videos/" + encodeURIComponent(defaut) : null, taille, date };
    });
  json(res, 200, { videos });
}
// Monuments : chaque image du dossier = un monument, son nom = le nom du fichier
// Un monument = une image ("Mairie de Chelles.jpg") OU un dossier ("Salle André Clément/")
// qui contient plusieurs photos (la 1re par ordre de nom sert de couverture).
// Dans un dossier, un fichier nom.txt peut donner le nom affiché (accents sûrs).
function nomLisible(n) { return n.replace(/^\d+[\s._-]+/, "").replace(/_+/g, " ").replace(/\s+/g, " ").trim(); }
function trierFr(a, b) { return a.localeCompare(b, "fr", { numeric: true, sensitivity: "base" }); }
function estImage(f) { return EXT_IMAGE.includes(path.extname(f).toLowerCase()) && !f.startsWith("."); }
function apiMonuments(req, res, dossier, prefixe) {
  dossier = dossier || DOSSIER_MONUMENTS; prefixe = prefixe || "/api/monuments/";
  let entrees = [];
  try { entrees = fs.readdirSync(dossier, { withFileTypes: true }); } catch (e) { return json(res, 200, { monuments: [] }); }
  const monuments = [];
  entrees.filter(e => !e.name.startsWith(".")).sort((a, b) => trierFr(a.name, b.name)).forEach(e => {
    if (e.isDirectory()) {
      let photos = [];
      try { photos = fs.readdirSync(path.join(dossier, e.name)).filter(estImage).sort(trierFr); } catch (er) { return; }
      if (!photos.length) return;
      let nom = nomLisible(e.name);
      let info = "";   // 2e ligne de nom.txt (facultative) : adresse ou petite description
      try { const l = fs.readFileSync(path.join(dossier, e.name, "nom.txt"), "utf8").replace(/^\uFEFF/, "").split(/\r?\n/).map(x => x.trim()); if (l[0]) nom = l[0].slice(0, 80); if (l[1]) info = l[1].slice(0, 120); } catch (er) {}
      const urls = photos.map(f => prefixe + encodeURIComponent(e.name) + "/" + encodeURIComponent(f));
      monuments.push({ nom, info, image: urls[0], photos: urls });
    } else if (estImage(e.name)) {
      const url = prefixe + encodeURIComponent(e.name);
      monuments.push({ nom: nomLisible(e.name.slice(0, -path.extname(e.name).length)), image: url, photos: [url] });
    }
  });
  json(res, 200, { monuments });
}
function envoyerImageMonument(req, res, reste, dossier) {
  dossier = dossier || DOSSIER_MONUMENTS;
  const morceaux = reste.split("/").map(m => { try { return decodeURIComponent(m); } catch (e) { return ""; } });
  if (morceaux.length > 2 || morceaux.some(m => !m || m.startsWith(".") || /[\\/]/.test(m))) return json(res, 404, { erreur: "introuvable" });
  if (morceaux.length === 1) return envoyerFichierVideo(req, res, encodeURIComponent(morceaux[0]), dossier);
  return envoyerFichierVideo(req, res, encodeURIComponent(morceaux[1]), path.join(dossier, morceaux[0]));
}

// ---------- Le foutoir 3D : toutes les photos du dossier (et de ses sous-dossiers), triées par nom ----------
// Vignette 600 px faite une seule fois par ImageMagick, rangée dans "<dossier>/.vignettes" ;
// si ImageMagick manque ou échoue, on envoie la photo d'origine.
function apiFoutoire(req, res) {
  const photos = [];
  let entrees = [];
  try { entrees = fs.readdirSync(DOSSIER_FOUTOIRE, { withFileTypes: true }); } catch (e) { return json(res, 200, { photos: [] }); }
  entrees.filter(e => !e.name.startsWith(".")).sort((a, b) => trierFr(a.name, b.name)).forEach(e => {
    if (e.isDirectory()) {
      let fs2 = [];
      try { fs2 = fs.readdirSync(path.join(DOSSIER_FOUTOIRE, e.name)).filter(estImage).sort(trierFr); } catch (er) { return; }
      fs2.forEach(f => {
        const rel = encodeURIComponent(e.name) + "/" + encodeURIComponent(f);
        photos.push({ nom: nomLisible(f.slice(0, -path.extname(f).length)), image: "/api/foutoire/" + rel, vignette: "/api/foutoire-vignette/" + rel });
      });
    } else if (estImage(e.name)) {
      const rel = encodeURIComponent(e.name);
      photos.push({ nom: nomLisible(e.name.slice(0, -path.extname(e.name).length)), image: "/api/foutoire/" + rel, vignette: "/api/foutoire-vignette/" + rel });
    }
  });
  json(res, 200, { photos });
}
const vignettesEnCours = new Map();   // une seule fabrication à la fois par photo
let fabricationsActives = 0; const fileVignettes = [];
function fabriquer(source, cible) {
  if (vignettesEnCours.has(cible)) return vignettesEnCours.get(cible);
  const p = new Promise(ok => {
    const lancer = () => {
      fabricationsActives++;
      try { fs.mkdirSync(path.dirname(cible), { recursive: true }); } catch (e) {}
      const tmp = cible + ".tmp.jpg";
      execFile("convert", [source + "[0]", "-auto-orient", "-thumbnail", "600x600>", "-strip", "-quality", "80", tmp], { timeout: 30000 }, err => {
        fabricationsActives--;
        if (!err) { try { fs.renameSync(tmp, cible); } catch (e) { err = e; } } else { try { fs.unlinkSync(tmp); } catch (e) {} }
        ok(!err);
        const suivant = fileVignettes.shift(); if (suivant) suivant();
      });
    };
    if (fabricationsActives < 2) lancer(); else fileVignettes.push(lancer);
  }).finally(() => vignettesEnCours.delete(cible));
  vignettesEnCours.set(cible, p);
  return p;
}
async function envoyerVignetteFoutoire(req, res, reste) {
  const morceaux = reste.split("/").map(m => { try { return decodeURIComponent(m); } catch (e) { return ""; } });
  if (morceaux.length > 2 || morceaux.some(m => !m || m.startsWith(".") || /[\\/]/.test(m)) || !estImage(morceaux[morceaux.length - 1])) return json(res, 404, { erreur: "introuvable" });
  const source = path.join(DOSSIER_FOUTOIRE, ...morceaux);
  const nomVignette = morceaux.join("__") + ".jpg";
  const dossierV = path.join(DOSSIER_FOUTOIRE, ".vignettes");
  const cible = path.join(dossierV, nomVignette);
  let st;
  try { st = fs.statSync(source); if (!st.isFile()) throw 0; } catch (e) { return json(res, 404, { erreur: "introuvable" }); }
  let pret = false;
  try { pret = fs.statSync(cible).mtimeMs >= st.mtimeMs; } catch (e) {}
  if (!pret) pret = await fabriquer(source, cible);
  if (pret) return envoyerFichierVideo(req, res, encodeURIComponent(nomVignette), dossierV);
  return envoyerImageMonument(req, res, reste, DOSSIER_FOUTOIRE);   // pas de vignette : photo d'origine
}

function envoyerFichierVideo(req, res, nom, dossier) {
  nom = path.basename(decodeURIComponent(nom));                     // pas de "../"
  const ext = path.extname(nom).toLowerCase();
  if (!TYPES[ext] || nom.startsWith(".")) return json(res, 404, { erreur: "introuvable" });
  const chemin = path.join(dossier || DOSSIER_VIDEOS, nom);
  let st;
  try { st = fs.statSync(chemin); if (!st.isFile()) throw 0; } catch (e) { return json(res, 404, { erreur: "introuvable" }); }
  res.setHeader("Content-Type", TYPES[ext]);
  res.setHeader("Accept-Ranges", "bytes");
  res.setHeader("Cache-Control", "public, max-age=3600");
  const plage = /^bytes=(\d*)-(\d*)$/.exec(req.headers.range || "");
  if (plage && (plage[1] || plage[2])) {
    let debut = plage[1] ? parseInt(plage[1], 10) : st.size - parseInt(plage[2], 10);
    let fin = plage[1] && plage[2] ? parseInt(plage[2], 10) : st.size - 1;
    debut = Math.max(0, debut); fin = Math.min(fin, st.size - 1);
    if (debut > fin) { res.writeHead(416, { "Content-Range": "bytes */" + st.size }); return res.end(); }
    res.writeHead(206, { "Content-Range": "bytes " + debut + "-" + fin + "/" + st.size, "Content-Length": fin - debut + 1 });
    if (req.method === "HEAD") return res.end();
    return fs.createReadStream(chemin, { start: debut, end: fin }).pipe(res);
  }
  res.writeHead(200, { "Content-Length": st.size });
  if (req.method === "HEAD") return res.end();
  fs.createReadStream(chemin).pipe(res);
}

// ---------- Classement du DERNIER REFUGE (vagues de zombies du jeu) ----------
// Le jeu envoie chaque partie terminée (commencée à la vague 1). Le classement garde le meilleur score de chaque pseudo.
const envoisScores = new Map();            // empreinte -> dernier envoi (ms)
const LIMITES_SCORE = { score: 50000000, vague: 1000, kills: 200000, joueurs: 16, intervalleMs: 15000 };

function lireCorps(req, max) {
  return new Promise((ok, ko) => {
    let taille = 0; const morceaux = [];
    req.on("data", m => { taille += m.length; if (taille > max) { ko(new Error("trop gros")); req.destroy(); } else morceaux.push(m); });
    req.on("end", () => ok(Buffer.concat(morceaux).toString("utf8")));
    req.on("error", ko);
  });
}
function entier(v, min, max) {
  const n = Math.floor(Number(v));
  return Number.isFinite(n) && n >= min && n <= max ? n : null;
}
async function meilleursScores(limite) {
  // Meilleure partie de chaque pseudo (à égalité de score : la vague la plus haute, puis la plus ancienne)
  const lignes = await sql(
    `SELECT pseudo, score, vague, kills, tetes, joueurs, quand FROM scores_refuge
     WHERE efface = 0 ORDER BY score DESC, vague DESC, quand ASC LIMIT 2000`);
  const vus = new Set(); const meilleurs = [];
  for (const l of lignes) {
    const cle = l.pseudo.toLowerCase();
    if (vus.has(cle)) continue;
    vus.add(cle);
    meilleurs.push(l);
  }
  return { meilleurs: meilleurs.slice(0, limite), tous: meilleurs };
}
async function apiClassementLire(req, res, url) {
  const limite = entier(url.searchParams.get("limite") || 10, 1, 100) || 10;
  const { meilleurs, tous } = await meilleursScores(limite);
  json(res, 200, { meilleurs: meilleurs.map((l, i) => Object.assign({ rang: i + 1 }, l)), total: tous.length });
}
async function apiClassementEnvoyer(req, res) {
  const e = empreinte(ipDe(req), "scores");
  const dernier = envoisScores.get(e) || 0;
  if (Date.now() - dernier < LIMITES_SCORE.intervalleMs) return json(res, 429, { erreur: "trop vite" });
  let m;
  try { m = JSON.parse(await lireCorps(req, 2048)); } catch (err) { return json(res, 400, { erreur: "format" }); }
  const score = entier(m.score, 1, LIMITES_SCORE.score);
  const vague = entier(m.vague, 1, LIMITES_SCORE.vague);
  const kills = entier(m.kills, 0, LIMITES_SCORE.kills);
  const tetes = entier(m.tetes, 0, LIMITES_SCORE.kills);
  const joueurs = entier(m.joueurs, 1, LIMITES_SCORE.joueurs);
  if (score === null || vague === null || kills === null || tetes === null || joueurs === null || tetes > kills)
    return json(res, 400, { erreur: "valeurs" });
  const pseudo = pseudoValide(m.pseudo) || ("Joueur-" + e.slice(0, 4));   // sans pseudo du chat : un nom par PC
  envoisScores.set(e, Date.now());
  await sql("INSERT INTO scores_refuge (pseudo, score, vague, kills, tetes, joueurs, empreinte) VALUES (?, ?, ?, ?, ?, ?, ?)",
    [pseudo, score, vague, kills, tetes, joueurs, e]);
  const { tous } = await meilleursScores(1);
  const rang = tous.findIndex(l => l.pseudo.toLowerCase() === pseudo.toLowerCase()) + 1;
  const record = rang > 0 && tous[rang - 1].score === score;
  journal("Score Refuge", pseudo, score, "vague", vague, "rang", rang);
  if (rang === 1 && record && !pseudo.startsWith("Joueur-"))
    diffuser(systeme("🧟 " + pseudo + " prend la 1re place du DERNIER REFUGE : " + score.toLocaleString("fr-FR") + " points, vague " + vague + " !"));
  json(res, 200, { ok: true, rang, record, total: tous.length });
}

const serveur = http.createServer(async (req, res) => {
  entetesCors(req, res);
  if (req.method === "OPTIONS") { res.writeHead(204); return res.end(); }
  const url = new URL(req.url, "http://localhost");
  try {
    if (req.method === "GET" && url.pathname === "/api/sante") return json(res, 200, { ok: true, chat: clients.size, depuis: DEMARRAGE });
    if (req.method === "GET" && url.pathname === "/api/compteur") return await apiCompteur(req, res, url);
    if (req.method === "GET" && url.pathname === "/api/chat/historique") return await apiHistorique(req, res);
    if (req.method === "GET" && url.pathname === "/api/classement") return await apiClassementLire(req, res, url);
    if (req.method === "POST" && url.pathname === "/api/classement") return await apiClassementEnvoyer(req, res);
    if (req.method === "GET" && url.pathname === "/api/videos") return apiVideos(req, res);
    if (req.method === "GET" && url.pathname === "/api/monuments") return apiMonuments(req, res);
    if ((req.method === "GET" || req.method === "HEAD") && url.pathname.startsWith("/api/monuments/")) return envoyerImageMonument(req, res, url.pathname.slice(15));
    if (req.method === "GET" && url.pathname === "/api/atelier") return apiMonuments(req, res, DOSSIER_ATELIER, "/api/atelier/");
    if ((req.method === "GET" || req.method === "HEAD") && url.pathname.startsWith("/api/atelier/")) return envoyerImageMonument(req, res, url.pathname.slice(13), DOSSIER_ATELIER);
    if (req.method === "GET" && url.pathname === "/api/foutoire") return apiFoutoire(req, res);
    if ((req.method === "GET" || req.method === "HEAD") && url.pathname.startsWith("/api/foutoire/")) return envoyerImageMonument(req, res, url.pathname.slice(14), DOSSIER_FOUTOIRE);
    if ((req.method === "GET" || req.method === "HEAD") && url.pathname.startsWith("/api/foutoire-vignette/")) return envoyerVignetteFoutoire(req, res, url.pathname.slice(23));
    if ((req.method === "GET" || req.method === "HEAD") && url.pathname.startsWith("/videos/")) return envoyerFichierVideo(req, res, url.pathname.slice(8), DOSSIER_VIDEOS);
    if ((req.method === "GET" || req.method === "HEAD") && url.pathname.startsWith("/sons/")) return envoyerFichierVideo(req, res, url.pathname.slice(6), DOSSIER_SONS);
    json(res, 404, { erreur: "introuvable" });
  } catch (e) {
    journal("ERREUR API", url.pathname, e.message);
    json(res, 500, { erreur: "serveur" });
  }
});
const DEMARRAGE = new Date().toISOString();

// ---------- Chat (WebSocket) ----------
const wss = new WebSocketServer({ server: serveur, path: "/chat", maxPayload: 4 * 1024 });
const clients = new Set();

function envoyer(c, msg) { if (c.ws.readyState === 1) c.ws.send(JSON.stringify(msg)); }
function diffuser(msg) { const s = JSON.stringify(msg); for (const c of clients) if (c.pseudo && c.ws.readyState === 1) c.ws.send(s); }
function listeJoueurs() { return [...clients].filter(c => c.pseudo).map(c => ({ pseudo: c.pseudo, source: c.source })); }
function diffuserPresence() { diffuser({ type: "presence", en_ligne: listeJoueurs().length, joueurs: listeJoueurs() }); }
function systeme(texte) { return { type: "systeme", texte, quand: new Date().toISOString() }; }

function pseudoValide(p) {
  p = nettoyer(p, LIMITES.pseudoMax).replace(/[^\p{L}\p{N} _\-.]/gu, "");
  if (p.length < LIMITES.pseudoMin) return null;
  if (/admin|modo|serveur|system/i.test(p)) return null;
  if (filtrer(p) !== p) return null;
  return p;
}
function pseudoLibre(p) {
  const pris = new Set([...clients].filter(c => c.pseudo).map(c => c.pseudo.toLowerCase()));
  if (!pris.has(p.toLowerCase())) return p;
  for (let i = 2; i < 100; i++) { const q = (p.slice(0, LIMITES.pseudoMax - 3) + i); if (!pris.has(q.toLowerCase())) return q; }
  return null;
}
function trouver(pseudo) {
  pseudo = String(pseudo || "").toLowerCase();
  return [...clients].find(c => c.pseudo && c.pseudo.toLowerCase() === pseudo);
}

async function commandeAdmin(c, texte) {
  const [cmd, cible, ...reste] = texte.slice(1).split(" ");
  const raison = reste.join(" ").slice(0, 120);
  const v = trouver(cible);
  switch ((cmd || "").toLowerCase()) {
    case "kick":
      if (!v) return envoyer(c, systeme("Joueur introuvable : " + cible));
      if (v.admin) return envoyer(c, systeme("Impossible d'expulser un administrateur."));
      envoyer(v, systeme("Vous avez été expulsé du chat." + (raison ? " Raison : " + raison : "")));
      v.ws.close(4001, "expulse");
      return diffuser(systeme(v.pseudo + " a été expulsé du chat."));
    case "ban":
      if (!v) return envoyer(c, systeme("Joueur introuvable : " + cible));
      if (v.admin) return envoyer(c, systeme("Impossible de bannir un administrateur."));
      await sql("REPLACE INTO bannis (empreinte, pseudo, raison) VALUES (?, ?, ?)", [v.empreinte, v.pseudo, raison]);
      envoyer(v, systeme("Vous êtes banni du chat." + (raison ? " Raison : " + raison : "")));
      v.ws.close(4003, "banni");
      return diffuser(systeme(v.pseudo + " a été banni du chat."));
    case "unban": {
      const r = await sql("DELETE FROM bannis WHERE LOWER(pseudo) = LOWER(?)", [cible || ""]);
      return envoyer(c, systeme(r.affectedRows ? cible + " n'est plus banni." : "Aucun banni nommé " + cible));
    }
    case "bannis": {
      const l = await sql("SELECT pseudo, raison, quand FROM bannis ORDER BY quand DESC LIMIT 30");
      return envoyer(c, systeme(l.length ? "Bannis : " + l.map(b => b.pseudo).join(", ") : "Aucun banni."));
    }
    case "effacer":
      await sql("UPDATE messages SET efface = 1");
      return diffuser({ type: "effacer" });
    case "score-effacer": {
      // Retire un tricheur du classement du DERNIER REFUGE (ses parties restent dans la base, marquées effacées)
      const r = await sql("UPDATE scores_refuge SET efface = 1 WHERE LOWER(pseudo) = LOWER(?)", [cible || ""]);
      return envoyer(c, systeme(r.affectedRows ? cible + " est retiré du classement (" + r.affectedRows + " partie(s))." : "Aucun score pour " + cible));
    }
    case "annonce":
      return diffuser(systeme("📢 " + filtrer(nettoyer([cible, ...reste].join(" "), LIMITES.texteMax))));
    default:
      return envoyer(c, systeme("Commandes : /kick pseudo, /ban pseudo raison, /unban pseudo, /bannis, /effacer, /annonce texte, /score-effacer pseudo"));
  }
}

wss.on("connection", (ws, req) => {
  const ip = ipDe(req);
  const c = { ws, pseudo: null, source: "site", admin: false, empreinte: empreinte(ip, "chat"), jetons: LIMITES.rafale, dernier: Date.now(), vivant: true };
  clients.add(c);
  ws.on("pong", () => { c.vivant = true; });

  ws.on("message", async (brut) => {
    let m;
    try { m = JSON.parse(String(brut)); } catch (e) { return; }
    try {
      // 1) Présentation : { type:"bonjour", pseudo:"Rachid", source:"jeu"|"site", admin:"cle" }
      if (m.type === "bonjour" && !c.pseudo) {
        const estAdmin = !!CLE_ADMIN && m.admin === CLE_ADMIN;
        const banni = estAdmin ? [] : await sql("SELECT raison FROM bannis WHERE empreinte = ?", [c.empreinte]);
        if (banni.length) { envoyer(c, { type: "refus", raison: "banni" }); return ws.close(4003, "banni"); }
        const p0 = pseudoValide(m.pseudo);
        if (!p0) return envoyer(c, { type: "refus", raison: "pseudo", texte: "Pseudo invalide (3 à 20 lettres ou chiffres)." });
        const p = pseudoLibre(p0);
        if (!p) return envoyer(c, { type: "refus", raison: "pseudo", texte: "Ce pseudo est déjà pris." });
        c.pseudo = p;
        c.source = m.source === "jeu" ? "jeu" : "site";
        c.admin = estAdmin;
        envoyer(c, { type: "bienvenue", pseudo: p, admin: c.admin, limites: { texteMax: LIMITES.texteMax } });
        envoyer(c, { type: "historique", messages: await historique() });
        diffuser(systeme(p + (c.source === "jeu" ? " a rejoint le chat depuis le jeu 🎮" : " a rejoint le chat 🌐")));
        diffuserPresence();
        journal("Arrivée", p, c.source, c.admin ? "(admin)" : "");
        return;
      }
      if (!c.pseudo) return;

      // 2) Message : { type:"message", texte:"Salut !" }
      if (m.type === "message") {
        const texte = nettoyer(m.texte, LIMITES.texteMax);
        if (!texte) return;
        if (c.admin && texte.startsWith("/")) return await commandeAdmin(c, texte);
        // anti-spam : 3 messages d'affilée puis 1 par seconde
        const maintenant = Date.now();
        c.jetons = Math.min(LIMITES.rafale, c.jetons + (maintenant - c.dernier) / LIMITES.recharge);
        c.dernier = maintenant;
        if (c.jetons < 1) return envoyer(c, systeme("Doucement ! Attendez une seconde entre deux messages."));
        c.jetons -= 1;
        const propre = filtrer(texte);
        const r = await sql("INSERT INTO messages (pseudo, texte, source, quand) VALUES (?, ?, ?, UTC_TIMESTAMP())", [c.pseudo, propre, c.source]);
        diffuser({ type: "message", id: Number(r.insertId), pseudo: c.pseudo, texte: propre, source: c.source, admin: c.admin, quand: new Date().toISOString() });
        return;
      }
      if (m.type === "ping") return envoyer(c, { type: "pong" });
    } catch (e) {
      journal("ERREUR chat", e.message);
      envoyer(c, systeme("Erreur du serveur, réessayez."));
    }
  });

  ws.on("close", () => {
    clients.delete(c);
    if (c.pseudo) { diffuser(systeme(c.pseudo + " a quitté le chat.")); diffuserPresence(); }
  });
  ws.on("error", () => {});
});

// connexions mortes (wifi coupé, jeu fermé brutalement) : nettoyées toutes les 30 s
setInterval(() => {
  for (const c of clients) {
    if (!c.vivant) { c.ws.terminate(); continue; }
    c.vivant = false;
    try { c.ws.ping(); } catch (e) {}
  }
}, 30000);

// nettoyage : on garde 5000 messages au maximum
setInterval(() => {
  sql("DELETE FROM messages WHERE id < (SELECT id FROM (SELECT id FROM messages ORDER BY id DESC LIMIT 1 OFFSET 5000) t)").catch(() => {});
}, 6 * 3600 * 1000);

serveur.listen(PORT, () => journal("CHELLES 3D : serveur prêt sur le port " + PORT));
process.on("SIGTERM", () => { journal("Arrêt"); serveur.close(); base.end().finally(() => process.exit(0)); });
