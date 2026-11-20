# Do it Asphalt

Pokojná inkrementálna simulácia asfaltovania v tretej osobe. Hráč nosí asfalt z výrobne v batohu na chrbte a vylieva ho na parkovisko v tvare veľkého šesťuholníka (499 malých šesťuholníkových buniek). Za vyliaty asfalt dostáva peniaze, za dokončené bunky shardy, a za ne si kupuje väčší batoh, širší nástroj, rýchlejšiu výrobňu a špeciálne vylepšenia na príjazdových cestách. Keď je vyasfaltované všetko, otvorí sa východ a prechodom bránou hra končí výhrou.

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
| `Scripts/uepy.sh <script.py>` | Spustí Python skript v editore bez okna a zlyhá pri `DIA_FAIL`, tracebacku alebo chýbajúcom `DIA_OK`. |
| `Scripts/setup_content.sh` | Skopíruje postavu Mannequin zo šablóny tretej osoby v engine do `Content/Characters/Mannequins` (ak tam už je, preskočí). |
| `Scripts/package.sh` | Zbalí Shipping build pre Mac (`RunUAT BuildCookRun`) do `Builds/`; na úspech vypíše `BUILD SUCCESSFUL` a cestu k `.app`. |
| `Scripts/run.sh [args]` | Otvorí projekt v Unreal Editore. |

Typický postup: `Scripts/build.sh && Scripts/test.sh`, potom `Scripts/run.sh`.

## Ovládanie

| Akcia | Klávesnica a myš |
|---|---|
| Pohyb | `W` `A` `S` `D` |
| Kamera | myš |
| Liať asfalt | ľavé tlačidlo myši (držať) |
| Šprint (po kúpe) | `Shift` |
| Interakcia (kiosk, prenosné miesto, auto) | `E` |
| Obchod | `Tab` |
| Pauza / menu | `Esc` |

V aute: `W`/`S` plyn a spiatočka, `A`/`D` zatáčanie, ľavé tlačidlo leje, `E` vystúpiť. Gamepad nie je podporovaný.

## Cheaty pre vývoj

V editore a vo vývojových buildoch sú konzolové príkazy `dia.*` (napríklad `dia.Give`, `dia.Complete`, `dia.CompleteRoad`, `dia.SetLevels`, `dia.Special`, `dia.Teleport`, `dia.Shop`, `dia.SkipMenu`, `dia.TopView`, `dia.ShotAndQuit`). Sú v `Source/DoItAsphalt/Game/DIACheats.cpp` pod `#if !UE_BUILD_SHIPPING`, takže v Shipping builde neexistujú. Skript `shot.sh` na nich stojí, preto sa nedá použiť so Shipping buildom.

Spustenie s vlastným slotom uloženej hry (aby sa nedotkol skutočného `DoItAsphalt_Slot0`): argument `-DIASlot=<meno>`.

## Štruktúra repozitára

| Cesta | Čo obsahuje |
|---|---|
| `Source/AsphaltCore` | Čisté jadro hry bez závislosti na hernej logike enginu (mriežka buniek, ekonomika, vylepšenia, uloženie) s vlastnými testami a simuláciou balansu. |
| `Source/DoItAsphalt` | Herný modul: postava, kamera, ovládanie, svet, HUD a menu v Slate, uloženie, cheaty, testy. |
| `Tools/ModelGen` | Blender skripty, ktoré generujú 3D modely (manifest v `manifest.json`, `build_all.sh`). |
| `Tools/UE` | Python skripty pre editor (import modelov, materiály, mapa `L_Main`), spúšťajú sa cez `Scripts/uepy.sh`. |
| `SourceAssets` | Zdrojové modely (`Models`) a ich náhľady (`Previews`). |
| `Content`, `Config` | Obsah projektu a nastavenia enginu. |
| `Scripts` | Pomocné príkazy opísané vyššie. |
| `docs` | Dokumentácia (slovensky). |

## Dokumentácia

Začni v [`docs/README.md`](docs/README.md). Odtiaľ vedú:

- [Herný dizajn](docs/01-herny-dizajn.md) – pravidlá, mapa, ceny, ovládanie, HUD.
- [Technický návrh](docs/02-technicky-navrh.md) – architektúra, testovanie, riziká.
- [3D modely](docs/03-3d-modely.md) – zoznam modelov a postup tvorby.
- [Implementačný plán](docs/superpowers/plans/2026-11-19-do-it-asphalt.md).

## Overené

Automaticky overené (2026-11-20, `Scripts/build.sh && Scripts/test.sh`):

- Automatické testy: `PASSED 125 / FAILED 0` (jadro `AsphaltCore` aj herný modul `DoItAsphalt`).
- Simulácia balansu (test `DoItAsphalt.Core.BalanceSim.Playthrough`, súčasť týchto 125 testov) prešla. Test vyžaduje: hra sa dá dohrať, východ sa otvorí, celkový čas je v rozsahu 45 – 100 minút, kúpi sa presne 35 vylepšení, všetky pred dokončením 75 % buniek, a v prvých 30 minútach nie je medzi nákupmi pauza dlhšia ako 4 minúty. Posledný beh nameral 50,4 min, 35 nákupov, posledný nákup pri 292 / 499 bunkách (približne 58 %), najdlhšia pauza v prvých 30 minútach 183 s. Cieľ z `docs/01-herny-dizajn.md` (približne 50 minút, všetky vylepšenia pred 60 % buniek) je návrhový cieľ; test ho nevynucuje presne, len v uvedených hraniciach.
- Shipping cieľ `DoItAsphalt Mac Shipping` sa skompiluje a zlinkuje (`DoItAsphalt-Mac-Shipping`, približne 281 MB).
- V Shipping binárke nie sú cheaty: v nej sa nenachádza žiadny reťazec `dia.Give`, `dia.Complete`, `dia.CompleteRoad`, `dia.SetLevels`, `dia.Special`, `dia.Teleport`, `dia.Prompt`, `dia.Toast`, `dia.Shop`, `dia.SkipMenu`, `dia.Camera`, `dia.TopView` ani `dia.ShotAndQuit`. Pozor: reťazce `TEXT()` sú v UE na Macu UTF-16, takže obyčajné `strings` ich nenájde ani tam, kde sú; hľadalo sa preto v UTF-16 aj v ASCII a kontrola sa overila na editorovej knižnici `libUnrealEditor-DoItAsphalt.dylib`, kde ich nájde (napríklad `dia.Give` 2x).

Zabalenie `.app` (`Scripts/package.sh`) tu zatiaľ **neprešlo**: Shipping binárka sa skompiluje, ale `codesign` balíka zlyhá s `resource fork, Finder information, or similar detritus not allowed`, lebo projekt leží na `~/Desktop` v iCloud Drive a ten pridáva priečinkom rozšírené atribúty. Treba projekt presunúť mimo iCloud Drive (napríklad do `~/Developer`) a skript spustiť znova; dovtedy `.app` neexistuje, preto nebolo čo skontrolovať, ani spustiť naprázdno. Preto je aj kontrola reťazcov urobená nad samostatnou Shipping binárkou, nie nad binárkou vnútri `.app`.

## Neoverené – čaká na ručnú kontrolu

Tieto body treba prejsť ručne v zabalenej hre (`.app` z `Scripts/package.sh`, hlavné menu, `New Game`); zatiaľ ich nikto nevyskúšal. Rovnako chýba overenie, že zabalená hra naštartuje bez pádu.

- Pohyb (`W` `A` `S` `D`) a kamera (myš) fungujú.
- Batoh sa pri výrobni naplní.
- Liatie zanecháva asfalt a pripisuje peniaze.
- Dokončená bunka dá shard.
- `Tab` otvorí obchod a nákup sa prejaví.
- Po zavretí a opätovnom otvorení hry ponúkne `Continue` rovnaký stav.
- Myš a kurzor: v hlavnom menu, v obchode a v pauze je kurzor viditeľný a tlačidlá sa dajú klikať; počas hry je kurzor skrytý a myš otáča kamerou.
- `Esc` otvorí pauzu a hra sa pozastaví; v pauze fungujú `Resume`, `New Game` a `Quit`, v hlavnom menu `Continue`, `New Game` a `Quit`.
- Zem a disk doplňovania pri výrobni sú viditeľné (základné tvary enginu sa pri balení vždy pribalia, `DirectoriesToAlwaysCook`).

## Neimplementované z GDD

Tieto dve veci z GDD nie sú v hre; zámerne sa nedoplnili a GDD zostáva nezmenené.

- Šípka HUD k výrobni počas prvých 20 s (GDD, kapitola 5).
- Krátke zablikanie dokončenej bunky (GDD, kapitola 4).
