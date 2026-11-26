// CHELLES 3D - Mode photo (26/11/2026). Voir ModePhotoChelles.h
#include "ModePhotoChelles.h"
#include "SModePhotoChelles.h"

#include "Async/Async.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"

DEFINE_LOG_CATEGORY_STATIC(LogChellesPhoto, Log, All);

namespace
{
	struct FFiltrePhoto
	{
		const TCHAR* Nom;
		float Saturation;
		float Contraste;
		FLinearColor Gain;
		float Vignette;
		float Grain;
	};

	static const FFiltrePhoto GFiltres[] =
	{
		{ TEXT("NORMAL"),          1.0f,  1.0f,  FLinearColor(1.f, 1.f, 1.f),      0.f,  0.f  },
		{ TEXT("NOIR ET BLANC"),   0.0f,  1.15f, FLinearColor(1.f, 1.f, 1.f),      0.3f, 0.15f },
		{ TEXT("SÉPIA"),           0.0f,  1.05f, FLinearColor(1.12f, 0.95f, 0.72f), 0.45f, 0.2f },
		{ TEXT("CINÉMA"),          0.85f, 1.2f,  FLinearColor(1.f, 0.98f, 0.95f),   0.8f, 0.3f },
		{ TEXT("COULEURS VIVES"),  1.6f,  1.1f,  FLinearColor(1.f, 1.f, 1.f),      0.f,  0.f  },
		{ TEXT("ÉTÉ À CHELLES"),   1.2f,  1.05f, FLinearColor(1.15f, 1.02f, 0.82f), 0.25f, 0.f },
		{ TEXT("NUIT BLEUE"),      0.8f,  1.1f,  FLinearColor(0.8f, 0.9f, 1.2f),   0.5f, 0.1f },
	};
	static const int32 GNombreFiltres = UE_ARRAY_COUNT(GFiltres);

	static TUniquePtr<FModePhotoChelles> GModePhoto;
}

// ------------------------------------------------------------------ pre-processeur

class FPreProcesseurPhoto : public IInputProcessor
{
public:
	explicit FPreProcesseurPhoto(FModePhotoChelles& InMode) : Mode(InMode) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& Slate, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& Slate, const FKeyEvent& Evenement) override
	{
		return Mode.ToucheAppuyee(Slate, Evenement.GetKey(), Evenement.IsRepeat());
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& Slate, const FKeyEvent& Evenement) override
	{
		return Mode.ToucheRelachee(Evenement.GetKey());
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& Slate, const FPointerEvent& Evenement) override
	{
		return Mode.SourisBouge(Evenement.GetCursorDelta());
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& Slate, const FPointerEvent& Evenement) override
	{
		return Mode.ToucheAppuyee(Slate, Evenement.GetEffectingButton(), false);
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& Slate, const FPointerEvent& Evenement) override
	{
		return Mode.ToucheRelachee(Evenement.GetEffectingButton());
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& Slate, const FPointerEvent& Evenement, const FPointerEvent* Geste) override
	{
		return Mode.Molette(Evenement.GetWheelDelta());
	}

private:
	FModePhotoChelles& Mode;
};

// ------------------------------------------------------------------ cycle de vie

FModePhotoChelles& FModePhotoChelles::Get()
{
	if (!GModePhoto.IsValid())
	{
		GModePhoto = MakeUnique<FModePhotoChelles>();
		GModePhoto->LireReglages();
	}
	return *GModePhoto;
}

void FModePhotoChelles::Detruire()
{
	if (!GModePhoto.IsValid())
	{
		return;
	}
	if (GModePhoto->bActif)
	{
		GModePhoto->Quitter();
	}
	if (GModePhoto->PreProcesseur.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(GModePhoto->PreProcesseur);
	}
	GModePhoto.Reset();
}

void FModePhotoChelles::LireReglages()
{
	Reglages = FReglagesPhoto();
	Reglages.Partage = TEXT("Partage tes photos : TikTok @meridja77  -  Discord discord.gg/MtUru8vyg");

	FString Chemin;
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ChellesPhoto"));
	if (Plugin.IsValid())
	{
		Chemin = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Photo.txt"));
	}
	TArray<FString> Lignes;
	if (Chemin.IsEmpty() || !FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		UE_LOG(LogChellesPhoto, Log, TEXT("[Photo] Photo.txt introuvable : reglages par defaut."));
		return;
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
		const bool bOui = Valeur.StartsWith(TEXT("O"), ESearchCase::IgnoreCase) || Valeur == TEXT("1");
		if (Cle == TEXT("FigerEnSolo")) { Reglages.bFigerEnSolo = bOui; }
		else if (Cle == TEXT("Vitesse")) { Reglages.Vitesse = FMath::Max(10.f, FCString::Atof(*Valeur)); }
		else if (Cle == TEXT("Sensibilite")) { Reglages.Sensibilite = FMath::Clamp(FCString::Atof(*Valeur.Replace(TEXT(","), TEXT("."))), 0.01f, 2.f); }
		else if (Cle == TEXT("DistanceMax")) { Reglages.DistanceMax = FMath::Max(0.f, FCString::Atof(*Valeur)); }
		else if (Cle == TEXT("Dossier")) { Reglages.Dossier = Valeur; }
		else if (Cle == TEXT("CadreTikTok")) { Reglages.bCadreTikTok = bOui; }
		else if (Cle == TEXT("Partage")) { Reglages.Partage = Valeur; }
	}
	UE_LOG(LogChellesPhoto, Log, TEXT("[Photo] Reglages lus : %s"), *Chemin);
}

FKey FModePhotoChelles::ToucheDuMode() const
{
	// Meme section que OPTIONS > CONTROLES (ChellesMenu, TouchesChelles.cpp, ligne "Photo")
	FString Nom;
	if (GConfig)
	{
		GConfig->GetString(TEXT("ChellesTouches"), TEXT("Photo"), Nom, GGameUserSettingsIni);
	}
	const FKey K = Nom.IsEmpty() ? FKey() : FKey(FName(*Nom));
	return K.IsValid() ? K : EKeys::F12;
}

FString FModePhotoChelles::DossierPhotos() const
{
	if (!Reglages.Dossier.IsEmpty())
	{
		return Reglages.Dossier;
	}
	return FPaths::Combine(FPlatformProcess::UserDir(), TEXT("CHELLES 3D - Photos"));
}

void FModePhotoChelles::Tick(float DeltaTime)
{
	if (!PreProcesseur.IsValid() && FSlateApplication::IsInitialized())
	{
		PreProcesseur = MakeShared<FPreProcesseurPhoto>(*this);
		// En premier : pendant le mode photo, le clavier ne doit plus atteindre le jeu ni les autres menus
		FSlateApplication::Get().RegisterInputPreProcessor(PreProcesseur, 0);
		UE_LOG(LogChellesPhoto, Log, TEXT("[Photo] Mode photo pret (touche %s)."), *ToucheDuMode().ToString());
	}
	if (!bActif)
	{
		return;
	}

	// Partie arretee, carte changee, joueur parti : on sort proprement
	if (!Monde.IsValid() || !PC.IsValid() || !Camera.IsValid() || !GEngine || !GEngine->GameViewport || GEngine->GameViewport->GetWorld() != Monde.Get())
	{
		Quitter();
		return;
	}

	const float Dt = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.1f); // temps reel : le monde peut etre fige

	// Deplacement
	FVector Dir = FVector::ZeroVector;
	const FRotationMatrix M(Rotation);
	const FVector Avant = M.GetUnitAxis(EAxis::X);
	const FVector Droite = M.GetUnitAxis(EAxis::Y);
	auto Tenue = [this](std::initializer_list<FKey> L)
	{
		for (const FKey& K : L)
		{
			if (Enfoncees.Contains(K))
			{
				return true;
			}
		}
		return false;
	};
	if (Tenue({ EKeys::Z, EKeys::W, EKeys::Up })) { Dir += Avant; }
	if (Tenue({ EKeys::S, EKeys::Down })) { Dir -= Avant; }
	if (Tenue({ EKeys::D, EKeys::Right })) { Dir += Droite; }
	if (Tenue({ EKeys::Q, EKeys::A, EKeys::Left })) { Dir -= Droite; }
	if (Tenue({ EKeys::SpaceBar, EKeys::E })) { Dir += FVector::UpVector; }
	if (Tenue({ EKeys::C, EKeys::LeftControl, EKeys::RightControl })) { Dir -= FVector::UpVector; }

	float Vitesse = Reglages.Vitesse;
	if (Tenue({ EKeys::LeftShift, EKeys::RightShift })) { Vitesse *= 3.f; }
	if (Tenue({ EKeys::LeftAlt, EKeys::RightAlt })) { Vitesse *= 0.25f; }

	FVector Pos = Camera->GetActorLocation();
	if (!Dir.IsNearlyZero())
	{
		Pos += Dir.GetSafeNormal() * Vitesse * Dt;
		if (Reglages.DistanceMax > 0.f)
		{
			const float Max = Reglages.DistanceMax * 100.f;
			const FVector Ecart = Pos - Depart;
			if (Ecart.Size() > Max)
			{
				Pos = Depart + Ecart.GetSafeNormal() * Max;
			}
		}
	}
	Camera->SetActorLocationAndRotation(Pos, Rotation);

	// Flou de fond : mise au point sur ce qui est au centre de l'image
	if (bFlou)
	{
		FHitResult Impact;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ChellesPhotoMiseAuPoint), true, Camera.Get());
		const FVector Fin = Pos + Rotation.Vector() * 50000.f;
		const float Distance = Monde->LineTraceSingleByChannel(Impact, Pos, Fin, ECC_Visibility, Params) ? Impact.Distance : 5000.f;
		UCameraComponent* Comp = Camera->GetCameraComponent();
		Comp->PostProcessSettings.DepthOfFieldFocalDistance = FMath::Max(Distance, 30.f);
	}

	// Un autre plugin a pu reprendre la vue : on la garde sur la camera photo
	if (PC->PlayerCameraManager && PC->PlayerCameraManager->GetViewTarget() != Camera.Get())
	{
		PC->PlayerCameraManager->SetViewTarget(Camera.Get());
	}

	if (bCaptureDemandee && !FScreenshotRequest::IsScreenshotRequested())
	{
		bCaptureDemandee = false;
	}
}

// ------------------------------------------------------------------ entrer / sortir

bool FModePhotoChelles::PeutEntrer(FSlateApplication& Slate) const
{
	UGameViewportClient* Vue = GEngine ? GEngine->GameViewport : nullptr;
	UWorld* W = Vue ? Vue->GetWorld() : nullptr;
	if (!W || !W->IsGameWorld())
	{
		return false;
	}
	if (Vue->ViewportConsole && Vue->ViewportConsole->ConsoleActive())
	{
		return false;
	}
	APlayerController* Joueur = W->GetFirstPlayerController();
	if (!Joueur || !Joueur->IsLocalController() || Joueur->bShowMouseCursor)
	{
		return false; // un menu est ouvert (pause, ARMEMENTS, OPTIONS...)
	}
	const TSharedPtr<SViewport> Fenetre = Vue->GetGameViewportWidget();
	bool bDansLeJeu = false;
	for (TSharedPtr<SWidget> Wid = Slate.GetUserFocusedWidget(0); Wid.IsValid(); Wid = Wid->GetParentWidget())
	{
		const FString Type = Wid->GetTypeAsString();
		if (Type.Contains(TEXT("EditableText")) || Type == TEXT("SMenuChelles"))
		{
			return false;
		}
		if (Fenetre.IsValid() && Wid == Fenetre)
		{
			bDansLeJeu = true;
		}
	}
	return bDansLeJeu || !GIsEditor;
}

void FModePhotoChelles::Basculer()
{
	if (bActif)
	{
		Quitter();
	}
	else if (FSlateApplication::IsInitialized())
	{
		Entrer();
	}
}

void FModePhotoChelles::Entrer()
{
	UGameViewportClient* Vue = GEngine ? GEngine->GameViewport : nullptr;
	UWorld* W = Vue ? Vue->GetWorld() : nullptr;
	APlayerController* Joueur = W ? W->GetFirstPlayerController() : nullptr;
	if (!Joueur || !Joueur->PlayerCameraManager)
	{
		return;
	}
	LireReglages();

	Monde = W;
	PC = Joueur;
	Perso = Joueur->GetPawn();
	CibleAvant = Joueur->PlayerCameraManager->GetViewTarget();

	// La camera part exactement de la vue du joueur
	Depart = Joueur->PlayerCameraManager->GetCameraLocation();
	Rotation = Joueur->PlayerCameraManager->GetCameraRotation();
	Rotation.Roll = 0.f;
	Fov = Joueur->PlayerCameraManager->GetFOVAngle();
	RotationDepart = Rotation;
	FovDepart = Fov;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ACameraActor* Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Depart, Rotation, Params);
	if (!Cam)
	{
		return;
	}
	Cam->SetActorTickEnabled(false);
	UCameraComponent* Comp = Cam->GetCameraComponent();
	Comp->bConstrainAspectRatio = false;
	Comp->SetFieldOfView(Fov);
	Comp->PostProcessBlendWeight = 1.f;
	Camera = Cam;

	// Le joueur lache toutes les touches (sinon il continue de courir pendant la photo)
	Joueur->FlushPressedKeys();
	Enfoncees.Reset();
	bClicDroit = false;

	Joueur->PlayerCameraManager->SetViewTarget(Cam);

	// Figer le monde : seulement en solo (en multi, le serveur decide du temps pour tout le monde)
	bFige = false;
	if (Reglages.bFigerEnSolo && W->GetNetMode() == NM_Standalone)
	{
		AWorldSettings* WS = W->GetWorldSettings();
		DilatationAvant = WS ? WS->TimeDilation : 1.f;
		UGameplayStatics::SetGlobalTimeDilation(W, 0.0001f);
		bFige = true;
	}

	bCadreTikTok = Reglages.bCadreTikTok;
	bGrille = false;
	bFlou = false;
	bPersoCache = false;
	bAide = true;
	AppliquerFiltre();

	// HUD du jeu et menus a l'ecran caches, notre aide par-dessus
	bHudAvant = Joueur->MyHUD ? Joueur->MyHUD->bShowHUD : true;
	if (Joueur->MyHUD)
	{
		Joueur->MyHUD->bShowHUD = false;
	}
	Ecran = SNew(SModePhotoChelles, this);
	Vue->AddViewportWidgetContent(Ecran.ToSharedRef(), 10000);
	CacherInterface(true);

	bActif = true;
	UE_LOG(LogChellesPhoto, Log, TEXT("[Photo] Mode photo ON (%s)."), bFige ? TEXT("monde fige") : TEXT("multijoueur, monde non fige"));
}

void FModePhotoChelles::Quitter()
{
	bActif = false;
	Enfoncees.Reset();

	CacherInterface(false);
	if (Ecran.IsValid())
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(Ecran.ToSharedRef());
		}
		Ecran.Reset();
	}

	if (Perso.IsValid() && bPersoCache)
	{
		Perso->SetActorHiddenInGame(false);
	}
	bPersoCache = false;

	if (bFige && Monde.IsValid())
	{
		UGameplayStatics::SetGlobalTimeDilation(Monde.Get(), DilatationAvant);
	}
	bFige = false;

	if (PC.IsValid())
	{
		if (PC->MyHUD)
		{
			PC->MyHUD->bShowHUD = bHudAvant;
		}
		AActor* Retour = CibleAvant.IsValid() ? CibleAvant.Get() : PC->GetPawn();
		if (PC->PlayerCameraManager && Retour)
		{
			PC->PlayerCameraManager->SetViewTarget(Retour);
		}
		else if (Retour)
		{
			PC->SetViewTarget(Retour);
		}
		PC->FlushPressedKeys();
	}

	if (Camera.IsValid())
	{
		Camera->Destroy();
	}
	if (PoigneeCapture.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(PoigneeCapture);
		PoigneeCapture.Reset();
	}
	bCaptureDemandee = false;
	Camera.Reset();
	PC.Reset();
	Perso.Reset();
	CibleAvant.Reset();
	Monde.Reset();
	UE_LOG(LogChellesPhoto, Log, TEXT("[Photo] Mode photo OFF."));
}

void FModePhotoChelles::CacherInterface(bool bCacher)
{
	if (!bCacher)
	{
		for (const TPair<TWeakPtr<SWidget>, float>& C : Caches)
		{
			if (TSharedPtr<SWidget> W = C.Key.Pin())
			{
				W->SetRenderOpacity(C.Value);
			}
		}
		Caches.Reset();
		return;
	}
	// Les widgets poses sur l'ecran (HUD CHELLES, mini-carte, chat...) sont freres de notre aide :
	// on les rend transparents le temps du mode photo (la photo, elle, ne contient jamais l'interface)
	Caches.Reset();
	TSharedPtr<SWidget> Parent = Ecran.IsValid() ? Ecran->GetParentWidget() : nullptr;
	if (!Parent.IsValid())
	{
		return;
	}
	FChildren* Enfants = Parent->GetChildren();
	for (int32 i = 0; Enfants && i < Enfants->Num(); ++i)
	{
		TSharedRef<SWidget> W = Enfants->GetChildAt(i);
		if (W == Ecran.ToSharedRef())
		{
			continue;
		}
		Caches.Add(TPair<TWeakPtr<SWidget>, float>(W, W->GetRenderOpacity()));
		W->SetRenderOpacity(0.f);
	}
}

// ------------------------------------------------------------------ filtres

void FModePhotoChelles::AppliquerFiltre()
{
	if (!Camera.IsValid())
	{
		return;
	}
	UCameraComponent* Comp = Camera->GetCameraComponent();
	FPostProcessSettings& P = Comp->PostProcessSettings;
	const FFiltrePhoto& F = GFiltres[FMath::Clamp(Filtre, 0, GNombreFiltres - 1)];

	P.bOverride_ColorSaturation = true;
	P.ColorSaturation = FVector4(F.Saturation, F.Saturation, F.Saturation, 1.f);
	P.bOverride_ColorContrast = true;
	P.ColorContrast = FVector4(F.Contraste, F.Contraste, F.Contraste, 1.f);
	P.bOverride_ColorGain = true;
	P.ColorGain = FVector4(F.Gain.R, F.Gain.G, F.Gain.B, 1.f);
	P.bOverride_VignetteIntensity = F.Vignette > 0.f;
	P.VignetteIntensity = F.Vignette;
	P.bOverride_GrainIntensity = F.Grain > 0.f;
	P.GrainIntensity = F.Grain;

	P.bOverride_DepthOfFieldFstop = bFlou;
	P.DepthOfFieldFstop = 1.8f;
	P.bOverride_DepthOfFieldFocalDistance = bFlou;
	if (!bFlou)
	{
		P.DepthOfFieldFocalDistance = 0.f;
	}
	Comp->SetFieldOfView(Fov);
}

// ------------------------------------------------------------------ photo

void FModePhotoChelles::PrendrePhoto()
{
	if (bCaptureDemandee)
	{
		return;
	}
	if (!PoigneeCapture.IsValid())
	{
		PoigneeCapture = UGameViewportClient::OnScreenshotCaptured().AddRaw(this, &FModePhotoChelles::PhotoRecue);
	}
	bCaptureDemandee = true;
	// false = sans l'interface : seule l'image du jeu est capturee (pas l'aide, pas le cadre)
	FScreenshotRequest::RequestScreenshot(false);
}

void FModePhotoChelles::PhotoRecue(int32 Largeur, int32 Hauteur, const TArray<FColor>& Pixels)
{
	// Une seule photo par demande : on se debranche tout de suite pour ne pas voler les captures des autres
	if (PoigneeCapture.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(PoigneeCapture);
		PoigneeCapture.Reset();
	}
	bCaptureDemandee = false;
	if (Largeur <= 0 || Hauteur <= 0 || Pixels.Num() < Largeur * Hauteur)
	{
		Message(TEXT("La photo a échoué, réessaie."));
		return;
	}

	// Recadrage TikTok 9:16 (au centre)
	int32 X0 = 0;
	int32 L = Largeur;
	if (bCadreTikTok)
	{
		L = FMath::Min(Largeur, FMath::RoundToInt(Hauteur * 9.f / 16.f));
		X0 = (Largeur - L) / 2;
	}
	TArray<FColor> Image;
	Image.SetNumUninitialized(L * Hauteur);
	for (int32 y = 0; y < Hauteur; ++y)
	{
		FMemory::Memcpy(&Image[y * L], &Pixels[y * Largeur + X0], L * sizeof(FColor));
	}
	for (FColor& C : Image)
	{
		C.A = 255;
	}

	const FString Dossier = DossierPhotos();
	const FString Nom = FString::Printf(TEXT("CHELLES3D_%s%s.png"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S")), bCadreTikTok ? TEXT("_TikTok") : TEXT(""));
	const FString Chemin = FPaths::Combine(Dossier, Nom);
	Message(FString::Printf(TEXT("PHOTO ENREGISTRÉE : %s   (O = ouvrir le dossier)"), *Nom), true);
	UE_LOG(LogChellesPhoto, Log, TEXT("[Photo] %s (%dx%d)"), *Chemin, L, Hauteur);

	// Compression PNG hors du jeu pour ne pas le faire ramer
	IImageWrapperModule* ModuleImage = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	Async(EAsyncExecution::ThreadPool, [ImageAEcrire = MoveTemp(Image), ModuleImage, L, Hauteur, Dossier, Chemin]()
	{
		TSharedPtr<IImageWrapper> Png = ModuleImage->CreateImageWrapper(EImageFormat::PNG);
		if (!Png.IsValid() || !Png->SetRaw(ImageAEcrire.GetData(), ImageAEcrire.Num() * sizeof(FColor), L, Hauteur, ERGBFormat::BGRA, 8))
		{
			UE_LOG(LogChellesPhoto, Warning, TEXT("[Photo] Compression PNG impossible."));
			return;
		}
		const auto& Donnees = Png->GetCompressed();
		TArray<uint8> Fichier;
		Fichier.Append(Donnees.GetData(), static_cast<int32>(Donnees.Num()));
		IFileManager::Get().MakeDirectory(*Dossier, true);
		if (!FFileHelper::SaveArrayToFile(Fichier, *Chemin))
		{
			UE_LOG(LogChellesPhoto, Warning, TEXT("[Photo] Impossible d'ecrire %s"), *Chemin);
		}
	});
}

void FModePhotoChelles::Message(const FString& Texte, bool bFlash)
{
	if (Ecran.IsValid())
	{
		Ecran->Message(Texte, bFlash);
	}
}

FEtatPhoto FModePhotoChelles::Etat() const
{
	FEtatPhoto E;
	E.Filtre = GFiltres[FMath::Clamp(Filtre, 0, GNombreFiltres - 1)].Nom;
	E.Fov = Fov;
	E.bCadreTikTok = bCadreTikTok;
	E.bGrille = bGrille;
	E.bFlou = bFlou;
	E.bPersoCache = bPersoCache;
	E.bAide = bAide;
	E.bFige = bFige;
	E.ToucheMode = ToucheDuMode().GetDisplayName().ToString().ToUpper();
	E.Partage = Reglages.Partage;
	return E;
}

// ------------------------------------------------------------------ clavier / souris

bool FModePhotoChelles::ToucheAppuyee(FSlateApplication& Slate, const FKey& Touche, bool bRepete)
{
	// F12 est aussi acceptee : si le joueur a choisi une autre touche dans OPTIONS, le traducteur de
	// touches de ChellesMenu (TouchesChelles.cpp) renvoie F12 quand il appuie sur sa touche.
	const bool bToucheMode = Touche == ToucheDuMode() || Touche == EKeys::F12;
	if (!bActif)
	{
		// Hors mode photo : seule la touche du mode nous interesse, le reste va au jeu
		if (bToucheMode && !bRepete && !Touche.IsGamepadKey() && PeutEntrer(Slate))
		{
			Entrer();
			return bActif;
		}
		return false;
	}

	// Console (²) : on la laisse passer
	if (Touche.GetFName() == FName(TEXT("²")) || Touche == EKeys::Tilde)
	{
		return false;
	}
	if (Touche.IsGamepadKey())
	{
		return true;
	}
	if (bRepete)
	{
		return true;
	}

	Enfoncees.Add(Touche);
	if (bToucheMode || Touche == EKeys::Escape)
	{
		Quitter();
	}
	else if (Touche == EKeys::LeftMouseButton || Touche == EKeys::Enter)
	{
		PrendrePhoto();
	}
	else if (Touche == EKeys::RightMouseButton)
	{
		bClicDroit = true;
	}
	else if (Touche == EKeys::F)
	{
		Filtre = (Filtre + 1) % GNombreFiltres;
		AppliquerFiltre();
		Message(FString::Printf(TEXT("FILTRE : %s"), GFiltres[Filtre].Nom));
	}
	else if (Touche == EKeys::B)
	{
		bFlou = !bFlou;
		AppliquerFiltre();
		Message(bFlou ? TEXT("FLOU DE FOND : OUI (net au centre)") : TEXT("FLOU DE FOND : NON"));
	}
	else if (Touche == EKeys::T)
	{
		bCadreTikTok = !bCadreTikTok;
		Message(bCadreTikTok ? TEXT("CADRE TIKTOK 9:16 : la photo sera verticale") : TEXT("CADRE NORMAL (écran entier)"));
	}
	else if (Touche == EKeys::G)
	{
		bGrille = !bGrille;
	}
	else if (Touche == EKeys::H)
	{
		bAide = !bAide;
	}
	else if (Touche == EKeys::X)
	{
		if (Perso.IsValid())
		{
			bPersoCache = !bPersoCache;
			Perso->SetActorHiddenInGame(bPersoCache);
		}
	}
	else if (Touche == EKeys::R)
	{
		if (Camera.IsValid())
		{
			Camera->SetActorLocation(Depart);
		}
		Rotation = RotationDepart;
		Fov = FovDepart;
		AppliquerFiltre();
		Message(TEXT("CAMÉRA REMISE AU DÉPART"));
	}
	else if (Touche == EKeys::O)
	{
		const FString Dossier = DossierPhotos();
		IFileManager::Get().MakeDirectory(*Dossier, true);
		FPlatformProcess::ExploreFolder(*Dossier);
		Message(FString::Printf(TEXT("DOSSIER : %s"), *Dossier));
	}
	return true; // tout le reste est avale : le perso ne bouge pas pendant la photo
}

bool FModePhotoChelles::ToucheRelachee(const FKey& Touche)
{
	if (!bActif)
	{
		return false;
	}
	if (Touche.GetFName() == FName(TEXT("²")) || Touche == EKeys::Tilde)
	{
		return false;
	}
	Enfoncees.Remove(Touche);
	if (Touche == EKeys::RightMouseButton)
	{
		bClicDroit = false;
	}
	return true;
}

bool FModePhotoChelles::SourisBouge(const FVector2D& Delta)
{
	if (!bActif)
	{
		return false;
	}
	Rotation.Yaw += Delta.X * Reglages.Sensibilite;
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch - Delta.Y * Reglages.Sensibilite, -89.f, 89.f);
	return true;
}

bool FModePhotoChelles::Molette(float Delta)
{
	if (!bActif)
	{
		return false;
	}
	if (bClicDroit)
	{
		Rotation.Roll = FMath::Clamp(Rotation.Roll + Delta * 3.f, -45.f, 45.f);
	}
	else
	{
		Fov = FMath::Clamp(Fov - Delta * 5.f, 10.f, 120.f);
		AppliquerFiltre();
	}
	return true;
}
