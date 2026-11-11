// CHELLES 3D - Roue des modes : maintenir Tab pour choisir Soldat / Spider-Man / Motoneige / Automobile (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/PlayerInput.h"
#include "RoueModes.generated.h"

class APawn;
class APlayerController;
class UInputComponent;
class UUserWidget;
class SRoueModes;
class USkeletalMeshComponent;
class ARoueRelais;
class USkeletalMesh;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ETypeModeRoue : uint8
{
	APied     UMETA(DisplayName = "A pied (soldat)"),
	Vol       UMETA(DisplayName = "Vol (Spider-Male)"),
	Vehicule  UMETA(DisplayName = "Vehicule"),
	Personnage UMETA(DisplayName = "Personnage de pack (pris en main directement)"),
	Libre     UMETA(DisplayName = "Libre (fonction a venir)")
};

UENUM(BlueprintType)
enum class EIconeRoue : uint8
{
	Viseur,
	Toile,
	Flocon,
	Voiture,
	Etoile,
	Plus,
	Question,
	FerACheval,
	Eclair,
	Flamme,
	Epees,
	Griffes   // PANTERA (2026-11-11)
};

/** Une case de la roue. */
USTRUCT(BlueprintType)
struct FModeRoue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	FText Nom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	ETypeModeRoue Type = ETypeModeRoue::APied;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	EIconeRoue Icone = EIconeRoue::Etoile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	FLinearColor Couleur = FLinearColor(0.3f, 0.7f, 1.f);

	/** Pour un vehicule : le Blueprint a conduire (WheeledVehicle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue", meta = (MetaClass = "Pawn"))
	FSoftClassPath ClasseVehicule;

	/** Optionnel : mesh (SkeletalMesh) a mettre sur le personnage choisi, ex. le costume Spider-Man. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue", meta = (AllowedClasses = "SkeletalMesh"))
	FSoftObjectPath Costume;

	/** Nom du composant qui recoit le costume (vide = le corps du personnage). Ex. Horse_Rider pour le cavalier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	FName ComposantCostume;

	/** Vrai : la roue fait venir le vehicule a cote du joueur, on monte en s'approchant et en appuyant sur E. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	bool bMonterAvecE = false;

	/** Faux : E ne fait pas descendre (le personnage du pack se sert de E). On change de mode avec la roue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	bool bDescendreAvecE = true;

	/** Actions du jeu coupees tant que ce mode est joue (pour eviter les conflits de touches). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	TArray<FName> ActionsCoupees;

	/** Touches ajoutees tant que ce mode est joue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	TArray<FInputActionKeyMapping> ActionsAjoutees;

	/** Vrai : + et - font defiler les sorts 1 a 5 du pack (touches 1..5 simulees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	bool bSortsPlusMoins = false;

	/** Circulation pendant ce mode : nombre maximum de voitures, une par rue (0 = circulation normale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue", meta = (ClampMin = "0"))
	int32 LimiteCirculation = 0;

	/** Case de vol : secondes de vol maximum avant atterrissage force (0 = sans limite). Il faut toucher le sol pour revoler. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue", meta = (ClampMin = "0"))
	float DureeVolMax = 0.f;

	/** Vrai : deux yeux blancs en amande poses sur le masque (PANTERA). Reglables en jeu : console Chelles.PanteraYeux */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	bool bYeuxMasque = false;

	/**
	 * Case de vol : au sol on joue le SOLDAT (armes, tir, visee, recharge, ramassage, vie) habille avec CostumeSoldat.
	 * Pour s'envoler : sauter puis rappuyer sur Espace en l'air. En se posant, on reprend le soldat et ses armes.
	 * (Spider-Male, 2026-11-11, demande MERIDJA)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	bool bFonctionsSoldat = false;

	/** Corps mis sur le soldat pour bFonctionsSoldat (squelette du soldat FPS_Game). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue", meta = (AllowedClasses = "SkeletalMesh"))
	FSoftObjectPath CostumeSoldat;
};

/** Spider-Male soldat : ce que le soldat portait avant le costume (rendu en quittant le mode). */
struct FRoueCorpsAvant
{
	TWeakObjectPtr<USkeletalMeshComponent> Composant;
	TWeakObjectPtr<USkeletalMesh> Mesh;
	TArray<TWeakObjectPtr<UMaterialInterface>> Materiaux;
};

/**
 * Un seul par niveau. Il est ajoute tout seul au lancement du jeu (Play) s'il n'y en a pas.
 * Maintenir Tab : la roue apparait, le temps ralentit. On vise une case avec la souris,
 * on relache Tab (ou clic gauche) pour choisir. Clic droit : annuler.
 * Il remplace les touches F (vol) et M (motoneige) en pilotant les plugins
 * ChellesBasculeVol et ChellesMotoneige, sans en dependre.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Roue des modes (touche Tab)"))
class CHELLESROUE_API ARoueModes : public AActor
{
	GENERATED_BODY()

	friend class ARoueRelais;

public:
	ARoueModes();

	/** Les cases de la roue, dans le sens des aiguilles d'une montre a partir du haut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	TArray<FModeRoue> Modes;

	/** Touche a maintenir pour ouvrir la roue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	FKey Touche;

	/** Vitesse du jeu pendant que la roue est ouverte (1 = pas de ralenti). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue", meta = (ClampMin = "0.05", ClampMax = "1"))
	float Ralenti = 0.3f;

	/** Sensibilite de la souris dans la roue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	float Sensibilite = 8.f;

	/** Retire les anciennes touches F (vol) et M (motoneige) : tout passe par la roue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roue")
	bool bRetirerTouchesFetM = true;

	/** Passe au mode numero Index (0 = premiere case). Appelable depuis un Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Roue")
	void ChoisirMode(int32 Index);

	/** Numero du mode joue en ce moment. */
	UFUNCTION(BlueprintPure, Category = "Roue")
	int32 ModeActuel() const;

	UFUNCTION(BlueprintPure, Category = "Roue")
	bool EstOuverte() const { return bOuverte; }

	/** Passe au mode qui porte ce nom (ex. "PANTERA", appele par le menu HABILLAGE). Vrai si c'est maintenant le mode joue. */
	UFUNCTION(BlueprintCallable, Category = "Roue")
	bool ChoisirModeParNom(const FString& NomMode);

	/** Yeux blancs du masque PANTERA : pose (bAvecYeux) ou retire les yeux sur ce corps. */
	static void MettreYeuxMasque(USkeletalMeshComponent* Corps, bool bAvecYeux);
	/** Console Chelles.PanteraYeux : relit les reglages des yeux et les remet sur tous les personnages. */
	static void ReglerYeux(UWorld* Monde, const TArray<FString>& Arguments);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	APlayerController* GetPC() const;
	void LierClavier();

	void Ouvrir();
	void Relacher();
	void Valider();
	void Annuler();
	void Fermer();
	void SourisX(float Valeur);
	void SourisY(float Valeur);
	void ManetteX(float Valeur);
	void ManetteY(float Valeur);
	void MettreAJourSurvol();
	void EnvoyerALaRoue();

	// Liens avec les autres plugins (par reflexion)
	void ChercherPlugins();
	void RetirerAnciennesTouches();
	AActor* ControleurVehicule(int32 Index);
	bool EstDisponible(int32 Index) const;
	bool EstEnVol() const;
	void MettreCostume(const FModeRoue& Mode);
	void HabillerComposant(AActor* Acteur, const FModeRoue& Mode, bool bVisible);
	APawn* VehiculeDe(AActor* Controleur) const;
	void AppelerVehicule(int32 Index);
	void ToucheMonter();
	void SuivreMontures();
	void JouerMontee(APawn* Monture, const FModeRoue& Mode);
	void FinMontee();
	void ChiffreAzerty(FKey ToucheAzerty, bool bAppui);
	void EmpecherChute();
	void AppliquerTouchesDuMode();
	void RetablirTouches();
	void SortSuivant(int32 Sens);
	void SortPlus() { SortSuivant(1); }
	void SortMoins() { SortSuivant(-1); }
	static void RendreSesReferences(APawn* Perso, APawn* Ancien);
	bool EntrerPersonnage(int32 Index);
	void SortirPersonnage();
	void HabillerPion(APawn* Pion, const FModeRoue& Mode, bool bAvertir);
	/** Limite de vol (DureeVolMax) du personnage joue sur cette machine. */
	void LimiterVol(float DeltaSeconds);

	// Spider-Male avec les fonctions du soldat (bFonctionsSoldat) : soldat au sol, volant en l'air
	void SuivreSoldatVolant(float DeltaSeconds);
	void HabillerSoldat(APawn* Soldat, bool bCostume);
	void FinSoldatVolant();
	void ToucheSaut();
	void PreparerCasesSoldatVolant();

	// Multijoueur : chaque joueur a un ARoueRelais, le serveur fait les changements de mode
	bool EstMultijoueur() const;
	ARoueRelais* MonRelais() const;
	int32 IndexDuType(ETypeModeRoue Type) const;
	void ChoisirModeReseau(ARoueRelais* Relais, int32 Index);
	void GererRelaisServeur();
	void SuivreJoueursReseau();
	void ReglerCirculation();

	void CacherHudPack(const FString& Racine);
	void MontrerHudPack(const FString& Racine);
	static FString RacinePack(const FSoftClassPath& Chemin);
	void Message(const FString& Texte) const;

	UPROPERTY(Transient)
	AActor* Bascule = nullptr;

	UPROPERTY(Transient)
	UClass* ClasseMotoneige = nullptr;

	UPROPERTY(Transient)
	UClass* ClasseCirculation = nullptr;

	/** Limite de circulation appliquee en dernier (-1 = jamais reglee). */
	int32 LimiteCirculationAppliquee = -1;

	/** Un acteur Motoneige par case vehicule (meme index que Modes). */
	UPROPERTY(Transient)
	TArray<AActor*> Controleurs;

	UPROPERTY(Transient)
	TArray<UClass*> ClassesVehicules;

	UPROPERTY(Transient)
	UInputComponent* EntreesRoue = nullptr;

	TMap<FString, TArray<TWeakObjectPtr<UUserWidget>>> HudCaches;

	TSharedPtr<SRoueModes> Widget;

	FVector2D Curseur = FVector2D::ZeroVector;
	FVector2D Manette = FVector2D::ZeroVector;
	TArray<TWeakObjectPtr<AActor>> ActeursMotoneige;
	int32 Survol = -1;
	float Apparition = 0.f;
	float AncienRalenti = 1.f;
	float ProchaineRecherche = 0.f;
	bool bOuverte = false;
	bool bLieClavier = false;
	bool bIgnoreVue = false;

	/** Case (bMonterAvecE) sur laquelle on etait monte au tick precedent, -1 sinon. */
	int32 MontureActive = -1;
	TWeakObjectPtr<APawn> MontureAnimee;
	FTimerHandle MinuteurMontee;

	/** Anti-chute sous la carte : dernier endroit ou le personnage joue touchait le sol. */
	TWeakObjectPtr<APawn> PionSurveille;
	FVector DernierSol = FVector::ZeroVector;
	bool bSolConnu = false;

	/** Profil de touches applique (index du mode), et ce qu'il a change. */
	int32 ModeTouches = -1;
	int32 SortCourant = 1;
	TArray<FInputActionKeyMapping> TouchesRetirees;
	TArray<FInputActionKeyMapping> TouchesAjoutees;

	/** Mode Personnage : le perso du pack pris en main, et le pieton (soldat) cache pendant ce temps. */
	TWeakObjectPtr<APawn> PersoJoue;
	TWeakObjectPtr<APawn> PietonCache;

	/** Multijoueur : mode vu au tick precedent (pour cacher / montrer le HUD des packs). */
	int32 ModeVuReseau = INDEX_NONE;
	/** Multijoueur : soldats caches des joueurs dont on a coupe la collision sur cette machine. */
	TArray<TWeakObjectPtr<APawn>> CollisionsCoupees;

	/** Limite de vol : secondes volees depuis le dernier contact avec le sol. */
	float TempsVol = 0.f;
	/** Atterrissage force fait : on ne revole qu'apres avoir touche le sol. */
	bool bAtterrissageForce = false;
	int32 DernierAvisVol = -1;
	TWeakObjectPtr<APawn> PionVolSuivi;

	/** Case bFonctionsSoldat jouee en ce moment (solo), INDEX_NONE sinon. */
	int32 ModeSoldatVol = INDEX_NONE;
	/** Soldat qui porte le costume, et ce qu'il portait avant (rendu en quittant le mode). */
	TWeakObjectPtr<APawn> SoldatCostume;
	TArray<FRoueCorpsAvant> CorpsAvant;
	/** Envol demande (Espace en l'air) : le vol du pack est lance au tick suivant. */
	bool bDemarrerVol = false;
	/** Secondes passees posees au sol avec le volant (on reprend le soldat apres un court instant). */
	float TempsPose = 0.f;
	bool bAvisCostumeSoldat = false;
};
