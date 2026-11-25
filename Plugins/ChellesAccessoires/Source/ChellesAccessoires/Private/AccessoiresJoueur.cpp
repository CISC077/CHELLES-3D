// CHELLES 3D - Accessoires du joueur : lampe torche, jumelles, grappin, kit de soin (voir AccessoiresChelles.h)
#include "AccessoiresChelles.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Styling/CoreStyle.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChellesAccessoires"

namespace
{
	// Style lobby bleu (meme palette que les menus ARMEMENTS / HABILLAGE)
	const FLinearColor AccBleuNuit = FLinearColor::FromSRGBColor(FColor(8, 18, 58, 235));
	const FLinearColor AccBleuRoi = FLinearColor::FromSRGBColor(FColor(30, 90, 230));
	const FLinearColor AccBleuClair = FLinearColor::FromSRGBColor(FColor(90, 170, 255));
	const FLinearColor AccJaune = FLinearColor::FromSRGBColor(FColor(255, 214, 40));
	const FLinearColor AccVert = FLinearColor::FromSRGBColor(FColor(70, 230, 110));
	const FLinearColor AccGris = FLinearColor(0.35f, 0.38f, 0.45f, 1.f);

	const TCHAR* AccNomsAffiches[ChellesAccessoires::Nombre] = { TEXT("LAMPE TORCHE"), TEXT("JUMELLES"), TEXT("GRAPPIN"), TEXT("KIT DE SOIN") };

	FSlateFontInfo AccPolice(int32 Taille, bool bGras = true)
	{
		return FCoreStyle::GetDefaultFontStyle(bGras ? "Bold" : "Regular", Taille);
	}

	/** Nom du fichier de sauvegarde de CE joueur (meme regle que les autres plugins : -Joueur=Nom, fenetres PIE). */
	FString AccEmplacement(const UObject* Contexte, const TCHAR* Base)
	{
		FString Nom = Base;
		FString Joueur;
		if (FParse::Value(FCommandLine::Get(), TEXT("Joueur="), Joueur))
		{
			FString Propre;
			for (const TCHAR C : Joueur)
			{
				if (FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-'))
				{
					Propre.AppendChar(C);
				}
			}
			if (!Propre.IsEmpty())
			{
				return Nom + TEXT("_") + Propre.Left(32);
			}
		}
		const UWorld* Monde = Contexte ? Contexte->GetWorld() : nullptr;
		if (Monde && Monde->WorldType == EWorldType::PIE)
		{
			const int32 Instance = Monde->GetOutermost()->PIEInstanceID;
			if (Instance > 0)
			{
				Nom += FString::Printf(TEXT("_PIE%d"), Instance + 1);
			}
		}
		return Nom;
	}

	/** Nom court de la 1re touche (pour l'afficher sur la carte). */
	FString AccNomTouche(uint8 Type)
	{
		const TArray<FKey>& T = ReglagesAccessoires().Touches[Type];
		if (T.Num() == 0)
		{
			return TEXT("-");
		}
		const FString N = T[0].GetFName().ToString();
		if (N == TEXT("Zero")) return TEXT("0");
		if (N == TEXT("One")) return TEXT("1");
		if (N == TEXT("Two")) return TEXT("2");
		if (N == TEXT("Three")) return TEXT("3");
		if (N == TEXT("Four")) return TEXT("4");
		if (N == TEXT("Five")) return TEXT("5");
		if (N == TEXT("Six")) return TEXT("6");
		if (N == TEXT("Seven")) return TEXT("7");
		if (N == TEXT("Eight")) return TEXT("8");
		if (N == TEXT("Nine")) return TEXT("9");
		return T[0].GetDisplayName().ToString();
	}

	// ---- Vie du personnage par reflexion (memes noms que ChellesHud / ChellesPouvoirs)
	FNumericProperty* AccProprieteVie(UObject* Objet, std::initializer_list<const TCHAR*> Noms)
	{
		if (!Objet)
		{
			return nullptr;
		}
		for (const TCHAR* Nom : Noms)
		{
			if (FNumericProperty* P = FindFProperty<FNumericProperty>(Objet->GetClass(), FName(Nom)))
			{
				return P;
			}
		}
		return nullptr;
	}

	double AccLire(UObject* Objet, FNumericProperty* P)
	{
		void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		return P->IsFloatingPoint() ? P->GetFloatingPointPropertyValue(Adresse) : double(P->GetSignedIntPropertyValue(Adresse));
	}

	void AccEcrire(UObject* Objet, FNumericProperty* P, double Valeur)
	{
		void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		if (P->IsFloatingPoint())
		{
			P->SetFloatingPointPropertyValue(Adresse, Valeur);
		}
		else
		{
			P->SetIntPropertyValue(Adresse, int64(FMath::RoundToInt(float(Valeur))));
		}
	}

	/** Cherche un widget Slate (par son type, ex. SMenuArmements) visible sous la fenetre du jeu. */
	bool AccChercherSlate(const TSharedRef<SWidget>& W, const TArray<FString>& Noms, int32 Profondeur)
	{
		if (!W->GetVisibility().IsVisible())
		{
			return false;
		}
		const FString Type = W->GetTypeAsString();
		for (const FString& N : Noms)
		{
			if (!N.IsEmpty() && N.StartsWith(TEXT("S")) && Type.Equals(N, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		if (Profondeur <= 0)
		{
			return false;
		}
		FChildren* Enfants = W->GetChildren();
		if (!Enfants)
		{
			return false;
		}
		for (int32 i = 0; i < Enfants->Num(); ++i)
		{
			if (AccChercherSlate(Enfants->GetChildAt(i), Noms, Profondeur - 1))
			{
				return true;
			}
		}
		return false;
	}
}

/** Voit les clics et la molette avant le reste de l'ecran. */
class FFiltreAccessoires : public IInputProcessor
{
public:
	explicit FFiltreAccessoires(AAccessoiresJoueur* InJoueur) : Joueur(InJoueur) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Evenement) override
	{
		AAccessoiresJoueur* J = Joueur.Get();
		if (J && Evenement.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			return J->Clic(Evenement.GetScreenSpacePosition());
		}
		return false;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Evenement, const FPointerEvent* Geste) override
	{
		AAccessoiresJoueur* J = Joueur.Get();
		if (J && Geste == nullptr && Evenement.GetWheelDelta() != 0.f)
		{
			return J->Molette(Evenement.GetWheelDelta());
		}
		return false;
	}

private:
	TWeakObjectPtr<AAccessoiresJoueur> Joueur;
};

// ====================================================================== Joueur

AAccessoiresJoueur::AAccessoiresJoueur()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true; // le menu ARMEMENTS peut mettre le jeu en pause
	bReplicates = true;
	bAlwaysRelevant = true; // la lampe et le cable se voient chez tout le monde
	SetReplicateMovement(false);
	SetCanBeDamaged(false);
	NetUpdateFrequency = 20.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Images.SetNum(ChellesAccessoires::Nombre);
}

void AAccessoiresJoueur::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAccessoiresJoueur, Pion);
	DOREPLIFETIME(AAccessoiresJoueur, bLampeAllumee);
	DOREPLIFETIME(AAccessoiresJoueur, bGrappinActif);
	DOREPLIFETIME(AAccessoiresJoueur, PointGrappin);
}

bool AAccessoiresJoueur::EstLocal() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	return PC && PC->IsLocalController();
}

bool AAccessoiresJoueur::Possede(uint8 Type) const
{
	if (Type == ChellesAccessoires::Kit)
	{
		return Kits > 0;
	}
	return Type < ChellesAccessoires::Nombre && (Possedes & (1 << Type)) != 0;
}

FString AAccessoiresJoueur::Resume() const
{
	return FString::Printf(TEXT("Joueur %s : lampe %s (%s), jumelles %s, grappin %s, %d kit(s) de soin"), *GetNameSafe(GetOwner()),
		Possede(ChellesAccessoires::Lampe) ? TEXT("oui") : TEXT("non"), bLampeAllumee ? TEXT("allumee") : TEXT("eteinte"),
		Possede(ChellesAccessoires::Jumelles) ? TEXT("oui") : TEXT("non"),
		Possede(ChellesAccessoires::Grappin) ? TEXT("oui") : TEXT("non"), Kits);
}

// ---------------------------------------------------------------------- Initialisation (machine du joueur)

void AAccessoiresJoueur::InitialiserLocal(APlayerController* PC)
{
	bLocalPret = true;
	const FReglagesAccessoires& R = ReglagesAccessoires();

	// ---- Ce que le joueur possede
	NomSauvegarde = AccEmplacement(this, TEXT("ChellesAccessoires"));
	UAccessoiresSauvegarde* Donnees = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(NomSauvegarde, 0))
	{
		Donnees = Cast<UAccessoiresSauvegarde>(UGameplayStatics::LoadGameFromSlot(NomSauvegarde, 0));
	}
	if (Donnees)
	{
		Possedes = Donnees->Possedes;
		Kits = Donnees->Kits;
	}
	if (!Donnees || !Donnees->bDepartDonne)
	{
		Kits = FMath::Max(Kits, R.KitsDepart);
	}
	for (int32 i = 0; i < ChellesAccessoires::Kit; ++i)
	{
		if (R.bDepart[i])
		{
			Possedes |= (1 << i); // donnes des le depart (Accessoires.txt)
		}
	}
	Kits = FMath::Clamp(Kits, 0, R.KitsMax);
	Sauvegarder();
	EnvoyerMasque();

	// ---- Icones (pack Craft Resources Icons)
	for (int32 i = 0; i < ChellesAccessoires::Nombre; ++i)
	{
		UTexture2D* T = R.Icones[i].IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *R.Icones[i], nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!T && !R.Icones[i].IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Accessoires] Icone introuvable : %s"), *R.Icones[i]);
		}
		Images[i] = T;
		Brosses[i] = FSlateBrush();
		if (T)
		{
			Brosses[i].SetResourceObject(T);
			Brosses[i].ImageSize = FVector2D(64.f, 64.f);
		}
	}
	CreerMasqueJumelles();
	CreerEcran();

	if (FSlateApplication::IsInitialized())
	{
		Filtre = MakeShared<FFiltreAccessoires>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Filtre);
	}
	UE_LOG(LogTemp, Log, TEXT("[Accessoires] %s"), *Resume());
}

void AAccessoiresJoueur::CreerMasqueJumelles()
{
	// Cache noir des jumelles : deux ronds transparents, fabrique ici (aucun fichier a importer)
	const int32 L = 512;
	const int32 H = 256;
	UTexture2D* T = UTexture2D::CreateTransient(L, H, PF_B8G8R8A8);
	if (!T || !T->PlatformData || T->PlatformData->Mips.Num() == 0)
	{
		return;
	}
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(L * H);
	const FVector2D Centre1(0.70f, 0.5f);
	const FVector2D Centre2(1.30f, 0.5f);
	const float Rayon = 0.44f;
	const float Flou = 0.03f;
	for (int32 y = 0; y < H; ++y)
	{
		for (int32 x = 0; x < L; ++x)
		{
			const FVector2D P(2.f * (x + 0.5f) / L, (y + 0.5f) / H);
			const float D = FMath::Min(FVector2D::Distance(P, Centre1), FVector2D::Distance(P, Centre2));
			const float Alpha = FMath::Clamp((D - Rayon) / Flou, 0.f, 1.f);
			Pixels[y * L + x] = FColor(0, 0, 0, uint8(FMath::RoundToInt(Alpha * 255.f)));
		}
	}
	FTexture2DMipMap& Mip = T->PlatformData->Mips[0];
	void* Donnees = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Donnees, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	T->SRGB = true;
	T->UpdateResource();
	MasqueJumelles = T;
	BrosseMasque = FSlateBrush();
	BrosseMasque.SetResourceObject(T);
	BrosseMasque.ImageSize = FVector2D(L, H);
}

const FSlateBrush* AAccessoiresJoueur::BrosseIcone(uint8 Type) const
{
	if (Type < ChellesAccessoires::Nombre && Images.IsValidIndex(Type) && Images[Type])
	{
		return &Brosses[Type];
	}
	return FCoreStyle::Get().GetBrush("WhiteBrush");
}

UTexture2D* AAccessoiresJoueur::Icone(uint8 Type) const
{
	return Images.IsValidIndex(Type) ? Images[Type] : nullptr;
}

void AAccessoiresJoueur::CreerEcran()
{
	UWorld* Monde = GetWorld();
	UGameViewportClient* LaVue = Monde ? Monde->GetGameViewport() : nullptr;
	if (!LaVue)
	{
		return;
	}
	const FReglagesAccessoires& R = ReglagesAccessoires();
	TWeakObjectPtr<AAccessoiresJoueur> Moi(this);

	// Place de la colonne ACCESSOIRES (Accessoires.txt : Case=)
	EHorizontalAlignment HA = HAlign_Left;
	EVerticalAlignment VA = VAlign_Bottom;
	const FString& Place = R.PlaceCase;
	if (Place.Contains(TEXT("Droite"), ESearchCase::IgnoreCase)) { HA = HAlign_Right; }
	if (Place.Contains(TEXT("Haut"), ESearchCase::IgnoreCase)) { VA = VAlign_Top; }
	else if (Place.Contains(TEXT("Centre"), ESearchCase::IgnoreCase) || Place.Contains(TEXT("Milieu"), ESearchCase::IgnoreCase)) { VA = VAlign_Center; }

	// Colonne verticale de cartes (regle des menus : disposition inversee)
	TSharedRef<SVerticalBox> Colonne = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor::White)
			.Padding(FMargin(14.f, 4.f))
			.RenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FShear2D::FromShearAngles(FVector2D(-8.f, 0.f)))))
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Titre", "ACCESSOIRES"))
				.Font(AccPolice(16))
				.ColorAndOpacity(FSlateColor(AccBleuNuit))
			]
		];
	for (uint8 i = 0; i < ChellesAccessoires::Nombre; ++i)
	{
		const uint8 Type = i;
		TSharedRef<SWidget> Carte = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([Moi, Type]()
			{
				if (!Moi.IsValid()) return FSlateColor(AccBleuRoi);
				if (Moi->Survol(Type)) return FSlateColor(AccJaune);
				if (!Moi->Possede(Type)) return FSlateColor(AccGris);
				const bool bActif = (Type == ChellesAccessoires::Lampe && Moi->LampeAllumee()) || (Type == ChellesAccessoires::Jumelles && Moi->JumellesActives());
				return FSlateColor(bActif ? AccVert : AccBleuClair);
			})
			.Padding(FMargin(2.f))
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(AccBleuNuit)
				.Padding(FMargin(8.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
					[
						SNew(SBox).WidthOverride(64.f).HeightOverride(64.f)
						[
							SNew(SImage)
							.Image_Lambda([Moi, Type]() { return Moi.IsValid() ? Moi->BrosseIcone(Type) : nullptr; })
							.ColorAndOpacity_Lambda([Moi, Type]()
							{
								if (!Moi.IsValid()) return FSlateColor(FLinearColor::White);
								if (!Moi->Icone(Type)) return FSlateColor(AccBleuRoi);
								return FSlateColor(Moi->Possede(Type) ? FLinearColor::White : FLinearColor(0.25f, 0.25f, 0.3f, 1.f));
							})
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
							[
								SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.BorderBackgroundColor(AccJaune)
								.Padding(FMargin(6.f, 0.f))
								[
									SNew(STextBlock)
									.Text(FText::FromString(AccNomTouche(Type)))
									.Font(AccPolice(12))
									.ColorAndOpacity(FSlateColor(AccBleuNuit))
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(AccNomsAffiches[Type]))
								.Font(AccPolice(14))
								.ColorAndOpacity(FSlateColor(FLinearColor::White))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
						[
							SNew(STextBlock)
							.Text_Lambda([Moi, Type]() { return Moi.IsValid() ? Moi->TexteCarte(Type) : FText(); })
							.Font(AccPolice(10, false))
							.ColorAndOpacity(FSlateColor(AccBleuClair))
							.AutoWrapText(true)
						]
					]
				]
			];
		Cartes[Type] = Carte;
		Colonne->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[Carte];
	}

	Ecran = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		// Cache des jumelles (plein ecran)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SImage)
			.Visibility_Lambda([Moi]() { return Moi.IsValid() && Moi->JumellesActives() && ReglagesAccessoires().bCacheJumelles && Moi->BrosseCache() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.Image_Lambda([Moi]() { return Moi.IsValid() ? Moi->BrosseCache() : nullptr; })
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 60.f))
		[
			SNew(STextBlock)
			.Visibility_Lambda([Moi]() { return Moi.IsValid() && Moi->JumellesActives() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.Text_Lambda([Moi]()
			{
				const float Z = Moi.IsValid() ? Moi->ZoomActuel() : 1.f;
				return FText::Format(LOCTEXT("Zoom", "JUMELLES  x{0}   -   molette = zoom   -   {1} = ranger"),
					FText::AsNumber(FMath::RoundToInt(Z)), FText::FromString(AccNomTouche(ChellesAccessoires::Jumelles)));
			})
			.Font(AccPolice(14))
			.ColorAndOpacity(FSlateColor(AccJaune))
			.ShadowColorAndOpacity(FLinearColor::Black)
			.ShadowOffset(FVector2D(1.5f, 1.5f))
		]
		// Colonne ACCESSOIRES (seulement avec le menu ARMEMENTS / inventaire)
		+ SOverlay::Slot()
		.HAlign(HA)
		.VAlign(VA)
		.Padding(FMargin(40.f, 110.f, 40.f, 40.f))
		[
			SNew(SBox)
			.WidthOverride(300.f)
			.Visibility_Lambda([Moi]() { return Moi.IsValid() && Moi->bColonneVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				Colonne
			]
		]
		// Message (gain, soin, refus)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 190.f, 0.f, 0.f))
		[
			SNew(SBorder)
			.Visibility_Lambda([Moi]() { return Moi.IsValid() && Moi->bMessageVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(AccBleuNuit)
			.Padding(FMargin(24.f, 10.f))
			[
				SNew(STextBlock)
				.Text_Lambda([Moi]() { return Moi.IsValid() ? Moi->TexteMessage : FText(); })
				.ColorAndOpacity_Lambda([Moi]() { return FSlateColor(Moi.IsValid() ? Moi->CouleurMessage : FLinearColor::White); })
				.Font(AccPolice(16))
				.Justification(ETextJustify::Center)
			]
		];
	LaVue->AddViewportWidgetContent(Ecran.ToSharedRef(), 70);
	Vue = LaVue;
}

FText AAccessoiresJoueur::TexteCarte(uint8 Type) const
{
	const FReglagesAccessoires& R = ReglagesAccessoires();
	if (Type == ChellesAccessoires::Kit)
	{
		if (Kits <= 0)
		{
			return LOCTEXT("KitVide", "Aucun kit : a gagner dans les coffres de butin");
		}
		return FText::Format(LOCTEXT("Kits", "{0} / {1} kits - rend {2} de vie"), FText::AsNumber(Kits), FText::AsNumber(R.KitsMax), FText::AsNumber(FMath::RoundToInt(R.SoinVie)));
	}
	if (!Possede(Type))
	{
		return LOCTEXT("AGagner", "A GAGNER dans les coffres de butin");
	}
	switch (Type)
	{
	case ChellesAccessoires::Lampe:
		return bLampeAllumee ? LOCTEXT("LampeOn", "ALLUMEE - clic pour eteindre") : LOCTEXT("LampeOff", "Eteinte - clic pour allumer");
	case ChellesAccessoires::Jumelles:
		return FText::Format(LOCTEXT("Jum", "Zoom x{0} puis x{1}"), FText::AsNumber(FMath::RoundToInt(R.ZoomJumelles1)), FText::AsNumber(FMath::RoundToInt(R.ZoomJumelles2)));
	case ChellesAccessoires::Grappin:
		return FText::Format(LOCTEXT("Grap", "Vise un mur ou un toit ({0} m max)"), FText::AsNumber(FMath::RoundToInt(R.GrappinPortee)));
	default:
		return FText();
	}
}

// ---------------------------------------------------------------------- Boucle

void AAccessoiresJoueur::EndPlay(const EEndPlayReason::Type Raison)
{
	if (bLocalPret)
	{
		ReglerZoom(1.f);
		Sauvegarder();
	}
	if (Filtre.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Filtre);
	}
	Filtre.Reset();
	if (Ecran.IsValid() && Vue.IsValid())
	{
		Vue->RemoveViewportWidgetContent(Ecran.ToSharedRef());
	}
	Ecran.Reset();
	if (Lumiere)
	{
		Lumiere->DestroyComponent();
		Lumiere = nullptr;
	}
	if (Cable)
	{
		Cable->DestroyComponent();
		Cable = nullptr;
	}
	Super::EndPlay(Raison);
}

void AAccessoiresJoueur::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!ReglagesAccessoires().bActif)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (HasAuthority() && PC)
	{
		APawn* Actuel = PC->GetPawn();
		if (Pion != Actuel)
		{
			Pion = Actuel;
			bGrappinActif = false;
		}
	}
	if (PC && PC->IsLocalController())
	{
		if (!bLocalPret)
		{
			InitialiserLocal(PC);
		}
		TickLocal(DeltaSeconds, PC);
	}
	TickGrappinMouvement(DeltaSeconds);
	TickVisuel(DeltaSeconds);
}

void AAccessoiresJoueur::TickLocal(float DeltaSeconds, APlayerController* PC)
{
	const FReglagesAccessoires& R = ReglagesAccessoires();
	UWorld* Monde = GetWorld();
	const float Maintenant = Monde->GetRealTimeSeconds();

	// ---- Colonne ACCESSOIRES : seulement quand le menu ARMEMENTS / inventaire est ouvert
	if (Maintenant >= ProchainControleMenu)
	{
		ProchainControleMenu = Maintenant + 0.25f;
		bColonneVisible = MenuInventaireOuvert();
	}

	// ---- Clic sur une carte (vu par le filtre)
	if (ActionDemandee >= 0)
	{
		const uint8 Type = uint8(ActionDemandee);
		ActionDemandee = -1;
		if (Type == ChellesAccessoires::Jumelles || Type == ChellesAccessoires::Grappin)
		{
			FermerMenus(PC); // il faut voir le jeu pour viser
		}
		Utiliser(Type);
	}

	// ---- Touches 7 8 9 0 (pas pendant un menu)
	if (!bColonneVisible && !Monde->IsPaused())
	{
		for (uint8 Type = 0; Type < ChellesAccessoires::Nombre; ++Type)
		{
			for (const FKey& K : R.Touches[Type])
			{
				if (PC->WasInputKeyJustPressed(K))
				{
					Utiliser(Type);
					break;
				}
			}
		}
	}

	// ---- Jumelles : on garde le zoom (une autre camera peut le remettre) ; on range si le perso change
	if (Zoom > 1.f)
	{
		UCameraComponent* Camera = CameraDuPion();
		if (Camera != CameraZoom.Get())
		{
			ReglerZoom(1.f);
		}
		else
		{
			ReglerZoom(Zoom);
		}
	}

	if (bMessageVisible && Maintenant >= FinMessage)
	{
		bMessageVisible = false;
	}
}

bool AAccessoiresJoueur::MenuInventaireOuvert() const
{
	const TArray<FString>& Noms = ReglagesAccessoires().MenusInventaire;

	// 1) Menu ARMEMENTS (Slate, plugin ChellesRaccourcis) : on cherche SMenuArmements dans la fenetre du jeu
	if (Vue.IsValid())
	{
		TSharedPtr<SWindow> Fenetre = Vue->GetWindow();
		if (Fenetre.IsValid() && AccChercherSlate(Fenetre.ToSharedRef(), Noms, 14))
		{
			return true;
		}
	}

	// 2) Anciens menus UMG (W_WeaponInventory_UI...)
	const UWorld* Monde = GetWorld();
	TArray<UObject*> Objets;
	GetObjectsOfClass(UUserWidget::StaticClass(), Objets, true, RF_ClassDefaultObject | RF_ArchetypeObject, EInternalObjectFlags::PendingKill);
	for (UObject* O : Objets)
	{
		UUserWidget* W = Cast<UUserWidget>(O);
		if (!W || W->GetWorld() != Monde || (!W->IsInViewport() && !W->GetParent()) || !W->IsVisible())
		{
			continue;
		}
		for (const UClass* K = W->GetClass(); K && K != UUserWidget::StaticClass(); K = K->GetSuperClass())
		{
			const FString Nom = K->GetName();
			for (const FString& Morceau : Noms)
			{
				if (!Morceau.StartsWith(TEXT("S")) && Nom.StartsWith(Morceau, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void AAccessoiresJoueur::FermerMenus(APlayerController* PC)
{
	if (PC && bColonneVisible)
	{
		// Rien ne se passe si ces plugins sont absents
		PC->ConsoleCommand(TEXT("Chelles.Armements Fermer"), false);
		PC->ConsoleCommand(TEXT("Chelles.Inventaire Fermer"), false);
		bColonneVisible = false;
	}
}

// ---------------------------------------------------------------------- Utilisation

void AAccessoiresJoueur::Utiliser(uint8 Type)
{
	if (Type >= ChellesAccessoires::Nombre)
	{
		return;
	}
	if (!Possede(Type))
	{
		AfficherMessage(Type == ChellesAccessoires::Kit
			? LOCTEXT("PasDeKit", "Plus de KIT DE SOIN : ouvre des coffres de butin")
			: FText::Format(LOCTEXT("PasEncore", "{0} : a gagner dans les coffres de butin"), FText::FromString(AccNomsAffiches[Type])),
			3.f, AccJaune);
		return;
	}
	switch (Type)
	{
	case ChellesAccessoires::Lampe: BasculerLampe(); break;
	case ChellesAccessoires::Jumelles: BasculerJumelles(); break;
	case ChellesAccessoires::Grappin: TirerGrappin(); break;
	case ChellesAccessoires::Kit: Soigner(); break;
	default: break;
	}
}

void AAccessoiresJoueur::BasculerLampe()
{
	const bool bAllumer = !bLampeAllumee;
	bLampeAllumee = bAllumer; // tout de suite chez soi, le serveur le dit aux autres
	ServeurLampe(bAllumer);
}

void AAccessoiresJoueur::ServeurLampe_Implementation(bool bAllumer)
{
	bLampeAllumee = bAllumer && (MasqueServeur & (1 << ChellesAccessoires::Lampe)) != 0;
}

UCameraComponent* AAccessoiresJoueur::CameraDuPion() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	AActor* Cible = PC ? PC->GetViewTarget() : nullptr;
	if (!Cible)
	{
		return nullptr;
	}
	TInlineComponentArray<UCameraComponent*> Cameras(Cible);
	for (UCameraComponent* C : Cameras)
	{
		if (C && C->IsActive())
		{
			return C;
		}
	}
	return nullptr;
}

void AAccessoiresJoueur::BasculerJumelles()
{
	const FReglagesAccessoires& R = ReglagesAccessoires();
	if (Zoom <= 1.f)
	{
		ReglerZoom(R.ZoomJumelles1);
	}
	else if (Zoom < R.ZoomJumelles2 - 0.01f)
	{
		ReglerZoom(R.ZoomJumelles2);
	}
	else
	{
		ReglerZoom(1.f);
	}
}

void AAccessoiresJoueur::ReglerZoom(float NouveauZoom)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		return;
	}
	auto FOVZoom = [](float Base, float Z)
	{
		const float Demi = FMath::DegreesToRadians(Base * 0.5f);
		return FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(Demi) / FMath::Max(1.f, Z)));
	};

	if (NouveauZoom <= 1.f)
	{
		// On range les jumelles : on remet la vue d'avant
		if (Zoom > 1.f)
		{
			if (UCameraComponent* C = CameraZoom.Get())
			{
				C->SetFieldOfView(FOVCameraAvant);
			}
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->UnlockFOV();
			}
		}
		Zoom = 1.f;
		CameraZoom.Reset();
		return;
	}

	UCameraComponent* Camera = CameraDuPion();
	if (Zoom <= 1.f)
	{
		// Premiere mise : on retient la vue normale
		CameraZoom = Camera;
		FOVCameraAvant = Camera ? Camera->FieldOfView : 90.f;
		FOVAvant = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 90.f;
	}
	Zoom = NouveauZoom;
	if (Camera && Camera == CameraZoom.Get())
	{
		Camera->SetFieldOfView(FOVZoom(FOVCameraAvant, Zoom));
	}
	else if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->SetFOV(FOVZoom(FOVAvant, Zoom));
	}
}

bool AAccessoiresJoueur::Molette(float Sens)
{
	if (Zoom <= 1.f || bColonneVisible)
	{
		return false;
	}
	const FReglagesAccessoires& R = ReglagesAccessoires();
	const float Max = FMath::Max(R.ZoomJumelles1, R.ZoomJumelles2);
	ReglerZoom(FMath::Clamp(Zoom + (Sens > 0.f ? 1.f : -1.f), 2.f, Max));
	return true; // la molette ne change pas d'arme pendant les jumelles
}

void AAccessoiresJoueur::TirerGrappin()
{
	const FReglagesAccessoires& R = ReglagesAccessoires();
	UWorld* Monde = GetWorld();
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Perso || !Perso->GetCharacterMovement())
	{
		AfficherMessage(LOCTEXT("GrapPerso", "Le grappin ne marche qu'a pied"), 2.5f, AccJaune);
		return;
	}
	if (bTireLocal)
	{
		// 2e appui pendant la traction = on lache
		bTireLocal = false;
		Perso->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		ServeurFinGrappin();
		return;
	}
	const float Maintenant = Monde->GetTimeSeconds();
	if (Maintenant < ProchainGrappin)
	{
		return;
	}
	FVector Oeil;
	FRotator Regard;
	PC->GetPlayerViewPoint(Oeil, Regard);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrappinChelles), true, Perso);
	FHitResult Touche;
	const FVector Fin = Oeil + Regard.Vector() * (R.GrappinPortee * 100.f);
	if (!Monde->LineTraceSingleByChannel(Touche, Oeil, Fin, ECC_Visibility, Params) || FVector::Dist(Touche.ImpactPoint, Perso->GetActorLocation()) < 200.f)
	{
		AfficherMessage(FText::Format(LOCTEXT("GrapRien", "Rien a accrocher (vise un mur a moins de {0} m)"), FText::AsNumber(FMath::RoundToInt(R.GrappinPortee))), 2.f, AccJaune);
		return;
	}
	ProchainGrappin = Maintenant + R.GrappinAttente;
	// Point un peu decolle du mur, pour ne pas rentrer dedans
	PointLocal = Touche.ImpactPoint + Touche.ImpactNormal * 60.f;
	bTireLocal = true;
	TempsGrappin = 0.f;
	TempsBloque = 0.f;
	DistancePrecedente = FVector::Dist(PointLocal, Perso->GetActorLocation());
	Perso->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	ServeurGrappin(PointLocal);
}

void AAccessoiresJoueur::ServeurGrappin_Implementation(FVector Point)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	const FReglagesAccessoires& R = ReglagesAccessoires();
	if (!Perso || (MasqueServeur & (1 << ChellesAccessoires::Grappin)) == 0
		|| FVector::Dist(Perso->GetActorLocation(), Point) > R.GrappinPortee * 100.f + 800.f)
	{
		return;
	}
	PointGrappin = Point;
	bGrappinActif = true;
	TempsGrappin = 0.f;
	if (UCharacterMovementComponent* M = Perso->GetCharacterMovement())
	{
		if (!Perso->IsLocallyControlled())
		{
			// Joueur distant : c'est sa machine qui le tire, le serveur accepte sa position pendant ce temps
			M->bIgnoreClientMovementErrorChecksAndCorrection = true;
			M->bServerAcceptClientAuthoritativePosition = true;
			M->SetMovementMode(MOVE_Flying);
		}
	}
}

void AAccessoiresJoueur::ServeurFinGrappin_Implementation()
{
	bGrappinActif = false;
	TempsGrappin = 0.f;
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (Perso && !Perso->IsLocallyControlled())
	{
		if (UCharacterMovementComponent* M = Perso->GetCharacterMovement())
		{
			M->SetMovementMode(MOVE_Falling);
			// On garde la tolerance encore un instant (le temps de retomber), puis on la remet
			TWeakObjectPtr<UCharacterMovementComponent> Faible(M);
			FTimerHandle Minuteur;
			GetWorldTimerManager().SetTimer(Minuteur, FTimerDelegate::CreateLambda([Faible]()
			{
				if (Faible.IsValid())
				{
					Faible->bIgnoreClientMovementErrorChecksAndCorrection = false;
					Faible->bServerAcceptClientAuthoritativePosition = false;
				}
			}), 1.5f, false);
		}
	}
}

void AAccessoiresJoueur::TickGrappinMouvement(float DeltaSeconds)
{
	// Serveur : securite si la machine du joueur ne dit jamais "fini"
	if (HasAuthority() && bGrappinActif && !EstLocal())
	{
		TempsGrappin += DeltaSeconds;
		if (TempsGrappin > 6.f)
		{
			ServeurFinGrappin_Implementation();
		}
	}
	if (!bTireLocal)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	UCharacterMovementComponent* M = Perso ? Perso->GetCharacterMovement() : nullptr;
	if (!M)
	{
		bTireLocal = false;
		ServeurFinGrappin();
		return;
	}
	TempsGrappin += DeltaSeconds;
	const FVector Ecart = PointLocal - Perso->GetActorLocation();
	const float Distance = Ecart.Size();
	if (Distance > DistancePrecedente - 2.f)
	{
		TempsBloque += DeltaSeconds; // on n'avance plus (obstacle)
	}
	else
	{
		TempsBloque = 0.f;
	}
	DistancePrecedente = Distance;

	if (Distance < 150.f || TempsGrappin > 5.f || TempsBloque > 0.35f)
	{
		// Arrive : petit saut pour passer le rebord d'un toit
		bTireLocal = false;
		M->SetMovementMode(MOVE_Falling);
		M->Velocity = Ecart.GetSafeNormal2D() * 350.f + FVector(0.f, 0.f, 520.f);
		ServeurFinGrappin();
		return;
	}
	if (M->MovementMode != MOVE_Flying)
	{
		M->SetMovementMode(MOVE_Flying);
	}
	M->Velocity = Ecart / Distance * ReglagesAccessoires().GrappinVitesse;
}

void AAccessoiresJoueur::Soigner()
{
	const FReglagesAccessoires& R = ReglagesAccessoires();
	const float Maintenant = GetWorld()->GetTimeSeconds();
	if (Maintenant < ProchainSoin)
	{
		return;
	}
	ProchainSoin = Maintenant + R.SoinAttente;
	ServeurSoigner();
}

void AAccessoiresJoueur::ServeurSoigner_Implementation()
{
	const float Maintenant = GetWorld()->GetTimeSeconds();
	if (Maintenant < ProchainSoinServeur)
	{
		return;
	}
	ProchainSoinServeur = Maintenant + FMath::Max(0.f, ReglagesAccessoires().SoinAttente - 0.3f);
	const float Rendu = SoignerPion(ReglagesAccessoires().SoinVie);
	ClientSoigne(Rendu);
}

float AAccessoiresJoueur::SoignerPion(float Quantite)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	APawn* LePion = PC ? PC->GetPawn() : Pion;
	if (!LePion)
	{
		return 0.f;
	}
	// La vie peut etre sur le personnage ou sur un de ses composants
	TArray<UObject*> Porteurs;
	Porteurs.Add(LePion);
	TInlineComponentArray<UActorComponent*> Composants(LePion);
	for (UActorComponent* C : Composants)
	{
		Porteurs.Add(C);
	}
	for (UObject* Porteur : Porteurs)
	{
		FNumericProperty* Vie = AccProprieteVie(Porteur, { TEXT("HealthCurrent"), TEXT("Health"), TEXT("CurrentHealth"), TEXT("Vie") });
		if (!Vie)
		{
			continue;
		}
		FNumericProperty* VieMax = AccProprieteVie(Porteur, { TEXT("HealthMax"), TEXT("MaxHealth"), TEXT("VieMax") });
		const double Actuelle = AccLire(Porteur, Vie);
		const double Max = VieMax ? AccLire(Porteur, VieMax) : 100.0;
		if (Actuelle <= 0.0 || Actuelle >= Max)
		{
			return 0.f; // mort (pas de resurrection) ou vie deja pleine
		}
		const double Nouvelle = FMath::Min(Max, Actuelle + double(Quantite));
		AccEcrire(Porteur, Vie, Nouvelle);
		UE_LOG(LogTemp, Log, TEXT("[Accessoires] %s soigne : %.0f -> %.0f"), *GetNameSafe(LePion), Actuelle, Nouvelle);
		return float(Nouvelle - Actuelle);
	}
	UE_LOG(LogTemp, Warning, TEXT("[Accessoires] %s : pas de variable de vie trouvee (Health, HealthCurrent...)"), *GetNameSafe(LePion));
	return 0.f;
}

void AAccessoiresJoueur::ClientSoigne_Implementation(float VieRendue)
{
	if (VieRendue <= 0.f)
	{
		AfficherMessage(LOCTEXT("ViePleine", "Vie deja pleine : kit garde"), 2.f, AccBleuClair);
		ProchainSoin = 0.f;
		return;
	}
	Kits = FMath::Max(0, Kits - 1);
	Sauvegarder();
	AfficherMessage(FText::Format(LOCTEXT("Soigne", "+{0} VIE   (kits : {1})"), FText::AsNumber(FMath::RoundToInt(VieRendue)), FText::AsNumber(Kits)), 2.5f, AccVert);
	JouerSon(ReglagesAccessoires().SonSoin);
}

// ---------------------------------------------------------------------- Gains

void AAccessoiresJoueur::Donner(uint8 Type, int32 Quantite, bool bMessage)
{
	const FReglagesAccessoires& R = ReglagesAccessoires();
	if (Type == ChellesAccessoires::Kit)
	{
		const int32 Avant = Kits;
		Kits = FMath::Clamp(Kits + Quantite, 0, R.KitsMax);
		if (bMessage)
		{
			AfficherMessage(Kits > Avant
				? FText::Format(LOCTEXT("GainKit", "+{0} KIT DE SOIN   ({1}/{2})"), FText::AsNumber(Kits - Avant), FText::AsNumber(Kits), FText::AsNumber(R.KitsMax))
				: LOCTEXT("KitPlein", "Sac de kits de soin plein"), 3.f, AccVert);
		}
	}
	else if (Type < ChellesAccessoires::Nombre)
	{
		const bool bNouveau = (Possedes & (1 << Type)) == 0;
		Possedes |= (1 << Type);
		if (bMessage && bNouveau)
		{
			AfficherMessage(FText::Format(LOCTEXT("GainAcc", "NOUVEL ACCESSOIRE : {0}  (touche {1})"),
				FText::FromString(AccNomsAffiches[Type]), FText::FromString(AccNomTouche(Type))), 4.f, AccJaune);
		}
	}
	if (bMessage)
	{
		JouerSon(R.SonGain);
	}
	Sauvegarder();
	EnvoyerMasque();
}

void AAccessoiresJoueur::ToutDonner()
{
	Possedes = (1 << ChellesAccessoires::Kit) - 1;
	Donner(ChellesAccessoires::Kit, ReglagesAccessoires().KitsMax, true);
}

void AAccessoiresJoueur::ClientGagner_Implementation(uint8 Type, int32 Quantite)
{
	Donner(Type, Quantite, true);
}

void AAccessoiresJoueur::ServeurMesAccessoires_Implementation(int32 Masque)
{
	MasqueServeur = Masque;
	if ((Masque & (1 << ChellesAccessoires::Lampe)) == 0)
	{
		bLampeAllumee = false;
	}
}

void AAccessoiresJoueur::EnvoyerMasque()
{
	ServeurMesAccessoires(Possedes);
}

void AAccessoiresJoueur::Sauvegarder()
{
	if (NomSauvegarde.IsEmpty())
	{
		return;
	}
	UAccessoiresSauvegarde* Donnees = Cast<UAccessoiresSauvegarde>(UGameplayStatics::CreateSaveGameObject(UAccessoiresSauvegarde::StaticClass()));
	if (!Donnees)
	{
		return;
	}
	Donnees->Possedes = Possedes;
	Donnees->Kits = Kits;
	Donnees->bDepartDonne = true;
	if (!UGameplayStatics::SaveGameToSlot(Donnees, NomSauvegarde, 0))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Accessoires] impossible d'ecrire %s.sav"), *NomSauvegarde);
	}
}

// ---------------------------------------------------------------------- Ecran / sons

void AAccessoiresJoueur::AfficherMessage(const FText& Texte, float Duree, const FLinearColor& Couleur)
{
	TexteMessage = Texte;
	CouleurMessage = Couleur;
	bMessageVisible = true;
	FinMessage = GetWorld()->GetRealTimeSeconds() + Duree;
}

void AAccessoiresJoueur::JouerSon(const FString& Chemin)
{
	if (Chemin.IsEmpty())
	{
		return;
	}
	if (USoundBase* Son = LoadObject<USoundBase>(nullptr, *Chemin, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySound2D(this, Son, 0.7f);
	}
}

bool AAccessoiresJoueur::Survol(uint8 Type) const
{
	if (!bColonneVisible || Type >= ChellesAccessoires::Nombre || !Cartes[Type].IsValid() || !FSlateApplication::IsInitialized())
	{
		return false;
	}
	const FGeometry& Geo = Cartes[Type]->GetTickSpaceGeometry();
	return Geo.GetLocalSize().X > 1.f && Geo.IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

bool AAccessoiresJoueur::Clic(const FVector2D& PositionEcran)
{
	if (!bColonneVisible)
	{
		return false;
	}
	for (uint8 Type = 0; Type < ChellesAccessoires::Nombre; ++Type)
	{
		if (!Cartes[Type].IsValid())
		{
			continue;
		}
		const FGeometry& Geo = Cartes[Type]->GetTickSpaceGeometry();
		if (Geo.GetLocalSize().X > 1.f && Geo.IsUnderLocation(PositionEcran))
		{
			ActionDemandee = Type; // fait a l'image suivante (pas pendant l'evenement Slate)
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------- Lampe et cable (chaque machine)

void AAccessoiresJoueur::TickVisuel(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return; // rien a afficher sur le serveur dedie
	}
	MettreAJourLampe();
	MettreAJourCable();
}

void AAccessoiresJoueur::MettreAJourLampe()
{
	const bool bVoir = bLampeAllumee && IsValid(Pion);
	if (!bVoir)
	{
		if (Lumiere && Lumiere->IsVisible())
		{
			Lumiere->SetVisibility(false);
		}
		return;
	}
	const FReglagesAccessoires& R = ReglagesAccessoires();
	if (!Lumiere)
	{
		Lumiere = NewObject<USpotLightComponent>(this, TEXT("LampeTorche"));
		Lumiere->SetMobility(EComponentMobility::Movable);
		Lumiere->SetupAttachment(RootComponent);
		Lumiere->RegisterComponent();
		Lumiere->SetAbsolute(true, true, true);
	}
	Lumiere->SetIntensity(R.LampeIntensite);
	Lumiere->SetOuterConeAngle(R.LampeAngle);
	Lumiere->SetInnerConeAngle(R.LampeAngle * 0.55f);
	Lumiere->SetAttenuationRadius(R.LampePortee);
	Lumiere->SetLightColor(R.LampeCouleur);
	Lumiere->SetCastShadows(R.bLampeOmbres);
	if (!Lumiere->IsVisible())
	{
		Lumiere->SetVisibility(true);
	}

	FVector Lieu;
	FRotator Regard;
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC && PC->IsLocalController())
	{
		PC->GetPlayerViewPoint(Lieu, Regard);
		// La lampe part de la poitrine du perso (pas de la camera 3e personne derriere lui)
		Lieu = Pion->GetPawnViewLocation() - FVector(0.f, 0.f, 25.f);
	}
	else
	{
		Lieu = Pion->GetPawnViewLocation() - FVector(0.f, 0.f, 25.f);
		Regard = Pion->GetBaseAimRotation();
	}
	Lumiere->SetWorldLocationAndRotation(Lieu + Regard.Vector() * 30.f, Regard);
}

void AAccessoiresJoueur::MettreAJourCable()
{
	const bool bLocalActif = bTireLocal && EstLocal();
	const bool bVoir = IsValid(Pion) && (bLocalActif || bGrappinActif);
	if (!bVoir)
	{
		if (Cable && Cable->IsVisible())
		{
			Cable->SetVisibility(false);
		}
		return;
	}
	if (!Cable)
	{
		UStaticMesh* Cylindre = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (!Cylindre)
		{
			return;
		}
		Cable = NewObject<UStaticMeshComponent>(this, TEXT("CableGrappin"));
		Cable->SetMobility(EComponentMobility::Movable);
		Cable->SetStaticMesh(Cylindre);
		Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cable->SetCastShadow(false);
		Cable->SetupAttachment(RootComponent);
		Cable->RegisterComponent();
		Cable->SetAbsolute(true, true, true);
	}
	const FVector Point = bLocalActif ? PointLocal : FVector(PointGrappin);
	const FVector Depart = Pion->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const FVector Ecart = Point - Depart;
	const float Longueur = Ecart.Size();
	if (Longueur < 10.f)
	{
		Cable->SetVisibility(false);
		return;
	}
	// Le cylindre du moteur fait 100 de haut et 100 de large, centre sur son milieu
	const FRotator Sens = FRotationMatrix::MakeFromZ(Ecart / Longueur).Rotator();
	Cable->SetWorldLocationAndRotation(Depart + Ecart * 0.5f, Sens);
	Cable->SetWorldScale3D(FVector(0.035f, 0.035f, Longueur / 100.f));
	if (!Cable->IsVisible())
	{
		Cable->SetVisibility(true);
	}
}

// ====================================================================== Gestionnaire (serveur)

AAccessoiresChelles::AAccessoiresChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = false;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void AAccessoiresChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* Monde = GetWorld();
	if (!Monde || !ReglagesAccessoires().bActif)
	{
		return;
	}
	// Joueurs partis : on enleve leur relais
	for (auto It = Joueurs.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			if (AAccessoiresJoueur* Ancien = It->Value.Get())
			{
				Ancien->Destroy();
			}
			It.RemoveCurrent();
		}
	}
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		TWeakObjectPtr<AAccessoiresJoueur>& Relais = Joueurs.FindOrAdd(PC);
		if (Relais.IsValid())
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = PC;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Relais = Monde->SpawnActor<AAccessoiresJoueur>(AAccessoiresJoueur::StaticClass(), FTransform::Identity, Params);
	}
	if (ReglagesAccessoires().bButin)
	{
		SurveillerCoffres();
	}
}

void AAccessoiresChelles::SurveillerCoffres()
{
	UWorld* Monde = GetWorld();
	const float Maintenant = Monde->GetTimeSeconds();
	// Classe ACoffreButin du plugin ChellesButin, trouvee par son nom (aucune dependance)
	if (!ClasseCoffre.IsValid())
	{
		if (Maintenant < ProchaineRechercheClasse)
		{
			return;
		}
		ProchaineRechercheClasse = Maintenant + 10.f;
		ClasseCoffre = FindObject<UClass>(ANY_PACKAGE, TEXT("CoffreButin"));
		if (!ClasseCoffre.IsValid())
		{
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("[Accessoires] Coffres de butin trouves : les accessoires et kits sont a gagner dedans"));
	}
	UClass* Classe = ClasseCoffre.Get();
	FBoolProperty* POuvert = FindFProperty<FBoolProperty>(Classe, TEXT("bOuvert"));
	FByteProperty* PRarete = FindFProperty<FByteProperty>(Classe, TEXT("Rarete"));
	if (!POuvert)
	{
		return;
	}
	for (auto It = CoffresVus.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	const FReglagesAccessoires& R = ReglagesAccessoires();
	for (TActorIterator<AActor> It(Monde, Classe); It; ++It)
	{
		AActor* Coffre = *It;
		if (!POuvert->GetPropertyValue_InContainer(Coffre) || CoffresVus.Contains(Coffre))
		{
			continue;
		}
		CoffresVus.Add(Coffre);
		const int32 Rarete = FMath::Clamp(PRarete ? int32(PRarete->GetPropertyValue_InContainer(Coffre)) : 0, 0, 2);

		// Le joueur le plus proche du coffre est celui qui l'a ouvert
		AAccessoiresJoueur* Gagnant = nullptr;
		float Meilleure = FMath::Square(R.DistanceCoffre * 100.f);
		for (auto& Paire : Joueurs)
		{
			AAccessoiresJoueur* J = Paire.Value.Get();
			APawn* P = J ? J->Pion : nullptr;
			if (!P)
			{
				continue;
			}
			const float D = FVector::DistSquared(P->GetActorLocation(), Coffre->GetActorLocation());
			if (D < Meilleure)
			{
				Meilleure = D;
				Gagnant = J;
			}
		}
		if (!Gagnant)
		{
			continue;
		}
		if (FMath::RandRange(1, 100) <= R.ChanceKit[Rarete] && R.KitsParCoffre[Rarete] > 0)
		{
			Gagnant->ClientGagner(ChellesAccessoires::Kit, R.KitsParCoffre[Rarete]);
		}
		if (FMath::RandRange(1, 100) <= R.ChanceAccessoire[Rarete])
		{
			TArray<uint8> Manquants;
			for (uint8 T = 0; T < ChellesAccessoires::Kit; ++T)
			{
				if (!Gagnant->ServeurPossede(T))
				{
					Manquants.Add(T);
				}
			}
			if (Manquants.Num() > 0)
			{
				Gagnant->ClientGagner(Manquants[FMath::RandRange(0, Manquants.Num() - 1)], 1);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
