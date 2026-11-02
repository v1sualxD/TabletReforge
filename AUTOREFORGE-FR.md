# TabletReforge — guide français

## État

Sources expérimentales de TabletReforge 0.1.0, plugin indépendant dérivé de TabletHelper 1.4.7.
Aucune DLL n'a été compilée ici : cette machine n'a ni MSVC ni le SDK Windows nécessaires.
Les tests C++ sont fournis mais n'ont pas été exécutés ici. Le workflow GitHub compile la DLL
et exécute les tests avant de fabriquer TabletReforge.zip. Le fonctionnement en jeu reste à valider.

## Sélection des tablettes

Dans Types de tablettes, modifier les règles de chaque catégorie (Abyss, Expedition, Breach, etc.) :
mods recherchés, valeurs minimum/maximum, mods requis et utilisations restantes.
Le profil actif définit les tablettes à GARDER : elles sont surlignées et ne sont pas reforgées.
Une règle active sans bonus sélectionné conserve toutes les tablettes de sa catégorie.
La règle Toutes les catégories peut également protéger une tablette.

Dans Auto reforge, cocher séparément les catégories dont les rejets peuvent être consommés.
Par défaut aucune catégorie n'est autorisée. La lecture des mods doit être activée.
Le plugin n'inverse pas un échec de lecture en rejet : mods illisibles et catégories inconnues sont protégés.
Les tablettes uniques, corrompues et non identifiées sont exclues.
Les trios utilisent la même base exacte et la même rareté : jamais Abyss + Expedition dans un lot.
Un reliquat de une ou deux tablettes reste dans le sac.

La liste est figée au lancement. Chaque résultat est récupéré puis exclu des lots suivants.
Une tablette non identifiée reste également exclue si tu relances plus tard.
Les règles sont copiées au lancement et chaque tablette est vérifiée à nouveau avant son transfert.
Modifier les réglages pendant la séquence l'arrête.

## Compiler et installer depuis GitHub

1. Créer un dépôt PUBLIC nommé exactement TabletReforge, sans ajouter de README initial.
2. Décompresser l'archive des sources. Mettre le CONTENU du dossier TabletReforge à la racine du dépôt.
   On doit voir TabletReforge.sln, Plugins, POEFixer, tests et .github à la racine.
   Avec GitHub Desktop : cloner le dépôt vide, copier ces fichiers dans son dossier local,
   puis Commit to main et Push origin. Vérifier que le dossier caché .github est bien inclus.
   GitHub Desktop peut demander le nom de branche choisi pour le dépôt : conserver la branche par défaut.
3. Sur GitHub : Actions → Build TabletReforge → Run workflow → Run workflow.
4. Attendre la réussite du job. Ouvrir son exécution et télécharger l'artefact TabletReforge.
5. Décompresser l'artefact : il contient TabletReforge.zip, l'archive destinée à POEFixer.
   Ne pas utiliser l'archive des sources ni l'enveloppe de téléchargement de l'artefact comme asset.
6. Releases → Draft a new release. Créer le tag v0.1.0, titre TabletReforge 0.1.0,
   joindre le fichier TabletReforge.zip et publier la release.
7. POEFixer → Plugins → Plugin Manager → Add from GitHub.
   Coller https://github.com/TON-PSEUDO/TabletReforge puis installer.

Pour une version suivante, changer la version dans POEFIXER_PLUGIN_INFO, recompiler,
puis publier un nouveau tag et une nouvelle archive avec la même version.
Les sources modifiées restent distribuées sous GPL-3.0 avec leurs crédits.
Le workflow ne publie aucune release automatiquement.

## Compilation locale alternative

Visual Studio 2022 avec Développement Desktop en C++, MSVC v143, SDK Windows 10/11.
Ouvrir TabletReforge.sln, choisir Release | x64, compiler.
DLL : x64/Release/Plugins/TabletReforge/TabletReforge.dll.
Installation manuelle : copier cette DLL et le dossier lang dans Plugins/TabletReforge
sous le dossier de POEFixer. Le chargeur normal de plugins suffit, aucun injecteur externe.
Désactiver TabletHelper si ses surlignages font doublon avec ceux de TabletReforge.
TabletReforge possède sa propre configuration ; configurer ses filtres avant utilisation.

## Première utilisation

1. Activer le plugin puis l'option Auto reforge.
2. Configurer les mods à garder dans les catégories et choisir les catégories autorisées à la reforge.
3. Ouvrir le reforger, vider ses trois entrées et son résultat, fermer le menu POEFixer.
4. Survoler le centre du bouton Reforger en jeu et appuyer sur F6.
5. Survoler le centre de l'emplacement du résultat et appuyer sur F7.
6. Rouvrir les réglages, cocher J'ai vidé les 3 entrées et le résultat.
7. Cliquer sur Lancer la reforge, puis fermer le menu sous 30 secondes.
8. La séquence démarre quand le jeu récupère le focus. Échap l'arrête.

Commencer la validation en jeu avec trois tablettes sans valeur qui ne correspondent pas aux filtres,
et une tablette à garder pour vérifier qu'elle reste dans le sac.
Laisser clavier et souris au repos pendant la séquence et ne pas ouvrir le chat.
Le statut apparaît en haut à gauche du jeu et dans le menu du plugin.

Le calibrage reste en mémoire pendant la session uniquement. Il peut devoir être refait si le jeu
recrée le panneau. Le SDK ne fournit pas de commande de reforge : le plugin utilise Ctrl + clic
sur les entrées et le résultat, et un clic sur le bouton calibré.
La calibration et la confirmation du panneau vide sont nécessaires : l'API ne permet pas de
certifier l'identité du panneau ni de vérifier que tous ses emplacements sont vides.

Chaque transfert attend un nouveau snapshot du sac et vérifie les objets restants.
Délai par défaut : 750 ms, réglable de 250 à 2 000 ms ; timeout de confirmation : 6 secondes.
Arrêt en cas de focus perdu, menu rouvert, panneau fermé/déplacé, résolution/zone modifiée,
automatisation concurrente signalée par l'hôte ou inventaire inattendu.
Après un arrêt, récupérer manuellement les objets restés au reforger avant de relancer.
Le plugin exige les snapshots datés et la géométrie visible du sac 12 × 5 fournis par le SDK.

## Vérifications fournies

- ReforgePlanTests.cpp : trios, séparation base/rareté, doublons, reliquats, unid et exclusions.
- ReforgeFilterTests.cpp : règles de mods, seuils, protection des surlignages et règle globale.
- Workflow manuel Windows : compilation de la DLL, exécution des deux tests et archive installable.
- Vérifications locales effectuées : XML du projet, références aux fichiers et SDK/ImGui inchangés.

## Origine

Dérivé de POEFixer/TabletHelper, commit ac8fa16e04a6237d1df95917051cfe2d8d709828.
Travail original : Omer Faruk ARPA et POEFixer. Licence GPL-3.0, voir LICENSE.
TabletReforge est une modification indépendante non publiée par POEFixer.