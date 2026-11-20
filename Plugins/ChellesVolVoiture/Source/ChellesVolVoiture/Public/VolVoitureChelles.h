// CHELLES 3D - Vol de voiture facon GTA (UE 4.27). Demande MERIDJA 2026-11-20.
//
// Un gestionnaire par machine (ajoute tout seul au Play, rien a poser dans la carte) :
//  - F pres d'une voiture de la circulation (plugin ChellesCirculation) : le conducteur est ejecte et s'enfuit
//    en courant, la voiture devient une vraie voiture conduite (Blueprint du pack correspondant au modele) ;
//  - F pres d'une voiture garee ou deja volee : on monte ; F (ou E) dans la voiture : on descend ;
//  - des autos supplementaires sont garees au bord des rues autour du joueur (Vehicle Variety Pack, sportives du
//    Sports Vehicles Pack, buggy), loin des yeux, et retirees quand on s'eloigne (fluidite) ;
//  - niveau RECHERCHE (etoiles) : voler une voiture dans la circulation donne une etoile ; la police poursuit
//    le joueur (1 voiture par etoile, 3 au plus) ; loin de la police les etoiles baissent ; arrete tout pres
//    d'une voiture de police = ARRETE (etoiles effacees).
// Ne modifie aucun autre plugin ni Blueprint : la circulation et la police sont lues par leur nom de classe.
// Reglages : Plugins\ChellesVolVoiture\VolVoiture.txt (Bloc-notes, pas besoin de recompiler).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "VolVoitureChelles.generated.h"

class APawn;
class APlayerController;
class ARelaisVolVoiture;
class UCameraComponent;
class USpringArmComponent;
class USkeletalMesh;
class UAnimationAsset;

/** Une voiture de police lancee a la poursuite d'un joueur (serveur). */
USTRUCT()
struct FPoursuivantVol
{
	GENERATED_BODY()

	UPROPERTY()
	APawn* Voiture = nullptr;

	UPROPERTY()
	ARelaisVolVoiture* Cible = nullptr;

	float TempsBloque = 0.f;
	float MarcheArriere = 0.f;
	float TempsRenverse = 0.f;
	float Age = 0.f;
	bool bSirene = false;
};

UCLASS(NotPlaceable, Transient)
class CHELLESVOLVOITURE_API AVolVoitureChelles : public AActor
{
	GENERATED_BODY()

	friend class ARelaisVolVoiture;

public:
	AVolVoitureChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console (touche ²) : Chelles.Vol [Info|Relire|Etoiles N|Garer] */
	void Commande(const TArray<FString>& Arguments);

	void Message(const FString& Texte, float Duree = 4.f, int32 Cle = -1) const;

private:
	// ---- Reglages
	void LireReglages();
	void ChargerClasses();
	UClass* ClassePourModele(const FString& Modele);

	// ---- Reseau
	bool EstServeur() const;
	APlayerController* PCLocal() const;
	ARelaisVolVoiture* MonRelais() const;
	void GererRelaisServeur();
	void Avis(APlayerController* PC, const FString& Texte, float Duree = 4.f) const;

	// ---- Voitures
	UClass* ClasseCirculation();
	AActor* VoitureCirculationProche(const APawn* Pieton, float Rayon);
	APawn* NotreVoitureLibreProche(const APawn* Pieton, float Rayon) const;
	static FString ModeleDe(const AActor* VoitureCirculation);
	bool EstNotreVoiture(const AActor* Acteur) const;
	bool EstConduiteParJoueur(const APawn* Voiture) const;
	APawn* FaireApparaitre(UClass* Classe, const FVector& Lieu, float Yaw, bool bGaree);
	void RetirerCirculationPres(const FVector& Lieu, float Rayon);
	void EjecterConducteur(const FVector& Lieu, float Yaw);

	// ---- Serveur : vol, monter, descendre
	void VolerServeur(ARelaisVolVoiture* R, const FVector& Lieu, float Yaw, const FString& Modele);
	void MonterServeur(ARelaisVolVoiture* R, APawn* Voiture, bool bAvecEtoile);
	void DescendreServeur(ARelaisVolVoiture* R);
	void SuivreConducteurs();
	void RendreLePietonALaRoue(APlayerController* PC, APawn* Voiture, APawn* Pieton);

	// ---- Serveur : autos garees
	void GererVoituresGarees();
	bool LoinDesJoueurs(const FVector& Point, float Distance, bool bHorsDeVue) const;

	// ---- Serveur : niveau RECHERCHE et police
	void AjouterEtoiles(ARelaisVolVoiture* R, int32 Nombre);
	void GererRecherche(float Dt);
	void EnvoyerPolice(ARelaisVolVoiture* R);
	void ConduirePoursuivant(FPoursuivantVol& P, float Dt);
	bool TrouverPlaceHorsDeVue(const FVector& Centre, float DistMin, float DistMax, FVector& OutLieu, float& OutYaw);

	// ---- Joueur local
	void JoueurLocal(float Dt);
	void InstallerCamera(APawn* Voiture);
	void RetirerCamera();
	void AfficherEtoiles();

	// ---- Outils
	static void CacherPion(APawn* Pion, bool bCacher);
	static void ArreterVoiture(APawn* Voiture);
	static FVector PlaceADescendre(const APawn* Voiture, const APawn* Pieton);
	void Aider(APawn* Voiture, float Dt) const;

	// ---- Reglages (VolVoiture.txt)
	FKey ToucheVoler;
	FKey ToucheDescendre2;
	float DistanceVol = 400.f;          // cm
	TMap<FString, FString> CheminsModeles; // nom de modele (sans SM_/SK_) -> Blueprint conduit
	TArray<FString> CheminsSecours;
	FString CheminPolice;
	FString CheminFuyard;
	FString CheminAnimFuyard;
	bool bFuyard = true;
	int32 NombreGarees = 8;
	float RayonGarees = 15000.f;        // cm
	int32 MaxVoitures = 20;
	float EcartTrottoir = 220.f;        // cm
	bool bRecherche = true;
	int32 EtoilesParVol = 1;
	int32 EtoilesMax = 5;
	int32 PoliceParEtoile = 1;
	int32 PoliceMax = 3;
	float DureeEtoile = 25.f;           // s loin de la police pour perdre une etoile
	float DistanceVuePolice = 12000.f;  // cm
	float VitessePolice = 110.f;        // km/h
	float DistanceCamera = 650.f;
	float HauteurCamera = 170.f;
	bool bAide = true;
	float Adherence = 8.f;

	// ---- Etat
	UPROPERTY(Transient)
	TMap<FString, UClass*> ClassesModeles;

	UPROPERTY(Transient)
	TArray<UClass*> ClassesSecours;

	UPROPERTY(Transient)
	UClass* ClassePolice = nullptr;

	UPROPERTY(Transient)
	UClass* ClasseCirc = nullptr;

	UPROPERTY(Transient)
	USkeletalMesh* MeshFuyard = nullptr;

	UPROPERTY(Transient)
	UAnimationAsset* AnimFuyard = nullptr;

	/** Serveur : voitures que ce plugin a fait apparaitre (garees ou volees). */
	TArray<TWeakObjectPtr<APawn>> NosVoitures;

	/** Serveur : voitures de police lancees a la poursuite. */
	UPROPERTY(Transient)
	TArray<FPoursuivantVol> Poursuivants;

	/** Camera installee sur la voiture conduite par le joueur local. */
	TWeakObjectPtr<APawn> VoitureCamera;
	TWeakObjectPtr<USpringArmComponent> BrasCamera;
	TWeakObjectPtr<UCameraComponent> Camera;
	TArray<TWeakObjectPtr<UCameraComponent>> CamerasEteintes;
	float TempsSansSouris = 0.f;
	bool bAideAffichee = false;
	bool bClassesChargees = false;
	float ProchainRelais = 0.f;
	float ProchainesGarees = 3.f;
	float ProchaineRechercheCirc = 0.f;
	int32 EtoilesAffichees = -1;
};
