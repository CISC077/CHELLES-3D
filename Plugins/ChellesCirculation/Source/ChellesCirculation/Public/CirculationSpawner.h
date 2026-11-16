// A poser dans le niveau : fait circuler des voitures sur les routes de la ville.
// Les routes viennent du fichier OpenStreetMap deja utilise pour construire Chelles
// (Tools/out/osm_ways.json), et/ou de splines posees a la main.
// Pour tout retirer : supprimer cet acteur du niveau. Rien d'autre n'est modifie.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VehiculeCirculation.h"
#include "CirculationSpawner.generated.h"

class UBillboardComponent;
class USoundBase;

/** Un point du reseau routier (carrefour ou point de forme d'une rue). */
USTRUCT()
struct FNoeudRoutier
{
	GENERATED_BODY()

	UPROPERTY() FVector Position = FVector::ZeroVector;
	/** Troncons qui partent de ce point. */
	UPROPERTY() TArray<int32> Sorties;
	/** Nombre de points voisins (3 ou plus = carrefour). */
	UPROPERTY() int32 NombreDeVoisins = 0;
	/** Altitude donnee par une spline (sinon mesuree sur le sol au lancement). */
	UPROPERTY() bool bAltitudeFixe = false;
};

/** Un morceau de rue a sens unique entre deux points (une rue a double sens = deux troncons). */
USTRUCT()
struct FTronconRoutier
{
	GENERATED_BODY()

	UPROPERTY() int32 De = INDEX_NONE;
	UPROPERTY() int32 Vers = INDEX_NONE;
	/** Troncon dans l'autre sens (INDEX_NONE si sens unique). */
	UPROPERTY() int32 Inverse = INDEX_NONE;
	UPROPERTY() float Longueur = 0.f;
	/** Decalage vers la droite (cm) : on roule a droite sur une rue a double sens. */
	UPROPERTY() float Decalage = 0.f;
	/** Vitesse maximale (cm/s). */
	UPROPERTY() float VitesseMax = 1390.f;
	/** Troncon d'un rond-point : prioritaire sur les voitures qui entrent. */
	UPROPERTY() bool bRondPoint = false;
	/** Direction horizontale (normalisee). */
	UPROPERTY() FVector Direction = FVector::ForwardVector;
};

/** Qui traverse un carrefour : les voitures venant du meme troncon peuvent passer a la suite. */
struct FReservationCarrefour
{
	int32 TronconEntree = INDEX_NONE;
	int32 Nombre = 0;
	/** Voiture qui attend depuis une autre rue : elle passe des que le carrefour se libere. */
	int32 Demandeur = INDEX_NONE;
};

UCLASS(BlueprintType, meta = (DisplayName = "Circulation Spawner (Chelles)"))
class CHELLESCIRCULATION_API ACirculationSpawner : public AActor
{
	GENERATED_BODY()

public:
	ACirculationSpawner();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------ Reglages principaux

	/** Decocher pour couper la circulation sans supprimer l'acteur. */
	UPROPERTY(EditAnywhere, Category = "Circulation")
	bool bActif = true;

	/** Nombre de voitures en circulation. */
	UPROPERTY(EditAnywhere, Category = "Circulation", meta = (ClampMin = "1", ClampMax = "400"))
	int32 NombreDeVoitures = 50;

	/** Multiplie toutes les limitations de vitesse (1 = respecte le code de la route). */
	UPROPERTY(EditAnywhere, Category = "Circulation", meta = (ClampMin = "0.2", ClampMax = "3"))
	float MultiplicateurDeVitesse = 1.f;

	/** Au-dela de cette distance (cm) les voitures ne sont plus dessinees. 0 = toujours dessinees. */
	UPROPERTY(EditAnywhere, Category = "Circulation", meta = (ClampMin = "0"))
	float DistanceAffichage = 25000.f;

	/** Les voitures trop loin du joueur sont deplacees pres de lui (hors de sa vue) : la ville parait plus vivante. */
	UPROPERTY(EditAnywhere, Category = "Circulation")
	bool bSuivreLeJoueur = true;

	/** Rayon (cm) autour du joueur dans lequel on garde les voitures (si "Suivre le joueur"). */
	UPROPERTY(EditAnywhere, Category = "Circulation", meta = (ClampMin = "5000", EditCondition = "bSuivreLeJoueur"))
	float RayonAutourDuJoueur = 30000.f;

	/** Les voitures s'arretent devant les pietons et le joueur. */
	UPROPERTY(EditAnywhere, Category = "Circulation")
	bool bSArreterPourLesPietons = true;

	/** Son de klaxon quand un pieton bloque la voiture (optionnel). */
	UPROPERTY(EditAnywhere, Category = "Circulation")
	USoundBase* Klaxon = nullptr;

	/** Couleur de carrosserie au hasard (si le materiau de la voiture le permet). */
	UPROPERTY(EditAnywhere, Category = "Circulation")
	bool bCouleursAleatoires = true;

	/** Dessine le reseau routier (lignes vertes, fleches au bout des troncons) en jeu, pour verifier. */
	UPROPERTY(EditAnywhere, Category = "Circulation")
	bool bAfficherLeReseau = false;

	/**
	 * Limite de voitures pendant la partie (0 = toutes). Reglee par la roue Tab (LimiteCirculation des modes) :
	 * les voitures en trop sont garees, cachees et sans collision, puis reviennent quand la limite saute.
	 */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Circulation")
	int32 LimiteEnJeu = 0;

	/** Avec une limite : une seule voiture par rue (ecart minimum entre deux voitures, voir EcartUneParRue). */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Circulation")
	bool bUneVoitureParRue = false;

	/**
	 * Voiture de police (plugin ChellesPolice, 2026-11-16) : les voitures qui roulent DANS LE MEME SENS devant elle
	 * (couloir de Longueur x Largeur cm a partir de Position, dans Direction) se rangent a droite et s'arretent
	 * pendant Duree secondes. Celles qui viennent en face ou d'une rue en travers ne bougent pas.
	 * Renvoie le nombre de voitures rangees.
	 */
	UFUNCTION(BlueprintCallable, Category = "Circulation")
	int32 DegagerDevant(FVector Position, FVector Direction, float Longueur, float Largeur, float Duree);

	/** Ecart (cm) vers la droite quand une voiture se range pour laisser passer la police. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Circulation", meta = (ClampMin = "0", ClampMax = "800"))
	float EcartRangement = 320.f;

	/** Ecart minimum (cm) entre deux voitures quand "une voiture par rue" est actif. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Circulation", meta = (ClampMin = "2000"))
	float EcartUneParRue = 12000.f;

	// ------------------------------------------------------------------ Vehicules

	/**
	 * Bouton : cherche les voitures (Static Mesh / Skeletal Mesh de taille voiture) dans le Content
	 * et remplit "Modeles". Indispensable pour un jeu package.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Circulation|Vehicules", meta = (DisplayName = "Remplir la liste des voitures"))
	void RemplirLaListeDesVoitures();

	/** Modeles de voitures. Si vide, recherche automatique dans le Content au lancement. */
	UPROPERTY(EditAnywhere, Category = "Circulation|Vehicules")
	TArray<FModeleVehicule> Modeles;

	/**
	 * Longueur maximale d'un vehicule (cm). Les plus longs (camions, bus, camping-cars) ne sont pas utilises :
	 * ils ne passent pas dans les petits ronds-points de Chelles et creent des bouchons.
	 * 620 = voitures et pick-up seulement. Mettez 1400 pour remettre camions et bus.
	 */
	UPROPERTY(EditAnywhere, Category = "Circulation|Vehicules", meta = (ClampMin = "300", ClampMax = "1400"))
	float LongueurMaximale = 620.f;

	/** Dossier ou chercher les voitures ("/Game" = tout le Content). */
	UPROPERTY(EditAnywhere, Category = "Circulation|Vehicules")
	FString DossierDeRecherche = TEXT("/Game");

	// ------------------------------------------------------------------ Reseau routier

	/**
	 * Bouton : lit le fichier OpenStreetMap et construit le reseau routier (sauvegarde avec le niveau).
	 * A refaire si vous relancez chelles_complet.py ou si vous deplacez la ville.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Circulation|Reseau", meta = (DisplayName = "Construire le reseau routier"))
	void ConstruireLeReseauRoutier();

	/** Utiliser les rues OpenStreetMap (celles qui ont servi a construire Chelles). */
	UPROPERTY(EditAnywhere, Category = "Circulation|Reseau")
	bool bUtiliserOpenStreetMap = true;

	/** Fichier des rues, relatif au dossier du projet. */
	UPROPERTY(EditAnywhere, Category = "Circulation|Reseau", meta = (EditCondition = "bUtiliserOpenStreetMap"))
	FString FichierOpenStreetMap = TEXT("Tools/out/osm_ways.json");

	/** Types de rues OSM ou circulent les voitures (les memes que celles dessinees par build_roads.py). */
	UPROPERTY(EditAnywhere, Category = "Circulation|Reseau", meta = (EditCondition = "bUtiliserOpenStreetMap"))
	TArray<FString> TypesDeRues;

	/** Centre de la carte (doit correspondre a CENTER dans Tools/build_buildings.py). */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Circulation|Reseau")
	double LatitudeCentre = 48.8795;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Circulation|Reseau")
	double LongitudeCentre = 2.5912;

	/** Demi-cote de la zone construite (cm) : les rues au-dela n'ont pas de route 3D. 0 = pas de limite. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Circulation|Reseau", meta = (ClampMin = "0"))
	float LimiteDeLaZone = 52000.f;

	/** Decalage de toute la carte dans le niveau (cm), si la ville a ete deplacee. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Circulation|Reseau")
	FVector2D DecalageDeLaCarte = FVector2D::ZeroVector;

	/**
	 * Acteurs contenant des splines (Spline Component) a utiliser comme routes en plus d'OSM.
	 * Double sens par defaut ; ajoutez le Tag "SensUnique" a l'acteur pour un sens unique (sens de la spline).
	 */
	UPROPERTY(EditAnywhere, Category = "Circulation|Reseau")
	TArray<AActor*> RoutesManuelles;

	/** Largeur des routes manuelles (cm). */
	UPROPERTY(EditAnywhere, Category = "Circulation|Reseau", meta = (ClampMin = "200"))
	float LargeurRoutesManuelles = 700.f;

	/** Vitesse maximale sur les routes manuelles (km/h). */
	UPROPERTY(EditAnywhere, Category = "Circulation|Reseau", meta = (ClampMin = "5"))
	float VitesseRoutesManuelles = 40.f;

	/** Reseau enregistre avec le niveau. */
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Circulation|Reseau")
	TArray<FNoeudRoutier> Noeuds;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Circulation|Reseau")
	TArray<FTronconRoutier> Troncons;

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UBillboardComponent* Icone;
#endif

	UPROPERTY(Transient)
	TArray<AVehiculeCirculation*> Vehicules;

	/** Modeles retenus pour cette partie. */
	UPROPERTY(Transient)
	TArray<FModeleVehicule> ModelesActifs;

	TArray<FReservationCarrefour> ReservationsCarrefours;

	/** Noeuds situes sur un rond-point (on y cede le passage aux voitures deja engagees). */
	TArray<bool> NoeudSurRondPoint;

	/** Longueurs cumulees des troncons (pour tirer un point au hasard, proportionnellement a la longueur). */
	TArray<float> LongueursCumulees;

	/** Meme index que Vehicules : voiture garee a cause de LimiteEnJeu. */
	TArray<bool> Garees;

	int32 VoituresRestantesAFaireApparaitre = 0;
	float ProchainRecyclage = 0.f;
	float FrequenceTotale = 0.f;

	// ---- Construction du reseau
	bool LireOpenStreetMap(TArray<FNoeudRoutier>& OutNoeuds, TArray<FTronconRoutier>& OutTroncons, FString& OutMessage) const;
	void AjouterRoutesManuelles(TArray<FNoeudRoutier>& OutNoeuds, TArray<FTronconRoutier>& OutTroncons) const;
	static void FinaliserReseau(TArray<FNoeudRoutier>& InOutNoeuds, TArray<FTronconRoutier>& InOutTroncons);
	void MesurerAltitudes();
	bool TrouverSol(const FVector& Point, float DemiPlage, float& OutZ) const;
	void DessinerReseau(float Duree) const;

	// ---- Vehicules
	void ChercherVoituresDansContent(TArray<FModeleVehicule>& OutModeles) const;
	const FModeleVehicule* TirerUnModele() const;
	void FaireApparaitreUnLot();
	bool PlacerVehicule(AVehiculeCirculation* Vehicule, bool bLoinDuJoueur);
	void EtendreItineraire(AVehiculeCirculation* Vehicule) const;
	void LibererCarrefours(AVehiculeCirculation* Vehicule, int32 IndexVehicule, bool bTous);

	// ---- Conduite
	void ConduireVehicule(int32 IndexVehicule, float DeltaSeconds, const FVector& PositionJoueur, const FVector& VueJoueur, bool bJoueurValide);
	/** Assez de place apres le carrefour pour s'y engager sans le bloquer ? */
	bool SortieLibre(int32 IndexVehicule, int32 TronconSortie) const;
	/** Aucune voiture physiquement au milieu du carrefour ? */
	bool CarrefourDegage(int32 IndexVehicule, int32 Noeud) const;
	float DistanceAuVehiculeDevant(int32 IndexVehicule, float& OutVitesseDevant, bool bIgnorerCroisements, bool& bOutCroisement) const;
	float DistanceAuxPietons(AVehiculeCirculation* Vehicule, const FVector& Position, const FVector& Avant) const;
	void CalculerPose(const AVehiculeCirculation* Vehicule, FVector& OutPosition, FVector& OutTangente) const;
	FVector PointSurTroncon(int32 Troncon, float S) const;
	void RecyclerLesVoituresLointaines(const FVector& PositionJoueur, const FVector& VueJoueur, bool bJoueurValide);
	bool EstGaree(int32 IndexVehicule) const { return Garees.IsValidIndex(IndexVehicule) && Garees[IndexVehicule]; }
	void GarerVehicule(int32 IndexVehicule);
	void AppliquerLimite();
	void UneVoitureParRue(const FVector& PositionJoueur, const FVector& VueJoueur, bool bJoueurValide);

	void Annoncer(const FString& Texte, const FColor& Couleur) const;
};
