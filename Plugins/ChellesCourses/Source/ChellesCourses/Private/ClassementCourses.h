// CHELLES 3D - Courses de rue : classement de tous les joueurs sur le serveur CHELLES 3D du VPS
//   GET  /api/courses?course=<Id>&limite=10   -> { meilleurs: [{rang, pseudo, temps_ms, mode, quand}], total }
//   POST /api/courses { pseudo, course, nom, temps_ms, portes, longueur_m, mode } -> { ok, rang, record, total }
// Meme serveur que le classement du DERNIER REFUGE (ClassementEnLigne / AdresseClassement dans Courses.txt).
#pragma once

#include "CoreMinimal.h"

struct FLigneCourseEnLigne
{
	int32 Rang = 0;
	FString Pseudo;
	int32 TempsMs = 0;
	FString Mode;
	FString Date;
};

enum class EEtatClassementCourses : uint8
{
	Jamais,
	Chargement,
	Pret,
	HorsLigne,
	Coupe
};

struct FClassementUneCourse
{
	TArray<FLigneCourseEnLigne> Meilleurs;
	int32 Total = 0;
	EEtatClassementCourses Etat = EEtatClassementCourses::Jamais;
	double DerniereDemande = -1000.0;
	/** Place du dernier temps envoye depuis ce PC (0 = inconnue). */
	int32 RangEnvoye = 0;
	bool bReponseEnvoi = false;
};

class FClassementCourses
{
public:
	static FClassementCourses& Get();

	void Configurer(bool bEnLigne, const FString& Adresse);

	/** Pseudo du joueur de ce PC : celui du chat global (touche B), sinon "Joueur". */
	static FString PseudoLocal();

	/** Redemande le classement d'une course (au plus une fois toutes les 20 s, sauf bForcer). */
	void Rafraichir(const FString& Id, bool bForcer = false);
	/** Envoie un temps, puis recharge le classement de la course. */
	void Envoyer(const FString& Id, const FString& Nom, int32 TempsMs, int32 Portes, float LongueurM, const FString& Mode);

	const FClassementUneCourse* Course(const FString& Id) const { return Courses.Find(Id); }
	bool EnLigne() const { return bEnLigneActif; }

	/** "1:23.45" */
	static FString Temps(int32 Ms);

private:
	FString Url(const FString& Chemin) const { return Adresse + Chemin; }
	static FString Encoder(const FString& Texte);

	bool bEnLigneActif = true;
	FString Adresse = TEXT("http://51.178.54.208:8080");
	TMap<FString, FClassementUneCourse> Courses;
};
