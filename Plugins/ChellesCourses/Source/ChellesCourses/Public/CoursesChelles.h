// CHELLES 3D - COURSES DE RUE CHRONOMETREES (ajoutees toutes seules au lancement du jeu)
//
//  - Des departs lumineux (colonnes cyan) sont poses dans les rues du quartier.
//  - Sur un depart, touche Debut (Home) : compte a rebours 3, 2, 1, PARTEZ !
//  - Passez les portes dans l'ordre (a pied, en voiture, en moto...), jusqu'a l'ARRIVEE.
//  - Chrono, ecart avec votre record a chaque porte, record de ce PC, classement de tous les joueurs (VPS).
//  - Ailleurs, la touche Debut ouvre l'ecran COURSES (records, classement, montrer un depart).
//
// Les parcours sont fabriques tout seuls a partir des rues de la circulation (ChellesCirculation, lue par
// le NOM de ses variables Noeuds / Troncons, sans la modifier), puis enregistres dans Courses_Parcours.txt :
// ils sont identiques sur tous les PC, donc le classement compare les memes courses.
//
// Chaque PC fait SA course (rien ne passe par le serveur de jeu) : ca marche en solo comme en multijoueur.
// Reglages : Plugins\ChellesCourses\Courses.txt. Console : Chelles.Course 1 (montrer le depart),
// Chelles.CourseFin, Chelles.CoursesRefaire, Chelles.CourseCreer NOM / Chelles.CourseCreerFin, Chelles.Courses.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "CoursesChelles.generated.h"

class APawn;
class APlayerController;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class SCoursesEcran;
class SCoursesMenu;

/** Une course fabriquee depuis les rues, voulue dans Courses.txt (Course=NOM|longueur). */
struct FCourseVoulue
{
	FString Nom;
	float LongueurCm = 100000.f;
};

/** Reglages lus dans Plugins\ChellesCourses\Courses.txt (Bloc-notes, sans recompiler). */
struct FReglagesCourses
{
	bool bActif = true;
	FKey Touche = EKeys::Home;
	TArray<FCourseVoulue> Voulues;
	float EcartPortes = 12000.f;
	float RayonPorte = 1200.f;
	float RayonDepart = 1000.f;
	float EcartDeparts = 20000.f;
	int32 CompteARebours = 3;
	float TempsMax = 900.f;
	int32 XPArrivee = 50;
	int32 XPRecord = 50;
	bool bClassementEnLigne = true;
	FString AdresseClassement = TEXT("http://51.178.54.208:8080");
};

/** Un parcours : Points[0] = depart, puis les portes, le dernier point = ARRIVEE. */
struct FParcoursCourse
{
	FString Nom;
	TArray<FVector> Points;
	float LongueurCm = 0.f;
	/** Nom + empreinte des points : le classement du VPS separe les courses par cet identifiant. */
	FString Id;
	bool bFaiteMain = false;

	int32 NombrePortes() const { return FMath::Max(0, Points.Num() - 1); }
	void CalculerId();
};

/** Meilleur temps de ce PC sur une course, avec le temps a chaque porte (pour l'ecart en direct). */
struct FRecordCourse
{
	int32 TempsMs = 0;
	FString Date;
	TArray<int32> Passages;
};

namespace EEtatCourse
{
	enum Type : uint8
	{
		Libre = 0,      // pas de course, departs visibles
		Depart = 1,     // compte a rebours
		EnCourse = 2,
		Arrivee = 3,    // ecran d'arrivee
		Abandon = 4
	};
}

/**
 * Un par machine qui affiche le jeu (pas sur le serveur dedie), cree au lancement par le module.
 * Pas repliquee : chaque joueur court pour lui, le classement commun est sur le VPS.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class CHELLESCOURSES_API ACoursesChelles : public AActor
{
	GENERATED_BODY()

public:
	ACoursesChelles();

	static const FReglagesCourses& Reglages();

	// ---- Console ----
	void MontrerDepart(int32 Index);
	void Abandonner(const FString& Raison);
	void Refaire();
	void CommencerEnregistrement(const FString& Nom);
	void FinirEnregistrement();
	void DecrireEtat() const;
	void OuvrirMenu();
	void FermerMenu();
	void MessageLocal(const FString& Texte);

	const TArray<FParcoursCourse>& Parcours() const { return ListeParcours; }
	const FRecordCourse* Record(const FString& Id) const { return Records.Find(Id); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// ---- Parcours ----
	void ChargerParcours();
	void SauverParcours() const;
	/** Fabrique les courses de Courses.txt depuis les rues de la circulation. Rend faux si les rues ne sont pas pretes. */
	bool FabriquerDepuisLesRues(FString& Pourquoi);
	void ChargerRecords();
	void SauverRecords() const;

	// ---- Course ----
	void Lancer(int32 Index);
	void TickCourse(float DeltaSeconds, APlayerController* PC, APawn* Pion);
	void Arriver();
	int32 DepartProche(const APawn* Pion) const;
	bool MissionEnCours() const;
	FString ModeDuJoueur(const APawn* Pion) const;

	// ---- Affichage ----
	void CreerEcran();
	void RetirerEcran();
	void MettreAJourDeparts(float DeltaSeconds);
	void MettreAJourPorte(float DeltaSeconds);
	void RemplirEcran(APlayerController* PC, APawn* Pion);
	UStaticMeshComponent* NouvelleForme(const TCHAR* Forme, const FLinearColor& Couleur, UMaterialInstanceDynamic** Mat);
	void CacherPorte();

	TArray<FParcoursCourse> ListeParcours;
	TMap<FString, FRecordCourse> Records;
	bool bParcoursPrets = false;
	float ProchainEssaiRues = 0.f;
	float DebutEssaisRues = -1.f;

	// Course en cours
	uint8 Etat = EEtatCourse::Libre;
	int32 Course = INDEX_NONE;
	int32 Prochaine = 1;          // index dans Points de la prochaine porte
	double HeureDepart = 0.0;     // FPlatformTime au PARTEZ
	float CompteRestant = 0.f;
	float AgeEtat = 0.f;
	TArray<int32> Passages;       // temps (ms) a chaque porte franchie
	int32 TempsFinalMs = 0;
	int32 AncienRecordMs = 0;
	bool bNouveauRecord = false;
	int32 XPGagnee = 0;
	FString ModeCourse;
	int32 EcartMs = 0;            // ecart avec le record a la derniere porte
	float AgeEcart = 100.f;
	bool bEcartConnu = false;
	float SansPion = 0.f;
	FString RaisonAbandon;

	// GPS vers un depart (ecran COURSES / Chelles.Course N)
	int32 DepartMontre = INDEX_NONE;

	// Enregistrement d'un parcours fait main
	bool bEnregistre = false;
	FParcoursCourse Enregistrement;

	// Ecran
	TSharedPtr<SCoursesEcran> Ecran;
	TSharedPtr<SCoursesMenu> Menu;
	FString Message;
	float AgeMessage = 100.f;
	int32 DepartIci = INDEX_NONE;
	float Phase = 0.f;

	// Colonnes des departs
	UPROPERTY(Transient)
	TArray<UStaticMeshComponent*> Colonnes;

	UPROPERTY(Transient)
	TArray<UPointLightComponent*> Lumieres;

	// La prochaine porte : deux poteaux, une barre, une colonne visible de loin
	UPROPERTY(Transient)
	UStaticMeshComponent* PoteauGauche = nullptr;

	UPROPERTY(Transient)
	UStaticMeshComponent* PoteauDroit = nullptr;

	UPROPERTY(Transient)
	UStaticMeshComponent* Barre = nullptr;

	UPROPERTY(Transient)
	UStaticMeshComponent* Colonne = nullptr;

	UPROPERTY(Transient)
	UPointLightComponent* LumierePorte = nullptr;

	UPROPERTY(Transient)
	TArray<UMaterialInstanceDynamic*> MatsPorte;
};
