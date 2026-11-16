// Une voiture de la circulation. Elle ne se deplace pas toute seule : c'est le Circulation Spawner
// qui la fait avancer (un seul Tick pour toutes les voitures = beaucoup plus rapide).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VehiculeCirculation.generated.h"

class UBoxComponent;
class UStaticMesh;
class USkeletalMesh;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMeshComponent;

/** Un modele de voiture : un Static Mesh OU un Skeletal Mesh complet (carrosserie + roues). */
USTRUCT(BlueprintType)
struct FModeleVehicule
{
	GENERATED_BODY()

	/** Voiture en Static Mesh (le plus leger). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicule")
	UStaticMesh* MeshStatique = nullptr;

	/** Ou bien voiture en Skeletal Mesh (affichee dans sa pose de repos, sans physique). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicule")
	USkeletalMesh* MeshSquelettique = nullptr;

	/** Rotation a appliquer si la voiture roule "de travers" ou en marche arriere (degres : 0, 90, -90, 180). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicule")
	float CorrectionOrientation = 0.f;

	/** Frequence d'apparition relative (2 = deux fois plus souvent que les autres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicule", meta = (ClampMin = "0"))
	float Frequence = 1.f;

	bool EstValide() const { return MeshStatique != nullptr || MeshSquelettique != nullptr; }
};

UCLASS(NotPlaceable, meta = (DisplayName = "Vehicule de circulation (Chelles)"))
class CHELLESCIRCULATION_API AVehiculeCirculation : public AActor
{
	GENERATED_BODY()

public:
	AVehiculeCirculation();

	/** Donne son apparence a la voiture. Renvoie false si le modele est vide. */
	bool Configurer(const FModeleVehicule& Modele, float DistanceAffichage, bool bCouleurAleatoire);

	/** Apparence de secours (boite coloree) si aucune voiture n'a ete trouvee dans le Content. */
	void ConfigurerSilhouette(float DistanceAffichage);

	/** Longueur et largeur reelles de la voiture (cm), mesurees sur le modele. */
	float Longueur = 450.f;
	float Largeur = 180.f;

	// ---- Etat de conduite (gere par le Circulation Spawner) ----

	/** Troncons a parcourir : [0] = troncon actuel, puis les suivants deja choisis. */
	TArray<int32> Itineraire;
	/** Troncon d'ou l'on vient (pour arrondir le virage a la sortie d'un carrefour). */
	int32 TronconPrecedent = INDEX_NONE;
	/** Distance parcourue sur le troncon actuel (cm). */
	float Distance = 0.f;
	/** Vitesse actuelle (cm/s). */
	float Vitesse = 0.f;
	/** Temperament du conducteur : multiplie la vitesse autorisee (0.85 a 1.1). */
	float Temperament = 1.f;
	/** Carrefours reserves par cette voiture (pour ne pas se croiser au milieu). */
	TArray<int32, TInlineAllocator<2>> CarrefoursReserves;
	/** Temps d'attente devant un carrefour occupe ; au-dela d'un delai on passe quand meme. */
	float AttenteCarrefour = 0.f;
	int32 CarrefourForce = INDEX_NONE;
	/** Carrefour ou cette voiture attend son tour (INDEX_NONE sinon). */
	int32 CarrefourDemande = INDEX_NONE;
	/** Pietons / joueur devant la voiture. */
	float DistancePieton = BIG_NUMBER;
	float ProchainTestPieton = 0.f;
	float TempsBloqueParPieton = 0.f;
	float ProchainKlaxon = 0.f;
	/** Temps passe a l'arret (pour debloquer un embouteillage hors de vue). */
	float TempsALArret = 0.f;
	/** Temps bloque par une voiture qui coupe ma route (carrefour, rond-point) : anti-interblocage. */
	float TempsBloqueEnCroisement = 0.f;
	/** Jusqu'a cet instant, la voiture ignore les voitures qui coupent sa route (pour sortir d'un interblocage). */
	float FinPassageForce = 0.f;
	/** Orientation affichee (lissee). */
	FRotator RotationAffichee = FRotator::ZeroRotator;
	bool bPoseInitialisee = false;

	/** Remet l'etat de conduite a zero (avant une nouvelle apparition). */
	void ReinitialiserConduite();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Vehicule")
	USceneComponent* Racine;

	/** Boite de collision : le joueur ne traverse pas les voitures, les balles les touchent. */
	UPROPERTY(VisibleAnywhere, Category = "Vehicule")
	UBoxComponent* Caisse;

	UPROPERTY(VisibleAnywhere, Category = "Vehicule")
	UStaticMeshComponent* ModeleStatique;

	UPROPERTY(VisibleAnywhere, Category = "Vehicule")
	USkeletalMeshComponent* ModeleSquelettique;

	UPROPERTY()
	UStaticMesh* CubeSecours = nullptr;

	void ColorerCarrosserie(UMeshComponent* Composant);
};
