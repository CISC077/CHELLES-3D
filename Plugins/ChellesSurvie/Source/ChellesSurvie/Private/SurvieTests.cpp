// CHELLES 3D - Modes de survie : tests automatiques des calculs (vagues, points, catalogue).
//
// Pour les lancer dans l'editeur : Window > Developer Tools > Session Frontend > onglet Automation,
// cocher "Chelles.Survie" puis "Start Tests". Ils ne lancent pas la carte : quelques secondes suffisent.
#include "Misc/AutomationTest.h"
#include "SurvieCalculs.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const uint32 OptionsTest = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;
}

// ---- Vagues ----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSurvieVague1, "Chelles.Survie.Vagues.Vague1", OptionsTest)
bool FTestSurvieVague1::RunTest(const FString& Parameters)
{
	FCourbeVagues Courbe; // valeurs par defaut
	const FVagueCalculee V = Courbe.Calculer(1, 1);
	TestEqual(TEXT("Vague 1, 1 joueur : 6 zombies"), V.Nombre, 6);
	TestEqual(TEXT("Vague 1 : vie de base"), V.Vie, 100.f);
	TestEqual(TEXT("Vague 1 : vitesse de base"), V.Vitesse, 300.f);
	TestEqual(TEXT("Vague 1 : pas de costauds"), V.PartCostauds, 0.f);
	TestFalse(TEXT("Vague 1 : pas une vague de coureurs"), V.bCoureurs);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSurvieVaguesMontent, "Chelles.Survie.Vagues.DifficulteMonte", OptionsTest)
bool FTestSurvieVaguesMontent::RunTest(const FString& Parameters)
{
	FCourbeVagues Courbe;
	Courbe.CoureursToutesLes = 0; // la vitesse des coureurs fausserait la comparaison
	FVagueCalculee Avant = Courbe.Calculer(1, 1);
	for (int32 Vague = 2; Vague <= 100; ++Vague)
	{
		const FVagueCalculee V = Courbe.Calculer(Vague, 1);
		TestTrue(FString::Printf(TEXT("Vague %d : au moins autant de zombies"), Vague), V.Nombre >= Avant.Nombre);
		TestTrue(FString::Printf(TEXT("Vague %d : au moins autant de vie"), Vague), V.Vie >= Avant.Vie);
		TestTrue(FString::Printf(TEXT("Vague %d : au moins aussi rapide"), Vague), V.Vitesse >= Avant.Vitesse);
		TestTrue(FString::Printf(TEXT("Vague %d : plafond de zombies"), Vague), V.Nombre <= Courbe.ZombiesMax);
		TestTrue(FString::Printf(TEXT("Vague %d : plafond de vie"), Vague), V.Vie <= Courbe.VieMax);
		TestTrue(FString::Printf(TEXT("Vague %d : plafond de vitesse"), Vague), V.Vitesse <= Courbe.VitesseMax);
		TestTrue(FString::Printf(TEXT("Vague %d : part de costauds entre 0 et le max"), Vague), V.PartCostauds >= 0.f && V.PartCostauds <= Courbe.PartCostaudsMax);
		Avant = V;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSurvieVaguesJoueurs, "Chelles.Survie.Vagues.PlusDeJoueurs", OptionsTest)
bool FTestSurvieVaguesJoueurs::RunTest(const FString& Parameters)
{
	FCourbeVagues Courbe;
	const int32 Seul = Courbe.Calculer(3, 1).Nombre;   // 6 + 2 x 3 = 12
	const int32 Quatre = Courbe.Calculer(3, 4).Nombre; // 12 x (1 + 0,5 x 3) = 30
	TestEqual(TEXT("Vague 3 seul : 12"), Seul, 12);
	TestEqual(TEXT("Vague 3 a 4 joueurs : 30"), Quatre, 30);
	TestEqual(TEXT("0 joueur compte comme 1"), Courbe.Calculer(3, 0).Nombre, Seul);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSurvieVaguesSpeciales, "Chelles.Survie.Vagues.Speciales", OptionsTest)
bool FTestSurvieVaguesSpeciales::RunTest(const FString& Parameters)
{
	FCourbeVagues Courbe;
	TestTrue(TEXT("Vague 5 = coureurs"), Courbe.Calculer(5, 1).bCoureurs);
	TestTrue(TEXT("Vague 10 = coureurs"), Courbe.Calculer(10, 1).bCoureurs);
	TestFalse(TEXT("Vague 6 normale"), Courbe.Calculer(6, 1).bCoureurs);
	TestTrue(TEXT("Coureurs plus rapides que la vague d'avant"), Courbe.Calculer(5, 1).Vitesse > Courbe.Calculer(4, 1).Vitesse * 1.2f);
	TestEqual(TEXT("Costauds des la vague 4"), Courbe.Calculer(4, 1).PartCostauds, 0.05f);
	TestEqual(TEXT("Pas de costauds vague 3"), Courbe.Calculer(3, 1).PartCostauds, 0.f);
	TestEqual(TEXT("Vie d'un costaud = vie x 3"), Courbe.Calculer(4, 1).VieCostaud, Courbe.Calculer(4, 1).Vie * 3.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSurvieVaguesLecture, "Chelles.Survie.Vagues.LectureFichier", OptionsTest)
bool FTestSurvieVaguesLecture::RunTest(const FString& Parameters)
{
	FCourbeVagues Courbe;
	Courbe.Lire({
		TEXT("# commentaire"),
		TEXT(""),
		TEXT("ZombiesVague1 = 10"),
		TEXT("ZombiesEnPlusParVague=0,5"),
		TEXT("VitesseMax=100"),        // plus bas que la vitesse de depart : remonte a VitesseVague1
		TEXT("CleInconnue=42"),
		TEXT("ligne sans egal") });
	TestEqual(TEXT("ZombiesVague1 lu"), Courbe.ZombiesVague1, 10);
	TestEqual(TEXT("Virgule francaise acceptee"), Courbe.ZombiesEnPlusParVague, 0.5f);
	TestEqual(TEXT("VitesseMax jamais sous la vitesse de depart"), Courbe.VitesseMax, Courbe.VitesseVague1);
	return true;
}

// ---- Points --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSurviePoints, "Chelles.Survie.Economie.Points", OptionsTest)
bool FTestSurviePoints::RunTest(const FString& Parameters)
{
	FEconomieRefuge Eco;
	TestEqual(TEXT("Touche simple"), Eco.PointsPourCoup(false, false), 10);
	TestEqual(TEXT("Touche a la tete sans tuer = touche simple"), Eco.PointsPourCoup(false, true), 10);
	TestEqual(TEXT("Kill au corps"), Eco.PointsPourCoup(true, false), 60);
	TestEqual(TEXT("Kill a la tete"), Eco.PointsPourCoup(true, true), 100);
	TestEqual(TEXT("XP kill a la tete"), Eco.XPPourKill(true), 4);
	TestEqual(TEXT("Perte a terre : 10 % de 1234"), Eco.PerteQuandATerre(1234), 123);
	TestEqual(TEXT("Perte a terre sans points"), Eco.PerteQuandATerre(0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSurviePayer, "Chelles.Survie.Economie.Payer", OptionsTest)
bool FTestSurviePayer::RunTest(const FString& Parameters)
{
	int32 Points = 1000;
	TestTrue(TEXT("Achat a 900 accepte"), FEconomieRefuge::Payer(Points, 900));
	TestEqual(TEXT("Reste 100"), Points, 100);
	TestFalse(TEXT("Achat a 500 refuse"), FEconomieRefuge::Payer(Points, 500));
	TestEqual(TEXT("Refus : les points ne bougent pas"), Points, 100);
	TestTrue(TEXT("Prix exact accepte"), FEconomieRefuge::Payer(Points, 100));
	TestEqual(TEXT("Reste 0"), Points, 0);
	TestFalse(TEXT("Prix negatif refuse"), FEconomieRefuge::Payer(Points, -50));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSurvieEconomieLecture, "Chelles.Survie.Economie.LectureFichier", OptionsTest)
bool FTestSurvieEconomieLecture::RunTest(const FString& Parameters)
{
	FEconomieRefuge Eco;
	Eco.Lire({ TEXT("PointsParKill=80"), TEXT("PerteATerrePourcent=25") });
	TestEqual(TEXT("PointsParKill lu"), Eco.PointsParKill, 80);
	TestEqual(TEXT("Perte 25 % de 400"), Eco.PerteQuandATerre(400), 100);
	return true;
}

// ---- Catalogue -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTestSurvieCatalogue, "Chelles.Survie.Catalogue", OptionsTest)
bool FTestSurvieCatalogue::RunTest(const FString& Parameters)
{
	FCatalogueRefuge Cat;
	Cat.Lire({
		TEXT("Arme.Fusil=BPC_WeaponRifle|1200"),
		TEXT("Arme.Pompe = BPC_WeaponShotgun | 900"),
		TEXT("Arme.Fusil=BPC_WeaponRecoil|1300"), // remplace la premiere
		TEXT("Arme.Cassee=sans prix"),
		TEXT("PrixMunitions=350") });
	TestEqual(TEXT("2 armes"), Cat.Armes.Num(), 2);
	const FArticleRefuge* Fusil = Cat.Trouver(TEXT("fusil"));
	TestNotNull(TEXT("Recherche sans majuscules"), Fusil);
	if (Fusil)
	{
		TestEqual(TEXT("Derniere ligne gagne"), Fusil->Prix, 1300);
	}
	TestEqual(TEXT("PrixMunitions lu"), Cat.PrixMunitions, 350);
	TestEqual(TEXT("Nom court -> chemin FPS_Game"), FCatalogueRefuge::CheminClasse(TEXT("BPC_WeaponRifle")),
		FString(TEXT("/Game/FPS_Game/Blueprints/Weapons/BPC_WeaponRifle.BPC_WeaponRifle_C")));
	TestEqual(TEXT("Chemin sans point"), FCatalogueRefuge::CheminClasse(TEXT("/Game/Armes/BP_Arc")),
		FString(TEXT("/Game/Armes/BP_Arc.BP_Arc_C")));
	TestEqual(TEXT("Chemin complet garde"), FCatalogueRefuge::CheminClasse(TEXT("/Game/Armes/BP_Arc.BP_Arc_C")),
		FString(TEXT("/Game/Armes/BP_Arc.BP_Arc_C")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
