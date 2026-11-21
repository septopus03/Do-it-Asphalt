# Do it Asphalt

Pokojná inkrementálna simulácia asfaltovania v prvej osobe. Hráč vyleje asfalt na parkovisko v tvare písmena L (30 000 štvorcov 1 × 1 m) pred nákupným centrom. Začína lyžicou a miskou, neskôr používa vedro, kufor, vozík, hladidlo, finišer, robota a auto. Za každý hotový štvorec dostane 10 kreditov a za kredity kupuje lepšie vybavenie a rýchlejšiu výrobňu. Keď je vyasfaltované všetko, otvorí sa východ a prechodom bránou hra končí výhrou. Hra má podľa simulácie trvať asi 2 h 20 min.

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
| Pohľad | myš |
| Použiť ruku (nabrať, liať, hladiť; s vozíkom klásť pás, s finišerom hladiť, v aute liať) | ľavé tlačidlo myši (držať) |
| Pracovný pohľad | pravé tlačidlo myši (držať) |
| Ďalšia ruka / miska, lopatka, vedro, hladidlo | `Q` / `1` `2` `3` `4` |
| Chytiť, pustiť, nastúpiť a vystúpiť z auta | `E` |
| Klapka kufra | `F` (držať) |
| Poslať robota do sektora, v ktorom stojíš | `G` |
| Šprint (po odomknutí) | ľavý `Shift` |
| Obchod | `Tab` |
| Kontrolný pohľad | `V` (držať) |
| Pauza / zavrieť obchod | `Esc` |

V aute: `W`/`S` plyn a spiatočka, `A`/`D` zatáčanie, myš otáča hlavou, ľavé tlačidlo leje, `E` vystúpiť. Gamepad nie je podporovaný. Úplný opis je v [`docs/01-herny-dizajn.md`](docs/01-herny-dizajn.md).

## Cheaty pre vývoj

Konzolové príkazy `dia.*` sú v `Source/DoItAsphalt/Game/DIACheats.cpp` pod `#if !UE_BUILD_SHIPPING`, takže v Shipping builde neexistujú. Skript `shot.sh` na nich stojí, preto sa nedá použiť so Shipping buildom. Je ich 27:

`dia.Give`, `dia.Complete`, `dia.CompleteEntry`, `dia.Item`, `dia.Grab`, `dia.Drop`, `dia.RawStrip`, `dia.FinishStrip`, `dia.Place`, `dia.Plant`, `dia.EnterCar`, `dia.Teleport`, `dia.Prompt`, `dia.Toast`, `dia.SendPaver`, `dia.Save`, `dia.Load`, `dia.NewGame`, `dia.SkipMenu`, `dia.Camera`, `dia.WorkView`, `dia.CheckView`, `dia.Shop`, `dia.Hand`, `dia.Fill`, `dia.TopView`, `dia.ShotAndQuit`.

Argumenty a účinok každého sú v [`docs/01-herny-dizajn.md`](docs/01-herny-dizajn.md), kapitola 11. Spustenie s vlastným slotom uloženej hry (aby sa nedotkol skutočného `DoItAsphalt_Slot0`): argument `-DIASlot=<meno>`.

## Štruktúra repozitára

| Cesta | Čo obsahuje |
|---|---|
| `Source/AsphaltCore` | Čisté jadro hry bez enginu (`FLotGrid`, `FLotGame`, ekonomika, uloženie „DIA2“) s vlastnými testami a simuláciou balansu. |
| `Source/DoItAsphalt` | Herný modul: postava, ovládanie, svet, HUD a menu v Slate, uloženie, cheaty, testy. |
| `Tools/ModelGen` | Blender skripty, ktoré generujú 3D modely (manifest v `manifest.json`, `build_all.sh`). |
| `Tools/UE` | Python skripty pre editor (import modelov, materiály, mapa `L_Main`), spúšťajú sa cez `Scripts/uepy.sh`. |
| `SourceAssets` | Zdrojové modely (`Models`) a ich náhľady (`Previews`). |
| `Content`, `Config` | Obsah projektu a nastavenia enginu. |
| `Scripts` | Pomocné príkazy opísané vyššie. |
| `docs` | Dokumentácia (slovensky). |

## Dokumentácia

Začni v [`docs/README.md`](docs/README.md). Dokumenty 1 – 3 popisujú hru v2:

- [Herný dizajn](docs/01-herny-dizajn.md) – pravidlá, ovládanie, HUD, cheaty, známe obmedzenia, zmeny oproti návrhu.
- [Technické poznámky](docs/02-technicky-navrh.md) – moduly, mriežka, uloženie, testy, Shipping.
- [3D modely](docs/03-3d-modely.md) – paleta, 12 nových modelov, pomocné funkcie.

História: [plán zmien](docs/04-plan-zmien-asphalt-self.md) a dva implementačné plány v `docs/superpowers/plans/`.

## Nástroje: `Tools/UE/create_materials.py`

Skript spustený cez `Scripts/uepy.sh Tools/UE/create_materials.py` **nedobehne**: vypíše `DIA_OK /Game/Materials/M_Ground` a pri druhom materiáli (`M_DIA_Flat`) editor spadne na tvrdení `!IsRooted()` v `UMaterialEditingLibrary::DeleteAllMaterialExpressions`. Rovnako padne aj samostatný skript, ktorý len zavolá `delete_material_expression` na načítanom `M_DIA_Flat`, takže príčina nie je v logike skriptu. Zostane po ňom proces `CrashReportClient` (treba ho ukončiť) a prepísaný `Content/Materials/M_Ground.uasset` (treba vrátiť: `git checkout -- Content/Materials/M_Ground.uasset`). Materiály v `Content/Materials/` sú hotové, preto skript netreba spúšťať; zmeny materiálov sa zatiaľ robia v editore.

## Overené

Overené 2026-11-21 (nie viac):

- `Scripts/build.sh` skončil úspechom.
- `Scripts/test.sh`: `PASSED 219 / FAILED 0` (214 čistých testov a 5 s očakávaným varovaním). Skupiny: `DoItAsphalt.Lot.*` 87, `DoItAsphalt.Game.*` 131, `DoItAsphalt.Core.Smoke` 1.
- Shipping cieľ sa (pred posledným kolom opráv kabíny a ukladania, neopakované) skompiloval a zlinkoval príkazom `Build.sh DoItAsphalt Mac Shipping -project=<cesta>/DoItAsphalt.uproject` (`Binaries/Mac/DoItAsphalt-Mac-Shipping`, 281 774 456 bajtov). Dokončenie balíka `.app` zlyhalo na `codesign` (nižšie).
- V Shipping binárke nie sú cheaty: žiadny z 27 reťazcov `dia.*` v nej nie je ani v UTF-16, ani v ASCII. Reťazce `TEXT()` sú na Macu UTF-16, preto obyčajné `strings` ich nenájde ani tam, kde sú; kontrola sa overila na editorovej knižnici `libUnrealEditor-DoItAsphalt.dylib`, kde je v UTF-16 všetkých 27.
- Snímky `start`, `hand`, `shop`, `cart`, `finisher`, `paver`, `car`, `exit`, `site`, `win` a `load` (`Saved/Screenshots/`, v gite nie sú) boli otvorené a prezreté: štartová scéna s výrobňou a korytom, plná miska, obchod, vozík s kufrom, finišer, robot s oranžovým obrysom a riadkom „Paver: sector 23 · 0 / 400“, kabína auta pri pohľade na otvorené parkovisko, otvorená brána východu, nákupné centrum, obrazovka výhry s riadkami Time, Cells paved, Asphalt poured, Credits earned, Items bought a obnovený stav po `dia.Load`.
- Simulácia balansu prešla: východ sa otvorí o 2:21:27 simulovaného času (v jednom behu).

Zabalenie `.app` (`Scripts/package.sh`) sa nespúšťalo. Pri kompilácii Shipping cieľa `codesign` balíka zlyhal s `resource fork, Finder information, or similar detritus not allowed`, lebo projekt leží na `~/Desktop` v iCloud Drive a ten pridáva priečinkom rozšírené atribúty. Projekt sa nemá presúvať; `.app` preto neexistuje a kontrola reťazcov je urobená nad samostatnou Shipping binárkou.

Oprava kabíny auta (2026-11-21): zvnútra zakrývali výhľad nepriehľadné sklá modelu (farba `DIA_Glass`), a to pri základnom sklone hlavy −8° až po horizont. Karoséria je teraz z pohľadu z kabíny skrytá a dole v obraze je plochá žltá kapota. Snímky `car` (bez argumentu sklonu, `dia.SkipMenu; dia.Complete 6100; dia.CompleteEntry 4; dia.Item Car; dia.EnterCar`), `car_cabin_fix_yaw` (hlava otočená o 90°) a `car_exterior_fix` (auto zvonku po vystúpení, so sklami) boli otvorené a prezreté: v kabíne je vidieť oblohu, horizont a parkovisko, zvonku je auto také ako predtým. Nezmenila sa žiadna 3D šablóna.

## Neoverené – čaká na ručnú kontrolu

Nič z tohto nikto ručne nevyskúšal:

- celé prehratie hry od začiatku do výhry;
- pohľad myšou, zachytenie kurzora a `Esc`;
- zatvorenie obchodu klávesom `Tab`, kým je kurzor zobrazený;
- dvojklik na riadok výrobne a na riadok predmetu v obchode;
- pocit z pohľadu v kabíne (sklon −8°, citlivosť myši) a z jazdy;
- pocit z rýchlosti hladidla a finišera;
- zabalenie `.app` (`codesign` zlyhá, lebo projekt leží v iCloude; projekt sa nemá presúvať).

## Známe obmedzenia

Úplný zoznam je v [`docs/01-herny-dizajn.md`](docs/01-herny-dizajn.md), kapitola 12.

- V prvej osobe nie sú vidieť ruky ani paže a nie je animácia pohybu do strán.
- Hra nemá zvuk.
- Lištu finišera pred očami zakrýva jeho kryt.
- Lišta auta sa pri plnom vychýlení volantu môže pohybovať o niečo rýchlejšie, než dovoľuje strop liatia.
- Pri liatí z auta hráča nič neupozorní, že zásoba výrobne sa minula.
- Obrys sektora robota (10 cm) je zďaleka zhora tenký.
- Kým sú zobrazené tri iné oznamy, oznamy o kreditoch čakajú.
- Valček, dusadlo a lata s vodováhou zo špecifikácie v hre nie sú (otvorená otázka O1).
- Míľnik 800 políčok v simulácii má rezervu pod minútu (41:17 oproti limitu 42:00).
