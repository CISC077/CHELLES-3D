// CHELLES 3D - Agent 5 "Testeur" : test automatique du jeu (25/11/2026)
//
//  Ne demarre QUE si le jeu est lance avec -ChellesTest (Tools\Testeur\TESTER_LE_JEU.bat,
//  lance tout seul chaque soir par la tache Windows de Tools\VerifErreurs).
//  En jeu normal, dans l'editeur ou sur le serveur : ce plugin ne fait rien.
//
//  Le parcours (reglages dans Plugins\ChellesTesteur\Testeur.txt) :
//   1. attend que la carte Rachid soit chargee et passe le menu de depart (SOLO) ;
//   2. teleporte le joueur dans 9 zones de la carte (grille 3 x 3) et fait un tour de camera ;
//   3. fait marcher le personnage quelques secondes ;
//   4. ouvre et ferme les menus (ARMEMENTS, HABILLAGE, PAUSE) ;
//   5. passe sur chaque personnage / vehicule de la liste (Soldat, Spider-Male, VOITURE...) ;
//   6. ecrit Saved\Testeur\resultats.json (FPS, erreurs, problemes par etape) et ferme le jeu.
//  Le rapport en francais est fabrique ensuite par Tools\Testeur\tester_le_jeu.py.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TesteurChelles.generated.h"

class APawn;
class APlayerController;

/** Une etape du parcours de test. */
struct FEtapeTest
{
	FString Nom;
	FString Categorie;      // Zone, Marche, Menu, Personnage
	float Duree = 8.f;      // secondes
	TFunction<void()> Debut;
	TFunction<void(float)> Pendant;
	TFunction<void()> Fin;

	// Resultats
	FString Etat = TEXT("pas fait"); // ok, probleme, non teste, pas fait
	TArray<FString> Problemes;
	TArray<FString> Remarques;
	int32 Images = 0;
	double Temps = 0.0;
	float FPSMin = 0.f;
	float PireImageMs = 0.f;
	int32 Saccades = 0;     // images de plus de 100 ms
	int32 Erreurs = 0;
	int32 Avertissements = 0;
	TArray<FString> ExemplesErreurs;
	FVector Position = FVector::ZeroVector;
	bool bPosition = false;
	FString Pion;           // classe du personnage / vehicule pendant l'etape
};

/** Un endroit a visiter. */
struct FPointTest
{
	FString Nom;
	FVector Lieu = FVector::ZeroVector;
};

/** Ou en est le test. */
enum class ETestPhase : uint8
{
	AttenteJoueur,
	Prechauffage,
	Etapes,
	Fin
};

UCLASS(NotPlaceable, Transient)
class CHELLESTESTEUR_API ATesteurChelles : public AActor
{
	GENERATED_BODY()

public:
	ATesteurChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// ---- Reglages (Testeur.txt) ----
	void LireReglages();
	TArray<FString> Modes;
	int32 TailleGrille = 3;
	float SecondesParZone = 8.f;
	float SecondesParMode = 8.f;
	float SecondesMarche = 5.f;
	float AttenteDepart = 15.f;
	float FPSFaible = 30.f;
	bool bMenus = true;
	bool bMarche = true;
	FString ModeFinal;
	TArray<FPointTest> PointsEnPlus;

	// ---- Deroulement ----
	ETestPhase Phase = ETestPhase::AttenteJoueur;
	double DebutPhase = 0.0;
	double ProchainSolo = 0.0;
	double DebutTest = 0.0;
	double ChargementSecondes = 0.0;
	int32 EtapeEnCours = INDEX_NONE;
	double DebutEtape = 0.0;
	TArray<FEtapeTest> Etapes;
	bool bEtapesConstruites = false;
	bool bSortieDemandee = false;

	void ConstruireEtapes();
	void AjouterZones();
	void AjouterMarche();
	void AjouterMenus();
	void AjouterModes();
	void CommencerEtape(int32 Index);
	void TerminerEtape();
	void EcrireResultats(bool bTermine) const;
	void Quitter();

	// ---- Aides ----
	APlayerController* PC() const;
	APawn* Pion() const;
	bool Commande(const FString& Ligne, FEtapeTest& Etape);
	static bool CommandeExiste(const FString& Nom);
	bool Sol(const FVector& Point, FVector& Trouve) const;
	void Teleporter(const FVector& Lieu);
	void TourneCamera(float Delta, float Duree);
	void VerifierChute(FEtapeTest& Etape, float ZDepart);

	// Mesure des images (FPS)
	double FenetreTemps = 0.0;
	int32 FenetreImages = 0;
	float FPSMinGlobal = 0.f;
	double TempsGlobal = 0.0;
	int32 ImagesGlobal = 0;
	uint64 MemoireMax = 0;
	float ZDepartEtape = 0.f;
	FVector DepartMarche = FVector::ZeroVector;
	FString PionAvant;
	int32 ErreursDebut = 0;
	int32 AvertissementsDebut = 0;
};
