#include "ParolesPassants.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"

DEFINE_LOG_CATEGORY_STATIC(LogChellesParoles, Log, All);

namespace
{
	FString DossierDuPlugin()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ChellesParoles"));
		return Plugin.IsValid() ? Plugin->GetBaseDir() : FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesParoles"));
	}

	bool ContientUnMot(const FString& Nom, const TCHAR* const* Mots, int32 NbMots)
	{
		for (int32 i = 0; i < NbMots; ++i)
		{
			if (Nom.Contains(Mots[i]))
			{
				return true;
			}
		}
		return false;
	}

	/** Lit un .wav PCM 16 bits (format ecrit par la synthese vocale de Windows). */
	bool LireWav(const TArray<uint8>& Octets, FSonCharge& Sortie)
	{
		if (Octets.Num() < 44 || FMemory::Memcmp(Octets.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(Octets.GetData() + 8, "WAVE", 4) != 0)
		{
			return false;
		}
		auto Lire16 = [&Octets](int32 i) { return int32(Octets[i]) | (int32(Octets[i + 1]) << 8); };
		auto Lire32 = [&Octets](int32 i) { return int32(uint32(Octets[i]) | (uint32(Octets[i + 1]) << 8) | (uint32(Octets[i + 2]) << 16) | (uint32(Octets[i + 3]) << 24)); };

		bool bFormatOk = false;
		int32 Pos = 12;
		while (Pos + 8 <= Octets.Num())
		{
			const int32 Taille = Lire32(Pos + 4);
			const int32 Debut = Pos + 8;
			if (Taille < 0 || Debut + Taille > Octets.Num() + 1)
			{
				return false;
			}
			if (FMemory::Memcmp(Octets.GetData() + Pos, "fmt ", 4) == 0 && Taille >= 16)
			{
				const int32 Format = Lire16(Debut);
				Sortie.Canaux = Lire16(Debut + 2);
				Sortie.Frequence = Lire32(Debut + 4);
				const int32 Bits = Lire16(Debut + 14);
				bFormatOk = ((Format == 1 || Format == 0xFFFE) && Bits == 16 && Sortie.Canaux > 0 && Sortie.Frequence > 0);
			}
			else if (FMemory::Memcmp(Octets.GetData() + Pos, "data", 4) == 0)
			{
				if (!bFormatOk)
				{
					return false;
				}
				const int32 Utile = FMath::Min(Taille, Octets.Num() - Debut);
				Sortie.PCM.SetNumUninitialized(Utile);
				FMemory::Memcpy(Sortie.PCM.GetData(), Octets.GetData() + Debut, Utile);
				Sortie.Duree = float(Utile) / float(2 * Sortie.Canaux * Sortie.Frequence);
				return Sortie.Duree > 0.f;
			}
			Pos = Debut + Taille + (Taille & 1);
		}
		return false;
	}
}

AParolesPassants::AParolesPassants()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // 10 fois par seconde suffit
	SetActorHiddenInGame(true);
}

void AParolesPassants::BeginPlay()
{
	Super::BeginPlay();

	ChargerRepliques();

	// Le son vient de la bouche du passant et baisse avec la distance.
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
	Attenuation->Attenuation.FalloffDistance = FMath::Max(100.f, PorteeDeLaVoix - 300.f);
	Attenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
}

void AParolesPassants::ChargerRepliques()
{
	const FString Dossier = DossierDuPlugin();

	// 1) Repliques.txt : [Categorie] puis une phrase par ligne.
	TArray<FString> Lignes;
	if (FFileHelper::LoadFileToStringArray(Lignes, *FPaths::Combine(Dossier, TEXT("Repliques.txt"))))
	{
		FString Categorie = TEXT("Salut");
		for (FString Ligne : Lignes)
		{
			Ligne.TrimStartAndEndInline();
			if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")))
			{
				continue;
			}
			if (Ligne.StartsWith(TEXT("[")) && Ligne.EndsWith(TEXT("]")))
			{
				Categorie = Ligne.Mid(1, Ligne.Len() - 2).TrimStartAndEnd();
				continue;
			}
			Textes.FindOrAdd(Categorie).Add(Ligne);
		}
	}
	else
	{
		UE_LOG(LogChellesParoles, Warning, TEXT("Repliques.txt introuvable dans %s"), *Dossier);
	}

	// 2) Voix/index.txt : Categorie|F ou M|numero de voix|fichier.wav|texte
	Lignes.Reset();
	const FString DossierVoix = FPaths::Combine(Dossier, TEXT("Voix"));
	if (FFileHelper::LoadFileToStringArray(Lignes, *FPaths::Combine(DossierVoix, TEXT("index.txt"))))
	{
		for (const FString& Ligne : Lignes)
		{
			TArray<FString> Morceaux;
			Ligne.ParseIntoArray(Morceaux, TEXT("|"), false);
			if (Morceaux.Num() < 5)
			{
				continue;
			}
			FRepliqueVoix R;
			R.Categorie = Morceaux[0].TrimStartAndEnd();
			R.bFeminin = Morceaux[1].TrimStartAndEnd().Equals(TEXT("F"), ESearchCase::IgnoreCase);
			R.Voix = FCString::Atoi(*Morceaux[2]);
			R.Fichier = FPaths::Combine(DossierVoix, Morceaux[3].TrimStartAndEnd());
			// le texte peut contenir des | : on recolle la fin
			R.Texte = Morceaux[4];
			for (int32 i = 5; i < Morceaux.Num(); ++i)
			{
				R.Texte += TEXT("|") + Morceaux[i];
			}
			R.Texte.TrimStartAndEndInline();
			if (!FPaths::FileExists(R.Fichier))
			{
				continue;
			}
			TArray<int32>& Liste = R.bFeminin ? VoixFeminines : VoixMasculines;
			Liste.AddUnique(R.Voix);
			Voix.Add(MoveTemp(R));
		}
	}

	int32 NbTextes = 0;
	for (const TPair<FString, TArray<FString>>& Paire : Textes)
	{
		NbTextes += Paire.Value.Num();
	}
	UE_LOG(LogChellesParoles, Log, TEXT("Paroles des passants : %d phrases, %d enregistrements (%d voix feminines, %d masculines)."),
		NbTextes, Voix.Num(), VoixFeminines.Num(), VoixMasculines.Num());
	if (Voix.Num() == 0)
	{
		UE_LOG(LogChellesParoles, Warning, TEXT("Aucune voix : lancez Plugins/ChellesParoles/Generer_les_voix.bat (les phrases s'afficheront en texte en attendant)."));
	}
}

bool AParolesPassants::EstUnPassant(const ACharacter* Personnage) const
{
	return Personnage && !Personnage->IsPlayerControlled() && Personnage->GetClass()->GetName().Contains(MotCleClasse);
}

void AParolesPassants::PreparerPassant(ACharacter* Passant, FEtatPassant& Etat)
{
	Etat.bInitialise = true;

	// Homme ou femme ? On devine d'apres le nom du personnage 3D.
	FString Nom;
	if (const USkeletalMeshComponent* Mesh = Passant->GetMesh())
	{
		if (Mesh->SkeletalMesh)
		{
			Nom = Mesh->SkeletalMesh->GetPathName();
		}
	}
	static const TCHAR* const MotsFeminins[] = { TEXT("female"), TEXT("Female"), TEXT("FEMALE"), TEXT("girl"), TEXT("Girl"), TEXT("woman"), TEXT("Woman"),
		TEXT("women"), TEXT("Women"), TEXT("femme"), TEXT("Femme"), TEXT("fille"), TEXT("Fille"), TEXT("lady"), TEXT("Lady"), TEXT("_F_"), TEXT("Feminin") };
	static const TCHAR* const MotsMasculins[] = { TEXT("male"), TEXT("Male"), TEXT("MALE"), TEXT("man"), TEXT("Man"), TEXT("homme"), TEXT("Homme"),
		TEXT("guy"), TEXT("Guy"), TEXT("boy"), TEXT("Boy"), TEXT("Bandit"), TEXT("bandit"), TEXT("Spider"), TEXT("Soldier"), TEXT("_M_") };

	// Tirage au sort stable : le meme passant garde toujours la meme voix.
	FRandomStream Hasard(int32(GetTypeHash(Passant->GetName())));
	if (ContientUnMot(Nom, MotsFeminins, UE_ARRAY_COUNT(MotsFeminins)))
	{
		Etat.bFeminin = true; // a tester en premier : "female" contient "male"
	}
	else if (ContientUnMot(Nom, MotsMasculins, UE_ARRAY_COUNT(MotsMasculins)))
	{
		Etat.bFeminin = false;
	}
	else
	{
		Etat.bFeminin = Hasard.FRand() < 0.5f;
	}

	const TArray<int32>& Bonnes = Etat.bFeminin ? VoixFeminines : VoixMasculines;
	const TArray<int32>& Autres = Etat.bFeminin ? VoixMasculines : VoixFeminines;
	if (Bonnes.Num() > 0)
	{
		Etat.Voix = Bonnes[Hasard.RandRange(0, Bonnes.Num() - 1)];
		Etat.Hauteur = Hasard.FRandRange(0.94f, 1.07f);
	}
	else if (Autres.Num() > 0)
	{
		// Pas de voix du bon genre installee : on change la hauteur de l'autre voix.
		Etat.Voix = Autres[Hasard.RandRange(0, Autres.Num() - 1)];
		Etat.Hauteur = Etat.bFeminin ? Hasard.FRandRange(1.18f, 1.28f) : Hasard.FRandRange(0.74f, 0.82f);
	}
}

const FSonCharge* AParolesPassants::ChargerSon(const FString& Fichier)
{
	if (const FSonCharge* Deja = Sons.Find(Fichier))
	{
		return Deja->Duree > 0.f ? Deja : nullptr;
	}
	FSonCharge& Son = Sons.Add(Fichier);
	TArray<uint8> Octets;
	if (!FFileHelper::LoadFileToArray(Octets, *Fichier) || !LireWav(Octets, Son))
	{
		UE_LOG(LogChellesParoles, Warning, TEXT("Son illisible : %s"), *Fichier);
		Son.Duree = 0.f;
		return nullptr;
	}
	return &Son;
}

void AParolesPassants::Parler(ACharacter* Passant, FEtatPassant& Etat, const FString& Categorie, bool bPrioritaire)
{
	UWorld* Monde = GetWorld();
	const double Maintenant = Monde->GetTimeSeconds();

	if (!bPrioritaire)
	{
		if (Maintenant - DerniereParoleGlobale < AttenteEntrePassants || Maintenant - Etat.DerniereParole < AttenteParPassant)
		{
			return;
		}
		if (Etat.Audio.IsValid() && Etat.Audio->IsPlaying())
		{
			return;
		}
	}

	// Choix de la phrase : d'abord celles dites avec la voix de ce passant.
	const FString* Derniere = DernierePhrase.Find(Categorie);
	TArray<const FRepliqueVoix*> Choix;
	for (int32 Passe = 0; Passe < 3 && Choix.Num() == 0; ++Passe)
	{
		for (const FRepliqueVoix& R : Voix)
		{
			if (R.Categorie != Categorie)
			{
				continue;
			}
			if (Passe == 0 && R.Voix != Etat.Voix)
			{
				continue;
			}
			if (Passe == 1 && R.bFeminin != Etat.bFeminin)
			{
				continue;
			}
			if (Derniere && R.Texte == *Derniere)
			{
				continue;
			}
			Choix.Add(&R);
		}
	}

	FString Texte;
	float Duree = 0.f;
	if (Choix.Num() > 0)
	{
		const FRepliqueVoix* R = Choix[FMath::RandRange(0, Choix.Num() - 1)];
		Texte = R->Texte;
		if (const FSonCharge* Son = ChargerSon(R->Fichier))
		{
			// Un son "procedural" est rempli avec les echantillons du .wav (pas besoin d'importer dans Unreal).
			USoundWaveProcedural* Onde = NewObject<USoundWaveProcedural>(this);
			Onde->SetSampleRate(Son->Frequence);
			Onde->NumChannels = Son->Canaux;
			Onde->Duration = Son->Duree;
			Onde->SoundGroup = SOUNDGROUP_Voice;
			Onde->bLooping = false;
			Onde->QueueAudio(Son->PCM.GetData(), Son->PCM.Num());

			if (Etat.Audio.IsValid())
			{
				Etat.Audio->Stop();
			}
			UAudioComponent* Audio = UGameplayStatics::SpawnSoundAttached(Onde, Passant->GetRootComponent(), NAME_None,
				FVector(0.f, 0.f, 70.f), EAttachLocation::KeepRelativeOffset, true, Volume, Etat.Hauteur, 0.f, Attenuation);
			Etat.Audio = Audio;
			Duree = Son->Duree / FMath::Max(0.1f, Etat.Hauteur);
		}
	}
	else if (const TArray<FString>* Liste = Textes.Find(Categorie))
	{
		if (Liste->Num() > 0)
		{
			Texte = (*Liste)[FMath::RandRange(0, Liste->Num() - 1)];
			if (Derniere && Texte == *Derniere && Liste->Num() > 1)
			{
				Texte = (*Liste)[FMath::RandRange(0, Liste->Num() - 1)];
			}
		}
	}

	if (Texte.IsEmpty())
	{
		return;
	}

#if ENABLE_DRAW_DEBUG
	if (bToujoursAfficherLeTexte || (Duree <= 0.f && bTexteSiPasDeVoix))
	{
		const float Affichage = FMath::Max(2.5f, Texte.Len() * 0.07f);
		const FColor Couleur = (Categorie == TEXT("Salut")) ? FColor::White : FColor(255, 200, 80);
		DrawDebugString(Monde, FVector(0.f, 0.f, 115.f), Texte, Passant, Couleur, Affichage, true, 1.2f);
	}
#endif

	DernierePhrase.Add(Categorie, Texte);
	Etat.DerniereParole = Maintenant;
	Etat.FinParole = Maintenant + (Duree > 0.f ? Duree + 0.15f : 0.0);
	DerniereParoleGlobale = Maintenant;
}

void AParolesPassants::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* Monde = GetWorld();
	APawn* Joueur = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Monde || !Joueur)
	{
		return;
	}

	const double Maintenant = Monde->GetTimeSeconds();
	const FVector PosJoueur = Joueur->GetActorLocation();
	const float VitesseJoueur = Joueur->GetVelocity().Size2D();
	const float RayonJoueur = FMath::Min(Joueur->GetSimpleCollisionRadius(), 300.f);
	const float Portee = FMath::Max3(DistanceSalut, DistanceVehicule, 400.f);

	for (TActorIterator<ACharacter> It(Monde); It; ++It)
	{
		ACharacter* Passant = *It;
		if (Passant == Joueur || !EstUnPassant(Passant))
		{
			continue;
		}

		const FVector PosPassant = Passant->GetActorLocation();
		const float Distance = FVector::Dist2D(PosPassant, PosJoueur);
		FEtatPassant* EtatTrouve = Etats.Find(Passant);

		// Trop loin, ou joueur en vol au-dessus des toits : rien a dire.
		if (Distance > Portee || FMath::Abs(PosPassant.Z - PosJoueur.Z) > 300.f)
		{
			if (EtatTrouve)
			{
				EtatTrouve->bEtaitProche = false;
				EtatTrouve->TempsAFixer = 0.f;
			}
			continue;
		}

		FEtatPassant& Etat = EtatTrouve ? *EtatTrouve : Etats.Add(Passant);
		if (!Etat.bInitialise)
		{
			PreparerPassant(Passant, Etat);
		}

		const float Contact = Passant->GetCapsuleComponent()->GetScaledCapsuleRadius() + RayonJoueur + 25.f;
		const bool bProche = Distance < DistanceSalut;

		if (Distance < Contact && VitesseJoueur > VitesseBousculade && Maintenant - Etat.DerniereBousculade > 2.5)
		{
			// Bousculade (ou renverse par un vehicule) : il reagit tout de suite.
			Etat.DerniereBousculade = Maintenant;
			if (bPousserQuandBouscule)
			{
				FVector Direction = PosPassant - PosJoueur;
				Direction.Z = 0.f;
				Direction = Direction.GetSafeNormal();
				Passant->LaunchCharacter(Direction * FMath::Clamp(VitesseJoueur * 0.6f, 150.f, 600.f) + FVector(0.f, 0.f, 80.f), true, false);
			}
			Parler(Passant, Etat, VitesseJoueur > VitesseVehicule ? TEXT("Vehicule") : TEXT("Bousculade"), true);
		}
		else if (VitesseJoueur > VitesseVehicule && Distance < DistanceVehicule)
		{
			Parler(Passant, Etat, TEXT("Vehicule"), false);
		}
		else if (bProche && !Etat.bEtaitProche && FMath::FRand() < ChanceSalut)
		{
			Parler(Passant, Etat, TEXT("Salut"), false);
		}

		// Le joueur reste plante devant lui sans bouger.
		if (Distance < 220.f && VitesseJoueur < 30.f)
		{
			Etat.TempsAFixer += DeltaSeconds;
			if (Etat.TempsAFixer > TempsAvantRemarque)
			{
				Etat.TempsAFixer = -2.f * TempsAvantRemarque; // ne repete pas tout de suite
				Parler(Passant, Etat, TEXT("Remarque"), false);
			}
		}
		else if (Etat.TempsAFixer > 0.f)
		{
			Etat.TempsAFixer = 0.f;
		}

		Etat.bEtaitProche = bProche;
	}

	// Coupe les voix terminees (un son procedural ne s'arrete pas tout seul).
	for (TPair<TWeakObjectPtr<ACharacter>, FEtatPassant>& Paire : Etats)
	{
		FEtatPassant& Etat = Paire.Value;
		if (Etat.Audio.IsValid() && Etat.FinParole > 0.0 && Maintenant > Etat.FinParole)
		{
			Etat.Audio->Stop();
			Etat.Audio.Reset();
			Etat.FinParole = 0.0;
		}
	}

	// De temps en temps, on oublie les passants qui ont disparu.
	TempsAvantNettoyage -= DeltaSeconds;
	if (TempsAvantNettoyage <= 0.f)
	{
		TempsAvantNettoyage = 10.f;
		for (auto Iter = Etats.CreateIterator(); Iter; ++Iter)
		{
			if (!Iter->Key.IsValid())
			{
				Iter.RemoveCurrent();
			}
		}
	}
}
