// CHELLES 3D - Dernier Refuge : classement des joueurs
//   - celui de ce PC : Saved/ChellesSurvie/Classement.txt (20 meilleures parties)
//   - celui de tous les joueurs : serveur CHELLES 3D du VPS (/api/classement), si ClassementEnLigne=Oui dans Refuge.txt
// Chaque PC enregistre et envoie SA partie (avec le pseudo du chat global), a la fin d'une partie classee.
#pragma once

#include "CoreMinimal.h"
#include "SurvieCalculs.h"

class UWorld;

enum class EEtatClassementEnLigne : uint8
{
	Jamais,      // pas encore demande
	Chargement,
	Pret,
	HorsLigne,   // serveur injoignable, ou ClassementEnLigne=Non
	Coupe        // ClassementEnLigne=Non
};

class FClassementSurvie
{
public:
	static FClassementSurvie& Get();

	/** Relit ClassementEnLigne et AdresseClassement dans Refuge.txt. */
	void ChargerReglages();

	/** Pseudo du joueur de ce PC : celui du chat global (touche B), sinon NomDeSecours. */
	static FString PseudoLocal(const FString& NomDeSecours);

	// ---- Ce PC ----
	const TArray<FLigneClassement>& Local();
	/** Enregistre la partie. AncienRecord = meilleur score de ce PC avant elle. Renvoie sa place (0 = hors du top 20). */
	int32 AjouterLocal(const FLigneClassement& Ligne, int32& AncienRecord);

	// ---- Tous les joueurs (VPS) ----
	/** Redemande le classement au serveur (au plus une fois toutes les 20 s, sauf bForcer). */
	void Rafraichir(bool bForcer = false);
	/** Envoie la partie au serveur, puis recharge le classement. */
	void Envoyer(const FLigneClassement& Ligne);
	const TArray<FLigneClassement>& EnLigne() const { return ListeEnLigne; }
	EEtatClassementEnLigne Etat() const { return EtatEnLigne; }
	/** Place de la derniere partie envoyee dans le classement de tous les joueurs (0 = inconnue). */
	int32 DernierRang() const { return RangEnvoye; }
	void OublierRang() { RangEnvoye = 0; }
	/** Nombre de joueurs classes sur le serveur. */
	int32 NombreClasses() const { return TotalEnLigne; }

	/** Le classement a montrer : celui du serveur s'il est la, sinon celui de ce PC. Titre rempli en consequence. */
	const TArray<FLigneClassement>& AMontrer(FString& Titre);

private:
	void ChargerLocal();
	void SauverLocal() const;
	FString Url(const TCHAR* Chemin) const;

	bool bReglagesLus = false;
	bool bEnLigneActif = true;
	FString Adresse = TEXT("http://51.178.54.208:8080");

	bool bLocalLu = false;
	TArray<FLigneClassement> ListeLocale;

	TArray<FLigneClassement> ListeEnLigne;
	EEtatClassementEnLigne EtatEnLigne = EEtatClassementEnLigne::Jamais;
	double DerniereDemande = -1000.0;
	int32 RangEnvoye = 0;
	int32 TotalEnLigne = 0;
};
