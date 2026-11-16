// CHELLES 3D - Voitures de police (pack "Sports and Police Cars - Drivable", dossier SportVehiclesPack). UE 4.27.
// Demande MERIDJA 2026-11-16 : la POLICE remplace le SUV dans la roue TAB ; l'administrateur du jeu (le joueur en
// solo, l'hote en multijoueur) pose des voitures de police sur les routes ; tous les joueurs peuvent les conduire ;
// quand une voiture de police roule, les voitures devant elle DANS SON SENS se rangent (celles d'en face, non).
//
// Un gestionnaire par machine (ajoute tout seul au Play, rien a poser dans la carte) :
//  - conduite : camera derriere la voiture, Z/S gaz et marche arriere, Q/D volant, Espace frein a main,
//    souris = regarder autour, L = sirene et gyrophares, H = klaxon (plugin ChellesSonsVehicules) ;
//  - E pres d'une voiture de police libre = monter ; E dedans = descendre ;
//  - F8 = poser une voiture de police la ou l'on vise, F9 = retirer celle visee (administrateur seulement) ;
//    les voitures posees sont gardees dans Police_Positions.txt et reviennent a chaque partie ;
//  - route degagee : appelle DegagerDevant du plugin ChellesCirculation (par reflexion, sans en dependre) ;
//  - aide a la conduite (la voiture ne glisse pas sur le cote, comme l'ancien plugin du SUV).
// Reglages : Plugins\ChellesPolice\Police.txt (Bloc-notes, pas besoin de recompiler).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "PoliceChelles.generated.h"

class APawn;
class APlayerController;
class ARelaisPolice;
class UCameraComponent;
class USpringArmComponent;

/** Valeurs d'origine d'une classe de roue (remises a la fin du jeu). */
USTRUCT()
struct FRouePoliceOrigine
{
	GENERATED_BODY()

	UPROPERTY()
	UClass* Classe = nullptr;

	UPROPERTY()
	UObject* Pneu = nullptr;

	float LatStiffValue = 0.f;
	float LongStiffValue = 0.f;
	float SteerAngle = 0.f;
	float SuspensionDampingRatio = 0.f;
};

UCLASS(NotPlaceable, Transient)
class CHELLESPOLICE_API APoliceChelles : public AActor
{
	GENERATED_BODY()

	friend class ARelaisPolice;

public:
	APoliceChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console (touche ²) : Chelles.Police [Info|Relire|Liste] */
	void Commande(const TArray<FString>& Arguments);

	/** Vrai si cet acteur est une voiture de police (nom de sa classe ou d'une classe parente dans Classes=). */
	bool EstPolice(const AActor* Acteur) const;

private:
	// ---- Reglages
	void LireReglages();
	FString DossierPlugin() const;

	// ---- Reseau / roles
	bool EstServeur() const;      // solo, hote ou serveur dedie
	bool EstMultijoueur() const;  // hote, client ou serveur dedie
	APlayerController* PCLocal() const;
	ARelaisPolice* MonRelais() const;
	bool EstAdministrateur(APlayerController* PC) const;
	void GererRelaisServeur();
	void Avis(APlayerController* PC, const FString& Texte, float Duree = 4.f) const;
	void Message(const FString& Texte, float Duree = 4.f, int32 Cle = -1) const;

	// ---- Voitures connues
	void ChercherVoitures();
	TArray<APawn*> Voitures() const;
	bool EstConduite(const APawn* Voiture) const;
	bool EstPosee(const AActor* Voiture) const;

	// ---- Pose par l'administrateur (serveur)
	UClass* ClasseAPoser();
	APawn* PoserVoiture(const FVector& Lieu, float Yaw, bool bSauver);
	bool RetirerVoitureProche(const FVector& Point, FString& OutRaison);
	void ChargerPositions();
	void SauverPositions() const;
	bool ViserLeSol(APlayerController* PC, FVector& OutPoint, float& OutYaw) const;

	// ---- Monter / descendre
	APawn* VoitureLibreProche(const APawn* Pieton, float Rayon) const;
	void MonterSolo(APawn* Voiture);
	void SuivreMonteeSolo();
	void MonterServeur(ARelaisPolice* Relais, APawn* Voiture);
	void DescendreServeur(ARelaisPolice* Relais);
	void SuivreMonteesServeur();
	AActor* ControleurMotoneigePolice(bool bCreer);
	static FVector PlaceADescendre(const APawn* Voiture, const APawn* Pieton);

	// ---- Conduite du joueur local
	void ConduireLocal(float DeltaSeconds);
	void InstallerCamera(APawn* Voiture);
	void RetirerCamera();
	void ToucherSirene();

	// ---- Sirene / gyrophares (sur chaque machine)
	bool SireneVoulue(const APawn* Voiture) const;
	void MettreAJourSirenes();
	static void AppelerFonctionPack(AActor* Acteur, const TCHAR* Nom, bool bValeur);

	// ---- Route degagee (circulation locale a chaque machine)
	void DegagerLaRoute();

	// ---- Aide a la conduite (repris de l'ancien plugin ChellesConduite du SUV)
	void PreparerVoiture(APawn* Voiture);
	void RegleRoue(UClass* ClasseRoue, bool bAvant);
	void RemettreRoues();
	void Aider(APawn* Voiture, float Dt);

public:
	static void ArreterVoiture(APawn* Voiture);
	static void CacherPion(APawn* Pion, bool bCacher);

private:
	// ---- Reglages (Police.txt)
	TArray<FString> Classes;
	FString CheminClassePosee;
	FKey TouchePoser;
	FKey ToucheRetirer;
	FKey ToucheSirene;
	FKey ToucheMonter;
	TArray<FString> Administrateurs;
	int32 MaxVoitures = 30;
	float DistanceMonter = 450.f;      // cm
	bool bSireneAuto = true;
	bool bDegager = true;
	float VitesseDegager = 10.f;       // km/h
	float DistanceDegager = 7000.f;    // cm
	float LargeurDegager = 450.f;      // cm
	float DureeRangement = 3.f;        // s
	float DistanceCamera = 650.f;      // cm
	float HauteurCamera = 170.f;       // cm
	bool bAide = true;
	float Adherence = 10.f;
	float VirageMaxDeg = 70.f;
	float RayonMini = 6.f;
	float AccelLaterale = 9.f;
	float AntiRoulis = 6.f;
	float Appui = 0.4f;
	float AbaisserCentre = -30.f;
	float Friction = 1.6f;
	float Rigidite = 1.3f;
	float Braquage = 40.f;
	float Amortissement = 0.6f;

	// ---- Etat
	UPROPERTY(Transient)
	UClass* ClasseCirculation = nullptr;

	UPROPERTY(Transient)
	UClass* ClassePosee = nullptr;

	UPROPERTY(Transient)
	UClass* ClasseMotoneige = nullptr;

	/** Voitures de police connues (posees, de la roue, ou mises dans la carte). */
	TArray<TWeakObjectPtr<APawn>> Suivies;
	TSet<TWeakObjectPtr<APawn>> Preparees;

	/** Serveur : voitures posees par l'administrateur (sauvees dans Police_Positions.txt). */
	TArray<TWeakObjectPtr<APawn>> Posees;
	/** Serveur : ou chaque voiture posee a ete mise (meme index que Posees). */
	TArray<FTransform> PlacesPosees;

	/** Sirene allumee en ce moment sur cette machine (par voiture). */
	TMap<TWeakObjectPtr<APawn>, bool> SirenesAllumees;

	/** Solo : sirene voulue par le joueur (touche L). */
	bool bSireneSolo = true;

	/** Solo : montee faite en empruntant l'acteur Motoneige de la case POLICE de la roue. */
	TWeakObjectPtr<AActor> MotoneigeEmpruntee;
	TWeakObjectPtr<APawn> MotoAvantEmprunt;
	bool bEmprunt = false;

	/** Camera installee sur la voiture conduite par le joueur local. */
	TWeakObjectPtr<APawn> VoitureCamera;
	TWeakObjectPtr<USpringArmComponent> BrasCamera;
	TWeakObjectPtr<UCameraComponent> Camera;
	float TempsSansSouris = 0.f;
	bool bAideAffichee = false;
	bool bAvisAdmin = false;

	UPROPERTY()
	TArray<UObject*> Pneus;

	UPROPERTY()
	TArray<FRouePoliceOrigine> Origines;

	float ProchaineRecherche = 0.f;
	float ProchainDegagement = 0.f;
	float ProchainRelais = 0.f;
	bool bPositionsChargees = false;
	float AttenteChargement = 1.5f;
};
