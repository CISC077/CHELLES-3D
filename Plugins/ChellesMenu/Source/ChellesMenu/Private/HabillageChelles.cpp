// CHELLES 3D - HABILLAGE : catalogue lu dans les packs, pose des vetements sur le soldat, multijoueur, apercu du menu
#include "HabillageChelles.h"

#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

// Pas d'espace de noms anonyme : le projet compile plusieurs fichiers ensemble (unity build).
namespace HabillageInterne
{
	static const TCHAR* SectionIni = TEXT("ChellesHabillage");
	static const TCHAR* TableTetes = TEXT("/Game/Action_Male_and_Female/Data/Heads/DT_Heads.DT_Heads");
	static const TCHAR* TablesMannequin[] =
	{
		TEXT("/Game/Female_Mannequin/Blueprints/DT_Presets_Mannequin.DT_Presets_Mannequin"),
		TEXT("/Game/Female_Mannequin/Blueprints/DT_Presets_BodyForms.DT_Presets_BodyForms")
	};

	/** Une ligne de tableau lue par reflexion (les structures des packs sont des Blueprints). */
	struct FLigneLue
	{
		TMap<FString, UObject*> Objets;
		TMap<FString, int32> Entiers;
		TMap<FString, bool> Bools;
		TMap<FString, UObject*> Tables;
	};

	static FString CleDeCarte(const FProperty* P, const void* Valeur)
	{
		if (const FNameProperty* PN = CastField<FNameProperty>(P))
		{
			return PN->GetPropertyValue(Valeur).ToString();
		}
		if (const FStrProperty* PS = CastField<FStrProperty>(P))
		{
			return PS->GetPropertyValue(Valeur);
		}
		if (const FEnumProperty* PE = CastField<FEnumProperty>(P))
		{
			const int64 V = PE->GetUnderlyingProperty()->GetSignedIntPropertyValue(Valeur);
			return PE->GetEnum() ? PE->GetEnum()->GetNameStringByValue(V) : FString::FromInt(int32(V));
		}
		if (const FByteProperty* PB = CastField<FByteProperty>(P))
		{
			const uint8 V = PB->GetPropertyValue(Valeur);
			return PB->Enum ? PB->Enum->GetNameStringByValue(V) : FString::FromInt(V);
		}
		return FString();
	}

	static UObject* ObjetDe(const FProperty* P, const void* Valeur)
	{
		if (CastField<FSoftObjectProperty>(P))
		{
			const FSoftObjectPtr* Doux = static_cast<const FSoftObjectPtr*>(Valeur);
			return Doux->ToSoftObjectPath().IsNull() ? nullptr : Doux->ToSoftObjectPath().TryLoad();
		}
		if (const FObjectPropertyBase* PO = CastField<FObjectPropertyBase>(P))
		{
			return PO->GetObjectPropertyValue(Valeur);
		}
		return nullptr;
	}

	static bool LireLigne(UDataTable* Table, FName NomLigne, FLigneLue& Sortie)
	{
		if (!Table || !Table->GetRowStruct() || NomLigne.IsNone())
		{
			return false;
		}
		uint8* const* Donnees = Table->GetRowMap().Find(NomLigne);
		if (!Donnees || !*Donnees)
		{
			return false;
		}
		const UScriptStruct* Struct = Table->GetRowStruct();
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* P = *It;
			const FString Nom = Struct->GetAuthoredNameForField(P).ToLower();
			const void* Valeur = P->ContainerPtrToValuePtr<void>(*Donnees);
			if (CastField<FObjectPropertyBase>(P))
			{
				Sortie.Objets.Add(Nom, ObjetDe(P, Valeur));
			}
			else if (const FBoolProperty* PB = CastField<FBoolProperty>(P))
			{
				Sortie.Bools.Add(Nom, PB->GetPropertyValue(Valeur));
			}
			else if (const FNumericProperty* PNum = CastField<FNumericProperty>(P))
			{
				if (PNum->IsInteger())
				{
					Sortie.Entiers.Add(Nom, int32(PNum->GetSignedIntPropertyValue(Valeur)));
				}
			}
			else if (const FMapProperty* PM = CastField<FMapProperty>(P))
			{
				FScriptMapHelper Aide(PM, Valeur);
				for (int32 i = 0; i < Aide.GetMaxIndex(); ++i)
				{
					if (!Aide.IsValidIndex(i))
					{
						continue;
					}
					const FString Cle = CleDeCarte(PM->KeyProp, Aide.GetKeyPtr(i)).ToLower();
					if (!Cle.IsEmpty())
					{
						Sortie.Tables.Add(Cle, ObjetDe(PM->ValueProp, Aide.GetValuePtr(i)));
					}
				}
			}
		}
		return true;
	}

	template <typename T>
	static T* Premier(const FLigneLue& L, const TCHAR* Cle)
	{
		UObject* const* O = L.Objets.Find(FString(Cle).ToLower());
		return O ? Cast<T>(*O) : nullptr;
	}

	// ------------------------------------------------------------ Noms en francais

	static FString Traduire(const FString& Mot)
	{
		static TMap<FString, FString> Dico;
		if (Dico.Num() == 0)
		{
			const TCHAR* Paires[][2] =
			{
				{TEXT("hoodie"), TEXT("SWEAT CAPUCHE")}, {TEXT("tshirt"), TEXT("T-SHIRT")}, {TEXT("tank"), TEXT("D\u00C9BARDEUR")},
				{TEXT("sportsuit"), TEXT("COMBI SPORT")}, {TEXT("long"), TEXT("LONGUES")}, {TEXT("sleeves"), TEXT("MANCHES")},
				{TEXT("with"), TEXT("AVEC")}, {TEXT("without"), TEXT("SANS")}, {TEXT("neck"), TEXT("COL")}, {TEXT("jeans"), TEXT("JEAN")},
				{TEXT("baggy"), TEXT("BAGGY")}, {TEXT("shorty"), TEXT("SHORT")}, {TEXT("volleyball"), TEXT("SHORT VOLLEY")},
				{TEXT("yoga"), TEXT("YOGA")}, {TEXT("yogapants"), TEXT("LEGGING")}, {TEXT("pants"), TEXT("PANTALON")}, {TEXT("cyclist"), TEXT("CYCLISTE")},
				{TEXT("sneakers"), TEXT("BASKETS")}, {TEXT("toeshoes"), TEXT("CHAUSSONS")}, {TEXT("cap"), TEXT("CASQUETTE")},
				{TEXT("high"), TEXT("HAUTE")}, {TEXT("low"), TEXT("BASSE")}, {TEXT("backward"), TEXT("\u00C0 L'ENVERS")},
				{TEXT("beanie"), TEXT("BONNET")}, {TEXT("baghead"), TEXT("SAC SUR LA T\u00CATE")}, {TEXT("bald"), TEXT("CHAUVE")},
				{TEXT("short"), TEXT("COURTS")}, {TEXT("locks"), TEXT("DREADS")}, {TEXT("mini"), TEXT("MINI")}, {TEXT("pompadour"), TEXT("BANANE")},
				{TEXT("slicked"), TEXT("PLAQU\u00C9S")}, {TEXT("medium"), TEXT("MI-LONGS")}, {TEXT("straight"), TEXT("LISSES")},
				{TEXT("wavy"), TEXT("ONDUL\u00C9S")}, {TEXT("bob"), TEXT("CARR\u00C9")}, {TEXT("buns"), TEXT("CHIGNONS")}, {TEXT("tail"), TEXT("QUEUE")},
				{TEXT("simple"), TEXT("SIMPLE")}, {TEXT("bangs"), TEXT("FRANGE")}, {TEXT("beard"), TEXT("BARBE")}, {TEXT("jacket"), TEXT("VESTE COURTE")},
				{TEXT("accents"), TEXT("D\u00C9TAILS")}, {TEXT("gloves"), TEXT("GANTS")}, {TEXT("casual"), TEXT("LUNETTES")},
				{TEXT("tacticool"), TEXT("LUNETTES TACTIQUES")}, {TEXT("formal"), TEXT("LUNETTES CLASSE")}, {TEXT("facemask"), TEXT("MASQUE")},
				{TEXT("mask"), TEXT("MASQUE")}, {TEXT("surgeon"), TEXT("CHIRURGIEN")}, {TEXT("clear"), TEXT("TRANSPARENTS")}, {TEXT("sun"), TEXT("SOLAIRES")},
				{TEXT("lenses"), TEXT("VERRES")}, {TEXT("and"), TEXT("ET")}, {TEXT("lining"), TEXT("DOUBLURE")}, {TEXT("tris"), TEXT("TRIANGLES")},
				{TEXT("black"), TEXT("NOIR")}, {TEXT("white"), TEXT("BLANC")}, {TEXT("red"), TEXT("ROUGE")}, {TEXT("blue"), TEXT("BLEU")},
				{TEXT("yellow"), TEXT("JAUNE")}, {TEXT("grey"), TEXT("GRIS")}, {TEXT("gray"), TEXT("GRIS")}, {TEXT("brown"), TEXT("MARRON")},
				{TEXT("orange"), TEXT("ORANGE")}, {TEXT("purple"), TEXT("VIOLET")}, {TEXT("green"), TEXT("VERT")}, {TEXT("pink"), TEXT("ROSE")},
				{TEXT("gold"), TEXT("OR")}, {TEXT("silver"), TEXT("ARGENT")}, {TEXT("blonde"), TEXT("BLOND")}, {TEXT("lightbrown"), TEXT("BEIGE")},
				{TEXT("light"), TEXT("CLAIR")}, {TEXT("arctic"), TEXT("ARCTIQUE")}, {TEXT("alt"), TEXT("BIS")}, {TEXT("full"), TEXT("TOUT")},
				{TEXT("military"), TEXT("MILITAIRE")}, {TEXT("female"), TEXT("FEMME")}, {TEXT("male"), TEXT("HOMME")}, {TEXT("form"), TEXT("LISSE")},
				{TEXT("robot"), TEXT("ROBOT")}, {TEXT("eyes"), TEXT("YEUX")}, {TEXT("human"), TEXT("HUMAIN")}, {TEXT("detailled"), TEXT("D\u00C9TAILL\u00C9")}
			};
			for (const auto& P : Paires)
			{
				Dico.Add(P[0], P[1]);
			}
		}
		const FString* Trouve = Dico.Find(Mot.ToLower());
		return Trouve ? *Trouve : Mot.ToUpper();
	}

	/** "Hoodie_BlackGold" -> "SWEAT CAPUCHE NOIR OR" */
	static FString Joli(const FString& Brut)
	{
		FString Espace;
		for (int32 i = 0; i < Brut.Len(); ++i)
		{
			const TCHAR C = Brut[i];
			if (C == TEXT('_') || C == TEXT('-') || C == TEXT(' '))
			{
				Espace += TEXT(' ');
				continue;
			}
			if (i > 0 && FChar::IsUpper(C) && FChar::IsLower(Brut[i - 1]))
			{
				Espace += TEXT(' ');
			}
			Espace += C;
		}
		TArray<FString> Morceaux;
		Espace.ParseIntoArray(Morceaux, TEXT(" "), true);
		FString Resultat;
		for (const FString& M : Morceaux)
		{
			Resultat += (Resultat.IsEmpty() ? TEXT("") : TEXT(" ")) + Traduire(M);
		}
		return Resultat;
	}

	// ------------------------------------------------------------ Materiaux

	static bool MotInutile(const FString& M)
	{
		static const TCHAR* Liste[] =
		{
			TEXT("mi"), TEXT("unisex"), TEXT("clothes"), TEXT("male"), TEXT("female"), TEXT("regular"), TEXT("tall"), TEXT("buff"),
			TEXT("and"), TEXT("with"), TEXT("without"), TEXT("default"), TEXT("black"), TEXT("white"), TEXT("red"), TEXT("blue"),
			TEXT("yellow"), TEXT("grey"), TEXT("gray"), TEXT("brown"), TEXT("orange"), TEXT("purple"), TEXT("green"), TEXT("pink"),
			TEXT("gold"), TEXT("silver"), TEXT("blonde"), TEXT("dark"), TEXT("clear"), TEXT("deep"), TEXT("light"), TEXT("lightbrown"),
			TEXT("grad"), TEXT("gradient"), TEXT("wave"), TEXT("nowave"), TEXT("accents"), TEXT("alt"), TEXT("arctic"), TEXT("military"),
			TEXT("inst"), TEXT("master"), TEXT("amf"), TEXT("fullyellow"), TEXT("blackgold"), TEXT("blacksilver"), TEXT("redsilver"),
			TEXT("whitea"), TEXT("whiteb"), TEXT("tris"), TEXT("blacktris")
		};
		if (M.Len() < 3)
		{
			return true;
		}
		for (const TCHAR* L : Liste)
		{
			if (M == L)
			{
				return true;
			}
		}
		return false;
	}

	static TArray<FString> Mots(const FString& Texte)
	{
		TArray<FString> Sortie;
		FString Courant;
		for (const TCHAR C : Texte.ToLower())
		{
			if (FChar::IsAlpha(C))
			{
				Courant += C;
			}
			else
			{
				if (!MotInutile(Courant))
				{
					Sortie.AddUnique(Courant);
				}
				Courant.Reset();
			}
		}
		if (!MotInutile(Courant))
		{
			Sortie.AddUnique(Courant);
		}
		return Sortie;
	}

	static bool SeRessemblent(const FString& A, const FString& B)
	{
		return A == B || (A.Len() >= 3 && B.Contains(A)) || (B.Len() >= 3 && A.Contains(B));
	}

	/** Trouve l'emplacement du mesh qui ressemble le plus au materiau (ex. MI_Cap_* -> emplacement "03_Cap"). */
	static int32 EmplacementPour(USkeletalMesh* Mesh, UMaterialInterface* Materiau, const TArray<int32>& DejaPris)
	{
		const TArray<FSkeletalMaterial>& Liste = Mesh->GetMaterials();
		if (Liste.Num() <= 1)
		{
			return 0;
		}
		const TArray<FString> Cherches = Mots(Materiau->GetName());
		int32 Meilleur = INDEX_NONE;
		int32 MeilleurScore = 0;
		for (int32 i = 0; i < Liste.Num(); ++i)
		{
			if (DejaPris.Contains(i))
			{
				continue;
			}
			TArray<FString> Ici = Mots(Liste[i].MaterialSlotName.ToString());
			if (Liste[i].MaterialInterface)
			{
				Ici.Append(Mots(Liste[i].MaterialInterface->GetName()));
			}
			int32 Score = 0;
			for (const FString& A : Cherches)
			{
				for (const FString& B : Ici)
				{
					if (SeRessemblent(A, B))
					{
						++Score;
					}
				}
			}
			if (Score > MeilleurScore)
			{
				MeilleurScore = Score;
				Meilleur = i;
			}
		}
		if (Meilleur == INDEX_NONE)
		{
			Meilleur = DejaPris.Contains(0) ? INDEX_NONE : 0;
		}
		return Meilleur;
	}

	static bool EstPeau(const FString& Nom)
	{
		const FString N = Nom.ToLower();
		return N.Contains(TEXT("body")) || N.Contains(TEXT("skin")) || N.Contains(TEXT("flesh")) || N.Contains(TEXT("head"));
	}

	static bool AUnBassin(USkeletalMesh* Mesh)
	{
		return Mesh && Mesh->GetRefSkeleton().FindBoneIndex(FName(TEXT("pelvis"))) != INDEX_NONE;
	}

	static FTransform PoseDeReference(USkeletalMesh* Mesh, FName Os)
	{
		FTransform T = FTransform::Identity;
		if (!Mesh)
		{
			return T;
		}
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		int32 i = Ref.FindBoneIndex(Os);
		while (i != INDEX_NONE)
		{
			T = T * Ref.GetRefBonePose()[i];
			i = Ref.GetParentIndex(i);
		}
		return T;
	}

	/** Cheveux, chapeau, lunettes : ils ont leur petit squelette, on les accroche a la tete. */
	static void PreparerAccroche(ChellesHabillage::FPiece& Piece, USkeletalMesh* Tete, USkeletalMesh* Corps)
	{
		Piece.bSuitSquelette = AUnBassin(Piece.Mesh);
		if (Piece.bSuitSquelette)
		{
			return;
		}
		Piece.Os = FName(TEXT("head"));
		Piece.Relatif = FTransform::Identity;
		const FBoxSphereBounds Bornes = Piece.Mesh->GetImportedBounds();
		if (Bornes.Origin.Z > 80.f)
		{
			// Modele a hauteur de tete (et pas autour de l'origine) : on annule la position de la tete au repos
			Piece.Relatif = PoseDeReference(Corps ? Corps : Tete, Piece.Os).Inverse();
			return;
		}
		USkeletalMeshSocket* Prise = nullptr;
		for (USkeletalMesh* M : { Tete, Corps })
		{
			if (M && !Prise)
			{
				Prise = M->FindSocket(FName(TEXT("head_accessories")));
			}
		}
		if (Prise)
		{
			Piece.Os = Prise->BoneName;
			Piece.Relatif = Prise->GetSocketLocalTransform();
		}
	}

	// ------------------------------------------------------------ Catalogue

	struct FEmplacementConnu
	{
		const TCHAR* Cle;
		const TCHAR* Titre;
		bool bCorps;      // partie du corps : jamais "aucun" (sinon il manque un bout du corps)
		const TCHAR* Sans; // nom de la ligne Unequipped
	};

	static const FEmplacementConnu EmplacementsConnus[] =
	{
		{ TEXT("Upper"), TEXT("HAUT"), true, TEXT("SANS (PEAU)") },
		{ TEXT("UpperAddonItem"), TEXT("VESTE"), false, TEXT("AUCUNE") },
		{ TEXT("Lower"), TEXT("BAS"), true, TEXT("SANS (PEAU)") },
		{ TEXT("Shoes"), TEXT("CHAUSSURES"), true, TEXT("PIEDS NUS") },
		{ TEXT("Gloves"), TEXT("MAINS"), true, TEXT("MAINS NUES") },
		{ TEXT("Hair"), TEXT("CHEVEUX"), false, TEXT("AUCUNS") },
		{ TEXT("Hats"), TEXT("CHAPEAU"), false, TEXT("AUCUN") },
		{ TEXT("EyeWear"), TEXT("LUNETTES / MASQUE"), false, TEXT("AUCUN") }
	};

	static const FEmplacementConnu* Connu(const FString& Cle)
	{
		for (const FEmplacementConnu& E : EmplacementsConnus)
		{
			if (Cle.Equals(E.Cle, ESearchCase::IgnoreCase))
			{
				return &E;
			}
		}
		return nullptr;
	}

	static UDataTable* ChargerTable(const TCHAR* Chemin)
	{
		UDataTable* T = LoadObject<UDataTable>(nullptr, Chemin, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (T && !T->IsRooted())
		{
			T->AddToRoot(); // garde le catalogue en memoire pendant toute la partie
		}
		return T;
	}

	static FString NomModele(const FString& NomLigne, bool& bFemme, bool& bValide)
	{
		struct FGroupe { const TCHAR* Prefixe; const TCHAR* Nom; bool bF; };
		static const FGroupe Groupes[] =
		{
			{ TEXT("UE4_Tall_Female_"), TEXT("GRANDE"), true },
			{ TEXT("UE4_Female_"), TEXT("CLASSIQUE"), true },
			{ TEXT("UE4_Buff_Male_"), TEXT("ATHL\u00C9TIQUE"), false },
			{ TEXT("UE4_Male_"), TEXT("CLASSIQUE"), false }
		};
		bValide = false;
		for (const FGroupe& G : Groupes)
		{
			if (!NomLigne.StartsWith(G.Prefixe))
			{
				continue;
			}
			bValide = true;
			bFemme = G.bF;
			const FString Reste = NomLigne.Mid(FCString::Strlen(G.Prefixe));
			FString Peau = Joli(Reste);
			if (Reste.StartsWith(TEXT("White")))
			{
				Peau = TEXT("PEAU CLAIRE");
			}
			else if (Reste.StartsWith(TEXT("Black")))
			{
				Peau = TEXT("PEAU FONC\u00C9E");
			}
			else if (Reste.StartsWith(TEXT("Asian")))
			{
				Peau = TEXT("PEAU MATE");
			}
			else if (Reste.StartsWith(TEXT("Form")))
			{
				Peau = TEXT("MANNEQUIN LISSE");
			}
			if (Reste.Contains(TEXT("beard")))
			{
				const TCHAR Dernier = Reste[Reste.Len() - 1];
				Peau += FChar::IsDigit(Dernier) ? FString::Printf(TEXT(" + BARBE %c"), Dernier) : FString(TEXT(" + BARBE"));
			}
			return FString(G.Nom) + TEXT(" - ") + Peau;
		}
		return FString();
	}

	static void RemplirEmplacement(ChellesHabillage::FEmplacement& E, const FEmplacementConnu& Info, UDataTable* Table)
	{
		E.Cle = Info.Cle;
		E.Titre = Info.Titre;
		E.Table = Table;
		bool bASans = false;
		TArray<FName> Autres;
		for (const TPair<FName, uint8*>& Paire : Table->GetRowMap())
		{
			if (Paire.Key == FName(TEXT("Unequipped")))
			{
				bASans = true;
			}
			else
			{
				Autres.Add(Paire.Key);
			}
		}
		if (bASans)
		{
			E.Lignes.Add(FName(TEXT("Unequipped")));
			E.Noms.Add(Info.Sans);
		}
		else if (!Info.bCorps)
		{
			E.Lignes.Add(NAME_None);
			E.Noms.Add(Info.Sans);
		}
		for (const FName& L : Autres)
		{
			E.Lignes.Add(L);
			E.Noms.Add(Joli(L.ToString()));
		}
	}

	static TArray<ChellesHabillage::FModele> ChargerCatalogue()
	{
		TArray<ChellesHabillage::FModele> Liste;
		if (UDataTable* Tetes = ChargerTable(TableTetes))
		{
			for (const TPair<FName, uint8*>& Paire : Tetes->GetRowMap())
			{
				bool bFemme = false;
				bool bValide = false;
				const FString Nom = NomModele(Paire.Key.ToString(), bFemme, bValide);
				FLigneLue L;
				if (!bValide || !LireLigne(Tetes, Paire.Key, L) || !Premier<USkeletalMesh>(L, TEXT("Mesh")))
				{
					continue;
				}
				ChellesHabillage::FModele M;
				M.Id = TEXT("AMF:") + Paire.Key.ToString();
				M.Nom = Nom;
				M.bFemme = bFemme;
				M.Table = Tetes;
				M.Ligne = Paire.Key;
				for (const FEmplacementConnu& Info : EmplacementsConnus)
				{
					UObject* const* T = L.Tables.Find(FString(Info.Cle).ToLower());
					if (UDataTable* Table = T ? Cast<UDataTable>(*T) : nullptr)
					{
						ChellesHabillage::FEmplacement E;
						RemplirEmplacement(E, Info, Table);
						if (E.Lignes.Num() > 0)
						{
							M.Emplacements.Add(MoveTemp(E));
						}
					}
				}
				Liste.Add(MoveTemp(M));
			}
		}
		int32 NumeroF = 0;
		int32 NumeroM = 0;
		for (const TCHAR* Chemin : TablesMannequin)
		{
			UDataTable* Table = ChargerTable(Chemin);
			if (!Table)
			{
				continue;
			}
			for (const TPair<FName, uint8*>& Paire : Table->GetRowMap())
			{
				FLigneLue L;
				if (!LireLigne(Table, Paire.Key, L))
				{
					continue;
				}
				int32 Meshes = 0;
				bool bHomme = false;
				for (const TPair<FString, UObject*>& O : L.Objets)
				{
					if (USkeletalMesh* SM = Cast<USkeletalMesh>(O.Value))
					{
						++Meshes;
						const FString Chemin2 = SM->GetPathName();
						bHomme |= Chemin2.Contains(TEXT("/Male_Bodyform/")) || Chemin2.Contains(TEXT("SK_Male_"));
					}
				}
				if (Meshes == 0)
				{
					continue;
				}
				ChellesHabillage::FModele M;
				M.Id = FString::Printf(TEXT("MNQ:%s:%s"), *Table->GetName(), *Paire.Key.ToString());
				M.Nom = FString::Printf(TEXT("MANNEQUIN %d"), bHomme ? ++NumeroM : ++NumeroF);
				M.bFemme = !bHomme;
				M.bMannequin = true;
				M.Table = Table;
				M.Ligne = Paire.Key;
				Liste.Add(MoveTemp(M));
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[Habillage] Catalogue : %d modeles trouves dans les packs."), Liste.Num());
		return Liste;
	}

	static bool EstCle(const FString& A, const TCHAR* B)
	{
		return A.Equals(B, ESearchCase::IgnoreCase);
	}
}

// ---------------------------------------------------------------- API

namespace ChellesHabillage
{
	using namespace HabillageInterne;

	int32 FEmplacement::IndexDe(const FString& NomLigne) const
	{
		const FName Nom = NomLigne.IsEmpty() ? NAME_None : FName(*NomLigne);
		return Lignes.IndexOfByKey(Nom);
	}

	FString FTenue::VersTexte() const
	{
		FString T = FString::Printf(TEXT("Sexe=%d;Modele=%s"), Sexe, *Modele);
		TArray<FString> Cles;
		Choix.GetKeys(Cles);
		Cles.Sort();
		for (const FString& C : Cles)
		{
			T += FString::Printf(TEXT(";%s=%s"), *C, *Choix[C]);
		}
		return T;
	}

	FTenue FTenue::DepuisTexte(const FString& Texte)
	{
		FTenue T;
		TArray<FString> Morceaux;
		Texte.ParseIntoArray(Morceaux, TEXT(";"), true);
		for (const FString& M : Morceaux)
		{
			FString Cle, Valeur;
			if (!M.Split(TEXT("="), &Cle, &Valeur))
			{
				continue;
			}
			if (Cle == TEXT("Sexe"))
			{
				T.Sexe = FCString::Atoi(*Valeur);
			}
			else if (Cle == TEXT("Modele"))
			{
				T.Modele = Valeur;
			}
			else
			{
				T.Choix.Add(Cle, Valeur);
			}
		}
		return T;
	}

	const TArray<FModele>& Modeles()
	{
		static TArray<FModele> Liste;
		static bool bCharge = false;
		if (!bCharge)
		{
			bCharge = true;
			Liste = ChargerCatalogue();
		}
		return Liste;
	}

	bool PacksPresents()
	{
		return Modeles().Num() > 0;
	}

	const FModele* TrouverModele(const FString& Id)
	{
		for (const FModele& M : Modeles())
		{
			if (M.Id == Id)
			{
				return &M;
			}
		}
		return nullptr;
	}

	TArray<const FModele*> ModelesDuSexe(int32 Sexe)
	{
		TArray<const FModele*> Sortie;
		for (const FModele& M : Modeles())
		{
			if (M.bFemme == (Sexe == Feminin))
			{
				Sortie.Add(&M);
			}
		}
		return Sortie;
	}

	static FString ParDefaut(const FEmplacement& E)
	{
		const FEmplacementConnu* Info = Connu(E.Cle);
		const bool bCorps = Info && Info->bCorps;
		const bool bCheveux = EstCle(E.Cle, TEXT("Hair"));
		if ((bCorps && !EstCle(E.Cle, TEXT("Gloves"))) || bCheveux)
		{
			for (const FName& L : E.Lignes)
			{
				if (!L.IsNone() && L != FName(TEXT("Unequipped")) && L != FName(TEXT("Bald")))
				{
					return L.ToString();
				}
			}
		}
		return E.Lignes.Num() > 0 && !E.Lignes[0].IsNone() ? E.Lignes[0].ToString() : FString();
	}

	void Completer(FTenue& Tenue)
	{
		if (Tenue.EstSpiderMan())
		{
			Tenue.Sexe = SpiderMan;
			return;
		}
		const FModele* M = TrouverModele(Tenue.Modele);
		if (!M || M->bFemme != (Tenue.Sexe == Feminin))
		{
			const TArray<const FModele*> Liste = ModelesDuSexe(Tenue.Sexe);
			M = Liste.Num() > 0 ? Liste[0] : nullptr;
		}
		if (!M)
		{
			Tenue.Modele.Reset();
			return;
		}
		Tenue.Modele = M->Id;
		for (const FEmplacement& E : M->Emplacements)
		{
			const FString* Actuel = Tenue.Choix.Find(E.Cle);
			if (!Actuel || E.IndexDe(*Actuel) == INDEX_NONE)
			{
				Tenue.Choix.Add(E.Cle, ParDefaut(E));
			}
		}
	}

	void Hasard(FTenue& Tenue)
	{
		if (Tenue.EstSpiderMan())
		{
			return;
		}
		const TArray<const FModele*> Liste = ModelesDuSexe(Tenue.Sexe);
		if (Liste.Num() == 0)
		{
			return;
		}
		// Les modeles "mannequin" sont plus rares au tirage : on prefere les tenues de ville
		const FModele* M = Liste[FMath::RandRange(0, Liste.Num() - 1)];
		if (M->bMannequin && FMath::FRand() < 0.6f)
		{
			M = Liste[FMath::RandRange(0, Liste.Num() - 1)];
		}
		Tenue.Modele = M->Id;
		Tenue.Choix.Reset();
		for (const FEmplacement& E : M->Emplacements)
		{
			const FEmplacementConnu* Info = Connu(E.Cle);
			const bool bAccessoire = Info && !Info->bCorps;
			int32 i = FMath::RandRange(0, E.Lignes.Num() - 1);
			if ((bAccessoire && !EstCle(E.Cle, TEXT("Hair")) && FMath::FRand() < 0.55f) || (EstCle(E.Cle, TEXT("Gloves")) && FMath::FRand() < 0.8f))
			{
				i = 0;
			}
			if (Info && Info->bCorps && !EstCle(E.Cle, TEXT("Gloves")) && E.Lignes.Num() > 1 && E.Lignes[i] == FName(TEXT("Unequipped")))
			{
				i = FMath::RandRange(1, E.Lignes.Num() - 1); // pas de tirage "sans haut" / "sans bas"
			}
			Tenue.Choix.Add(E.Cle, E.Lignes[i].IsNone() ? FString() : E.Lignes[i].ToString());
		}
		Completer(Tenue);
	}

	bool Assembler(const FTenue& Tenue, TArray<FPiece>& Pieces, UClass*& ClasseAnim, USkeletalMesh*& Squelette)
	{
		Pieces.Reset();
		ClasseAnim = nullptr;
		Squelette = nullptr;
		if (Tenue.EstSpiderMan())
		{
			return false;
		}
		const FModele* M = TrouverModele(Tenue.Modele);
		if (!M || !M->Table.IsValid())
		{
			return false;
		}
		FLigneLue Base;
		if (!LireLigne(M->Table.Get(), M->Ligne, Base))
		{
			return false;
		}

		auto Ajouter = [&Pieces](USkeletalMesh* Mesh) -> FPiece&
		{
			FPiece& P = Pieces.AddDefaulted_GetRef();
			P.Mesh = Mesh;
			return P;
		};

		if (M->bMannequin)
		{
			// Ligne "tout fait" : chaque mesh (Uppermesh, Lowermesh, Mainmeshhead, Hat1model...) avec son materiau
			for (const TPair<FString, UObject*>& O : Base.Objets)
			{
				USkeletalMesh* SM = Cast<USkeletalMesh>(O.Value);
				if (UClass* C = Cast<UClass>(O.Value))
				{
					if (C->IsChildOf(UAnimInstance::StaticClass()))
					{
						ClasseAnim = C;
					}
				}
				if (!SM)
				{
					continue;
				}
				FString Racine = O.Key.Replace(TEXT("mesh"), TEXT("")).Replace(TEXT("model"), TEXT(""));
				Racine.RemoveFromEnd(TEXT("_"));
				FPiece& P = Ajouter(SM);
				P.bJambes = O.Key.Contains(TEXT("lower")) || O.Key.Contains(TEXT("feet")) || O.Key.Contains(TEXT("leg"));
				for (const TPair<FString, UObject*>& O2 : Base.Objets)
				{
					UMaterialInterface* Mat = Cast<UMaterialInterface>(O2.Value);
					if (Mat && O2.Key.Replace(TEXT("material"), TEXT("")) == Racine)
					{
						TArray<int32> Pris;
						const int32 i = EmplacementPour(SM, Mat, Pris);
						if (i != INDEX_NONE)
						{
							P.Materiaux.Add(TPair<int32, UMaterialInterface*>(i, Mat));
						}
					}
				}
			}
		}
		else
		{
			USkeletalMesh* Tete = Premier<USkeletalMesh>(Base, TEXT("Mesh"));
			UMaterialInterface* MatTete = Premier<UMaterialInterface>(Base, TEXT("Material"));
			UMaterialInterface* PeauHaut = Premier<UMaterialInterface>(Base, TEXT("UpperMaterial"));
			UMaterialInterface* PeauBas = Premier<UMaterialInterface>(Base, TEXT("LowerMaterial"));
			ClasseAnim = Premier<UClass>(Base, TEXT("AnimationBlueprint"));
			if (!Tete)
			{
				return false;
			}
			FPiece& PT = Ajouter(Tete);
			if (MatTete)
			{
				TArray<int32> Pris;
				const int32 i = EmplacementPour(Tete, MatTete, Pris);
				if (i != INDEX_NONE)
				{
					PT.Materiaux.Add(TPair<int32, UMaterialInterface*>(i, MatTete));
				}
			}

			for (const FEmplacement& E : M->Emplacements)
			{
				const FString* Choisi = Tenue.Choix.Find(E.Cle);
				if (!Choisi || Choisi->IsEmpty() || !E.Table.IsValid())
				{
					continue;
				}
				FLigneLue L;
				if (!LireLigne(E.Table.Get(), FName(**Choisi), L))
				{
					continue;
				}
				USkeletalMesh* SM = Premier<USkeletalMesh>(L, TEXT("Mesh"));
				if (!SM)
				{
					continue;
				}
				FPiece& P = Ajouter(SM);
				P.bJambes = EstCle(E.Cle, TEXT("Lower")) || EstCle(E.Cle, TEXT("Shoes"));
				const bool bBas = P.bJambes;
				UMaterialInterface* Peau = bBas ? PeauBas : PeauHaut;
				TArray<int32> Pris;
				const bool* Chair = L.Bools.Find(TEXT("hasflesh"));
				if (Chair)
				{
					// Vetement du pack "corps" : la peau suit le modele, le tissu va dans l'emplacement FabricMatID
					const TArray<FSkeletalMaterial>& Liste = SM->GetMaterials();
					UMaterialInterface* Tissu = Premier<UMaterialInterface>(L, TEXT("Material"));
					const bool bTissuEstPeau = Tissu && EstPeau(Tissu->GetName()) && !Tissu->GetName().Contains(TEXT("Suit"));
					const int32* Id = L.Entiers.Find(TEXT("fabricmatid"));
					const int32 Fabric = FMath::Clamp(Id ? *Id : 0, 0, FMath::Max(0, Liste.Num() - 1));
					if (Peau)
					{
						for (int32 i = 0; i < Liste.Num(); ++i)
						{
							const bool bPeauIci = EstPeau(Liste[i].MaterialSlotName.ToString()) ||
								(Liste[i].MaterialInterface && EstPeau(Liste[i].MaterialInterface->GetName()));
							if ((bPeauIci && (i != Fabric || bTissuEstPeau)) || (bTissuEstPeau && i == Fabric))
							{
								P.Materiaux.Add(TPair<int32, UMaterialInterface*>(i, Peau));
								Pris.Add(i);
							}
						}
					}
					if (Tissu && !bTissuEstPeau && Liste.Num() > 0)
					{
						P.Materiaux.Add(TPair<int32, UMaterialInterface*>(Fabric, Tissu));
					}
				}
				else
				{
					// Cheveux, chapeau, lunettes : chaque materiau de la ligne va dans l'emplacement qui lui ressemble
					for (const TPair<FString, UObject*>& O : L.Objets)
					{
						if (UMaterialInterface* Mat = Cast<UMaterialInterface>(O.Value))
						{
							const int32 i = EmplacementPour(SM, Mat, Pris);
							if (i != INDEX_NONE)
							{
								P.Materiaux.Add(TPair<int32, UMaterialInterface*>(i, Mat));
								Pris.Add(i);
							}
						}
					}
				}
			}

			// Corps complet pour l'apercu : SK_<silhouette>_Form_Standalone, a cote de la tete
			FString Chemin = Tete->GetPathName();
			FString Dossier, NomTete;
			if (Chemin.Split(TEXT("/"), &Dossier, &NomTete, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
			{
				FString Prefixe;
				for (const TCHAR* Coupe : { TEXT("_Body_Head"), TEXT("_Form_Head") })
				{
					int32 Pos = NomTete.Find(Coupe);
					if (Pos != INDEX_NONE && Prefixe.IsEmpty())
					{
						Prefixe = NomTete.Left(Pos);
					}
				}
				if (!Prefixe.IsEmpty())
				{
					const FString Complet = FString::Printf(TEXT("%s/%s_Form_Standalone.%s_Form_Standalone"), *Dossier, *Prefixe, *Prefixe);
					Squelette = LoadObject<USkeletalMesh>(nullptr, *Complet, nullptr, LOAD_NoWarn | LOAD_Quiet);
				}
			}
		}

		// Corps de repli : le morceau qui a le plus d'os
		if (!Squelette)
		{
			int32 Os = 0;
			for (const FPiece& P : Pieces)
			{
				if (P.Mesh && AUnBassin(P.Mesh) && P.Mesh->GetRefSkeleton().GetNum() > Os)
				{
					Os = P.Mesh->GetRefSkeleton().GetNum();
					Squelette = P.Mesh;
				}
			}
		}
		USkeletalMesh* Tete = Pieces.Num() > 0 ? Pieces[0].Mesh : nullptr;
		for (FPiece& P : Pieces)
		{
			PreparerAccroche(P, Tete, Squelette);
		}
		return Pieces.Num() > 0;
	}

	void Construire(AActor* Proprio, USkeletalMeshComponent* Chef, USkeletalMeshComponent* Jambes,
		const TArray<FPiece>& Pieces, TArray<USkeletalMeshComponent*>& Crees, bool bPendantPause)
	{
		if (!Proprio || !Chef)
		{
			return;
		}
		auto Creer = [&](const FPiece& P, USkeletalMeshComponent* Parent) -> USkeletalMeshComponent*
		{
			USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(Proprio, NAME_None, RF_Transient);
			C->PrimaryComponentTick.bTickEvenWhenPaused = bPendantPause;
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetGenerateOverlapEvents(false);
			C->bReceivesDecals = false;
			C->SetSkeletalMesh(P.Mesh);
			for (const TPair<int32, UMaterialInterface*>& Mat : P.Materiaux)
			{
				C->SetMaterial(Mat.Key, Mat.Value);
			}
			if (P.bSuitSquelette)
			{
				C->SetupAttachment(Parent);
				C->bUseBoundsFromMasterPoseComponent = true;
			}
			else
			{
				C->SetupAttachment(Parent, P.Os);
				C->SetRelativeTransform(P.Relatif);
			}
			C->SetOwnerNoSee(Parent->bOwnerNoSee);
			C->SetOnlyOwnerSee(Parent->bOnlyOwnerSee);
			C->RegisterComponent();
			if (P.bSuitSquelette)
			{
				C->SetMasterPoseComponent(Parent);
			}
			return C;
		};
		for (const FPiece& P : Pieces)
		{
			if (!P.Mesh)
			{
				continue;
			}
			Crees.Add(Creer(P, Chef));
		}
		if (Jambes)
		{
			for (const FPiece& P : Pieces)
			{
				if (P.Mesh && P.bJambes && P.bSuitSquelette)
				{
					USkeletalMeshComponent* C = Creer(P, Jambes);
					C->ComponentTags.Add(FName(TEXT("ChellesJambes")));
					Crees.Add(C);
				}
			}
		}
	}

	FTenue TenueLocale()
	{
		FString Texte;
		if (GConfig)
		{
			GConfig->GetString(SectionIni, TEXT("Tenue"), Texte, GGameUserSettingsIni);
		}
		FTenue T = FTenue::DepuisTexte(Texte);
		Completer(T);
		return T;
	}

	void EnregistrerTenue(const FTenue& Tenue)
	{
		if (GConfig)
		{
			GConfig->SetString(SectionIni, TEXT("Tenue"), *Tenue.VersTexte(), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}
}

// ---------------------------------------------------------------- Rendre le corps d'origine invisible

namespace HabillageInterne
{
static void RenduCache(USkeletalMeshComponent* C, bool bCacher, bool bOmbreDOrigine)
{
	if (!C)
	{
		return;
	}
	if (bCacher)
	{
		// Le corps reste anime (il porte les armes et sert de squelette) mais ne se dessine plus
		C->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		C->bRenderInDepthPass = false;
		C->SetRenderInMainPass(false);
		C->SetCastShadow(false);
	}
	else
	{
		C->bRenderInDepthPass = true;
		C->SetRenderInMainPass(true);
		C->SetCastShadow(bOmbreDOrigine);
	}
}
}

// ---------------------------------------------------------------- Relais (multijoueur)

AHabillageRelais::AHabillageRelais()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	NetUpdateFrequency = 5.f;
	SetActorHiddenInGame(true);
	PrimaryActorTick.bCanEverTick = false;
}

void AHabillageRelais::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHabillageRelais, Joueur);
	DOREPLIFETIME(AHabillageRelais, Tenue);
}

void AHabillageRelais::ServeurTenue_Implementation(const FString& Texte)
{
	Tenue = Texte.Left(1500);
	ForceNetUpdate();
}

// ---------------------------------------------------------------- Habilleur

AHabillageChelles::AHabillageChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorHiddenInGame(true);
	bReplicates = false;
}

void AHabillageChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	TArray<TWeakObjectPtr<APawn>> Pions;
	Habilles.GetKeys(Pions);
	for (const TWeakObjectPtr<APawn>& P : Pions)
	{
		if (P.IsValid())
		{
			Deshabiller(P.Get());
		}
	}
	Habilles.Reset();
	Super::EndPlay(Raison);
}

bool AHabillageChelles::EstSoldat(APawn* Pion) const
{
	ACharacter* Perso = Cast<ACharacter>(Pion);
	if (!Perso || !Perso->GetMesh() || !HabillageInterne::AUnBassin(Perso->GetMesh()->SkeletalMesh))
	{
		return false;
	}
	UWorld* Monde = GetWorld();
	AGameStateBase* Etat = Monde ? Monde->GetGameState() : nullptr;
	if (Etat && Etat->GameModeClass)
	{
		const AGameModeBase* Jeu = Etat->GameModeClass->GetDefaultObject<AGameModeBase>();
		if (Jeu && Jeu->DefaultPawnClass)
		{
			return Pion->IsA(Jeu->DefaultPawnClass);
		}
	}
	const FString Classe = Pion->GetClass()->GetName();
	return Classe.Contains(TEXT("Character")) && !Classe.Contains(TEXT("Vol"));
}

void AHabillageChelles::GererRelais()
{
	UWorld* Monde = GetWorld();
	TArray<APlayerController*> Servis;
	TArray<AHabillageRelais*> Relais;
	for (TActorIterator<AHabillageRelais> It(Monde); It; ++It)
	{
		Relais.Add(*It);
	}
	for (AHabillageRelais* R : Relais)
	{
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		if (!PC)
		{
			R->Destroy(); // joueur parti
			continue;
		}
		Servis.Add(PC);
		if (R->Joueur != PC->PlayerState)
		{
			R->Joueur = PC->PlayerState;
			R->ForceNetUpdate();
		}
	}
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || Servis.Contains(PC))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = PC;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AHabillageRelais* Nouveau = Monde->SpawnActor<AHabillageRelais>(AHabillageRelais::StaticClass(), FTransform::Identity, Params))
		{
			Nouveau->Joueur = PC->PlayerState;
		}
	}
}

void AHabillageChelles::EnvoyerMaTenue()
{
	UWorld* Monde = GetWorld();
	APlayerController* Moi = Monde ? Monde->GetFirstPlayerController() : nullptr;
	if (!Moi || !Moi->IsLocalController())
	{
		return;
	}
	const FString Texte = ChellesHabillage::TenueLocale().VersTexte();
	for (TActorIterator<AHabillageRelais> It(Monde); It; ++It)
	{
		if (It->GetOwner() != Moi)
		{
			continue;
		}
		if (It->Tenue == Texte)
		{
			return;
		}
		if (It->HasAuthority())
		{
			It->Tenue = Texte; // hote d'une partie LAN
			It->ForceNetUpdate();
		}
		else if (Texte != DernierEnvoi || ProchainEnvoi <= 0.f)
		{
			It->ServeurTenue(Texte);
			DernierEnvoi = Texte;
			ProchainEnvoi = 3.f;
		}
		return;
	}
}

void AHabillageChelles::Deshabiller(APawn* Pion)
{
	FHabille* H = Habilles.Find(Pion);
	if (!H)
	{
		return;
	}
	for (const TWeakObjectPtr<USkeletalMeshComponent>& C : H->Corps)
	{
		if (C.IsValid())
		{
			C->DestroyComponent();
		}
	}
	for (const TWeakObjectPtr<USkeletalMeshComponent>& C : H->Bas)
	{
		if (C.IsValid())
		{
			C->DestroyComponent();
		}
	}
	HabillageInterne::RenduCache(H->Chef.Get(), false, H->bChefOmbre);
	HabillageInterne::RenduCache(H->Jambes.Get(), false, H->bJambesOmbre);
	Habilles.Remove(Pion);
}

USkeletalMeshComponent* AHabillageChelles::Cavalier(APawn* Pion)
{
	if (!Pion)
	{
		return nullptr;
	}
	TArray<USkeletalMeshComponent*> Composants;
	Pion->GetComponents<USkeletalMeshComponent>(Composants);
	for (USkeletalMeshComponent* C : Composants)
	{
		if (C && C->GetName().Contains(TEXT("Rider")) && HabillageInterne::AUnBassin(C->SkeletalMesh))
		{
			return C;
		}
	}
	return nullptr;
}

void AHabillageChelles::Habiller(APawn* Pion, const FString& Texte, USkeletalMeshComponent* Chef)
{
	if (FHabille* Deja = Habilles.Find(Pion))
	{
		if (Deja->Texte == Texte && Deja->Chef.Get() == Chef)
		{
			return;
		}
	}
	Deshabiller(Pion);

	const ChellesHabillage::FTenue Tenue = ChellesHabillage::FTenue::DepuisTexte(Texte);
	TArray<ChellesHabillage::FPiece> Pieces;
	UClass* Anim = nullptr;
	USkeletalMesh* Squelette = nullptr;
	ACharacter* Perso = Cast<ACharacter>(Pion);
	if (!Chef || !ChellesHabillage::Assembler(Tenue, Pieces, Anim, Squelette))
	{
		return;
	}
	FHabille H;
	H.Texte = Texte;
	H.Chef = Chef;
	H.bChefOmbre = Chef->CastShadow;
	USkeletalMeshComponent* Jambes = nullptr;
	if (Perso && Chef == Perso->GetMesh())
	{
		// Soldat : jambes vues en vue a la 1re personne (pas pour le cavalier du cheval)
		TArray<USkeletalMeshComponent*> Composants;
		Perso->GetComponents<USkeletalMeshComponent>(Composants);
		for (USkeletalMeshComponent* C : Composants)
		{
			if (C && C != Chef && C->GetName().Contains(TEXT("Legs")) && HabillageInterne::AUnBassin(C->SkeletalMesh))
			{
				Jambes = C;
				break;
			}
		}
	}
	if (Jambes)
	{
		H.Jambes = Jambes;
		H.bJambesOmbre = Jambes->CastShadow;
	}
	TArray<USkeletalMeshComponent*> Crees;
	ChellesHabillage::Construire(Pion, Chef, Jambes, Pieces, Crees, false);
	for (USkeletalMeshComponent* C : Crees)
	{
		if (C->ComponentHasTag(FName(TEXT("ChellesJambes"))))
		{
			H.Bas.Add(C);
		}
		else
		{
			H.Corps.Add(C);
		}
	}
	HabillageInterne::RenduCache(Chef, true, false);
	HabillageInterne::RenduCache(Jambes, true, false);
	Habilles.Add(Pion, MoveTemp(H));
	UE_LOG(LogTemp, Log, TEXT("[Habillage] %s habille : %s (%d morceaux)."), *Pion->GetName(), *Tenue.Modele, Crees.Num());
}

void AHabillageChelles::Suivre()
{
	// Les morceaux copient ce que le jeu fait du corps (vue 1re / 3e personne, cache, visible)
	auto Copier = [](USkeletalMeshComponent* Source, const TArray<TWeakObjectPtr<USkeletalMeshComponent>>& Liste)
	{
		if (!Source)
		{
			return;
		}
		if (Source->bRenderInMainPass)
		{
			HabillageInterne::RenduCache(Source, true, false); // quelqu'un a remis le corps : on le recache
		}
		for (const TWeakObjectPtr<USkeletalMeshComponent>& C : Liste)
		{
			if (!C.IsValid())
			{
				continue;
			}
			if (C->bOwnerNoSee != Source->bOwnerNoSee)
			{
				C->SetOwnerNoSee(Source->bOwnerNoSee);
			}
			if (C->bOnlyOwnerSee != Source->bOnlyOwnerSee)
			{
				C->SetOnlyOwnerSee(Source->bOnlyOwnerSee);
			}
			if (C->bHiddenInGame != Source->bHiddenInGame)
			{
				C->SetHiddenInGame(Source->bHiddenInGame); // cavalier cache tant qu'on n'est pas en selle
			}
			if (C->IsVisible() != Source->IsVisible())
			{
				C->SetVisibility(Source->IsVisible());
			}
		}
	};
	for (TPair<TWeakObjectPtr<APawn>, FHabille>& Paire : Habilles)
	{
		Copier(Paire.Value.Chef.Get(), Paire.Value.Corps);
		Copier(Paire.Value.Jambes.Get(), Paire.Value.Bas);
	}
}

void AHabillageChelles::Tick(float Delta)
{
	Super::Tick(Delta);
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return;
	}
	ProchainEnvoi -= Delta;
	ProchainControle -= Delta;
	if (ProchainControle <= 0.f)
	{
		ProchainControle = 0.25f;
		const ENetMode Mode = GetNetMode();
		if (Mode == NM_ListenServer || Mode == NM_DedicatedServer)
		{
			GererRelais();
		}
		if (Mode != NM_Standalone)
		{
			EnvoyerMaTenue();
		}

		// Qui porte quoi
		TMap<APlayerState*, FString> Tenues;
		if (Mode == NM_Standalone)
		{
			if (APlayerController* Moi = Monde->GetFirstPlayerController())
			{
				if (Moi->PlayerState)
				{
					Tenues.Add(Moi->PlayerState, ChellesHabillage::TenueLocale().VersTexte());
				}
			}
		}
		else
		{
			for (TActorIterator<AHabillageRelais> It(Monde); It; ++It)
			{
				if (It->Joueur && !It->Tenue.IsEmpty())
				{
					Tenues.Add(It->Joueur, It->Tenue);
				}
			}
		}

		TArray<APawn*> Vus;
		for (TActorIterator<APawn> It(Monde); It; ++It)
		{
			APawn* Pion = *It;
			const FString* Texte = Pion->GetPlayerState() ? Tenues.Find(Pion->GetPlayerState()) : nullptr;
			USkeletalMeshComponent* Chef = nullptr;
			if (EstSoldat(Pion))
			{
				Chef = CastChecked<ACharacter>(Pion)->GetMesh();
			}
			else if (USkeletalMeshComponent* Selle = Cavalier(Pion))
			{
				// Cheval : le cavalier porte la tenue du joueur (sinon la roue TAB lui met Spider-Man)
				Chef = Selle;
				if (!Texte && Mode == NM_Standalone && !Selle->bHiddenInGame && Tenues.Num() > 0)
				{
					Texte = &Tenues.CreateConstIterator().Value(); // en solo, le cavalier visible, c'est nous
				}
			}
			if (Chef && Texte && !ChellesHabillage::FTenue::DepuisTexte(*Texte).EstSpiderMan())
			{
				Habiller(Pion, *Texte, Chef);
				Vus.Add(Pion);
			}
		}
		TArray<TWeakObjectPtr<APawn>> Anciens;
		Habilles.GetKeys(Anciens);
		for (const TWeakObjectPtr<APawn>& P : Anciens)
		{
			if (!P.IsValid())
			{
				Habilles.Remove(P);
			}
			else if (!Vus.Contains(P.Get()))
			{
				Deshabiller(P.Get()); // Spider-Man choisi, ou personnage pris par un autre mode
			}
		}
	}
	Suivre();
}

// ---------------------------------------------------------------- Apercu du menu

AApercuHabillage::AApercuHabillage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bReplicates = false;

	USceneComponent* Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Racine);

	Corps = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Corps"));
	Corps->SetupAttachment(Pivot);
	Corps->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // les mannequins regardent vers +Y
	Corps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Corps->PrimaryComponentTick.bTickEvenWhenPaused = true;
	Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Camera = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Camera"));
	Camera->SetupAttachment(Racine);
	Camera->SetRelativeLocation(FVector(330.f, 0.f, 100.f));
	Camera->SetRelativeRotation(FRotator(-2.f, 180.f, 0.f));
	Camera->FOVAngle = 34.f;
	Camera->bCaptureEveryFrame = false;
	Camera->bCaptureOnMovement = false;
	Camera->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Camera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Camera->ShowFlags.SetFog(false);
	Camera->ShowFlags.SetMotionBlur(false);
	Camera->ShowFlags.SetEyeAdaptation(false);

	Lampe1 = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lampe1"));
	Lampe1->SetupAttachment(Racine);
	Lampe1->SetRelativeLocation(FVector(260.f, 170.f, 230.f));
	Lampe1->SetIntensity(9000.f);
	Lampe1->SetAttenuationRadius(900.f);
	Lampe1->SetLightColor(FLinearColor(1.f, 0.95f, 0.9f));
	Lampe1->SetCastShadows(false);

	Lampe2 = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lampe2"));
	Lampe2->SetupAttachment(Racine);
	Lampe2->SetRelativeLocation(FVector(-120.f, -220.f, 160.f));
	Lampe2->SetIntensity(6000.f);
	Lampe2->SetAttenuationRadius(900.f);
	Lampe2->SetLightColor(FLinearColor(0.35f, 0.85f, 1.f)); // contre-jour cyan, comme le menu
	Lampe2->SetCastShadows(false);
}

void AApercuHabillage::BeginPlay()
{
	Super::BeginPlay();
	SetTickableWhenPaused(true);
	Cible = NewObject<UTextureRenderTarget2D>(this);
	Cible->ClearColor = FLinearColor(0.01f, 0.012f, 0.03f, 1.f);
	Cible->InitAutoFormat(512, 768);
	Camera->TextureTarget = Cible;
	Camera->ShowOnlyActors.Reset();
	Camera->ShowOnlyActors.Add(this);
}

void AApercuHabillage::Montrer(const ChellesHabillage::FTenue& Tenue, USkeletalMesh* MeshSpiderMan, UClass* AnimSpiderMan)
{
	for (USkeletalMeshComponent* C : Morceaux)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	Morceaux.Reset();

	TArray<ChellesHabillage::FPiece> Pieces;
	UClass* Anim = nullptr;
	USkeletalMesh* Squelette = nullptr;
	if (!Tenue.EstSpiderMan() && ChellesHabillage::Assembler(Tenue, Pieces, Anim, Squelette) && Squelette)
	{
		Corps->SetSkeletalMesh(Squelette);
		HabillageInterne::RenduCache(Corps, true, false);
	}
	else
	{
		Corps->SetSkeletalMesh(MeshSpiderMan);
		Anim = AnimSpiderMan;
		HabillageInterne::RenduCache(Corps, false, true);
		Pieces.Reset();
	}
	if (Anim)
	{
		Corps->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Corps->SetAnimInstanceClass(Anim);
	}
	else
	{
		Corps->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
	TArray<USkeletalMeshComponent*> Crees;
	ChellesHabillage::Construire(this, Corps, nullptr, Pieces, Crees, true);
	Morceaux = Crees;
}

void AApercuHabillage::Tick(float Delta)
{
	Super::Tick(Delta);
	if (Pause > 0.f)
	{
		Pause -= Delta;
	}
	else
	{
		Angle += Delta * 22.f; // tourne doucement
	}
	Pivot->SetRelativeRotation(FRotator(0.f, Angle, 0.f));
	if (Cible)
	{
		Camera->CaptureScene();
	}
}
