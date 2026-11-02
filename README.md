# TabletReforge

Plugin C++ pour POEFixer / Path of Exile 2, dérivé de TabletHelper 1.4.7 (GPL-3.0).
Identifiant et DLL : **TabletReforge**. Version initiale : **0.1.0**.

Configure des filtres de mods par catégorie pour **surligner et conserver** les bonnes tablettes.
Le bouton **Lancer la reforge** traite les autres tablettes identifiées du sac, uniquement dans
les catégories explicitement autorisées : Abyss ×3, Expedition ×3, etc., avec même base et rareté.
Les résultats sont remis dans le sac ; les tablettes non identifiées ne sont jamais sélectionnées.

**État : DLL Release x64 compilée sur GitHub Actions, avec 0 erreur et 0 avertissement de compilation. Les deux programmes de tests ont réussi. Le comportement en jeu reste expérimental et non validé.**
Le code des tests est fourni et le workflow les exécute avant de produire l'archive de la DLL.

Lire [le guide français](AUTOREFORGE-FR.md) pour compiler, calibrer et installer.

## Compiler sur GitHub

1. Créer un dépôt nommé **TabletReforge** et y mettre le contenu de ce dossier à la racine.
2. Ouvrir **Actions → Build TabletReforge → Run workflow**.
3. Si la compilation et les deux programmes de tests réussissent, télécharger l'artefact **TabletReforge**.
4. L'archive de l'artefact contient `TabletReforge.zip`, à extraire dans le dossier de POEFixer.

Le workflow produit une archive ; il ne publie aucune release automatiquement.
Pour distribuer via **Add from GitHub**, publier une release `v0.1.0` avec l'asset **TabletReforge.zip**.
Le nom du dépôt, l'identifiant du plugin, le dossier installé et la DLL portent tous le nom TabletReforge.

## Compiler localement

Visual Studio 2022, charge de travail Desktop C++, MSVC v143 et SDK Windows 10/11.
Ouvrir **TabletReforge.sln**, compiler **Release | x64**.
DLL produite : `x64/Release/Plugins/TabletReforge/TabletReforge.dll`.

## Origine et licence

Base : [POEFixer/TabletHelper](https://github.com/POEFixer/TabletHelper), commit
`ac8fa16e04a6237d1df95917051cfe2d8d709828` du 31 octobre 2026.
Travail original : Omer Faruk ARPA et POEFixer. Sources et crédits conservés ; licence GPL-3.0.
TabletReforge est une modification indépendante, pas une release officielle de POEFixer.
Les noms internes de classes et de namespace TabletHelper sont conservés pour limiter les modifications.
