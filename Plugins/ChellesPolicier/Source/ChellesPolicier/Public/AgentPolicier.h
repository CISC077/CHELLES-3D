// CHELLES 3D - Un agent de police qui regle la circulation (pose par APolicierChelles).
// Sans animation importee, ses bras sont calcules par le plugin (gestes passez / stop).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AgentPolicier.generated.h"

class UCapsuleComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UPoseableMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UAnimSequenceBase;

UENUM()
enum class EGestePolicier : uint8
{
	Repos,
	Passez,
	Stop
};

UCLASS(NotPlaceable, Transient, meta = (DisplayName = "Agent de police (Chelles)"))
class CHELLESPOLICIER_API AAgentPolicier : public AActor
{
	GENERATED_BODY()

public:
	AAgentPolicier();

	/**
	 * Donne son apparence a l'agent. Avec AnimRepos : mode animations ; sinon gestes calcules.
	 * bRotationAuto : rotation du modele calculee sur le squelette ; sinon CorrectionLacet (degres).
	 */
	bool Configurer(USkeletalMesh* Modele, float Taille, bool bRotationAuto, float CorrectionLacet,
		UAnimSequenceBase* AnimRepos, UAnimSequenceBase* AnimPassez, UAnimSequenceBase* AnimStop);

	/**
	 * Apparence avec un modele fige (Static Mesh, par exemple l'agente de Personnages/POLICES) :
	 * pas de gestes, pas de physique, toujours debout. Redressement = rotation appliquee au modele
	 * avant tout (tangage, roulis) s'il est couche ; Lacet = rotation pour qu'il regarde devant.
	 */
	bool ConfigurerFixe(UStaticMesh* Modele, float Taille, float Lacet, const FRotator& Redressement);

	/** Ordre donne par le gestionnaire : geste, direction a regarder (lacet monde) et cote du passage (+1 droite, -1 gauche). */
	void DonnerOrdre(EGestePolicier Geste, float LacetVise, float CotePassage);

	EGestePolicier GesteActuel() const { return Geste; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Policier")
	UCapsuleComponent* Capsule;

	/** Mode animations (Mixamo ou autre). */
	UPROPERTY(Transient)
	USkeletalMeshComponent* Corps = nullptr;

	/** Mode modele fige (Static Mesh). */
	UPROPERTY(Transient)
	UStaticMeshComponent* CorpsFixe = nullptr;

	/** Mode gestes calcules. */
	UPROPERTY(Transient)
	UPoseableMeshComponent* CorpsPose = nullptr;

	UPROPERTY(Transient) UAnimSequenceBase* AnimRepos = nullptr;
	UPROPERTY(Transient) UAnimSequenceBase* AnimPassez = nullptr;
	UPROPERTY(Transient) UAnimSequenceBase* AnimStop = nullptr;

	EGestePolicier Geste = EGestePolicier::Repos;
	EGestePolicier GesteAffiche = EGestePolicier::Repos;
	float LacetVise = 0.f;
	float Cote = 1.f;
	float DebutGeste = 0.f;
	float Temps = 0.f;
	bool bPoseFaite = false;

	// ---- Gestes calcules : os des bras et repere du modele (espace du composant)
	struct FBras
	{
		int32 Epaule = INDEX_NONE;  // parent du bras
		int32 Bras = INDEX_NONE;
		int32 AvantBras = INDEX_NONE;
		int32 Main = INDEX_NONE;
		FTransform RefEpaule;       // poses de reference, espace composant
		FTransform RefBras;
		FTransform RefAvantBras;
		FTransform RefMain;
		bool EstValide() const { return Bras != INDEX_NONE && AvantBras != INDEX_NONE && Main != INDEX_NONE; }
	};
	FBras BrasGauche;
	FBras BrasDroit;
	FVector Devant = FVector::ForwardVector;   // devant du modele (espace composant)
	FVector Gauche = -FVector::RightVector;    // gauche du modele (espace composant)

	void PlacerBras(const FBras& B, const FVector& DirBras, const FVector& DirAvantBras);
	void AppliquerGestesCalcules();
	void JouerAnimation(EGestePolicier NouveauGeste);
};
