# Do it Asphalt

Pokojná inkrementálna simulácia asfaltovania v prvej osobe. Hráč vyleje asfalt na parkovisko v tvare písmena L (30 000 štvorcov 1 × 1 m) pred nákupným centrom. Začína lyžicou a miskou, neskôr používa vedro, kanister, vaňu, kufor, vozík, robota a auto; robot a auto si chodia po asfalt do výdajne. Hra má zvuky (stroje, liatie, okolie) a bežné nastavenia. Za každý hotový štvorec dostane 10 kreditov a za každý vyliaty liter 1 kredit, a za kredity kupuje lepšie vybavenie a rýchlejšiu výrobňu. Keď je vyasfaltované všetko, otvorí sa východ a prechodom bránou hra končí výhrou. Hra má podľa simulácie trvať asi 2 h 20 min.

Projekt je v Unreal Engine 5.8 (C++ a Slate UI), cieľová platforma je macOS (vývoj) a Windows.

## Požiadavky

- **Unreal Engine 5.8** nainštalovaný v `/Users/Shared/Epic Games/UE_5.8` (iná cesta sa dá zadať premennou prostredia `UE_ROOT`, pozri `Scripts/env.sh`).
- **Xcode** s nástrojmi príkazového riadku (kompilácia C++ pre Mac).
- **Blender 5.3** – len na generovanie 3D modelov (`Tools/ModelGen`); na hranie ani na build nie je potrebný.
- Python 3 (test skript ním číta správu o testoch).

## Príkazy (`Scripts/`)

Všetky skripty bežia z koreňa projektu a berú cesty z `Scripts/env.sh`. Nikdy nespúšťaj dva z nich naraz (zdieľajú `Saved/` a `Intermediate/`).

| Príkaz | Čo robí |
|---|---|
| `Scripts/build.sh` | Skompiluje editorový cieľ `DoItAsphaltEditor` (Mac, Development). |
| `Scripts/test.sh [filter]` | Spustí automatické testy editora bez okna (predvolený filter `DoItAsphalt`) a vypíše `PASSED n / FAILED m`; skončí chybou, ak nejaký test padol alebo chýba správa. |
| `Scripts/shot.sh [--menu] <meno> ["cheat; cheat"]` | Spustí hru v okne na `L_Main`, vykoná cheaty a uloží `Saved/Screenshots/<meno>.png`. Používa vlastný slot `DIA_Shot`, skutočné uloženie sa nedotkne. |
| `Scripts/uepy.sh <script.py>` | Spustí Python skript v editore bez okna a zlyhá pri `DIA_FAIL`, tracebacku alebo chýbajúcom `DIA_OK`. Pozri poznámku k `create_materials.py` nižšie. |
| `Scripts/setup_content.sh` | Skopíruje postavu Mannequin zo šablóny tretej osoby v engine do `Content/Characters/Mannequins` (ak tam už je, preskočí). |
| `Scripts/package.sh` | Zbalí Shipping build pre Mac (`RunUAT BuildCookRun`) do `Builds/`; na úspech vypíše `BUILD SUCCESSFUL` a cestu k `.app`. |
| `Scripts/run.sh [args]` | Otvorí projekt v Unreal Editore. |

Typický postup: `Scripts/build.sh && Scripts/test.sh`, potom `Scripts/run.sh`.

## Ovládanie

| Akcia | Vstup |
|---|---|
| Chôdza | `W` `A` `S` `D` |
| Pohľad (v aute otáča kamerou okolo auta) | myš |
| Použiť ruku (liať, vedro leje prúd na zameraný bod; s vozíkom klásť pás, v aute liať) | ľavé tlačidlo myši (držať) |
| Pracovný pohľad | pravé tlačidlo myši (držať) |
| Ďalšia ruka / miska, vedro, kanister, vaňa | `Q` / `1` `2` `3` `4` |
| Chytiť, pustiť, nastúpiť a vystúpiť z auta | `E` |
| Naberať (koryto → miska lyžicou, naplniť vedro, kanister a vaňu pod prúdom) | `R` (držať) |
| Klapka kufra | `F` (držať) |
| Mapa sektorov robota (klik na sektor ho tam pošle) | `G` |
| Poslať robota do sektora, v ktorom stojíš | `H` |
| Šprint (po odomknutí) | ľavý `Shift` |
| Obchod | `Tab` |
| Kontrolný pohľad | `V` (držať) |
| Pauza / zavrieť obchod / zavrieť nastavenia | `Esc` |

V aute (pohľad z tretej osoby): `W`/`S` plyn a spiatočka, `A`/`D` zatáčanie, myš otáča kamerou okolo auta, ľavé tlačidlo leje z nádrže, `E` vystúpiť. Nádrž sa plní vo výdajni (bránka pod parkoviskom, alebo kúpená presuvná Station). Gamepad nie je podporovaný. Úplný opis je v [`docs/01-herny-dizajn.md`](docs/01-herny-dizajn.md).

## Cheaty pre vývoj

Konzolové príkazy `dia.*` sú v `Source/DoItAsphalt/Game/DIACheats.cpp` pod `#if !UE_BUILD_SHIPPING`, takže v Shipping builde neexistujú. Skript `shot.sh` na nich stojí, preto sa nedá použiť so Shipping buildom. Je ich 27 (`dia.Item` pozná aj `Canister` a `Tub`, `dia.Hand` ruky `Bowl`, `Bucket`, `Canister`, `Tub`):

`dia.Give`, `dia.Complete`, `dia.CompleteEntry`, `dia.Item`, `dia.Grab`, `dia.Drop`, `dia.CartStrip`, `dia.Place`, `dia.Plant`, `dia.EnterCar`, `dia.Teleport`, `dia.Prompt`, `dia.Toast`, `dia.SendPaver`, `dia.Save`, `dia.Load`, `dia.NewGame`, `dia.SkipMenu`, `dia.Camera`, `dia.WorkView`, `dia.CheckView`, `dia.Shop`, `dia.Hand`, `dia.Fill`, `dia.TopView`, `dia.Screen`, `dia.ShotAndQuit`.

Argumenty a účinok každého sú v [`docs/01-herny-dizajn.md`](docs/01-herny-dizajn.md), kapitola 11. Spustenie s vlastným slotom uloženej hry (aby sa nedotkol skutočného `DoItAsphalt_Slot0`): argument `-DIASlot=<meno>`.

## Štruktúra repozitára

| Cesta | Čo obsahuje |
|---|---|
| `Source/AsphaltCore` | Čisté jadro hry bez enginu (`FLotGrid`, `FLotGame`, ekonomika, uloženie „DIA2“) s vlastnými testami a simuláciou balansu. |
| `Source/DoItAsphalt` | Herný modul: postava, ovládanie, svet, HUD a menu v Slate, uloženie, cheaty, testy. |
| `Tools/ModelGen` | Blender skripty, ktoré generujú 3D modely (manifest v `manifest.json`, `build_all.sh`). |
| `Tools/UE` | Python skripty pre editor (import modelov a zvukov, materiály, mapa `L_Main`), spúšťajú sa cez `Scripts/uepy.sh`. |
| `Tools/Audio` | `gen_sounds.py` vygeneruje všetky zvuky (numpy, slučky sú presne periodické) do `SourceAssets/Audio`; `Tools/UE/import_sounds.py` ich naimportuje ako `/Game/Audio/SW_*`. |
| `SourceAssets` | Zdrojové modely (`Models`) a ich náhľady (`Previews`). |
| `Content`, `Config` | Obsah projektu a nastavenia enginu. |
| `Scripts` | Pomocné príkazy opísané vyššie. |
| `docs` | Dokumentácia (slovensky). |

## Dokumentácia

Začni v [`docs/README.md`](docs/README.md). Dokumenty 1 – 3 popisujú hru v2:

- [Herný dizajn](docs/01-herny-dizajn.md) – pravidlá, ovládanie, HUD, cheaty, známe obmedzenia, zmeny oproti návrhu.
- [Technické poznámky](docs/02-technicky-navrh.md) – moduly, mriežka, uloženie, testy, Shipping.
- [3D modely](docs/03-3d-modely.md) – paleta, modely, pomocné funkcie.

História: [plán zmien](docs/04-plan-zmien-asphalt-self.md) a dva implementačné plány v `docs/superpowers/plans/`.

## Nástroje: `Tools/UE/create_materials.py`

Skript spustený cez `Scripts/uepy.sh Tools/UE/create_materials.py` **nedobehne**: vypíše `DIA_OK /Game/Materials/M_Ground` a pri druhom materiáli (`M_DIA_Flat`) editor spadne na tvrdení `!IsRooted()` v `UMaterialEditingLibrary::DeleteAllMaterialExpressions`. Rovnako padne aj samostatný skript, ktorý len zavolá `delete_material_expression` na načítanom `M_DIA_Flat`, takže príčina nie je v logike skriptu. Zostane po ňom proces `CrashReportClient` (treba ho ukončiť) a prepísaný `Content/Materials/M_Ground.uasset` (treba vrátiť: `git checkout -- Content/Materials/M_Ground.uasset`). Materiály v `Content/Materials/` sú hotové, preto skript netreba spúšťať; zmeny materiálov sa zatiaľ robia v editore.

## Overené

Overené 2026-11-23 (nie viac):

- `Scripts/build.sh` skončil úspechom.
- `Scripts/test.sh` (po tretej osobe v aute, výdajniach, mape robota, nových nádobách, zvukoch a nastaveniach): 246 testov, všetky prešli. Skupiny: `DoItAsphalt.Lot.*` 96, `DoItAsphalt.Game.*` 149, `DoItAsphalt.Core.Smoke` 1.
- Simulácia balansu prešla: východ sa otvorí o 2:21:12 simulovaného času (limit 105 – 165 min); míľniky: 1 políčko 0:42, 10 políčok 1:55, 100 políčok 6:34, 800 políčok 23:04, 6 000 políčok 48:41. Robot a auto čerpajú z nádrží, ktoré plnia vo výdajni, preto je druhá polovica dlhšia než predtým (1:51).
- Shipping cieľ a snímky z 2026-11-21 sú staršie než tieto zmeny a nezopakovali sa; v Shipping binárke boli tehdy overené chýbajúce cheaty (kontrola reťazcov `dia.*`).

Zabalenie `.app` (`Scripts/package.sh`) sa nespúšťalo. Pri kompilácii Shipping cieľa `codesign` balíka zlyhal s `resource fork, Finder information, or similar detritus not allowed`, lebo projekt leží na `~/Desktop` v iCloud Drive a ten pridáva priečinkom rozšírené atribúty. Projekt sa nemá presúvať; `.app` preto neexistuje a kontrola reťazcov je urobená nad samostatnou Shipping binárkou.

## Neoverené – čaká na ručnú kontrolu

Nič z tohto nikto ručne nevyskúšal:

- celé prehratie hry od začiatku do výhry;
- pohľad myšou, zachytenie kurzora a `Esc`;
- zatvorenie obchodu klávesom `Tab`, kým je kurzor zobrazený;
- dvojklik na riadok výrobne a na riadok predmetu v obchode;
- pocit z jazdy z pohľadu tretej osoby (vzdialenosť a výška kamery, citlivosť myši);
- **zvuky**: nikto ich nepočul; hlasitosť, zmiešanie a pocit sú len z kódu (zvuky sú syntetizované skriptom, nie nahrávky);
- **animácie** naberania a liatia (naklonenie nádoby, pohyb lyžice do koryta): pózy ruky a lyžice nie sú odladené podľa snímok;
- vzhľad výdajne (`SM_VehicleStation`), mapy robota (`G`) a lišty so štyrmi nádobami (na snímkach nevideté);
- obrazovka nastavení (posuvník, prepínače, kvalita) a prepínanie celej obrazovky / V-Sync na skutočnom okne;
- vyváženie po zmene: ceny nových nádob a veľkosti nádrží (robot 2 000 l, auto 6 000 / 8 000 l) sú nastavené len podľa simulácie;
- zabalenie `.app` (`codesign` zlyhá, lebo projekt leží v iCloude; projekt sa nemá presúvať).

## Známe obmedzenia

Úplný zoznam je v [`docs/01-herny-dizajn.md`](docs/01-herny-dizajn.md), kapitola 12.

- V prvej osobe nie je animácia pohybu do strán.
- Zvuky sú syntetizované (bez nahrávok) a nikto ich nepočul.
- Starý uložený stav (verzia 2) sa nenačíta: uloženie má verziu 3 a nová hra začne od začiatku.
- Lišta auta sa pri plnom vychýlení volantu môže pohybovať o niečo rýchlejšie, než dovoľuje strop liatia.
- Pri liatí z auta hráča upozorní len riadok „Tank low“; keď sa minie nádrž, auto prestane liať.
- Obrys sektora robota (10 cm) je zďaleka zhora tenký.
- Kým sú zobrazené tri iné oznamy, oznamy o kreditoch čakajú.
- Valček, dusadlo a lata s vodováhou zo špecifikácie v hre nie sú (otvorená otázka O1).
- Tekutý asfalt je vidieť len ako postupujúce pokrytie zeme (bez vlastnej hrúbky či lesku); simulácia vyváženia počíta s okamžitým liatím, takže tekutinu nemeria.
