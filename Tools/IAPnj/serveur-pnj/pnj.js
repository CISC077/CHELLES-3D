/*
 * CHELLES 3D : les PNJ (passants) parlent grâce à une IA, depuis le VPS.
 *
 *   GET /api/pnj?categorie=Salut&genre=F&nb=10   -> { repliques: [...], source: "..." }
 *   GET /api/pnj/sante                            -> { ok: true, ... } (sans aucune clé)
 *
 * Les clés API restent UNIQUEMENT dans le fichier .env de ce dossier (chmod 600).
 * Elles ne sont jamais envoyées au jeu, jamais écrites dans le journal.
 *
 * Économie : l'IA fabrique 25 phrases d'un coup par catégorie, gardées en cache
 * (cache.json). Le jeu reçoit des phrases tirées du cache : très peu d'appels à l'IA.
 * Si une clé ne marche plus, on passe automatiquement à la suivante (.env, ordre de priorité).
 * Si plus rien ne marche, le jeu reçoit une liste vide et garde ses répliques fixes.
 *
 * Tester les clés :  sudo -u chelles node /opt/chelles3d/pnj/pnj.js --tester
 * (affiche seulement le nom du fournisseur et OK/KO, jamais la clé)
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
const PORT = parseInt(ENV.PORT_PNJ || "8081", 10);
const FICHIER_CACHE = path.join(__dirname, "cache.json");

const REGLAGES = {
  phrasesParAppel: 25,                 // phrases fabriquées par appel à l'IA
  renouvellementMs: 45 * 60 * 1000,    // une catégorie est rafraîchie au plus toutes les 45 min
  appelsIaParHeure: parseInt(ENV.APPELS_IA_PAR_HEURE || "40", 10),   // plafond (coût)
  appelsIaParJour: parseInt(ENV.APPELS_IA_PAR_JOUR || "400", 10),
  requetesParMinuteParIp: 30,          // limite de débit côté jeu
  nbMax: 20,
  delaiIaMs: 15000
};

const CATEGORIES = {
  Salut: "le joueur passe près de toi dans la rue ; tu lui dis bonjour ou tu fais une petite remarque de la vie de tous les jours",
  Bousculade: "le joueur vient de te bousculer en courant ; tu râles",
  Vehicule: "le joueur fonce tout près de toi en voiture, en moto ou en motoneige ; tu as peur ou tu cries",
  Remarque: "le joueur reste planté devant toi sans rien dire depuis plusieurs secondes ; tu te demandes ce qu'il veut"
};

// ---------- Journal (jamais de clé dedans) ----------
const SECRETS = Object.keys(ENV).filter(k => /^CLE_/.test(k)).map(k => ENV[k]).filter(v => v && v.length > 8);
function masquer(texte) {
  let t = String(texte);
  for (const s of SECRETS) t = t.split(s).join("***");
  return t.replace(/(sk-|gsk_|xai-|AIza|AQ\.|tgp_|key_)[A-Za-z0-9_\-.]{8,}/g, "***");
}
function journal(...m) { console.log(new Date().toISOString(), masquer(m.join(" "))); }

// ---------- Fournisseurs d'IA, par ordre de priorité ----------
// Format OpenAI (chat/completions) pour tous sauf Gemini.
const FOURNISSEURS = [];
function ajouter(nom, cle, type, url, modeles) {
  if (cle) FOURNISSEURS.push({ nom, cle, type, url, modeles, horsServiceJusqua: 0, echecs: 0 });
}
for (let i = 1; i <= 5; i++) {
  ajouter("gemini" + i, ENV["CLE_GEMINI_" + i], "gemini", "https://generativelanguage.googleapis.com/v1beta/models/",
    (ENV.MODELES_GEMINI || "gemini-3.5-flash-lite,gemini-flash-lite-latest").split(","));
}
// OpenRouter : la clé la plus récente en premier (CLE_OPENROUTER_5 ... _2), puis CLE_OPENROUTER
for (const nomCle of ["CLE_OPENROUTER_5", "CLE_OPENROUTER_4", "CLE_OPENROUTER_3", "CLE_OPENROUTER_2", "CLE_OPENROUTER"]) {
  ajouter(nomCle === "CLE_OPENROUTER" ? "openrouter" : "openrouter" + nomCle.slice(-1), ENV[nomCle], "openai", "https://openrouter.ai/api/v1/chat/completions",
    (ENV.MODELES_OPENROUTER || "google/gemini-flash-lite-latest,meta-llama/llama-3.1-8b-instruct").split(","));
}
ajouter("deepseek", ENV.CLE_DEEPSEEK, "openai", "https://api.deepseek.com/chat/completions", ["deepseek-chat"]);
ajouter("groq", ENV.CLE_GROQ, "openai", "https://api.groq.com/openai/v1/chat/completions",
  (ENV.MODELES_GROQ || "llama-3.1-8b-instant").split(","));
for (let i = 1; i <= 5; i++) {
  ajouter("openai" + i, ENV["CLE_OPENAI_" + i], "openai", "https://api.openai.com/v1/chat/completions",
    (ENV.MODELES_OPENAI || "gpt-4o-mini").split(","));
}
ajouter("xai", ENV.CLE_XAI, "openai", "https://api.x.ai/v1/chat/completions", (ENV.MODELES_XAI || "grok-3-mini").split(","));
ajouter("together", ENV.CLE_TOGETHER, "openai", "https://api.together.xyz/v1/chat/completions",
  (ENV.MODELES_TOGETHER || "meta-llama/Llama-3.2-3B-Instruct-Turbo").split(","));

function consigne(categorie, genre, moment) {
  const qui = genre === "F" ? "une habitante" : "un habitant";
  return [
    `Tu écris les répliques de passants pour un jeu vidéo qui se passe à Chelles (Seine-et-Marne, 77), en banlieue est de Paris.`,
    `Le passant est ${qui} du quartier. Situation : ${CATEGORIES[categorie]}. Moment : ${moment}.`,
    `Écris ${REGLAGES.phrasesParAppel} répliques différentes, en français parlé, familier mais sans insulte ni gros mot, tutoiement.`,
    `Chaque réplique fait au maximum 12 mots. Variété : humour, météo, RER E, bus, marché de Chelles, boulangerie, travaux, l'abbaye, la gare, la Marne, le quotidien.`,
    `Pas de prénoms, pas de politique, pas de religion, rien de choquant.`,
    `Réponds UNIQUEMENT par un tableau JSON de chaînes, par exemple ["Phrase 1", "Phrase 2"].`
  ].join("\n");
}

function momentDeLaJournee() {
  const maintenant = new Date();
  const h = parseInt(maintenant.toLocaleString("fr-FR", { timeZone: "Europe/Paris", hour: "2-digit", hour12: false }), 10);
  const mois = parseInt(maintenant.toLocaleString("fr-FR", { timeZone: "Europe/Paris", month: "numeric" }), 10);
  const partie = h < 6 ? "la nuit" : h < 12 ? "le matin" : h < 18 ? "l'après-midi" : h < 22 ? "le soir" : "la nuit";
  const saison = (mois === 12) ? ", période de Noël, il fait froid" : (mois <= 2 || mois === 11) ? ", c'est l'hiver" : (mois >= 6 && mois <= 8) ? ", c'est l'été" : "";
  return { cle: partie, texte: partie + saison };
}

async function appeler(f, modele, texte) {
  const ctrl = new AbortController();
  const minuterie = setTimeout(() => ctrl.abort(), REGLAGES.delaiIaMs);
  try {
    let rep, donnees;
    if (f.type === "gemini") {
      rep = await fetch(f.url + encodeURIComponent(modele) + ":generateContent", {
        method: "POST", signal: ctrl.signal,
        headers: { "Content-Type": "application/json", "x-goog-api-key": f.cle },
        body: JSON.stringify({
          contents: [{ role: "user", parts: [{ text: texte }] }],
          generationConfig: { temperature: 1.0, maxOutputTokens: 1200, responseMimeType: "application/json" }
        })
      });
      donnees = await rep.json().catch(() => ({}));
      if (!rep.ok) return { code: rep.status, erreur: donnees && donnees.error && donnees.error.message };
      const parts = (((donnees.candidates || [])[0] || {}).content || {}).parts || [];
      return { code: 200, texte: parts.map(p => p.text || "").join("") };
    }
    rep = await fetch(f.url, {
      method: "POST", signal: ctrl.signal,
      headers: { "Content-Type": "application/json", "Authorization": "Bearer " + f.cle },
      body: JSON.stringify({ model: modele, temperature: 1.0, max_tokens: 1200, messages: [{ role: "user", content: texte }] })
    });
    donnees = await rep.json().catch(() => ({}));
    if (!rep.ok) return { code: rep.status, erreur: donnees && donnees.error && (donnees.error.message || donnees.error) };
    return { code: 200, texte: (((donnees.choices || [])[0] || {}).message || {}).content || "" };
  } catch (e) {
    return { code: 0, erreur: e.name === "AbortError" ? "trop long" : e.message };
  } finally {
    clearTimeout(minuterie);
  }
}

function extrairePhrases(texte) {
  let liste = [];
  const m = /\[[\s\S]*\]/.exec(texte || "");
  try { liste = JSON.parse(m ? m[0] : texte); } catch (e) {
    liste = String(texte || "").split(/\r?\n/).map(s => s.replace(/^\s*[-*\d.)"]+\s*/, ""));
  }
  if (!Array.isArray(liste)) return [];
  const vues = new Set();
  return liste.map(s => String(s).replace(/[\u0000-\u001f\u007f"<>{}\[\]|\\]/g, " ").replace(/\s+/g, " ").trim())
    .filter(s => s.length >= 2 && s.length <= 90 && s.split(" ").length <= 16)
    .filter(s => { const k = s.toLowerCase(); if (vues.has(k)) return false; vues.add(k); return true; });
}

// Essaie chaque fournisseur dans l'ordre ; passe au suivant si la clé ne marche plus.
async function demanderALIA(texte) {
  const maintenant = Date.now();
  for (const f of FOURNISSEURS) {
    if (f.horsServiceJusqua > maintenant) continue;
    for (const modele of f.modeles) {
      const r = await appeler(f, modele.trim(), texte);
      if (r.code === 200) {
        const phrases = extrairePhrases(r.texte);
        if (phrases.length >= 3) { f.echecs = 0; return { source: f.nom + "/" + modele.trim(), phrases }; }
        journal("PNJ :", f.nom, modele, "réponse inutilisable");
        continue;
      }
      journal("PNJ :", f.nom, modele, "échec", r.code, String(r.erreur || "").slice(0, 120));
      const cleRefusee = /api.?key|apikey|auth|credit|billing|quota/i.test(String(r.erreur || ""));
      if ((r.code === 404 || r.code === 400) && !cleRefusee) continue;   // modèle inconnu : essayer le modèle suivant
      f.echecs++;
      // clé refusée ou crédit épuisé : on la met de côté 6 h ; trop de demandes : 10 min ; panne : 1 min
      const pause = (cleRefusee || r.code === 401 || r.code === 402 || r.code === 403) ? 6 * 3600e3 : r.code === 429 ? 10 * 60e3 : 60e3;
      f.horsServiceJusqua = Date.now() + pause;
      break;
    }
  }
  return null;
}

// ---------- Cache ----------
let cache = {};      // "Salut|F|le matin" -> { phrases: [...], date: ms, source }
try { cache = JSON.parse(fs.readFileSync(FICHIER_CACHE, "utf8")) || {}; } catch (e) { cache = {}; }
function sauverCache() {
  try { fs.writeFileSync(FICHIER_CACHE + ".tmp", JSON.stringify(cache)); fs.renameSync(FICHIER_CACHE + ".tmp", FICHIER_CACHE); }
  catch (e) { journal("PNJ : cache non enregistré", e.message); }
}

const compteur = { heure: 0, nbHeure: 0, jour: "", nbJour: 0 };
function budgetDisponible() {
  const h = Math.floor(Date.now() / 3600e3), j = new Date().toISOString().slice(0, 10);
  if (compteur.heure !== h) { compteur.heure = h; compteur.nbHeure = 0; }
  if (compteur.jour !== j) { compteur.jour = j; compteur.nbJour = 0; }
  return compteur.nbHeure < REGLAGES.appelsIaParHeure && compteur.nbJour < REGLAGES.appelsIaParJour;
}

const enCours = new Map();
async function rafraichir(cle, categorie, genre, moment) {
  if (enCours.has(cle)) return enCours.get(cle);
  if (!budgetDisponible() || FOURNISSEURS.length === 0) return null;
  compteur.nbHeure++; compteur.nbJour++;
  const p = (async () => {
    const r = await demanderALIA(consigne(categorie, genre, moment));
    if (r) {
      cache[cle] = { phrases: r.phrases, date: Date.now(), source: r.source };
      sauverCache();
      journal("PNJ :", r.phrases.length, "phrases", cle, "par", r.source);
    }
    return r;
  })().finally(() => enCours.delete(cle));
  enCours.set(cle, p);
  return p;
}

function tirer(liste, nb) {
  const copie = liste.slice();
  for (let i = copie.length - 1; i > 0; i--) { const j = Math.floor(Math.random() * (i + 1)); [copie[i], copie[j]] = [copie[j], copie[i]]; }
  return copie.slice(0, nb);
}

// ---------- Limite de débit ----------
const visites = new Map();   // ip -> { debut, nb }
function tropDeRequetes(ip) {
  const t = Date.now();
  let v = visites.get(ip);
  if (!v || t - v.debut > 60e3) { v = { debut: t, nb: 0 }; visites.set(ip, v); }
  v.nb++;
  if (visites.size > 5000) for (const [k, x] of visites) if (t - x.debut > 60e3) visites.delete(k);
  return v.nb > REGLAGES.requetesParMinuteParIp;
}

function json(res, code, donnees) {
  res.writeHead(code, { "Content-Type": "application/json; charset=utf-8", "Access-Control-Allow-Origin": "*", "Cache-Control": "no-store" });
  res.end(JSON.stringify(donnees));
}

async function apiPnj(req, res, url) {
  const ip = String(req.headers["x-forwarded-for"] || req.socket.remoteAddress || "").split(",")[0].trim();
  if (tropDeRequetes(ip)) return json(res, 429, { repliques: [], source: "limite" });
  const categorie = Object.keys(CATEGORIES).find(c => c.toLowerCase() === String(url.searchParams.get("categorie") || "Salut").toLowerCase());
  if (!categorie) return json(res, 400, { repliques: [], source: "categorie inconnue" });
  const genre = String(url.searchParams.get("genre") || "M").toUpperCase() === "F" ? "F" : "M";
  const nb = Math.max(1, Math.min(REGLAGES.nbMax, parseInt(url.searchParams.get("nb") || "8", 10) || 8));
  const moment = momentDeLaJournee();
  const cle = categorie + "|" + genre + "|" + moment.cle;
  let entree = cache[cle];

  if (!entree || entree.phrases.length === 0) {
    await rafraichir(cle, categorie, genre, moment.texte);          // 1re fois : on attend l'IA (quelques secondes)
    entree = cache[cle];
  } else if (Date.now() - entree.date > REGLAGES.renouvellementMs) {
    rafraichir(cle, categorie, genre, moment.texte).catch(() => {});   // en arrière-plan, on sert l'ancien cache
  }
  if (!entree) {
    // aucune IA disponible : on dépanne avec une autre période de la journée, sinon liste vide (répliques fixes du jeu)
    const autre = Object.keys(cache).find(k => k.startsWith(categorie + "|" + genre + "|"));
    if (autre) return json(res, 200, { repliques: tirer(cache[autre].phrases, nb), source: "cache" });
    return json(res, 200, { repliques: [], source: "secours" });
  }
  return json(res, 200, { repliques: tirer(entree.phrases, nb), source: "ia" });
}

// ---------- Mode test des clés ----------
async function tester() {
  console.log(`${FOURNISSEURS.length} fournisseur(s) dans .env, essai de chacun :`);
  for (const f of FOURNISSEURS) {
    let ok = false, info = "";
    for (const modele of f.modeles) {
      const r = await appeler(f, modele.trim(), 'Réponds uniquement par ce JSON : ["Salut !", "Ça va ?", "Bonne journée !"]');
      if (r.code === 200 && extrairePhrases(r.texte).length) { ok = true; info = modele.trim(); break; }
      info = `${modele.trim()} : erreur ${r.code} ${String(r.erreur || "").slice(0, 80)}`;
    }
    console.log(ok ? `  OK  ${f.nom}  (${info})` : `  KO  ${f.nom}  ${masquer(info)}`);
  }
}

if (process.argv.includes("--tester")) {
  tester().then(() => process.exit(0));
} else {
  const serveur = http.createServer(async (req, res) => {
    try {
      const url = new URL(req.url, "http://localhost");
      if (req.method === "GET" && url.pathname === "/api/pnj/sante")
        return json(res, 200, { ok: true, fournisseurs: FOURNISSEURS.map(f => ({ nom: f.nom, actif: f.horsServiceJusqua <= Date.now() })), categoriesEnCache: Object.keys(cache).length, appelsIaAujourdhui: compteur.nbJour });
      if (req.method === "GET" && url.pathname === "/api/pnj") return await apiPnj(req, res, url);
      json(res, 404, { erreur: "inconnu" });
    } catch (e) {
      journal("PNJ : erreur", e.message);
      json(res, 500, { repliques: [], source: "secours" });
    }
  });
  serveur.listen(PORT, () => journal(`CHELLES 3D : IA des PNJ prête sur le port ${PORT} (${FOURNISSEURS.length} fournisseur(s))`));
}
