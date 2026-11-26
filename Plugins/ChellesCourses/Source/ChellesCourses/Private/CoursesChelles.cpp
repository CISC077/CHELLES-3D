// CHELLES 3D - COURSES DE RUE : gestionnaire (voir CoursesChelles.h)
//
// Aucun asset ni Blueprint du projet n'est modifie, aucun autre plugin non plus :
//  - les rues viennent du Circulation Spawner (ChellesCirculation), seulement LU par le nom de ses variables ;
//  - pas de course pendant une mission (ChellesMissions, variable Phase lue par son nom) ;
//  - l'XP est ajoutee a XPLocale de ChellesPouvoirs, comme le font les Missions et le Dernier Refuge.
#include "CoursesChelles.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#include "ClassementCourses.h"
#include "CoursesRues.h"
#include "SCoursesEcran.h"

namespace
{
	const FLinearColor CouleurDepart(0.f, 0.85f, 1.f);
	const FLinearColor CouleurPorte(1.f, 0.75f, 0.05f);
	const FLinearColor CouleurArrivee(1.f, 0.1f, 0.75f);
	const float AffichageFin = 8.f;

	FString DossierPlugin()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesCourses"));
	}

	/** Un fichier de parcours par carte (Courses_Parcours_Rachid.txt) : rien dans le lobby ni sur les autres cartes. */
	FString FichierParcours(const UWorld* Monde)
	{
		const FString Carte = Monde ? UWorld::RemovePIEPrefix(Monde->GetMapName()) : FString(TEXT("Carte"));
		return FPaths::Combine(DossierPlugin(), FString::Printf(TEXT("Courses_Parcours_%s.txt"), *Carte));
	}

	/** Les records sont a cote des reglages : le dossier Saved est parfois vide sur le PC. */
	FString FichierRecords()
	{
		return FPaths::Combine(DossierPlugin(), TEXT("Records_Courses.txt"));
	}

	FString FichierRecordsSecours()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ChellesCourses"), TEXT("Records_Courses.txt"));
	}

	float Nombre(const FString& Texte)
	{
		return FCString::Atof(*Texte.Replace(TEXT(","), TEXT(".")));
	}

	bool Oui(const FString& Texte)
	{
		return Texte.StartsWith(TEXT("O"), ESearchCase::IgnoreCase) || Texte.StartsWith(TEXT("Y"), ESearchCase::IgnoreCase)
			|| Texte == TEXT("1") || Texte.Equals(TEXT("vrai"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	FString NomPropre(const FString& Brut)
	{
		FString N;
		for (const TCHAR C : Brut)
		{
			if (C >= 32 && C != TEXT('|') && C != TEXT(';') && C != TEXT('='))
			{
				N.AppendChar(C);
			}
		}
		N = N.TrimStartAndEnd().Left(40);
		return N.IsEmpty() ? FString(TEXT("COURSE")) : N.ToUpper();
	}

	FReglagesCourses LireReglages()
	{
		FReglagesCourses R;
		TArray<FString> Lignes;
		const FString Chemin = FPaths::Combine(DossierPlugin(), TEXT("Courses.txt"));
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			UE_LOG(LogTemp, Log, TEXT("[Courses] %s absent : reglages par defaut."), *Chemin);
		}
		for (FString Ligne : Lignes)
		{
			Ligne.TrimStartAndEndInline();
			FString Cle, Valeur;
			if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Cle, &Valeur))
			{
				continue;
			}
			Cle.TrimStartAndEndInline();
			Valeur.TrimStartAndEndInline();
			const float V = Nombre(Valeur);
			const int32 I = FMath::RoundToInt(V);
			const float M = V * 100.f; // metres -> cm

			if (Cle == TEXT("Actif")) { R.bActif = Oui(Valeur); }
			else if (Cle == TEXT("Touche")) { const FKey K(*Valeur); if (K.IsValid()) { R.Touche = K; } }
			else if (Cle == TEXT("Course"))
			{
				FString Nom, Longueur;
				if (Valeur.Split(TEXT("|"), &Nom, &Longueur))
				{
					FCourseVoulue C;
					C.Nom = NomPropre(Nom);
					C.LongueurCm = FMath::Clamp(Nombre(Longueur), 300.f, 20000.f) * 100.f;
					R.Voulues.Add(C);
				}
			}
			else if (Cle == TEXT("EcartPortesMetres")) { R.EcartPortes = FMath::Clamp(M, 3000.f, 100000.f); }
			else if (Cle == TEXT("RayonPorteMetres")) { R.RayonPorte = FMath::Clamp(M, 300.f, 5000.f); }
			else if (Cle == TEXT("RayonDepartMetres")) { R.RayonDepart = FMath::Clamp(M, 300.f, 5000.f); }
			else if (Cle == TEXT("EcartDepartsMetres")) { R.EcartDeparts = FMath::Max(0.f, M); }
			else if (Cle == TEXT("CompteARebours")) { R.CompteARebours = FMath::Clamp(I, 0, 10); }
			else if (Cle == TEXT("TempsMaxMinutes")) { R.TempsMax = FMath::Max(1.f, V) * 60.f; }
			else if (Cle == TEXT("XPArrivee")) { R.XPArrivee = FMath::Max(0, I); }
			else if (Cle == TEXT("XPRecord")) { R.XPRecord = FMath::Max(0, I); }
			else if (Cle == TEXT("ClassementEnLigne")) { R.bClassementEnLigne = Oui(Valeur); }
			else if (Cle == TEXT("AdresseClassement") && Valeur.StartsWith(TEXT("http"))) { R.AdresseClassement = Valeur; }
		}
		if (R.Voulues.Num() == 0)
		{
			FCourseVoulue A; A.Nom = TEXT("SPRINT DU QUARTIER"); A.LongueurCm = 100000.f; R.Voulues.Add(A);
			FCourseVoulue B; B.Nom = TEXT("TOUR DE CHELLES"); B.LongueurCm = 200000.f; R.Voulues.Add(B);
			FCourseVoulue C; C.Nom = TEXT("GRANDE COURSE"); C.LongueurCm = 350000.f; R.Voulues.Add(C);
		}
		FClassementCourses::Get().Configurer(R.bClassementEnLigne, R.AdresseClassement);
		UE_LOG(LogTemp, Log, TEXT("[Courses] Reglages lus (%s). Touche %s, %d course(s) voulue(s)."),
			R.bActif ? TEXT("actif") : TEXT("Actif=Non"), *R.Touche.ToString(), R.Voulues.Num());
		return R;
	}

	FString NomTouche(const FKey& Touche)
	{
		if (Touche == EKeys::Home)
		{
			return TEXT("DÉBUT");
		}
		return Touche.GetDisplayName().ToString().ToUpper();
	}

	FString Km(float Cm)
	{
		return FString::Printf(TEXT("%.1f km"), Cm / 100000.f).Replace(TEXT("."), TEXT(","));
	}

	/** Lecture d'un nombre d'un autre plugin par son nom (ex. Phase de MissionsChelles, XPLocale de PouvoirsChelles). */
	FNumericProperty* ProprieteNombre(const UObject* Objet, const TCHAR* Nom)
	{
		if (!Objet)
		{
			return nullptr;
		}
		FProperty* P = FindFProperty<FProperty>(Objet->GetClass(), Nom);
		if (FEnumProperty* E = CastField<FEnumProperty>(P))
		{
			return E->GetUnderlyingProperty();
		}
		return CastField<FNumericProperty>(P);
	}

	AActor* ActeurDeClasse(UWorld* Monde, const TCHAR* NomClasse)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(Monde); It; ++It)
		{
			if (It->GetClass()->GetName() == NomClasse)
			{
				return *It;
			}
		}
		return nullptr;
	}
}

// ===================================================================================
//  Parcours
// ===================================================================================

void FParcoursCourse::CalculerId()
{
	// Seulement X et Y au metre pres : l'altitude mesuree peut varier un peu d'un PC a l'autre
	FString Empreinte;
	for (const FVector& P : Points)
	{
		Empreinte += FString::Printf(TEXT("%d,%d;"), FMath::RoundToInt(P.X / 100.f), FMath::RoundToInt(P.Y / 100.f));
	}
	FString Lisible;
	for (const TCHAR C : Nom)
	{
		Lisible.AppendChar(((C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('0') && C <= TEXT('9'))) ? C : TEXT('-'));
	}
	Id = FString::Printf(TEXT("%s-%08x"), *Lisible.Left(24), FCrc::StrCrc32(*Empreinte));
}

const FReglagesCourses& ACoursesChelles::Reglages()
{
	static const FReglagesCourses R = LireReglages();
	return R;
}

ACoursesChelles::ACoursesChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	bReplicates = false;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void ACoursesChelles::BeginPlay()
{
	Super::BeginPlay();
	Reglages();
	ChargerRecords();
	ChargerParcours();
}

void ACoursesChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	FermerMenu();
	RetirerEcran();
	Super::EndPlay(Raison);
}

void ACoursesChelles::ChargerParcours()
{
	ListeParcours.Reset();
	TArray<FString> Lignes;
	if (FFileHelper::LoadFileToStringArray(Lignes, *FichierParcours(GetWorld())))
	{
		for (FString Ligne : Lignes)
		{
			Ligne.TrimStartAndEndInline();
			if (!Ligne.StartsWith(TEXT("Course=")))
			{
				continue;
			}
			TArray<FString> Morceaux;
			Ligne.RightChop(7).ParseIntoArray(Morceaux, TEXT("|"), false);
			if (Morceaux.Num() < 4)
			{
				continue;
			}
			FParcoursCourse P;
			P.Nom = NomPropre(Morceaux[0]);
			P.bFaiteMain = Morceaux[2].TrimStartAndEnd().Equals(TEXT("main"), ESearchCase::IgnoreCase);
			TArray<FString> Points;
			Morceaux[3].ParseIntoArray(Points, TEXT(";"), true);
			for (const FString& Texte : Points)
			{
				TArray<FString> XYZ;
				Texte.ParseIntoArray(XYZ, TEXT(","), true);
				if (XYZ.Num() >= 3)
				{
					P.Points.Add(FVector(FCString::Atof(*XYZ[0]), FCString::Atof(*XYZ[1]), FCString::Atof(*XYZ[2])));
				}
			}
			if (P.Points.Num() < 3)
			{
				continue;
			}
			for (int32 i = 1; i < P.Points.Num(); ++i)
			{
				P.LongueurCm += FVector::Dist2D(P.Points[i - 1], P.Points[i]);
			}
			// Longueur des rues (plus juste que la ligne droite entre les portes)
			const float LongueurRues = Nombre(Morceaux[1]) * 100.f;
			if (LongueurRues > P.LongueurCm)
			{
				P.LongueurCm = LongueurRues;
			}
			P.CalculerId();
			ListeParcours.Add(P);
		}
	}
	bParcoursPrets = ListeParcours.Num() > 0;
	if (bParcoursPrets)
	{
		UE_LOG(LogTemp, Log, TEXT("[Courses] %d parcours lus dans %s."), ListeParcours.Num(), *FichierParcours(GetWorld()));
	}
}

void ACoursesChelles::SauverParcours() const
{
	FString Texte = TEXT("# CHELLES 3D - Parcours des COURSES DE RUE\r\n")
		TEXT("# Fabriques tout seuls a partir des rues (auto), ou enregistres en jouant avec Chelles.CourseCreer NOM (main).\r\n")
		TEXT("# Une ligne par course : Course=NOM|longueur en metres|auto ou main|x,y,z;x,y,z;... (le 1er point = depart, le dernier = arrivee)\r\n")
		TEXT("# Pour refaire les courses auto : effacez ce fichier, ou console Chelles.CoursesRefaire.\r\n");
	for (const FParcoursCourse& P : ListeParcours)
	{
		FString Points;
		for (const FVector& V : P.Points)
		{
			Points += FString::Printf(TEXT("%.0f,%.0f,%.0f;"), V.X, V.Y, V.Z);
		}
		Texte += FString::Printf(TEXT("Course=%s|%d|%s|%s\r\n"), *P.Nom, FMath::RoundToInt(P.LongueurCm / 100.f), P.bFaiteMain ? TEXT("main") : TEXT("auto"), *Points);
	}
	if (!FFileHelper::SaveStringToFile(Texte, *FichierParcours(GetWorld()), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Courses] Impossible d'ecrire %s"), *FichierParcours(GetWorld()));
	}
}

bool ACoursesChelles::FabriquerDepuisLesRues(FString& Pourquoi)
{
	FGrapheRues Graphe;
	if (!CoursesRues::Lire(GetWorld(), Graphe, Pourquoi))
	{
		return false;
	}
	const FReglagesCourses& R = Reglages();
	TArray<FVector> Departs;
	for (const FParcoursCourse& P : ListeParcours)
	{
		Departs.Add(P.Points[0]);
	}
	int32 Faites = 0;
	for (int32 k = 0; k < R.Voulues.Num(); ++k)
	{
		FParcoursCourse P;
		P.Nom = R.Voulues[k].Nom;
		if (!CoursesRues::Fabriquer(Graphe, 7700 + 131 * k, R.Voulues[k].LongueurCm, R.EcartPortes, Departs, R.EcartDeparts, P.Points, P.LongueurCm))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Courses] Pas de parcours trouve pour %s (%s) dans les rues."), *P.Nom, *Km(R.Voulues[k].LongueurCm));
			continue;
		}
		P.CalculerId();
		Departs.Add(P.Points[0]);
		ListeParcours.Add(P);
		++Faites;
		UE_LOG(LogTemp, Log, TEXT("[Courses] Parcours %s : %s, %d portes, depart %s."), *P.Nom, *Km(P.LongueurCm), P.NombrePortes(), *P.Points[0].ToString());
	}
	if (Faites == 0)
	{
		Pourquoi = TEXT("aucun parcours assez long dans les rues");
		return false;
	}
	SauverParcours();
	return true;
}

void ACoursesChelles::Refaire()
{
	if (Etat == EEtatCourse::Depart || Etat == EEtatCourse::EnCourse)
	{
		Abandonner(TEXT("Les courses sont refaites"));
	}
	ListeParcours.RemoveAll([](const FParcoursCourse& P) { return !P.bFaiteMain; });
	bParcoursPrets = false;
	ProchainEssaiRues = 0.f;
	DebutEssaisRues = -1.f;
	DepartMontre = INDEX_NONE;
	MessageLocal(TEXT("Les courses vont être refaites à partir des rues..."));
}

// ===================================================================================
//  Records de ce PC
// ===================================================================================

void ACoursesChelles::ChargerRecords()
{
	Records.Reset();
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *FichierRecords()))
	{
		FFileHelper::LoadFileToStringArray(Lignes, *FichierRecordsSecours());
	}
	for (const FString& Ligne : Lignes)
	{
		if (Ligne.StartsWith(TEXT("#")))
		{
			continue;
		}
		TArray<FString> Morceaux;
		Ligne.ParseIntoArray(Morceaux, TEXT("|"), false);
		if (Morceaux.Num() < 3)
		{
			continue;
		}
		FRecordCourse Rec;
		Rec.TempsMs = FCString::Atoi(*Morceaux[1]);
		Rec.Date = Morceaux[2];
		if (Morceaux.Num() >= 4)
		{
			TArray<FString> Passages;
			Morceaux[3].ParseIntoArray(Passages, TEXT(","), true);
			for (const FString& T : Passages)
			{
				Rec.Passages.Add(FCString::Atoi(*T));
			}
		}
		if (Rec.TempsMs > 0)
		{
			Records.Add(Morceaux[0].TrimStartAndEnd(), Rec);
		}
	}
}

void ACoursesChelles::SauverRecords() const
{
	FString Texte = TEXT("# CHELLES 3D - COURSES DE RUE : records de ce PC\r\n# Course|Temps (ms)|Date|temps a chaque porte\r\n");
	for (const TPair<FString, FRecordCourse>& Paire : Records)
	{
		FString Passages;
		for (const int32 T : Paire.Value.Passages)
		{
			Passages += FString::Printf(TEXT("%d,"), T);
		}
		Texte += FString::Printf(TEXT("%s|%d|%s|%s\r\n"), *Paire.Key, Paire.Value.TempsMs, *Paire.Value.Date, *Passages);
	}
	if (!FFileHelper::SaveStringToFile(Texte, *FichierRecords(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		FFileHelper::SaveStringToFile(Texte, *FichierRecordsSecours(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

// ===================================================================================
//  Course
// ===================================================================================

bool ACoursesChelles::MissionEnCours() const
{
	AActor* Missions = ActeurDeClasse(GetWorld(), TEXT("MissionsChelles"));
	FNumericProperty* P = ProprieteNombre(Missions, TEXT("Phase"));
	if (!P)
	{
		return false;
	}
	const int64 Phase = P->GetSignedIntPropertyValue(P->ContainerPtrToValuePtr<void>(Missions));
	return Phase == 1 || Phase == 2; // rejoindre / en cours
}

FString ACoursesChelles::ModeDuJoueur(const APawn* Pion) const
{
	if (!Pion)
	{
		return TEXT("?");
	}
	return Pion->IsA<ACharacter>() ? TEXT("a pied") : TEXT("vehicule");
}

int32 ACoursesChelles::DepartProche(const APawn* Pion) const
{
	if (!Pion)
	{
		return INDEX_NONE;
	}
	const FVector Ici = Pion->GetActorLocation();
	int32 Meilleur = INDEX_NONE;
	float Distance = Reglages().RayonDepart;
	for (int32 i = 0; i < ListeParcours.Num(); ++i)
	{
		const FVector& D = ListeParcours[i].Points[0];
		const float Dist = FVector::Dist2D(Ici, D);
		if (Dist < Distance && FMath::Abs(Ici.Z - D.Z) < 1500.f)
		{
			Distance = Dist;
			Meilleur = i;
		}
	}
	return Meilleur;
}

void ACoursesChelles::Lancer(int32 Index)
{
	if (!ListeParcours.IsValidIndex(Index))
	{
		return;
	}
	if (MissionEnCours())
	{
		MessageLocal(TEXT("Une mission est en cours : terminez-la avant de faire une course."));
		return;
	}
	const FReglagesCourses& R = Reglages();
	Course = Index;
	Prochaine = 1;
	Passages.Reset();
	bEcartConnu = false;
	AgeEcart = 100.f;
	SansPion = 0.f;
	DepartMontre = INDEX_NONE;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ModeCourse = ModeDuJoueur(PC ? PC->GetPawn() : nullptr);
	FClassementCourses::Get().Rafraichir(ListeParcours[Index].Id);
	AgeEtat = 0.f;
	if (R.CompteARebours > 0)
	{
		Etat = EEtatCourse::Depart;
		CompteRestant = (float)R.CompteARebours;
	}
	else
	{
		Etat = EEtatCourse::EnCourse;
		HeureDepart = FPlatformTime::Seconds();
	}
	UE_LOG(LogTemp, Log, TEXT("[Courses] Depart de %s (%s)."), *ListeParcours[Index].Nom, *ModeCourse);
}

void ACoursesChelles::Abandonner(const FString& Raison)
{
	if (Etat != EEtatCourse::Depart && Etat != EEtatCourse::EnCourse)
	{
		return;
	}
	Etat = EEtatCourse::Abandon;
	AgeEtat = 0.f;
	RaisonAbandon = Raison;
	UE_LOG(LogTemp, Log, TEXT("[Courses] Course abandonnee : %s"), *Raison);
}

void ACoursesChelles::TickCourse(float DeltaSeconds, APlayerController* PC, APawn* Pion)
{
	const FReglagesCourses& R = Reglages();
	if (!ListeParcours.IsValidIndex(Course))
	{
		Etat = EEtatCourse::Libre;
		return;
	}
	const FParcoursCourse& P = ListeParcours[Course];

	if (Etat == EEtatCourse::Depart)
	{
		if (!Pion || FVector::Dist2D(Pion->GetActorLocation(), P.Points[0]) > R.RayonDepart * 2.5f)
		{
			Etat = EEtatCourse::Libre;
			MessageLocal(TEXT("Faux départ : restez sur le départ pendant le compte à rebours."));
			return;
		}
		CompteRestant -= DeltaSeconds;
		if (CompteRestant <= 0.f)
		{
			Etat = EEtatCourse::EnCourse;
			AgeEtat = 0.f;
			HeureDepart = FPlatformTime::Seconds();
		}
		return;
	}

	if (Etat != EEtatCourse::EnCourse)
	{
		return;
	}
	const double Secondes = FPlatformTime::Seconds() - HeureDepart;
	if (!Pion)
	{
		SansPion += DeltaSeconds;
		if (SansPion > 5.f)
		{
			Abandonner(TEXT("Vous n'êtes plus dans la partie."));
		}
		return;
	}
	SansPion = 0.f;
	if (Secondes > R.TempsMax)
	{
		Abandonner(FString::Printf(TEXT("Plus de %d minutes : la course est arrêtée."), FMath::RoundToInt(R.TempsMax / 60.f)));
		return;
	}

	const FVector Ici = Pion->GetActorLocation();
	const FVector& Porte = P.Points[Prochaine];
	if (FVector::Dist2D(Ici, Porte) < R.RayonPorte && FMath::Abs(Ici.Z - Porte.Z) < 2500.f)
	{
		const int32 Ms = FMath::RoundToInt((float)(Secondes * 1000.0));
		Passages.Add(Ms);
		if (const FRecordCourse* Rec = Records.Find(P.Id))
		{
			if (Rec->Passages.IsValidIndex(Passages.Num() - 1))
			{
				EcartMs = Ms - Rec->Passages[Passages.Num() - 1];
				bEcartConnu = true;
				AgeEcart = 0.f;
			}
		}
		++Prochaine;
		if (Prochaine >= P.Points.Num())
		{
			Arriver();
		}
	}
}

void ACoursesChelles::Arriver()
{
	const FReglagesCourses& R = Reglages();
	const FParcoursCourse& P = ListeParcours[Course];
	TempsFinalMs = Passages.Num() > 0 ? Passages.Last() : 0;
	const FRecordCourse* Ancien = Records.Find(P.Id);
	AncienRecordMs = Ancien ? Ancien->TempsMs : 0;
	bNouveauRecord = !Ancien || TempsFinalMs < Ancien->TempsMs;
	if (bNouveauRecord)
	{
		FRecordCourse Nouveau;
		Nouveau.TempsMs = TempsFinalMs;
		Nouveau.Date = FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
		Nouveau.Passages = Passages;
		Records.Add(P.Id, Nouveau);
		SauverRecords();
	}

	XPGagnee = R.XPArrivee + (bNouveauRecord ? R.XPRecord : 0);
	if (XPGagnee > 0)
	{
		AActor* Pouvoirs = ActeurDeClasse(GetWorld(), TEXT("PouvoirsChelles"));
		if (FNumericProperty* XP = ProprieteNombre(Pouvoirs, TEXT("XPLocale")))
		{
			void* Adresse = XP->ContainerPtrToValuePtr<void>(Pouvoirs);
			if (XP->IsFloatingPoint())
			{
				XP->SetFloatingPointPropertyValue(Adresse, XP->GetFloatingPointPropertyValue(Adresse) + XPGagnee);
			}
			else
			{
				XP->SetIntPropertyValue(Adresse, XP->GetSignedIntPropertyValue(Adresse) + (int64)XPGagnee);
			}
			UE_LOG(LogTemp, Log, TEXT("[Courses] +%d XP (pouvoirs)."), XPGagnee);
		}
		else
		{
			XPGagnee = 0; // pas de pouvoirs dans la partie : on n'affiche pas d'XP
		}
	}

	FClassementCourses::Get().Envoyer(P.Id, P.Nom, TempsFinalMs, P.NombrePortes(), P.LongueurCm / 100.f, ModeCourse);
	Etat = EEtatCourse::Arrivee;
	AgeEtat = 0.f;
	UE_LOG(LogTemp, Log, TEXT("[Courses] ARRIVEE %s en %s%s."), *P.Nom, *FClassementCourses::Temps(TempsFinalMs), bNouveauRecord ? TEXT(" (nouveau record)") : TEXT(""));
}

void ACoursesChelles::MontrerDepart(int32 Index)
{
	if (!ListeParcours.IsValidIndex(Index))
	{
		MessageLocal(FString::Printf(TEXT("Pas de course n°%d (il y en a %d)."), Index + 1, ListeParcours.Num()));
		return;
	}
	DepartMontre = Index;
	MessageLocal(FString::Printf(TEXT("Suivez le losange cyan jusqu'au départ de %s."), *ListeParcours[Index].Nom));
}

// ===================================================================================
//  Parcours fait main : Chelles.CourseCreer NOM, roulez, Chelles.CourseCreerFin
// ===================================================================================

void ACoursesChelles::CommencerEnregistrement(const FString& Nom)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion)
	{
		MessageLocal(TEXT("Entrez dans la partie avant d'enregistrer une course."));
		return;
	}
	Enregistrement = FParcoursCourse();
	Enregistrement.Nom = NomPropre(Nom.IsEmpty() ? FString::Printf(TEXT("COURSE %d"), ListeParcours.Num() + 1) : Nom);
	Enregistrement.bFaiteMain = true;
	Enregistrement.Points.Add(Pion->GetActorLocation());
	bEnregistre = true;
	MessageLocal(FString::Printf(TEXT("Enregistrement de %s : faites le parcours, puis Chelles.CourseCreerFin à l'arrivée."), *Enregistrement.Nom));
}

void ACoursesChelles::FinirEnregistrement()
{
	if (!bEnregistre)
	{
		MessageLocal(TEXT("Aucun enregistrement en cours (Chelles.CourseCreer NOM pour commencer)."));
		return;
	}
	bEnregistre = false;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (Pion && FVector::Dist2D(Pion->GetActorLocation(), Enregistrement.Points.Last()) > 3000.f)
	{
		Enregistrement.Points.Add(Pion->GetActorLocation());
	}
	if (Enregistrement.Points.Num() < 3)
	{
		MessageLocal(TEXT("Parcours trop court (il faut au moins 2 portes) : rien n'est enregistré."));
		return;
	}
	Enregistrement.LongueurCm = 0.f;
	for (int32 i = 1; i < Enregistrement.Points.Num(); ++i)
	{
		Enregistrement.LongueurCm += FVector::Dist2D(Enregistrement.Points[i - 1], Enregistrement.Points[i]);
	}
	Enregistrement.CalculerId();
	ListeParcours.Add(Enregistrement);
	bParcoursPrets = true;
	SauverParcours();
	MessageLocal(FString::Printf(TEXT("Course %s enregistrée : %s, %d portes."), *Enregistrement.Nom, *Km(Enregistrement.LongueurCm), Enregistrement.NombrePortes()));
}

// ===================================================================================
//  Chaque image
// ===================================================================================

void ACoursesChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FReglagesCourses& R = Reglages();
	if (!R.bActif)
	{
		return;
	}
	UWorld* Monde = GetWorld();
	const float Heure = Monde->GetTimeSeconds();
	AgeMessage += DeltaSeconds;
	AgeEtat += DeltaSeconds;
	AgeEcart += DeltaSeconds;
	Phase += DeltaSeconds;

	// ---- Parcours fabriques depuis les rues (la circulation construit son reseau au lancement) ----
	if (!bParcoursPrets && Heure >= ProchainEssaiRues)
	{
		if (DebutEssaisRues < 0.f)
		{
			DebutEssaisRues = Heure;
		}
		FString Pourquoi;
		if (FabriquerDepuisLesRues(Pourquoi))
		{
			bParcoursPrets = true;
			MessageLocal(FString::Printf(TEXT("%d courses de rue dans le quartier : repérez les colonnes cyan !"), ListeParcours.Num()));
		}
		else if (Heure - DebutEssaisRues > 120.f)
		{
			bParcoursPrets = ListeParcours.Num() > 0; // les courses faites main restent
			ProchainEssaiRues = BIG_NUMBER;
			UE_LOG(LogTemp, Warning, TEXT("[Courses] Pas de courses fabriquees : %s. Enregistrez-en une avec Chelles.CourseCreer NOM."), *Pourquoi);
		}
		else
		{
			ProchainEssaiRues = Heure + 3.f;
		}
	}

	APlayerController* PC = Monde->GetFirstPlayerController();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	CreerEcran();

	// ---- Enregistrement d'un parcours fait main : une porte tous les EcartPortes ----
	if (bEnregistre && Pion && FVector::Dist2D(Pion->GetActorLocation(), Enregistrement.Points.Last()) >= R.EcartPortes)
	{
		Enregistrement.Points.Add(Pion->GetActorLocation());
		MessageLocal(FString::Printf(TEXT("Porte %d enregistrée"), Enregistrement.Points.Num() - 1));
	}

	// ---- Fin de l'ecran d'arrivee ----
	if ((Etat == EEtatCourse::Arrivee || Etat == EEtatCourse::Abandon) && AgeEtat > AffichageFin)
	{
		Etat = EEtatCourse::Libre;
	}
	const bool bLibre = Etat == EEtatCourse::Libre || Etat == EEtatCourse::Arrivee || Etat == EEtatCourse::Abandon;
	DepartIci = bLibre && !bEnregistre ? DepartProche(Pion) : INDEX_NONE;

	// ---- Touche ----
	if (PC && !Menu.IsValid() && PC->WasInputKeyJustPressed(R.Touche))
	{
		if (!bLibre)
		{
			Abandonner(FString::Printf(TEXT("Touche %s : course abandonnée."), *NomTouche(R.Touche)));
		}
		else if (DepartIci != INDEX_NONE)
		{
			Lancer(DepartIci);
		}
		else if (ListeParcours.Num() == 0 && !bEnregistre)
		{
			MessageLocal(bParcoursPrets || ProchainEssaiRues >= BIG_NUMBER
				? FString(TEXT("Pas de course de rue sur cette carte (console : Chelles.CourseCreer NOM pour en faire une)."))
				: FString(TEXT("Les courses de rue se pr\u00E9parent, r\u00E9essayez dans quelques secondes.")));
		}
		else
		{
			OuvrirMenu();
		}
	}

	if (!bLibre)
	{
		TickCourse(DeltaSeconds, PC, Pion);
	}
	if (DepartMontre != INDEX_NONE && DepartIci == DepartMontre)
	{
		DepartMontre = INDEX_NONE; // arrive au depart montre
	}

	MettreAJourDeparts(DeltaSeconds);
	MettreAJourPorte(DeltaSeconds);
	RemplirEcran(PC, Pion);
}

// ===================================================================================
//  Affichage : colonnes des departs, porte suivante
// ===================================================================================

UStaticMeshComponent* ACoursesChelles::NouvelleForme(const TCHAR* Forme, const FLinearColor& Couleur, UMaterialInstanceDynamic** Mat)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Forme));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetupAttachment(RootComponent);
	C->SetUsingAbsoluteLocation(true);
	C->SetUsingAbsoluteRotation(true);
	C->SetUsingAbsoluteScale(true);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Color"), Couleur);
		C->SetMaterial(0, M);
		if (Mat)
		{
			*Mat = M;
		}
	}
	C->RegisterComponent();
	return C;
}

void ACoursesChelles::MettreAJourDeparts(float DeltaSeconds)
{
	const bool bVoir = Etat == EEtatCourse::Libre || Etat == EEtatCourse::Arrivee || Etat == EEtatCourse::Abandon;
	while (Colonnes.Num() < ListeParcours.Num())
	{
		Colonnes.Add(NouvelleForme(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), CouleurDepart, nullptr));
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetUsingAbsoluteLocation(true);
		L->SetLightColor(CouleurDepart);
		L->SetAttenuationRadius(1500.f);
		L->SetCastShadows(false);
		L->RegisterComponent();
		Lumieres.Add(L);
	}
	for (int32 i = 0; i < Colonnes.Num(); ++i)
	{
		const bool bExiste = ListeParcours.IsValidIndex(i);
		UStaticMeshComponent* Col = Colonnes[i];
		UPointLightComponent* L = Lumieres.IsValidIndex(i) ? Lumieres[i] : nullptr;
		if (!Col || !L)
		{
			continue;
		}
		if (!bExiste || !bVoir)
		{
			Col->SetVisibility(false);
			L->SetVisibility(false);
			continue;
		}
		const FVector Pied = ListeParcours[i].Points[0];
		const bool bMontre = i == DepartMontre;
		Col->SetWorldScale3D(FVector(bMontre ? 0.8f : 0.5f, bMontre ? 0.8f : 0.5f, 60.f)); // 60 m de haut
		Col->SetWorldLocation(Pied + FVector(0.f, 0.f, 3000.f));
		Col->SetVisibility(true);
		L->SetWorldLocation(Pied + FVector(0.f, 0.f, 400.f));
		L->SetIntensity((bMontre ? 9000.f : 5000.f) + 2500.f * FMath::Sin(Phase * 3.f + i));
		L->SetVisibility(true);
	}
}

void ACoursesChelles::CacherPorte()
{
	for (UStaticMeshComponent* C : { PoteauGauche, PoteauDroit, Barre, Colonne })
	{
		if (C)
		{
			C->SetVisibility(false);
		}
	}
	if (LumierePorte)
	{
		LumierePorte->SetVisibility(false);
	}
}

void ACoursesChelles::MettreAJourPorte(float DeltaSeconds)
{
	if ((Etat != EEtatCourse::Depart && Etat != EEtatCourse::EnCourse) || !ListeParcours.IsValidIndex(Course)
		|| !ListeParcours[Course].Points.IsValidIndex(Prochaine))
	{
		CacherPorte();
		return;
	}
	if (!PoteauGauche)
	{
		MatsPorte.SetNum(4);
		PoteauGauche = NouvelleForme(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), CouleurPorte, &MatsPorte[0]);
		PoteauDroit = NouvelleForme(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), CouleurPorte, &MatsPorte[1]);
		Barre = NouvelleForme(TEXT("/Engine/BasicShapes/Cube.Cube"), CouleurPorte, &MatsPorte[2]);
		Colonne = NouvelleForme(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), CouleurPorte, &MatsPorte[3]);
		LumierePorte = NewObject<UPointLightComponent>(this);
		LumierePorte->SetupAttachment(RootComponent);
		LumierePorte->SetUsingAbsoluteLocation(true);
		LumierePorte->SetAttenuationRadius(2000.f);
		LumierePorte->SetCastShadows(false);
		LumierePorte->RegisterComponent();
	}
	const FParcoursCourse& P = ListeParcours[Course];
	const FVector Porte = P.Points[Prochaine];
	const bool bArrivee = Prochaine == P.Points.Num() - 1;
	const FLinearColor Couleur = bArrivee ? CouleurArrivee : CouleurPorte;
	for (UMaterialInstanceDynamic* M : MatsPorte)
	{
		if (M)
		{
			M->SetVectorParameterValue(TEXT("Color"), Couleur);
		}
	}

	FVector Dir = (Porte - P.Points[Prochaine - 1]).GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector::ForwardVector;
	}
	const FVector Cote(-Dir.Y, Dir.X, 0.f);
	const float Rayon = Reglages().RayonPorte;
	const float Hauteur = 700.f;
	PoteauGauche->SetWorldScale3D(FVector(0.5f, 0.5f, Hauteur / 100.f));
	PoteauDroit->SetWorldScale3D(FVector(0.5f, 0.5f, Hauteur / 100.f));
	PoteauGauche->SetWorldLocation(Porte + Cote * Rayon + FVector(0.f, 0.f, Hauteur * 0.5f));
	PoteauDroit->SetWorldLocation(Porte - Cote * Rayon + FVector(0.f, 0.f, Hauteur * 0.5f));
	Barre->SetWorldRotation(Dir.Rotation());
	Barre->SetWorldScale3D(FVector(0.4f, Rayon * 2.f / 100.f, 0.6f));
	Barre->SetWorldLocation(Porte + FVector(0.f, 0.f, Hauteur));
	Colonne->SetWorldScale3D(FVector(0.3f, 0.3f, 60.f));
	Colonne->SetWorldLocation(Porte + FVector(0.f, 0.f, 3000.f));
	LumierePorte->SetLightColor(Couleur);
	LumierePorte->SetWorldLocation(Porte + FVector(0.f, 0.f, 400.f));
	LumierePorte->SetIntensity(7000.f + 3000.f * FMath::Sin(Phase * 5.f));
	for (UStaticMeshComponent* C : { PoteauGauche, PoteauDroit, Barre, Colonne })
	{
		C->SetVisibility(true);
	}
	LumierePorte->SetVisibility(true);
}

// ===================================================================================
//  Ecran
// ===================================================================================

void ACoursesChelles::MessageLocal(const FString& Texte)
{
	Message = Texte;
	AgeMessage = 0.f;
	UE_LOG(LogTemp, Log, TEXT("[Courses] %s"), *Texte);
}

void ACoursesChelles::CreerEcran()
{
	UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (Ecran.IsValid() || !Vue)
	{
		return;
	}
	Ecran = SNew(SCoursesEcran);
	// Couche 7 : au-dessus du HUD (3), du Dernier Refuge (5) et des Missions (6), sous les menus
	Vue->AddViewportWidgetContent(Ecran.ToSharedRef(), 7);
}

void ACoursesChelles::RetirerEcran()
{
	if (Ecran.IsValid())
	{
		if (UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Vue->RemoveViewportWidgetContent(Ecran.ToSharedRef());
		}
		Ecran.Reset();
	}
}

void ACoursesChelles::RemplirEcran(APlayerController* PC, APawn* Pion)
{
	if (!Ecran.IsValid())
	{
		return;
	}
	const FReglagesCourses& R = Reglages();
	FEtatEcranCourses& E = Ecran->Etat;
	E.Etat = Etat;
	E.AgeEtat = AgeEtat;
	E.Message = Message;
	E.AgeMessage = AgeMessage;
	E.Invitation.Reset();
	E.InvitationDetail.Reset();
	E.bRepere = false;

	const FParcoursCourse* P = ListeParcours.IsValidIndex(Course) ? &ListeParcours[Course] : nullptr;
	if (P && Etat != EEtatCourse::Libre)
	{
		const FRecordCourse* Rec = Records.Find(P->Id);
		E.Nom = P->Nom;
		E.Portes = P->NombrePortes();
		E.Porte = Passages.Num();
		E.Secondes = Etat == EEtatCourse::EnCourse ? (float)(FPlatformTime::Seconds() - HeureDepart) : 0.f;
		E.Compte = FMath::CeilToInt(CompteRestant);
		E.Record = Rec ? FString::Printf(TEXT("RECORD %s"), *FClassementCourses::Temps(Rec->TempsMs)) : FString();
		E.bEcart = bEcartConnu;
		E.EcartMs = EcartMs;
		E.AgeEcart = AgeEcart;
		E.RaisonAbandon = RaisonAbandon;
		if (Etat == EEtatCourse::Arrivee)
		{
			E.TempsFinal = FClassementCourses::Temps(TempsFinalMs);
			E.bNouveauRecord = bNouveauRecord;
			E.XP = XPGagnee;
			E.LigneRecord = AncienRecordMs > 0
				? FString::Printf(TEXT("Ancien record de ce PC : %s"), *FClassementCourses::Temps(AncienRecordMs))
				: FString(TEXT("Premier temps sur cette course !"));
			const FClassementUneCourse* C = FClassementCourses::Get().Course(P->Id);
			if (!FClassementCourses::Get().EnLigne())
			{
				E.LigneClassement = TEXT("Classement en ligne coupé (Courses.txt)");
			}
			else if (!C || !C->bReponseEnvoi)
			{
				E.LigneClassement = TEXT("Envoi au classement de tous les joueurs...");
			}
			else if (C->RangEnvoye > 0)
			{
				E.LigneClassement = FString::Printf(TEXT("Classement de tous les joueurs : %d%s sur %d"), C->RangEnvoye, C->RangEnvoye == 1 ? TEXT("er") : TEXT("e"), FMath::Max(C->Total, C->RangEnvoye));
			}
			else
			{
				E.LigneClassement = TEXT("Serveur injoignable : le temps reste sur ce PC");
			}
		}
	}

	// ---- Invitation sur un depart ----
	if (DepartIci != INDEX_NONE && (Etat == EEtatCourse::Libre || AgeEtat > 3.f))
	{
		const FParcoursCourse& Ici = ListeParcours[DepartIci];
		FClassementCourses::Get().Rafraichir(Ici.Id);
		E.Invitation = FString::Printf(TEXT("%s  ·  appuyez sur %s pour partir"), *Ici.Nom, *NomTouche(R.Touche));
		FString Detail = FString::Printf(TEXT("%s · %d portes"), *Km(Ici.LongueurCm), Ici.NombrePortes());
		if (const FRecordCourse* Rec = Records.Find(Ici.Id))
		{
			Detail += FString::Printf(TEXT(" · votre record %s"), *FClassementCourses::Temps(Rec->TempsMs));
		}
		const FClassementUneCourse* C = FClassementCourses::Get().Course(Ici.Id);
		if (C && C->Etat == EEtatClassementCourses::Pret && C->Meilleurs.Num() > 0)
		{
			Detail += FString::Printf(TEXT(" · meilleur : %s %s"), *C->Meilleurs[0].Pseudo, *FClassementCourses::Temps(C->Meilleurs[0].TempsMs));
		}
		E.InvitationDetail = Detail;
	}

	// ---- Repere vers la prochaine porte, ou vers le depart montre ----
	FVector Cible = FVector::ZeroVector;
	bool bCible = false;
	E.bRepereArrivee = false;
	if (P && (Etat == EEtatCourse::Depart || Etat == EEtatCourse::EnCourse) && P->Points.IsValidIndex(Prochaine))
	{
		Cible = P->Points[Prochaine];
		bCible = true;
		E.bRepereArrivee = Prochaine == P->Points.Num() - 1;
	}
	else if (ListeParcours.IsValidIndex(DepartMontre) && Etat == EEtatCourse::Libre)
	{
		Cible = ListeParcours[DepartMontre].Points[0];
		bCible = true;
	}
	if (bCible && PC && PC->PlayerCameraManager)
	{
		const FVector Point = Cible + FVector(0.f, 0.f, 250.f);
		const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
		const FVector Local = PC->PlayerCameraManager->GetCameraRotation().UnrotateVector(Point - Camera);
		int32 L = 0, H = 0;
		PC->GetViewportSize(L, H);
		FVector2D SurEcran;
		if (Local.X > 10.f && L > 0 && H > 0 && PC->ProjectWorldLocationToScreen(Point, SurEcran, false))
		{
			E.Repere = FVector2D(SurEcran.X / L, SurEcran.Y / H);
			E.bRepereDerriere = false;
		}
		else
		{
			FVector2D Dir(Local.Y, -Local.Z * 0.3f);
			if (Dir.IsNearlyZero())
			{
				Dir = FVector2D(0.f, 1.f);
			}
			E.Repere = FVector2D(0.5f, 0.5f) + Dir.GetSafeNormal() * 2.f;
			E.bRepereDerriere = true;
		}
		const float Metres = Pion ? FVector::Dist(Pion->GetActorLocation(), Cible) / 100.f : 0.f;
		E.DistanceRepere = Pion ? FString::Printf(TEXT("%d m"), FMath::RoundToInt(Metres)) : FString();
		E.bRepere = true;
	}
}

// ===================================================================================
//  Ecran COURSES
// ===================================================================================

void ACoursesChelles::OuvrirMenu()
{
	UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Menu.IsValid() || !Vue || !PC)
	{
		return;
	}
	const FReglagesCourses& R = Reglages();
	const APawn* Pion = PC->GetPawn();

	TArray<FChoixCourse> Choix;
	for (int32 i = 0; i < ListeParcours.Num(); ++i)
	{
		const FParcoursCourse& P = ListeParcours[i];
		FClassementCourses::Get().Rafraichir(P.Id);
		FChoixCourse C;
		C.Index = i;
		C.Titre = P.Nom;
		C.Description = FString::Printf(TEXT("%s · %d portes"), *Km(P.LongueurCm), P.NombrePortes());
		if (Pion)
		{
			C.Description += FString::Printf(TEXT(" · départ à %d m"), FMath::RoundToInt(FVector::Dist2D(Pion->GetActorLocation(), P.Points[0]) / 100.f));
		}
		if (P.bFaiteMain)
		{
			C.Description += TEXT(" · faite main");
		}
		const FRecordCourse* Rec = Records.Find(P.Id);
		C.Record = Rec ? FString::Printf(TEXT("Votre record : %s (%s)"), *FClassementCourses::Temps(Rec->TempsMs), *Rec->Date)
			: FString(TEXT("Pas encore de temps sur ce PC"));
		const FString Id = P.Id;
		C.Classement = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([Id]()
		{
			if (!FClassementCourses::Get().EnLigne())
			{
				return FText::FromString(TEXT("Classement en ligne coupé (ClassementEnLigne=Non)"));
			}
			const FClassementUneCourse* Cl = FClassementCourses::Get().Course(Id);
			if (!Cl || Cl->Etat == EEtatClassementCourses::Jamais || Cl->Etat == EEtatClassementCourses::Chargement)
			{
				return FText::FromString(TEXT("Classement de tous les joueurs : chargement..."));
			}
			if (Cl->Etat == EEtatClassementCourses::HorsLigne)
			{
				return FText::FromString(TEXT("Classement de tous les joueurs : serveur injoignable"));
			}
			if (Cl->Meilleurs.Num() == 0)
			{
				return FText::FromString(TEXT("Personne n'a encore fini cette course : soyez le premier !"));
			}
			FString Texte = TEXT("Tous les joueurs : ");
			for (int32 k = 0; k < FMath::Min(5, Cl->Meilleurs.Num()); ++k)
			{
				const FLigneCourseEnLigne& L = Cl->Meilleurs[k];
				Texte += FString::Printf(TEXT("%s%d. %s %s"), k > 0 ? TEXT("   ") : TEXT(""), k + 1, *L.Pseudo, *FClassementCourses::Temps(L.TempsMs));
			}
			if (Cl->Total > 5)
			{
				Texte += FString::Printf(TEXT("   (%d joueurs)"), Cl->Total);
			}
			return FText::FromString(Texte);
		}));
		Choix.Add(C);
	}

	FString Aide = FString::Printf(TEXT("Allez sur un départ (colonne cyan) et appuyez sur %s. Passez les portes dorées dans l'ordre jusqu'à l'arrivée (rose). ")
		TEXT("%s pendant la course = abandonner. Créer votre course : console Chelles.CourseCreer NOM. %s ou Échap = fermer."),
		*NomTouche(R.Touche), *NomTouche(R.Touche), *NomTouche(R.Touche));
	if (bEnregistre)
	{
		Aide = FString::Printf(TEXT("Enregistrement de %s en cours (%d portes) : Chelles.CourseCreerFin à l'arrivée. "), *Enregistrement.Nom, Enregistrement.Points.Num() - 1) + Aide;
	}

	TWeakObjectPtr<ACoursesChelles> Moi(this);
	Menu = SNew(SCoursesMenu)
		.NomTouche(NomTouche(R.Touche))
		.Aide(Aide)
		.Choix(Choix)
		.QuandChoisie_Lambda([Moi](int32 Index)
		{
			if (Moi.IsValid())
			{
				Moi->FermerMenu();
				Moi->MontrerDepart(Index);
			}
		})
		.QuandFerme_Lambda([Moi]()
		{
			if (Moi.IsValid())
			{
				Moi->FermerMenu();
			}
		});
	Menu->ToucheFermer = R.Touche;
	Vue->AddViewportWidgetContent(Menu.ToSharedRef(), 900);

	FInputModeUIOnly Entree;
	Entree.SetWidgetToFocus(Menu);
	Entree.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Entree);
	PC->bShowMouseCursor = true;
}

void ACoursesChelles::FermerMenu()
{
	if (!Menu.IsValid())
	{
		return;
	}
	if (UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Vue->RemoveViewportWidgetContent(Menu.ToSharedRef());
	}
	Menu.Reset();
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

// ===================================================================================
//  Console
// ===================================================================================

void ACoursesChelles::DecrireEtat() const
{
	static const TCHAR* Etats[] = { TEXT("libre"), TEXT("compte a rebours"), TEXT("en course"), TEXT("arrivee"), TEXT("abandon") };
	UE_LOG(LogTemp, Display, TEXT("[Courses] Etat : %s | %d parcours | fichier %s"), Etat < 5 ? Etats[Etat] : TEXT("?"), ListeParcours.Num(), *FichierParcours(GetWorld()));
	for (int32 i = 0; i < ListeParcours.Num(); ++i)
	{
		const FParcoursCourse& P = ListeParcours[i];
		const FRecordCourse* Rec = Records.Find(P.Id);
		UE_LOG(LogTemp, Display, TEXT("[Courses]  %d. %s (%s) : %s, %d portes, depart %s, record %s"), i + 1, *P.Nom, *P.Id, *Km(P.LongueurCm),
			P.NombrePortes(), *P.Points[0].ToString(), Rec ? *FClassementCourses::Temps(Rec->TempsMs) : TEXT("aucun"));
	}
}
