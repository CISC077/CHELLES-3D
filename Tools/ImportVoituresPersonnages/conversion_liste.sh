B="/mnt/user-data/uploads/CHELLES 3D/Content"; O=/tmp/claude-0/out; P=/tmp/claude-0/bvenv/bin/python
rm -rf $O; 
c(){ $P conv.py "$1" "$2" "$O/$3/$4" "$4" "$5" "$6" > "logc_$4.txt" 2>&1; grep RESULTAT "logc_$4.txt" || { echo "ECHEC $4"; tail -5 "logc_$4.txt"; }; }
c car "$B/Voitures/1963_volkswagen_beetle_1200_deluxe.glb" Voitures SM_Voiture_VW_Coccinelle_1963 4.08
c car "$B/Voitures/1963_volkswagen_beetle_1200_sunroof_type_1.glb" Voitures SM_Voiture_VW_Coccinelle_1963_ToitOuvrant 4.08
c car "$B/Voitures/1968_volkswagen_beetle.glb" Voitures SM_Voiture_VW_Coccinelle_1968 4.08
c car "$B/Voitures/1976_volkswagen_golf_gti_mk1.glb" Voitures SM_Voiture_VW_Golf_GTI_1976 3.73
c car "$B/Voitures/1998_volkswagen_new_beetle.glb" Voitures SM_Voiture_VW_New_Beetle_1998 4.08
c car "$B/Voitures/2015_rallycross_volkswagen_beetle_grc.glb" Voitures SM_Voiture_VW_Beetle_Rallycross_2015 4.25
c car "$B/Voitures/2022_volkswagen_saveiro_1.6_robust.glb" Voitures SM_Voiture_VW_Saveiro_2022 4.49
c car "$B/Voitures/porsche_cayman_gts_off-road.glb" Voitures SM_Voiture_Porsche_Cayman_OffRoad 4.45
c car "$B/Voitures/porsche_macan_turbo_type_95b_off-road.glb" Voitures SM_Voiture_Porsche_Macan_OffRoad 4.70
c car "$B/Voitures/volkswagen_beetle_grc.glb" Voitures SM_Voiture_VW_Beetle_GRC 4.25
c skel "$B/Personnages/hulk_brand_new_day_-_marvel_future_fight.glb" Personnages SK_Hulk 2.15 walk,wait
c skel "$B/Personnages/bruce_banner_sm_bnd.glb" Personnages SK_BruceBanner 1.78
c skel "$B/Personnages/spider-man/source/Agreeing.fbx" Personnages SK_SpiderMan 1.80
c statue "$B/Personnages/anime_tomb_raider_girl_-_png_to_stl_-_supavoxel.glb" Personnages SM_Statue_TombRaider 1.70
c statue "$B/Personnages/black_cat_-_coastal_cat.glb" Personnages SM_Statue_BlackCat_Coastal 1.72
c statue "$B/Personnages/black_cat_marvel_furry_3d_model.glb" Personnages SM_Statue_BlackCat 1.72
c statue "$B/Personnages/black_panther_-_civil_war.glb" Personnages SM_Statue_BlackPanther 1.83
c statue "$B/Personnages/kyle_t_pose.glb" Personnages SM_Statue_Kyle 1.80
c statue "$B/Personnages/mercenary_captain.glb" Personnages SM_Statue_Mercenaire 1.85
c statue "$B/Personnages/ochaco_uraraka_my_hero_academia_3d_model.glb" Personnages SM_Statue_Ochaco 1.60
c statue "$B/Personnages/spider-gwen/source/ip-spider-man__spider-gwen__real__sexy.glb" Personnages SM_Statue_SpiderGwen 1.70
echo FINI
