// CHELLES 3D - Roue des modes : maintenir Tab pour choisir Soldat / Spider-Man / Motoneige / Automobile (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "UObject/SoftObjectPath.h"
#include "RoueModes.generated.h"

class APawn;
class APlayerController;
class UInputComponent;
class UUserWidget;
class SRoueModes;

UENUM(BlueprintType)
enum class ETypeModeRoue : uint8
{
	APied     UMETA(DisplayName = "A pied (soldat)"),
	Vol       UMETA(DisplayName = "Vol (Spider-Man)"),
	Vehicule  UMETA(DisplayName = "Vehicule"),
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
	Flamme
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

	void CacherHudPack(const FString& Racine);
	void MontrerHudPack(const FString& Racine);
	static FString RacinePack(const FSoftClassPath& Chemin);
	void Message(const FString& Texte) const;

	UPROPERTY(Transient)
	AActor* Bascule = nullptr;

	UPROPERTY(Transient)
	UClass* ClasseMotoneige = nullptr;

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
};
