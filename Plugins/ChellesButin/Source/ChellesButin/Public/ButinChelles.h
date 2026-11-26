// CHELLES 3D - Butin (2026-11-20, demande MERIDJA "idees Fortnite / Apex / Call of Duty"), UE 4.27.
//
// Un gestionnaire par machine (ajoute tout seul au Play, rien a poser dans la carte) :
//  - COFFRES DE BUTIN (Fortnite / Apex) : des coffres lumineux (bleu commun, violet rare, or legendaire) apparaissent
//    autour des joueurs, avec une colonne de lumiere visible de loin. E pres d'un coffre = XP, soins, munitions des
//    armes lourdes (plugin ChellesArmes) ;
//  - PING (Apex) : touche J = marque l'endroit vise pour tous les joueurs (ICI, ENNEMI !, BUTIN, VEHICULE) ;
//  - SERIES D'ELIMINATIONS (Call of Duty) : 3 a la suite sans mourir = RADAR (ennemis marques), 5 = FRAPPE AERIENNE
//    (touche 4 a l'endroit vise), 7 = bonus d'XP.
// Ne modifie aucun autre plugin ni Blueprint : XP, munitions et vie sont lues / donnees par leur nom.
// Reglages : Plugins\ChellesButin\Butin.txt (Bloc-notes, pas besoin de recompiler).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ButinTypes.h"
#include "ButinChelles.generated.h"

class ACoffreButin;
class APawn;
class APlayerController;
class ARelaisButin;
class SWidget;
class UDamageType;
class AController;

/** Ennemi touche par un joueur (serveur, pour compter les eliminations). */
struct FCibleButin
{
	TWeakObjectPtr<AController> Tireur;
	float Heure = 0.f;
};

/** Ennemi marque par le RADAR, en pixels de l'ecran (machine du joueur). */
struct FPointRadar
{
	FVector2D Ecran = FVector2D::ZeroVector;
	float Distance = 0.f;
};

/** Bombe de la frappe aerienne en attente (serveur). */
struct FBombeButin
{
	FVector Point = FVector::ZeroVector;
	float Heure = 0.f;
	TWeakObjectPtr<APlayerController> PC;
};

UCLASS(NotPlaceable, Transient)
class CHELLESBUTIN_API AButinChelles : public AActor
{
	GENERATED_BODY()

public:
	AButinChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console (touche ²) : Chelles.Butin [Info|Relire|Coffre|Serie N] */
	void Commande(const TArray<FString>& Arguments);

	// ---- Serveur (appele par le relais du joueur)
	void OuvrirServeur(ARelaisButin* R, ACoffreButin* Coffre);
	void PingServeur(ARelaisButin* R, FVector Point, uint8 Genre);
	void FrappeServeur(ARelaisButin* R, FVector Point);

	// ---- Machine du joueur
	void Annonce(const FString& Texte, uint8 Couleur, bool bSon);
	void DonnerXPLocale(int32 Quantite, const FString& Raison);

	/** Appele par les ennemis touches (OnTakeAnyDamage). */
	/** Ennemis marques par le RADAR (lu par l'ecran). */
	TArray<FPointRadar> PointsRadar;

	UFUNCTION()
	void EnnemiTouche(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur);

private:
	// ---- Reseau
	APlayerController* PCLocal() const;
	ARelaisButin* MonRelais() const;
	ARelaisButin* RelaisDe(APlayerController* PC) const;
	void GererRelais();

	// ---- Serveur : coffres
	void GererCoffres(float Dt);
	ACoffreButin* PoserCoffre(const FVector& Autour);
	void Recompenser(ARelaisButin* R, APlayerController* PC, uint8 Rarete);
	bool Soigner(APawn* Pion) const;
	int32 AjouterMunitions(APlayerController* PC, const TCHAR* Propriete, int32 Quantite) const;

	// ---- Serveur : series
	void SuivreEnnemis();
	void VerifierMorts(float Maintenant);
	void Elimination(AController* Tireur);
	void GererSeries(float Maintenant);
	void GererBombes(float Maintenant);
	static bool EstEnnemi(const AActor* Acteur);
	static bool EstMort(const AActor* Acteur);
	static bool PionMort(const APawn* Pion);

	// ---- Joueur local
	void JoueurLocal(float Dt);
	void Marquer(APlayerController* PC, APawn* Pion);
	void AppelerFrappe(APlayerController* PC, APawn* Pion);
	void GererRadar(APlayerController* PC, APawn* Pion, bool bActif);
	void CreerEcran();
	void RetirerEcran();

	ChellesButin::FReglagesButin Reglages;

	/** Coffre, coffre ouvert et son charges au demarrage (avant : lus au 1er coffre, ce qui gelait le jeu ~10 s). */
	UPROPERTY(Transient)
	TArray<UObject*> Precharges;

	// ---- Etat serveur
	TArray<TWeakObjectPtr<ACoffreButin>> Coffres;
	TMap<TWeakObjectPtr<AActor>, FCibleButin> Cibles;
	TArray<FBombeButin> Bombes;
	float ProchainRelais = 0.f;
	float ProchainSuivi = 1.f;
	float ProchainCoffre = 5.f;

	// ---- Etat local
	TSharedPtr<SWidget> Ecran;
	FString TexteAnnonce;
	FLinearColor CouleurAnnonce = FLinearColor::White;
	float AgeAnnonce = 100.f;
	FString TexteAide;
	FString TexteSerie;
	bool bCurseur = false;
	float ProchainRadar = 0.f;
};
