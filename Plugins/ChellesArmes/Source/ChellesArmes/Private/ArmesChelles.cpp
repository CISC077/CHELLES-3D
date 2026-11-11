// CHELLES 3D - Armes : nouvelles apparences des armes FPS_Game + tirs speciaux en plus du tir normal
#include "ArmesChelles.h"
#include "ArmesRelais.h"
#include "TirSpecialChelles.h"

#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

using namespace ChellesArmes;

namespace
{
	const TCHAR* const NomsMaillages[] = { TEXT("Weapon_Mesh_FPP"), TEXT("Weapon_Mesh_TPP") };

	// ------------------------------------------------------------------
	// Lecture des variables des Blueprints par leur nom (aucun lien de compilation avec FPS_Game)
	// ------------------------------------------------------------------
	UObject* LireObjet(UObject* Objet, FName Nom)
	{
		if (!Objet)
		{
			return nullptr;
		}
		FObjectPropertyBase* P = FindFProperty<FObjectPropertyBase>(Objet->GetClass(), Nom);
		return P ? P->GetObjectPropertyValue_InContainer(Objet) : nullptr;
	}

	bool LireEntier(UObject* Objet, FName Nom, int64& Valeur)
	{
		if (!Objet)
		{
			return false;
		}
		FNumericProperty* P = FindFProperty<FNumericProperty>(Objet->GetClass(), Nom);
		if (!P)
		{
			return false;
		}
		const void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		Valeur = P->IsFloatingPoint() ? int64(P->GetFloatingPointPropertyValue(Adresse)) : P->GetSignedIntPropertyValue(Adresse);
		return true;
	}

	bool LireBool(UObject* Objet, FName Nom, bool& Valeur)
	{
		if (!Objet)
		{
			return false;
		}
		FBoolProperty* P = FindFProperty<FBoolProperty>(Objet->GetClass(), Nom);
		if (!P)
		{
			return false;
		}
		Valeur = P->GetPropertyValue_InContainer(Objet);
		return true;
	}

	UActorComponent* TrouverComposant(AActor* Acteur, const TCHAR* Nom)
	{
		if (!Acteur)
		{
			return nullptr;
		}
		TInlineComponentArray<UActorComponent*> Liste;
		Acteur->GetComponents(Liste);
		for (UActorComponent* C : Liste)
		{
			if (C && C->GetName().Equals(Nom, ESearchCase::IgnoreCase))
			{
				return C;
			}
		}
		return nullptr;
	}

	/** bRenderInDepthPass n'a pas de fonction publique partout : on passe par son nom. */
	bool LireProfondeur(UPrimitiveComponent* C)
	{
		static FBoolProperty* P = FindFProperty<FBoolProperty>(UPrimitiveComponent::StaticClass(), TEXT("bRenderInDepthPass"));
		return P ? P->GetPropertyValue_InContainer(C) : true;
	}

	void EcrireProfondeur(UPrimitiveComponent* C, bool bOui)
	{
		static FBoolProperty* P = FindFProperty<FBoolProperty>(UPrimitiveComponent::StaticClass(), TEXT("bRenderInDepthPass"));
		if (P && P->GetPropertyValue_InContainer(C) != bOui)
		{
			P->SetPropertyValue_InContainer(C, bOui);
			C->MarkRenderStateDirty();
		}
	}

	/** Rend un composant invisible SANS toucher a sa visibilite (le Blueprint continue de la gerer). */
	void Masquer(UPrimitiveComponent* C)
	{
		if (C->bRenderInMainPass)
		{
			C->SetRenderInMainPass(false);
		}
		EcrireProfondeur(C, false);
		if (C->CastShadow)
		{
			C->SetCastShadow(false);
		}
	}

	// ------------------------------------------------------------------
	// Forme d'une arme : centre, taille, bout du canon, chargeur (dans le repere du modele)
	// ------------------------------------------------------------------
	struct FForme
	{
		FVector Centre = FVector::ZeroVector;
		FVector Demi = FVector(10.f);
		bool bBouche = false;
		FVector Bouche = FVector::ZeroVector;
		bool bBas = false;
		FVector Bas = FVector::ZeroVector;
	};

	bool EstChargeur(const FString& Nom)
	{
		return Nom.Contains(TEXT("mag"), ESearchCase::IgnoreCase) && !Nom.Contains(TEXT("rel"), ESearchCase::IgnoreCase)
			&& !Nom.Contains(TEXT("push"), ESearchCase::IgnoreCase) && !Nom.Contains(TEXT("bullet"), ESearchCase::IgnoreCase);
	}

	FForme LireForme(UObject* Modele)
	{
		FForme F;
		if (USkeletalMesh* SK = Cast<USkeletalMesh>(Modele))
		{
			const FBoxSphereBounds B = SK->GetImportedBounds();
			F.Centre = B.Origin;
			F.Demi = B.BoxExtent;
			const FReferenceSkeleton& Squelette = SK->GetRefSkeleton();

			for (USkeletalMeshSocket* S : SK->GetActiveSocketList())
			{
				if (!S)
				{
					continue;
				}
				const int32 Os = Squelette.FindBoneIndex(S->BoneName);
				const FTransform Repere = (Os != INDEX_NONE) ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Squelette, Os) : FTransform::Identity;
				const FVector Position = (S->GetSocketLocalTransform() * Repere).GetLocation();
				const FString Nom = S->SocketName.ToString();
				if (!F.bBouche && Nom.Contains(TEXT("muzzle"), ESearchCase::IgnoreCase))
				{
					F.bBouche = true;
					F.Bouche = Position;
				}
				else if (!F.bBas && EstChargeur(Nom))
				{
					F.bBas = true;
					F.Bas = Position;
				}
			}
			for (int32 i = 0; i < Squelette.GetNum(); ++i)
			{
				const FString Nom = Squelette.GetBoneName(i).ToString();
				if (!F.bBouche && Nom.Contains(TEXT("muzzle"), ESearchCase::IgnoreCase))
				{
					F.bBouche = true;
					F.Bouche = FAnimationRuntime::GetComponentSpaceTransformRefPose(Squelette, i).GetLocation();
				}
				else if (!F.bBas && EstChargeur(Nom))
				{
					F.bBas = true;
					F.Bas = FAnimationRuntime::GetComponentSpaceTransformRefPose(Squelette, i).GetLocation();
				}
			}
		}
		else if (UStaticMesh* SM = Cast<UStaticMesh>(Modele))
		{
			const FBoxSphereBounds B = SM->GetBounds();
			F.Centre = B.Origin;
			F.Demi = B.BoxExtent;
			for (UStaticMeshSocket* S : SM->Sockets)
			{
				if (!S)
				{
					continue;
				}
				const FString Nom = S->SocketName.ToString();
				if (!F.bBouche && Nom.Contains(TEXT("muzzle"), ESearchCase::IgnoreCase))
				{
					F.bBouche = true;
					F.Bouche = S->RelativeLocation;
				}
				else if (!F.bBas && EstChargeur(Nom))
				{
					F.bBas = true;
					F.Bas = S->RelativeLocation;
				}
			}
		}
		F.Demi = F.Demi.ComponentMax(FVector(0.5f));
		return F;
	}

	/** Axe principal (X, Y ou Z, avec son signe) le plus proche de V. */
	FVector AxePrincipal(const FVector& V)
	{
		const FVector A = V.GetAbs();
		if (A.X >= A.Y && A.X >= A.Z)
		{
			return FVector(FMath::Sign(V.X) >= 0.f ? 1.f : -1.f, 0.f, 0.f);
		}
		if (A.Y >= A.Z)
		{
			return FVector(0.f, FMath::Sign(V.Y) >= 0.f ? 1.f : -1.f, 0.f);
		}
		return FVector(0.f, 0.f, FMath::Sign(V.Z) >= 0.f ? 1.f : -1.f);
	}

	/** Axe le plus long de la boite (c'est en general le canon). */
	FVector AxeLePlusLong(const FVector& Demi)
	{
		if (Demi.X >= Demi.Y && Demi.X >= Demi.Z)
		{
			return FVector::ForwardVector;
		}
		return (Demi.Y >= Demi.Z) ? FVector::RightVector : FVector::UpVector;
	}

	/** Un axe perpendiculaire a Canon, de preference Z (le haut habituel des modeles). */
	FVector HautParDefaut(const FVector& Canon)
	{
		return FMath::Abs(Canon.Z) < 0.5f ? FVector::UpVector : FVector::ForwardVector;
	}

	FVector Perpendiculaire(FVector V, const FVector& Canon)
	{
		V -= Canon * FVector::DotProduct(V, Canon);
		return V;
	}

	float LongueurSelon(const FVector& Demi, const FVector& Axe)
	{
		return 2.f * (FMath::Abs(Axe.X) * Demi.X + FMath::Abs(Axe.Y) * Demi.Y + FMath::Abs(Axe.Z) * Demi.Z);
	}

	UObject* ModeleDe(UMeshComponent* C)
	{
		if (USkeletalMeshComponent* SK = Cast<USkeletalMeshComponent>(C))
		{
			return SK->SkeletalMesh;
		}
		if (UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C))
		{
			return SM->GetStaticMesh();
		}
		return nullptr;
	}
}

// =====================================================================================
//  Demarrage / fin
// =====================================================================================

AArmesChelles::AArmesChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // apres les Blueprints : on recopie leur visibilite
	bReplicates = false; // un par machine : les choses partagees passent par AArmesRelais
	SetActorHiddenInGame(true);
	for (float& T : DernierTirLocal)
	{
		T = -100.f;
	}
}

void AArmesChelles::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages(Reglages);

	if (UWorld* W = GetWorld())
	{
		AbonnementApparition = W->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AArmesChelles::Apparu));
	}
	Scanner();
	LierClavier();
	CreerEcran();
	UE_LOG(LogTemp, Log, TEXT("[Armes] Pret. Apparences %s, tirs speciaux %s. Touche %s = changer le tir special."),
		Reglages.bApparences ? TEXT("oui") : TEXT("non"), Reglages.bTirsSpeciaux ? TEXT("oui") : TEXT("non"), *Reglages.ToucheChanger);
}

void AArmesChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (UWorld* W = GetWorld())
	{
		W->RemoveOnActorSpawnedHandler(AbonnementApparition);
	}
	ToutRetirer();
	RetirerEcran();
	Super::EndPlay(Raison);
}

void AArmesChelles::Message(const FString& Texte)
{
	UE_LOG(LogTemp, Log, TEXT("[Armes] %s"), *Texte);
	Info = Texte;
	AgeInfo = 0.f;
}

void AArmesChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AgeInfo += DeltaSeconds;
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const float Maintenant = W->GetTimeSeconds();

	// Armes apparues depuis la derniere image (leurs composants sont prets maintenant)
	if (AVerifier.Num() > 0)
	{
		TArray<TWeakObjectPtr<AActor>> Liste = MoveTemp(AVerifier);
		AVerifier.Reset();
		for (const TWeakObjectPtr<AActor>& A : Liste)
		{
			if (A.IsValid() && !Habillees.Contains(A))
			{
				Habiller(A.Get());
			}
		}
	}
	if (Maintenant >= ProchainScan)
	{
		ProchainScan = Maintenant + 3.f;
		Scanner();
	}
	Synchroniser();

	if (GetNetMode() != NM_Client && Maintenant >= ProchainRelais)
	{
		ProchainRelais = Maintenant + 0.5f;
		GererRelais();
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		LierClavier();
		SuivreTirs();
	}
}

// =====================================================================================
//  Reconnaitre les armes
// =====================================================================================

bool AArmesChelles::EstArme(UClass* Classe)
{
	if (!Classe)
	{
		return false;
	}
	if (const bool* Connu = CacheClasses.Find(Classe))
	{
		return *Connu;
	}
	bool bArme = false;
	for (UClass* K = Classe; K; K = K->GetSuperClass())
	{
		if (K->GetName() == TEXT("BP_WeaponBase_C"))
		{
			bArme = true;
			break;
		}
	}
	CacheClasses.Add(Classe, bArme);
	return bArme;
}

FString AArmesChelles::NomArme(AActor* Arme) const
{
	FString Nom = Arme ? Arme->GetClass()->GetName() : FString();
	Nom.RemoveFromEnd(TEXT("_C"));
	return Nom;
}

namespace
{
	/** Cherche la classe de l'arme puis ses parents dans une table du fichier de reglages. */
	template <typename TValeur>
	FString TrouverCle(UClass* Classe, const TMap<FString, TValeur>& Table)
	{
		for (UClass* K = Classe; K; K = K->GetSuperClass())
		{
			FString Nom = K->GetName();
			Nom.RemoveFromEnd(TEXT("_C"));
			if (Table.Contains(Nom))
			{
				return Nom;
			}
			if (Nom == TEXT("BP_WeaponBase"))
			{
				break;
			}
		}
		return FString();
	}
}

void AArmesChelles::Apparu(AActor* Acteur)
{
	if (Acteur && EstArme(Acteur->GetClass()))
	{
		AVerifier.Add(Acteur);
	}
}

void AArmesChelles::Scanner()
{
	UWorld* W = GetWorld();
	if (!W || !Reglages.bApparences || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (A && !Habillees.Contains(A) && EstArme(A->GetClass()))
		{
			Habiller(A);
		}
	}
}

// =====================================================================================
//  Apparences
// =====================================================================================

FTransform AArmesChelles::Ajustement(UMeshComponent* Origine, UObject* NouveauModele, const FReglageApparence& Manuel, AActor* Arme) const
{
	const FForme O = LireForme(ModeleDe(Origine));
	const FForme N = LireForme(NouveauModele);

	// --- Bout du canon de l'arme d'origine : socket "Muzzle", sinon le point de depart des balles du Blueprint ---
	FVector BoucheO = O.Bouche;
	bool bBoucheO = O.bBouche;
	if (!bBoucheO && Arme)
	{
		const bool bFPP = Origine->GetName().Contains(TEXT("FPP"));
		USceneComponent* Depart = Cast<USceneComponent>(TrouverComposant(Arme, bFPP ? TEXT("ProjectileSpawnFromMuzzleFPP") : TEXT("ProjectileSpawnFromMuzzleTPP")));
		if (Depart)
		{
			const FVector Local = Origine->GetComponentTransform().InverseTransformPosition(Depart->GetComponentLocation());
			if (FVector::Dist(Local, O.Centre) < 3.f * O.Demi.Size())
			{
				BoucheO = Local;
				bBoucheO = true;
			}
		}
	}

	// --- Direction du canon (axe principal) ---
	const FVector CanonO = bBoucheO ? AxePrincipal(BoucheO - O.Centre) : AxeLePlusLong(O.Demi);
	const FVector CanonN = N.bBouche ? AxePrincipal(N.Bouche - N.Centre) : AxeLePlusLong(N.Demi);

	// --- Haut : le chargeur est en bas ; sinon l'origine du modele (la poignee) est sous le canon ---
	auto Haut = [](const FForme& F, const FVector& Canon, bool bOrigineEnBas) -> FVector
	{
		FVector V = FVector::ZeroVector;
		if (F.bBas)
		{
			V = Perpendiculaire(F.Centre - F.Bas, Canon);
		}
		else if (bOrigineEnBas)
		{
			V = Perpendiculaire(F.Centre, Canon);
		}
		FVector H = (V.Size() > 1.f) ? AxePrincipal(V) : HautParDefaut(Canon);
		if (FMath::Abs(FVector::DotProduct(H, Canon)) > 0.5f)
		{
			H = HautParDefaut(Canon);
		}
		return H;
	};
	const FVector HautO = Haut(O, CanonO, true);
	const FVector HautN = Haut(N, CanonN, false);
	const FVector CoteO = FVector::CrossProduct(HautO, CanonO);
	const FVector CoteN = FVector::CrossProduct(HautN, CanonN);

	// Rotation qui amene (canon, haut, cote) du nouveau modele sur ceux de l'ancien
	const FMatrix RepereN(CanonN, HautN, CoteN, FVector::ZeroVector);
	const FMatrix RepereO(CanonO, HautO, CoteO, FVector::ZeroVector);
	FQuat Rotation = FQuat(RepereN.GetTransposed() * RepereO);
	Rotation.Normalize();

	// Meme longueur de canon que l'arme d'origine
	const float LongueurO = LongueurSelon(O.Demi, CanonO);
	const float LongueurN = LongueurSelon(N.Demi, CanonN);
	float Echelle = (LongueurN > 1.f) ? FMath::Clamp(LongueurO / LongueurN, 0.05f, 20.f) : 1.f;

	// Reglage a la main (dans le repere de l'arme d'origine : avant = vers le bout du canon)
	const FQuat Main = FQuat(CanonO, FMath::DegreesToRadians(Manuel.Roulis))
		* FQuat(HautO, FMath::DegreesToRadians(Manuel.Lacet))
		* FQuat(CoteO, FMath::DegreesToRadians(-Manuel.Tangage));
	Rotation = Main * Rotation;
	Echelle *= Manuel.Echelle;

	// Le centre du nouveau modele se place sur le centre de l'ancien, puis le decalage a la main
	const FVector Decalage = CanonO * Manuel.Avant + HautO * Manuel.Haut + CoteO * Manuel.Cote;
	const FVector Position = O.Centre + Decalage - Rotation.RotateVector(N.Centre * Echelle);
	return FTransform(Rotation, Position, FVector(Echelle));
}

void AArmesChelles::Habiller(AActor* Arme)
{
	if (!Arme || Habillees.Contains(Arme))
	{
		return;
	}
	FArmeHabillee& Infos = Habillees.Add(Arme);
	if (!Reglages.bApparences || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Infos.Cle = TrouverCle(Arme->GetClass(), Reglages.Apparences);
	if (Infos.Cle.IsEmpty())
	{
		return; // pas de nouvelle apparence pour cette arme
	}

	// Premier modele qui existe dans la liste
	UObject* Modele = nullptr;
	for (const FString& Chemin : Reglages.Apparences[Infos.Cle])
	{
		Modele = LoadObject<UObject>(nullptr, *Chemin, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Cast<USkeletalMesh>(Modele) || Cast<UStaticMesh>(Modele))
		{
			break;
		}
		Modele = nullptr;
	}
	if (!Modele)
	{
		if (!ModelesAbsents.Contains(Infos.Cle))
		{
			ModelesAbsents.Add(Infos.Cle);
			UE_LOG(LogTemp, Warning, TEXT("[Armes] %s : aucun modele trouve (Arme.%s dans Armes.txt). Apparence d'origine gardee."), *Infos.Cle, *Infos.Cle);
		}
		return;
	}

	const FReglageApparence* Trouve = Reglages.Reglages.Find(Infos.Cle);
	const FReglageApparence Manuel = Trouve ? *Trouve : FReglageApparence();

	for (const TCHAR* NomMaillage : NomsMaillages)
	{
		UMeshComponent* Origine = Cast<UMeshComponent>(TrouverComposant(Arme, NomMaillage));
		if (!Origine || !ModeleDe(Origine))
		{
			continue;
		}

		UMeshComponent* Nouveau = nullptr;
		if (USkeletalMesh* SK = Cast<USkeletalMesh>(Modele))
		{
			USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(Arme, MakeUniqueObjectName(Arme, USkeletalMeshComponent::StaticClass(), TEXT("ChellesApparence")));
			C->SetSkeletalMesh(SK);
			Nouveau = C;
		}
		else
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Arme, MakeUniqueObjectName(Arme, UStaticMeshComponent::StaticClass(), TEXT("ChellesApparence")));
			C->SetStaticMesh(Cast<UStaticMesh>(Modele));
			Nouveau = C;
		}
		Nouveau->SetupAttachment(Origine);
		Nouveau->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Nouveau->SetGenerateOverlapEvents(false);
		Nouveau->CanCharacterStepUpOn = ECB_No;
		Nouveau->bOnlyOwnerSee = Origine->bOnlyOwnerSee;
		Nouveau->bOwnerNoSee = Origine->bOwnerNoSee;
		Nouveau->CastShadow = Origine->CastShadow;
		Nouveau->SetRelativeTransform(Ajustement(Origine, Modele, Manuel, Arme));
		Nouveau->RegisterComponent();
		Arme->AddInstanceComponent(Nouveau);

		FArmesPaire Paire;
		Paire.Origine = Origine;
		Paire.Nouveau = Nouveau;
		Paire.Modele = Modele;
		Paire.bRenduPrincipal = Origine->bRenderInMainPass;
		Paire.bProfondeur = LireProfondeur(Origine);
		Paire.bOmbre = Origine->CastShadow;
		Masquer(Origine);
		Infos.Paires.Add(Paire);
	}

	// Accessoires de l'ancienne arme (lunettes, silencieux...) : caches aussi
	if (!Reglages.bGarderAccessoires && Infos.Paires.Num() > 0)
	{
		TInlineComponentArray<UPrimitiveComponent*> Liste;
		Arme->GetComponents(Liste);
		for (UPrimitiveComponent* C : Liste)
		{
			const FString Nom = C ? C->GetName() : FString();
			if (Nom.StartsWith(TEXT("FPP_Attachment")) || Nom.StartsWith(TEXT("TPP_Attachment")))
			{
				FArmesMasque M;
				M.Composant = C;
				M.bRenduPrincipal = C->bRenderInMainPass;
				M.bProfondeur = LireProfondeur(C);
				M.bOmbre = C->CastShadow;
				Masquer(C);
				Infos.Accessoires.Add(M);
			}
		}
	}

	if (Infos.Paires.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Armes] %s habillee avec %s."), *NomArme(Arme), *Modele->GetName());
	}
}

void AArmesChelles::Deshabiller(FArmeHabillee& Infos)
{
	for (FArmesPaire& P : Infos.Paires)
	{
		if (P.Nouveau.IsValid())
		{
			P.Nouveau->DestroyComponent();
		}
		if (UMeshComponent* O = P.Origine.Get())
		{
			O->SetRenderInMainPass(P.bRenduPrincipal);
			EcrireProfondeur(O, P.bProfondeur);
			O->SetCastShadow(P.bOmbre);
		}
	}
	for (FArmesMasque& M : Infos.Accessoires)
	{
		if (UPrimitiveComponent* C = M.Composant.Get())
		{
			C->SetRenderInMainPass(M.bRenduPrincipal);
			EcrireProfondeur(C, M.bProfondeur);
			C->SetCastShadow(M.bOmbre);
		}
	}
	Infos.Paires.Reset();
	Infos.Accessoires.Reset();
}

void AArmesChelles::ToutRetirer()
{
	for (auto& Paire : Habillees)
	{
		Deshabiller(Paire.Value);
	}
	Habillees.Reset();
}

void AArmesChelles::ToutRecharger()
{
	ToutRetirer();
	ChargerReglages(Reglages);
	ModelesAbsents.Reset();
	Scanner();
}

void AArmesChelles::ActiverApparences(bool bOui)
{
	Reglages.bApparences = bOui;
	ToutRetirer();
	if (bOui)
	{
		Scanner();
	}
	Message(bOui ? TEXT("Nouvelles apparences des armes : ACTIVEES") : TEXT("Apparences d'origine des armes (Chelles.Armes 1 pour revenir)"));
}

void AArmesChelles::Synchroniser()
{
	for (auto It = Habillees.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		for (FArmesPaire& P : It.Value().Paires)
		{
			UMeshComponent* O = P.Origine.Get();
			UMeshComponent* N = P.Nouveau.Get();
			if (!O || !N)
			{
				continue;
			}
			// Le nouveau modele suit tout ce que le Blueprint fait a l'ancien (arme rangee, vue 1re / 3e personne...)
			if (N->GetVisibleFlag() != O->GetVisibleFlag())
			{
				N->SetVisibility(O->GetVisibleFlag());
			}
			if (N->bHiddenInGame != O->bHiddenInGame)
			{
				N->SetHiddenInGame(O->bHiddenInGame);
			}
			if (N->bOnlyOwnerSee != O->bOnlyOwnerSee)
			{
				N->SetOnlyOwnerSee(O->bOnlyOwnerSee);
			}
			if (N->bOwnerNoSee != O->bOwnerNoSee)
			{
				N->SetOwnerNoSee(O->bOwnerNoSee);
			}
			if (N->bRenderCustomDepth != O->bRenderCustomDepth)
			{
				N->SetRenderCustomDepth(O->bRenderCustomDepth);
			}
			if (N->CustomDepthStencilValue != O->CustomDepthStencilValue)
			{
				N->SetCustomDepthStencilValue(O->CustomDepthStencilValue);
			}
			Masquer(O);
		}
	}
}

// =====================================================================================
//  Tirs speciaux : machine du joueur
// =====================================================================================

APlayerController* AArmesChelles::GetPC() const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			return PC;
		}
	}
	return nullptr;
}

AArmesRelais* AArmesChelles::MonRelais() const
{
	APlayerController* PC = GetPC();
	UWorld* W = GetWorld();
	if (!PC || !W)
	{
		return nullptr;
	}
	for (TActorIterator<AArmesRelais> It(W); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

AActor* AArmesChelles::ArmeEnMain(APawn* Pion)
{
	if (!Pion)
	{
		return nullptr;
	}
	// Variable EquippedWeapon de BP_CharacterBase
	if (AActor* Arme = Cast<AActor>(LireObjet(Pion, TEXT("EquippedWeapon"))))
	{
		if (EstArme(Arme->GetClass()))
		{
			return Arme;
		}
	}
	// Sinon : une arme accrochee au personnage et visible
	TArray<AActor*> Attaches;
	Pion->GetAttachedActors(Attaches);
	for (AActor* A : Attaches)
	{
		if (!A || !EstArme(A->GetClass()))
		{
			continue;
		}
		for (const TCHAR* Nom : NomsMaillages)
		{
			USceneComponent* C = Cast<USceneComponent>(TrouverComposant(A, Nom));
			if (C && C->GetVisibleFlag() && !C->bHiddenInGame)
			{
				return A;
			}
		}
	}
	return nullptr;
}

uint8 AArmesChelles::TirChoisi(AActor* Arme)
{
	const FString Nom = NomArme(Arme);
	if (const uint8* Deja = Choix.Find(Nom))
	{
		return *Deja;
	}
	uint8 Tir = Aucun;
	int32 Sauve = -1;
	if (GConfig && GConfig->GetInt(TEXT("ChellesArmes"), *(TEXT("Tir.") + Nom), Sauve, GGameUserSettingsIni) && Sauve >= 0 && Sauve < NombreTirs)
	{
		Tir = uint8(Sauve); // choix fait avec la touche O lors d'une partie precedente
	}
	else
	{
		const FString Cle = TrouverCle(Arme->GetClass(), Reglages.TirsParDefaut);
		if (!Cle.IsEmpty())
		{
			Tir = Reglages.TirsParDefaut[Cle];
		}
	}
	Choix.Add(Nom, Tir);
	return Tir;
}

void AArmesChelles::ChoisirTir(AActor* Arme, uint8 Tir)
{
	const FString Nom = NomArme(Arme);
	Choix.Add(Nom, Tir);
	if (GConfig)
	{
		GConfig->SetInt(TEXT("ChellesArmes"), *(TEXT("Tir.") + Nom), Tir, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void AArmesChelles::LierClavier()
{
	if (bLieClavier)
	{
		return;
	}
	APlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	EnableInput(PC);
	if (!InputComponent)
	{
		return;
	}
	FKey Touche(*Reglages.ToucheChanger);
	if (!Touche.IsValid())
	{
		Touche = EKeys::O;
	}
	FInputKeyBinding& B = InputComponent->BindKey(Touche, IE_Pressed, this, &AArmesChelles::Changer);
	B.bConsumeInput = false;
	bLieClavier = true;
}

void AArmesChelles::Changer()
{
	APlayerController* PC = GetPC();
	if (!PC || PC->bShowMouseCursor)
	{
		return; // menu ouvert
	}
	if (!Reglages.bTirsSpeciaux)
	{
		Message(TEXT("Tirs speciaux coupes (TirsSpeciaux=Non dans Armes.txt)."));
		return;
	}
	AActor* Arme = ArmeEnMain(PC->GetPawn());
	if (!Arme && EstHeros(PC->GetPawn()))
	{
		// Heros sans arme : on passe au tir suivant (jamais "aucun", ses mains sont son arme)
		uint8 Suivant = uint8(TirDuHeros() + 1);
		if (Suivant >= NombreTirs)
		{
			Suivant = Laser;
		}
		ChoisirTirHeros(Suivant);
		Message(FString::Printf(TEXT("TIR DES MAINS : %s  -  clic gauche = tirer, %s = changer"), NomTir(Suivant), *Reglages.ToucheChanger));
		return;
	}
	if (!Arme)
	{
		Message(TEXT("Prends une arme pour choisir un tir special."));
		return;
	}
	const uint8 Suivant = uint8((TirChoisi(Arme) + 1) % NombreTirs);
	ChoisirTir(Arme, Suivant);
	if (Suivant == Aucun)
	{
		Message(TEXT("TIR SPECIAL : AUCUN  (tir normal seulement)"));
	}
	else
	{
		Message(FString::Printf(TEXT("TIR SPECIAL : %s  (en plus du tir normal)  -  %s = changer"), NomTir(Suivant), *Reglages.ToucheChanger));
	}
}

void AArmesChelles::SuivreTirs()
{
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	AActor* Arme = ArmeEnMain(Pion);
	bCurseur = PC && PC->bShowMouseCursor;

	if (Arme && !Habillees.Contains(Arme))
	{
		Habiller(Arme);
	}
	if (Arme != ArmeSuivie.Get())
	{
		ArmeSuivie = Arme;
		MunitionsVues = -1;
	}
	Etiquette.Reset();
	if (!Arme && Reglages.bTirsSpeciaux && EstHeros(Pion))
	{
		// Heros sans arme (PANTERA) : clic gauche = tir special depuis les mains
		const uint8 TirMains = TirDuHeros();
		Etiquette = FString::Printf(TEXT("MAINS : %s   (clic gauche, %s = changer)"), NomTir(TirMains), *Reglages.ToucheChanger);
		UWorld* Monde = GetWorld();
		if (!PC || PC->bShowMouseCursor || !Monde || Monde->IsPaused() || !PC->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			return;
		}
		const float Instant = Monde->GetTimeSeconds();
		if (Instant - DernierTirLocal[TirMains] < Reglages.Tirs[TirMains].Cadence)
		{
			return;
		}
		DernierTirLocal[TirMains] = Instant;
		TirerMains(TirMains, PC, Pion);
		return;
	}
	if (!Arme || !Reglages.bTirsSpeciaux)
	{
		return;
	}
	const uint8 Tir = TirChoisi(Arme);
	if (Tir != Aucun)
	{
		Etiquette = FString::Printf(TEXT("+ %s   (%s)"), NomTir(Tir), *Reglages.ToucheChanger);
	}

	// L'arme a-t-elle tire ? Ses munitions baissent. (Munitions infinies : on regarde le clic gauche.)
	bool bTir = false;
	bool bInfini = false;
	LireBool(Arme, TEXT("InfiniteAmmo"), bInfini);
	int64 Munitions = 0;
	if (!bInfini && LireEntier(Arme, TEXT("AmmoCurrent"), Munitions))
	{
		bTir = MunitionsVues >= 0 && Munitions < MunitionsVues;
		MunitionsVues = Munitions;
	}
	else
	{
		bTir = PC->IsInputKeyDown(EKeys::LeftMouseButton) && !PC->bShowMouseCursor;
	}

	UWorld* W = GetWorld();
	if (!bTir || Tir == Aucun || !W || W->IsPaused())
	{
		return;
	}
	const float Maintenant = W->GetTimeSeconds();
	if (Maintenant - DernierTirLocal[Tir] < Reglages.Tirs[Tir].Cadence)
	{
		return;
	}
	DernierTirLocal[Tir] = Maintenant;
	TirerLocal(Tir, Arme, PC, Pion);
}

void AArmesChelles::TirerLocal(uint8 Tir, AActor* Arme, APlayerController* PC, APawn* Pion)
{
	AArmesRelais* Relais = MonRelais();
	if (!Relais)
	{
		return;
	}
	FVector Camera;
	FRotator Regard;
	PC->GetPlayerViewPoint(Camera, Regard);
	const FVector Avant = Regard.Vector();

	// Depart : le bout du canon (vue 1re personne si l'arme FPP est affichee)
	USceneComponent* MaillageFPP = Cast<USceneComponent>(TrouverComposant(Arme, TEXT("Weapon_Mesh_FPP")));
	const bool bFPP = MaillageFPP && MaillageFPP->GetVisibleFlag() && !MaillageFPP->bHiddenInGame;
	USceneComponent* Bouche = Cast<USceneComponent>(TrouverComposant(Arme, bFPP ? TEXT("ProjectileSpawnFromMuzzleFPP") : TEXT("ProjectileSpawnFromMuzzleTPP")));
	const FVector Origine = Bouche ? Bouche->GetComponentLocation() : Camera + Avant * 60.f;
	TirerDepuis(Tir, Origine, PC, Pion);
}

void AArmesChelles::TirerDepuis(uint8 Tir, const FVector& Origine, APlayerController* PC, APawn* Pion)
{
	AArmesRelais* Relais = MonRelais();
	if (!Relais || !PC)
	{
		return;
	}
	FVector Camera;
	FRotator Regard;
	PC->GetPlayerViewPoint(Camera, Regard);
	const FVector Avant = Regard.Vector();

	// Cible : ce que vise le centre de l'ecran
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ViseeArmesChelles), false, Pion);
	TArray<AActor*> Attaches;
	if (Pion)
	{
		Pion->GetAttachedActors(Attaches);
	}
	Params.AddIgnoredActors(Attaches);
	FHitResult Impact;
	const FVector Loin = Camera + Avant * 30000.f;
	const FVector Cible = GetWorld()->LineTraceSingleByChannel(Impact, Camera, Loin, ECC_Visibility, Params) ? FVector(Impact.ImpactPoint) : Loin;
	FVector Direction = (Cible - Origine).GetSafeNormal();
	if (FVector::DotProduct(Direction, Avant) < 0.3f)
	{
		Direction = Avant;
	}
	Relais->ServeurTir(Tir, Origine, Direction);
}

// =====================================================================================
//  Heros sans arme (PANTERA, 2026-11-11) : tirs speciaux depuis les mains
// =====================================================================================

bool AArmesChelles::EstHeros(APawn* Pion) const
{
	const ACharacter* Perso = Cast<ACharacter>(Pion);
	const USkeletalMeshComponent* Corps = Perso ? Perso->GetMesh() : nullptr;
	if (!Corps || !Corps->SkeletalMesh || Corps->bHiddenInGame)
	{
		return false;
	}
	const FString Nom = Corps->SkeletalMesh->GetName();
	for (const FString& H : Reglages.Heros)
	{
		if (Nom.Contains(H))
		{
			return true;
		}
	}
	return false;
}

uint8 AArmesChelles::TirDuHeros()
{
	static const FString Cle(TEXT("HEROS"));
	if (const uint8* Deja = Choix.Find(Cle))
	{
		return *Deja;
	}
	uint8 Tir = Reglages.TirHeros;
	int32 Sauve = -1;
	if (GConfig && GConfig->GetInt(TEXT("ChellesArmes"), TEXT("Tir.HEROS"), Sauve, GGameUserSettingsIni) && Sauve > Aucun && Sauve < NombreTirs)
	{
		Tir = uint8(Sauve); // choix fait avec la touche O lors d'une partie precedente
	}
	if (Tir == Aucun || Tir >= NombreTirs)
	{
		Tir = Laser;
	}
	Choix.Add(Cle, Tir);
	return Tir;
}

void AArmesChelles::ChoisirTirHeros(uint8 Tir)
{
	Choix.Add(TEXT("HEROS"), Tir);
	if (GConfig)
	{
		GConfig->SetInt(TEXT("ChellesArmes"), TEXT("Tir.HEROS"), Tir, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void AArmesChelles::TirerMains(uint8 Tir, APlayerController* PC, APawn* Pion)
{
	ACharacter* Perso = Cast<ACharacter>(Pion);
	USkeletalMeshComponent* Corps = Perso ? Perso->GetMesh() : nullptr;
	if (!Corps)
	{
		return;
	}
	// Une main puis l'autre
	bMainGauche = !bMainGauche;
	const FName Main = bMainGauche ? FName(TEXT("hand_l")) : FName(TEXT("hand_r"));
	FVector Camera;
	FRotator Regard;
	PC->GetPlayerViewPoint(Camera, Regard);
	FVector Origine = Corps->DoesSocketExist(Main) ? Corps->GetSocketLocation(Main) : Pion->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	Origine += Regard.Vector() * 30.f; // un peu devant la main : le tir ne touche pas son propre corps
	TirerDepuis(Tir, Origine, PC, Pion);
}

// =====================================================================================
//  Serveur
// =====================================================================================

bool AArmesChelles::PvpActif() const
{
	if (GetNetMode() == NM_Standalone)
	{
		return true; // en solo il n'y a que des passants, des zombies et des ennemis
	}
	bool bPvp = true; // reglage PVP du menu (celui qui heberge decide)
	if (GConfig)
	{
		GConfig->GetBool(TEXT("ChellesMenu"), TEXT("Pvp"), bPvp, GGameUserSettingsIni);
	}
	return bPvp;
}

void AArmesChelles::GererRelais()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	TArray<APlayerController*> Servis;
	TArray<AArmesRelais*> Relais;
	for (TActorIterator<AArmesRelais> It(W); It; ++It)
	{
		Relais.Add(*It);
	}
	for (AArmesRelais* R : Relais)
	{
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		if (!PC)
		{
			R->Destroy(); // joueur parti
			continue;
		}
		Servis.Add(PC);
	}
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || Servis.Contains(PC))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = PC;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		W->SpawnActor<AArmesRelais>(AArmesRelais::StaticClass(), FTransform::Identity, Params);
	}
}

void AArmesChelles::TirServeur(AArmesRelais* Relais, uint8 Type, FVector Origine, FVector Direction)
{
	UWorld* W = GetWorld();
	if (!W || !Relais || !Reglages.bTirsSpeciaux || Type == Aucun || Type >= NombreTirs)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Relais->GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion)
	{
		return;
	}
	const FCaracTir& C = Reglages.Tirs[Type];
	const float Maintenant = W->GetTimeSeconds();
	if (Maintenant - Relais->DernierTir[Type] < C.Cadence * 0.8f)
	{
		return; // trop rapide
	}
	Relais->DernierTir[Type] = Maintenant;

	if (FVector::Dist(Origine, Pion->GetActorLocation()) > 600.f)
	{
		Origine = Pion->GetPawnViewLocation();
	}
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Pion->GetControlRotation().Vector();
	}

	const FTransform Depart(Direction.Rotation(), Origine);
	FActorSpawnParameters Params;
	Params.Owner = Pion;
	Params.Instigator = Pion;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ATirSpecialChelles* Tir = W->SpawnActor<ATirSpecialChelles>(ATirSpecialChelles::StaticClass(), Depart, Params);
	if (!Tir)
	{
		return;
	}
	Tir->Type = Type;
	Tir->Vitesse = C.Vitesse;
	Tir->Degats = C.Degats;
	Tir->DegatsZone = C.DegatsZone;
	Tir->Rayon = C.Rayon;
	Tir->DureeGel = C.DureeGel;
	Tir->Rebonds = C.Rebonds;
	Tir->bToucherJoueurs = PvpActif();
	Tir->FinishSpawning(Depart);
}

// =====================================================================================
//  Console : Chelles.Arme ...
// =====================================================================================

void AArmesChelles::Commande(const TArray<FString>& Arguments)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FString Ordre = Arguments.Num() > 0 ? Arguments[0].ToLower() : FString();
	const float Valeur = Arguments.Num() > 1 ? FCString::Atof(*Arguments[1]) : 0.f;

	if (Ordre == TEXT("recharger"))
	{
		ToutRecharger();
		Choix.Reset();
		Message(TEXT("Armes.txt relu, apparences refaites."));
		return;
	}

	APlayerController* PC = GetPC();
	AActor* Arme = ArmeEnMain(PC ? PC->GetPawn() : nullptr);
	if (!Arme)
	{
		Message(TEXT("Prends une arme en main, puis tape par exemple : Chelles.Arme Roulis 180"));
		return;
	}

	if (Ordre == TEXT("tir"))
	{
		const uint8 Tir = Arguments.Num() > 1 ? TirDepuisTexte(Arguments[1]) : uint8(Aucun);
		ChoisirTir(Arme, Tir);
		Message(FString::Printf(TEXT("%s : tir special %s"), *NomArme(Arme), NomTir(Tir)));
		return;
	}

	const FString Cle = TrouverCle(Arme->GetClass(), Reglages.Apparences);
	if (Cle.IsEmpty())
	{
		Message(FString::Printf(TEXT("%s n'a pas de nouvelle apparence (ajoutez Arme.%s=... dans Armes.txt)"), *NomArme(Arme), *NomArme(Arme)));
		return;
	}

	FReglageApparence Reg = Reglages.Reglages.FindRef(Cle);
	bool bChange = true;
	if (Ordre == TEXT("avant")) Reg.Avant += Valeur;
	else if (Ordre == TEXT("haut")) Reg.Haut += Valeur;
	else if (Ordre == TEXT("cote")) Reg.Cote += Valeur;
	else if (Ordre == TEXT("tangage")) Reg.Tangage += Valeur;
	else if (Ordre == TEXT("lacet")) Reg.Lacet += Valeur;
	else if (Ordre == TEXT("roulis")) Reg.Roulis += Valeur;
	else if (Ordre == TEXT("echelle")) Reg.Echelle = FMath::Clamp(Reg.Echelle * (Valeur > 0.f ? Valeur : 1.f), 0.05f, 20.f);
	else if (Ordre == TEXT("zero")) Reg = FReglageApparence();
	else if (Ordre == TEXT("original"))
	{
		EcrireLigne(TEXT("Arme.") + Cle, TEXT("Aucune"));
		ToutRecharger();
		Message(FString::Printf(TEXT("%s garde son apparence d'origine (Armes.txt mis a jour)."), *Cle));
		return;
	}
	else
	{
		bChange = false;
	}

	if (!bChange)
	{
		Message(FString::Printf(TEXT("%s : reglage %s  |  Chelles.Arme Avant/Haut/Cote <cm>, Tangage/Lacet/Roulis <degres>, Echelle <x>, Zero, Original, Tir <Laser|Feu|Glace|Foudre|Aucun>, Recharger"),
			*Cle, *Reg.VersTexte()));
		return;
	}

	Reglages.Reglages.Add(Cle, Reg);
	EcrireLigne(TEXT("Reglage.") + Cle, Reg.VersTexte());
	for (auto& Paire : Habillees)
	{
		if (Paire.Value.Cle != Cle)
		{
			continue;
		}
		for (FArmesPaire& P : Paire.Value.Paires)
		{
			if (P.Origine.IsValid() && P.Nouveau.IsValid() && P.Modele.IsValid())
			{
				P.Nouveau->SetRelativeTransform(Ajustement(P.Origine.Get(), P.Modele.Get(), Reg, Paire.Key.Get()));
			}
		}
	}
	Message(FString::Printf(TEXT("%s : %s  (enregistre dans Armes.txt)"), *Cle, *Reg.VersTexte()));
}

// =====================================================================================
//  Petit texte a l'ecran
// =====================================================================================

void AArmesChelles::CreerEcran()
{
	UWorld* W = GetWorld();
	if (Ecran.IsValid() || !W || GetNetMode() == NM_DedicatedServer || !FSlateApplication::IsInitialized())
	{
		return;
	}
	UGameViewportClient* Vue = W->GetGameViewport();
	if (!Vue)
	{
		return;
	}
	TWeakObjectPtr<AArmesChelles> Moi(this);
	Ecran = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 150.f))
		.Visibility(EVisibility::HitTestInvisible)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
			.ShadowOffset(FVector2D(2.f, 2.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			.Justification(ETextJustify::Center)
			.ColorAndOpacity_Lambda([Moi]()
			{
				if (Moi.IsValid() && Moi->AgeInfo < 3.5f)
				{
					return FSlateColor(FLinearColor(1.f, 0.85f, 0.3f, FMath::Clamp(3.5f - Moi->AgeInfo, 0.f, 1.f)));
				}
				return FSlateColor(FLinearColor(0.75f, 0.95f, 1.f, 0.85f));
			})
			.Text_Lambda([Moi]()
			{
				if (!Moi.IsValid() || Moi->bCurseur)
				{
					return FText::GetEmpty();
				}
				if (Moi->AgeInfo < 3.5f)
				{
					return FText::FromString(Moi->Info);
				}
				return FText::FromString(Moi->Etiquette);
			})
		];
	Vue->AddViewportWidgetContent(Ecran.ToSharedRef(), 4);
}

void AArmesChelles::RetirerEcran()
{
	if (!Ecran.IsValid())
	{
		return;
	}
	if (UWorld* W = GetWorld())
	{
		if (UGameViewportClient* Vue = W->GetGameViewport())
		{
			Vue->RemoveViewportWidgetContent(Ecran.ToSharedRef());
		}
	}
	Ecran.Reset();
}
