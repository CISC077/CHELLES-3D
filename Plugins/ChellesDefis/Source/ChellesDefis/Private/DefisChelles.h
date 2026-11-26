// CHELLES 3D - Defi de la semaine : lecture sur le serveur du VPS, bandeau au debut de la partie, XP de reussite.
//   - le serveur (VPS, service chelles3d-defis) choisit le defi chaque lundi et l'annonce sur le Discord
//   - le jeu le demande : GET <Adresse>/api/defis/semaine?pseudo=<pseudo du chat global>
//   - les defis du DERNIER REFUGE sont verifies par le serveur avec les parties envoyees au classement
//   - defi reussi : +XP dans XPLocale de ChellesPouvoirs (une seule fois par semaine et par PC)
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Interfaces/IHttpRequest.h"

class SDefisBandeau;
class UGameViewportClient;
class UWorld;

/** Le defi de la semaine tel que le serveur le donne. */
struct FDefiSemaine
{
	bool bValide = false;
	FString Semaine;          // "2026-S48"
	int32 Numero = 0;
	FString Id;
	FString Titre;
	FString Texte;
	FString Type;
	FString ObjectifTexte;    // "vague 10"
	int32 XP = 0;
	bool bVerifiable = false;
	double FinSecondes = 0.0; // FPlatformTime::Seconds() a la fin du defi
	FString FinTexte;         // "dimanche 29 novembre a 23 h 59"
	int32 Reussis = 0;
	FString Premier;          // "Rachid : vague 12"
	bool bMoi = false;
	FString MoiTexte;         // meilleur resultat du joueur de ce PC
	int32 MoiRang = 0;
	bool bMoiReussi = false;
};

class FDefisChelles
{
public:
	static FDefisChelles& Get();

	void Demarrer();
	void Arreter();

	/** Une nouvelle partie commence (monde de jeu pret). */
	void NouveauMonde(UWorld* Monde);

	/** Chelles.Defi : redemande le defi et remontre le bandeau. */
	void CommandeMontrer();
	/** Chelles.DefiEtat : etat dans le journal (Output Log). */
	void DecrireEtat() const;

private:
	struct FReglages
	{
		bool bActif = true;
		FString Adresse = TEXT("http://51.178.54.208:8082");
		float Duree = 14.f;
		float DelaiAvant = 4.f;
		float RafraichirMinutes = 5.f;
	};

	void LireReglages();
	bool Tick(float DeltaSeconds);
	void Demander();
	void Reponse(FHttpRequestPtr Requete, FHttpResponsePtr Reponse, bool bReussi);
	void Montrer(bool bReussite);
	void RetirerBandeau();
	bool JoueurPret(UWorld* Monde) const;
	void DonnerXP(UWorld* Monde);

	static FString PseudoLocal();

	FReglages R;
	bool bReglagesLus = false;
	FDelegateHandle TickHandle;

	TWeakObjectPtr<UWorld> MondeActuel;
	TWeakObjectPtr<UGameViewportClient> VueActuelle;
	TSharedPtr<SDefisBandeau> Bandeau;

	FDefiSemaine Defi;
	bool bDemandeEnCours = false;
	double DerniereDemande = -100000.0;
	double PretDepuis = -1.0;
	bool bMontreCettePartie = false;
	bool bMontrerDesQuePret = false;
	bool bReussiteAMontrer = false;
	bool bErreurDite = false;
};
