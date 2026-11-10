// A poser dans le niveau : fait apparaitre des passants qui se promenent autour de lui.
// Pour tout retirer : supprimer cet acteur du niveau. Rien d'autre n'est modifie.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimInstance.h"
#include "PassantsSpawner.generated.h"

class APassantCharacter;
class UAnimSequence;
class USphereComponent;
class UBillboardComponent;
class USkeletalMesh;
class UAnimInstance;

/** Un "modele" de passant : un personnage et ses animations (du meme squelette). */
USTRUCT(BlueprintType)
struct FTypePassant
{
	GENERATED_BODY()

	/** Le personnage (Skeletal Mesh habille). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passant")
	USkeletalMesh* Apparence = nullptr;

	/** Animation de marche (meme squelette que le personnage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passant")
	UAnimSequence* Marche = nullptr;

	/** Animation d'attente (optionnelle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passant")
	UAnimSequence* Repos = nullptr;
};

UCLASS(BlueprintType, meta = (DisplayName = "Passants Spawner (Chelles)"))
class CHELLESPASSANTS_API APassantsSpawner : public AActor
{
	GENERATED_BODY()

public:
	APassantsSpawner();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Decocher pour desactiver les passants sans supprimer l'acteur. */
	UPROPERTY(EditAnywhere, Category = "Passants")
	bool bActif = true;

	/** Nombre de passants dans la zone. */
	UPROPERTY(EditAnywhere, Category = "Passants", meta = (ClampMin = "1", ClampMax = "300"))
	int32 NombreDePassants = 40;

	/** Rayon de la zone de promenade en cm (20000 = 200 metres). La sphere verte la montre dans l'editeur. */
	UPROPERTY(EditAnywhere, Category = "Passants", meta = (ClampMin = "500"))
	float RayonDeZone = 20000.f;

	/** Au-dela de cette distance (cm) les passants ne sont plus dessines. 0 = toujours dessines. */
	UPROPERTY(EditAnywhere, Category = "Passants", meta = (ClampMin = "0"))
	float DistanceAffichage = 15000.f;

	/**
	 * Optimisation : les passants plus loin que "Distance Endormir" de tous les joueurs se mettent en pause
	 * (ils ne bougent plus, ne coutent presque plus rien) et repartent quand un joueur revient.
	 */
	UPROPERTY(EditAnywhere, Category = "Passants|Optimisation")
	bool bEndormirLoinDuJoueur = true;

	/** Distance (cm) au-dela de laquelle un passant se met en pause. 0 = la meme que "Distance Affichage". */
	UPROPERTY(EditAnywhere, Category = "Passants|Optimisation", meta = (ClampMin = "0", EditCondition = "bEndormirLoinDuJoueur"))
	float DistanceEndormir = 0.f;

	/** Les passants ne marchent que sur les trottoirs : jamais sur les routes, l'herbe, les toits ou les jumpers. */
	UPROPERTY(EditAnywhere, Category = "Passants|Trottoirs")
	bool bTrottoirsSeulement = true;

	/** Mot present dans le nom des trottoirs (acteurs "Chelles_Trottoir_..." ou meshes "SM_Trottoir_..."). */
	UPROPERTY(EditAnywhere, Category = "Passants|Trottoirs", meta = (EditCondition = "bTrottoirsSeulement"))
	FString MotCleTrottoir = TEXT("Trottoir");

	/**
	 * Bouton : cherche les personnages et leurs animations dans le Content et remplit "Types De Personnages".
	 * Indispensable pour un jeu package (seuls les assets references par le niveau sont inclus).
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Passants|Personnages", meta = (DisplayName = "Remplir la liste des personnages"))
	void RemplirLaListeDesPersonnages();

	/** Personnages avec leurs propres animations. Prioritaire sur tout le reste s'il est rempli. */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	TArray<FTypePassant> TypesDePersonnages;

	/** Cherche tout seul les personnages (habilles) et leurs animations dans le dossier Content (si les listes sont vides). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	bool bChercherPersonnagesAutomatiquement = true;

	/** Dossier ou chercher les personnages ("/Game" = tout le dossier Content). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages", meta = (EditCondition = "bChercherPersonnagesAutomatiquement"))
	FString DossierDeRecherche = TEXT("/Game");

	/** Dossiers ignores par la recherche (imports de batiments, personnages a assembler en morceaux...). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	TArray<FString> DossiersIgnores;

	/** Garder aussi les mannequins gris (sans vetements) ? Par defaut ils sont ignores s'il y a d'autres personnages. */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	bool bGarderMannequinSansVetements = false;

	/** Ancienne liste : personnages partageant les deux animations ci-dessous (optionnel). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	TArray<USkeletalMesh*> Personnages;

	/** Animation de marche pour les personnages ci-dessus (optionnel). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	UAnimSequence* AnimationMarche = nullptr;

	/** Animation d'attente pour les personnages ci-dessus (optionnel). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	UAnimSequence* AnimationRepos = nullptr;

	/** Ou bien un Animation Blueprint (remplace les animations ci-dessus). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	TSubclassOf<UAnimInstance> AnimationBlueprint;

	/** Vitesse de marche minimale (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Passants|Comportement")
	float VitesseMin = 100.f;

	/** Vitesse de marche maximale (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Passants|Comportement")
	float VitesseMax = 160.f;

	/** Pause minimale entre deux trajets (secondes). */
	UPROPERTY(EditAnywhere, Category = "Passants|Comportement")
	float PauseMin = 1.f;

	/** Pause maximale entre deux trajets (secondes). */
	UPROPERTY(EditAnywhere, Category = "Passants|Comportement")
	float PauseMax = 6.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Passants")
	USphereComponent* Zone;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UBillboardComponent* Icone;
#endif

	UPROPERTY(Transient)
	TArray<FTypePassant> Types;

	UPROPERTY(Transient)
	TArray<APassantCharacter*> Passants;

	FTimerHandle MinuteurRemplacement;
	FTimerHandle MinuteurApparition;
	FTimerHandle MinuteurEndormir;

	/** Met en pause les passants loin de tous les joueurs, reveille les autres (toutes les 0,5 s). */
	void EndormirLesPassantsLointains();

	/** Passants encore a faire apparaitre (apparition etalee sur plusieurs images). */
	int32 PassantsRestantsAFaireApparaitre = 0;

	void PreparerTypes();
	void ChercherPersonnagesDansContent(TArray<FTypePassant>& OutTypes) const;
	bool EstDansUnDossierIgnore(const FString& Chemin) const;
	bool TrouverPointDApparition(FVector& OutPoint) const;
	bool TrouverPointSurTrottoir(FVector& OutPoint) const;
	APassantCharacter* FaireApparaitreUnPassant();
	void FaireApparaitreUnLot();
	void RemplacerLesDisparus();
	void Annoncer(const FString& Texte, const FColor& Couleur) const;
};
