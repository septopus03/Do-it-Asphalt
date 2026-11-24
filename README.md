# Do it Asphalt

Pokojná inkrementálna simulácia asfaltovania v prvej osobe. Hráč vyleje asfalt na parkovisko v tvare písmena L (30 000 štvorcov 1 × 1 m) pred nákupným centrom. Začína lyžicou a miskou, neskôr používa vedro, kanister, vaňu, vozíček, vozík, až troch robotov a auto; roboti (pracujú sami) a auto si chodia po asfalt do výdajne. Nakupuje sa v troch budovách na južnom pruhu (Tools, Robots, Garage) klávesom `E` pri ich bode nákupu. Hra má zvuky (stroje, liatie, okolie) a bežné nastavenia. Za každý hotový štvorec dostane 10 kreditov a za každý vyliaty liter 1 kredit, a za kredity kupuje lepšie vybavenie a rýchlejšiu výrobňu. Keď je vyasfaltované všetko, otvorí sa východ a prechodom bránou hra končí výhrou. Roboti sa kupujú v budove Roboti a každý sa vylepšuje samostatne (rýchlosť a nádrž, po dve úrovne). Hra má podľa simulácie s tromi robotmi a ich vylepšeniami trvať asi 1 h 48 min (okno testu je 105 – 165 min, simulovaný hráč je len asi 3 minúty nad dolným okrajom); celý obchod stojí viac, než hra vyplatí, takže sa treba rozhodovať.

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
| Ďalšia ruka / miska, vedro, kanister, vaňa (len vlastnené; vybrať sa dajú aj v obchode s náradím) | `Q` / `1` `2` `3` `4` |
| Chytiť, pustiť, nastúpiť a vystúpiť z auta; pri budove otvoriť a zavrieť obchod | `E` |
| Naberať (koryto → miska lyžicou, naplniť vedro, kanister a vaňu pod prúdom) | `R` (držať) |
| Mapa sektorov robotov (prepínač robota 1 / 2 / 3, klik na sektor ho tam pošle, tlačidlo AUTO prepína samostatnú prácu); predtým `G` | `M` |
| Poslať vybraného robota do sektora, v ktorom stojíš | `H` |
| Šprint (po odomknutí) | ľavý `Shift` |
| Kontrolný pohľad | `V` (držať) |
| Pauza / zavrieť obchod alebo mapu / zavrieť nastavenia | `Esc` |

V aute (pohľad z tretej osoby): `W`/`S` plyn a spiatočka, `A`/`D` zatáčanie, myš otáča kamerou okolo auta, ľavé tlačidlo leje z nádrže, `E` vystúpiť. Nádrž sa plní vo výdajni (bránka pod parkoviskom, alebo kúpená presuvná Station). Dole vľavo je stále zapnutá legenda „CONTROLS" s týmito klávesmi (v aute kratšia; v menu, obchode, mape a na obrazovke výhry sa skryje). Gamepad nie je podporovaný. Úplný opis je v [`docs/01-herny-dizajn.md`](docs/01-herny-dizajn.md).

## Cheaty pre vývoj

Konzolové príkazy `dia.*` sú v `Source/DoItAsphalt/Game/DIACheats.cpp` pod `#if !UE_BUILD_SHIPPING`, takže v Shipping builde neexistujú. Skript `shot.sh` na nich stojí, preto sa nedá použiť so Shipping buildom. Je ich 28 (`dia.Item` pozná aj `Canister` a `Tub`, `dia.Hand` ruky `Bowl`, `Bucket`, `Canister`, `Tub`; `dia.PaverUpgrade <robot 1..3> <Speed|Capacity> <0..2>` nastaví vylepšenie robota):

`dia.Give`, `dia.Complete`, `dia.CompleteEntry`, `dia.Item`, `dia.Grab`, `dia.Drop`, `dia.CartStrip`, `dia.Place`, `dia.Plant`, `dia.EnterCar`, `dia.Teleport`, `dia.Prompt`, `dia.Toast`, `dia.SendPaver`, `dia.PaverUpgrade`, `dia.Save`, `dia.Load`, `dia.NewGame`, `dia.SkipMenu`, `dia.Camera`, `dia.WorkView`, `dia.CheckView`, `dia.Shop`, `dia.Hand`, `dia.Fill`, `dia.TopView`, `dia.Screen`, `dia.ShotAndQuit`.

Argumenty a účinok každého sú v [`docs/01-herny-dizajn.md`](docs/01-herny-dizajn.md), kapitola 11. Spustenie s vlastným slotom uloženej hry (aby sa nedotkol skutočného `DoItAsphalt_Slot0`): argument `-DIASlot=<meno>`.

## Štruktúra repozitára

| Cesta | Čo obsahuje |
|---|---|
| `Source/AsphaltCore` | Čisté jadro hry bez enginu (`FLotGrid`, `FLotGame`, ekonomika, uloženie „DIA2“) s vlastnými testami a simuláciou balansu. |
| `Source/DoItAsphalt` | Herný modul: postava, ovládanie, svet, HUD a menu v Slate, uloženie, cheaty, testy. |
| `Tools/ModelGen` | Blender skripty, ktoré generujú 3D modely (manifest v `manifest.json`, `build_all.sh`). |
| `Tools/UE` | Python skripty pre editor (import modelov, zvukov a ikon nástrojov, materiály, mapa `L_Main`), spúšťajú sa cez `Scripts/uepy.sh`. |
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

Overené 2026-11-23 a 2026-11-24 (nie viac; riadok o testoch je z 2026-11-23, od vtedy pribudli testy druhej vlny):

- `Scripts/build.sh` skončil úspechom.
- `Scripts/test.sh` (2026-11-24): 317 testov, všetky prešli. Skupiny: `DoItAsphalt.Lot.*` 124, `DoItAsphalt.Game.*` 192, `DoItAsphalt.Core.Smoke` 1.
- Simulácia balansu prešla (2026-11-24, tri roboty a ich vylepšenia): východ sa otvorí o 1:48:24 simulovaného času (limit 105 – 165 min, teda asi 3 minúty nad dolným okrajom); míľniky: 1 políčko 0:42, 10 políčok 1:55, 100 políčok 6:34, 350 políčok 13:33, 800 políčok 23:04, 6 000 políčok 52:34. Roboti sa kupujú o 23:05, 52:59 a 1:35:22. Simulovaný hráč kúpi všetko okrem najvyššieho stupňa auta (CarT4), takže celý obchod stojí viac, než hra vyplatí. Predchádzajúce údaje (2 h 21 min s jedným robotom, 1:26:02 s pôvodnými cenami troch robotov) už neplatia.
- Nákup robotov 2 a 3 a ich vylepšení cez skutočnú cestu kliku (tlačidlo BUY v paneli) prešiel v teste a objavili sa aktéri robotov; panel Roboti (štyri stĺpce, rezervované miesto vylepšení), legenda ovládania (pešo aj v aute) a mapa s „M / Esc close" sa pozreli na snímkach 1280 × 720.
- Shipping cieľ a snímky z 2026-11-21 sú staršie než tieto zmeny a nezopakovali sa; v Shipping binárke boli tehdy overené chýbajúce cheaty (kontrola reťazcov `dia.*`).

Zabalenie `.app` (`Scripts/package.sh`) sa nespúšťalo. Pri kompilácii Shipping cieľa `codesign` balíka zlyhal s `resource fork, Finder information, or similar detritus not allowed`, lebo projekt leží na `~/Desktop` v iCloud Drive a ten pridáva priečinkom rozšírené atribúty. Projekt sa nemá presúvať; `.app` preto neexistuje a kontrola reťazcov je urobená nad samostatnou Shipping binárkou.

## Neoverené – čaká na ručnú kontrolu

Nič z tohto nikto ručne nevyskúšal:

- celé prehratie hry od začiatku do výhry;
- pohľad myšou, zachytenie kurzora a `Esc`;
- zatvorenie obchodu klávesmi `E` a `Esc` pri bode nákupu, kým je kurzor zobrazený;
- cesta k bodu nákupu a nákup klávesom `E` a myšou v každej z troch budov; zatvorenie panela, keď hráč odíde;
- jazda autom do garáže a z nej cez bránku výdajne;
- traja roboti naraz: pridelenie sektora ručne na mape a klávesom `H`, odobratie sektora inému robotovi;
- načítanie skutočného uloženia vo verzii 4, 5 alebo 6 (najmä to, že staré globálne zrýchlenia sa zmenia na rýchlosť každého robota);
- dvojklik na riadok výrobne a na riadok predmetu v obchode;
- pocit z jazdy z pohľadu tretej osoby (vzdialenosť a výška kamery, citlivosť myši);
- **zvuky**: nikto ich nepočul; hlasitosť, zmiešanie a pocit sú len z kódu (zvuky sú syntetizované skriptom, nie nahrávky);
- **animácie** naberania a liatia (naklonenie nádoby, pohyb lyžice do koryta): pózy ruky a lyžice nie sú odladené podľa snímok;
- vzhľad výdajne (`SM_VehicleStation`), mapy robotov (otvára sa klávesom `M`) a lišty so štyrmi nádobami (na snímkach nevideté);
- **nákup robotov a ich vylepšení myšou v novom paneli Roboti** (skutočná myš, najazdenie a zachytenie; v teste je len simulovaný klik na tlačidlo BUY);
- **vzhľad legendy ovládania** (dole vľavo) počas skutočného hrania, nie len na snímkach;
- **kláves `M` pre mapu v aute** (kód ho v aute pustí a legenda ho uvádza; ručne sa neskúšalo);
- obrazovka nastavení (posuvník, prepínače, kvalita) a prepínanie celej obrazovky / V-Sync na skutočnom okne;
- vyváženie po zmene: ceny nových nádob, veľkosti nádrží (robot 2 000 / 3 000 / 4 500 l, auto 6 000 až 18 000 l), ceny robotov (3 000 / 40 000 / 80 000) a vylepšení (rýchlosť 20 000 a 60 000, kapacita 5 000 a 15 000) sú nastavené len podľa simulácie, nie podľa ručnej hry; skutočné tempo hráča (prvá polovica ručne, tri roboty) nikto nemeral;
- zabalenie `.app` (`codesign` zlyhá, lebo projekt leží v iCloude; projekt sa nemá presúvať).

## Známe obmedzenia

Úplný zoznam je v [`docs/01-herny-dizajn.md`](docs/01-herny-dizajn.md), kapitola 12.

- V prvej osobe nie je animácia pohybu do strán.
- Zvuky sú syntetizované (bez nahrávok) a nikto ich nepočul.
- Starý uložený stav (verzia 3 a staršie) sa nenačíta: uloženie má verziu 7 (načítajú sa aj verzie 4, 5 a 6) a nová hra začne od začiatku.
- Lišta auta sa pri plnom vychýlení volantu môže pohybovať o niečo rýchlejšie, než dovoľuje strop liatia.
- Pri liatí z auta hráča upozorní len riadok „Tank low“; keď sa minie nádrž, auto prestane liať.
- Obrys sektora robota (10 cm) je zďaleka zhora tenký.
- Roboti sa navzájom ani s budovami nezrážajú; robot môže pri výjazde zo státia prejsť cez stenu stanice.
- Kým sú zobrazené tri iné oznamy, oznamy o kreditoch čakajú.
- Valček, dusadlo a lata s vodováhou zo špecifikácie v hre nie sú (otvorená otázka O1).
- Tekutý asfalt je vidieť len ako postupujúce pokrytie zeme (bez vlastnej hrúbky či lesku); simulácia vyváženia počíta s okamžitým liatím, takže tekutinu nemeria.
