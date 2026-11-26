// CHELLES 3D - branche le classement des COURSES DE RUE (courses.js) dans serveur.js.
// Usage : node brancher_courses.js /chemin/serveur.js   (ne fait rien si c'est deja branche)
"use strict";
const fs = require("fs");
const fichier = process.argv[2];
if (!fichier) { console.error("Usage : node brancher_courses.js serveur.js"); process.exit(2); }
let s = fs.readFileSync(fichier, "utf8");
if (s.includes("// COURSES DE RUE")) { console.log("serveur.js : courses deja branchees."); process.exit(0); }

const ancre1 = "const DEMARRAGE = new Date().toISOString();";
const ancre2 = 'if (req.method === "GET" && url.pathname === "/api/classement") return await apiClassementLire(req, res, url);';
if (!s.includes(ancre1) || !s.includes(ancre2)) { console.error("serveur.js : reperes introuvables, rien n'est change."); process.exit(1); }

const fin = s.includes("\r\n") ? "\r\n" : "\n";
const bloc1 = [
  ancre1,
  "// COURSES DE RUE (plugin ChellesCourses du jeu) : GET/POST /api/courses, voir courses.js",
  "let routeCourses = () => null;",
  "try {",
  '  routeCourses = require("./courses.js")({ sql, json, ipDe, empreinte, lireCorps, entier, pseudoValide, diffuser, systeme, journal });',
  "} catch (e) { journal(\"Courses de rue non branchees :\", e.message); }"
].join(fin);
s = s.replace(ancre1, bloc1);
s = s.replace(ancre2, "{ const r = routeCourses(req, res, url); if (r) return await r; } // COURSES DE RUE" + fin + "    " + ancre2);
if (s.split(ancre2).length !== 2) { console.error("serveur.js : repere en double, rien n'est change."); process.exit(1); }
fs.writeFileSync(fichier, s);
console.log("serveur.js : courses branchees.");
