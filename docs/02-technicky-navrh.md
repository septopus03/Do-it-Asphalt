# Do it Asphalt – technický návrh

Verzia 1.0 · 2026-11-19 · implementuje `01-herny-dizajn.md`

---

## 1. Hlavná myšlienka: všetko z textu

Hru budú písať agenti (Sonnet 5.5), ktorí nevedia klikať v editore ani upravovať binárne `.uasset` súbory. Preto platí jedno pravidlo, z ktorého vychádza celá architektúra:

> **Každá vec v projekte vzniká z textového súboru v repozitári.** C++ pre logiku a scénu, Slate pre UI, Python skripty pre všetko, čo Unreal vyžaduje ako asset (level, materiály, import modelov), Blender skripty pre 3D modely.

Žiadne Blueprinty, žiadne ručne poskladané levely, žiadne UMG widgety. Jediný ručne nevytvorený binárny obsah je Mannequin skopírovaný zo šablóny enginu.

## 2. Prostredie

| | |
|---|---|
| Engine | Unreal Engine 5.8.4, `/Users/Shared/Epic Games/UE_5.8` (ďalej `$UE`) |
| Stroj | macOS 26.5, Apple Silicon, 16 GB RAM, Xcode 26.6 |
| Jazyk | C++ (štandard podľa enginu), Python 3.11 pre nástroje |
| Verziovanie | git + git-lfs (`*.uasset`, `*.umap`, `*.glb`, `*.png`) |
| 3D nástroj | Blender 5.3.0 v režime bez okna (`blender -b -P skript.py`), nainštalovaný cez `brew install --cask blender` |

Dôsledky pre 16 GB RAM: Lumen aj Nanite vypnuté, tiene bežnými kaskádami, žiadne ťažké post-procesy. Štylizovaný vizuál to nepotrebuje.

Cesta k projektu obsahuje medzery (`Do it Asphalt`). Všetky skripty preto cesty dôsledne úvodzovkujú; názov projektu a modulov medzery nemá (`DoItAsphalt`).

API enginu 5.8 si agenti overujú priamo v zdrojákoch na disku (`$UE/Engine/Source`, `$UE/Engine/Plugins`), nie z pamäti – verzia je novšia než znalosti modelov.

## 3. Štruktúra repozitára

```
Do it Asphalt/
├── DoItAsphalt.uproject
├── Config/                      DefaultEngine.ini, DefaultGame.ini, DefaultInput.ini
├── Source/
│   ├── DoItAsphalt.Target.cs
│   ├── DoItAsphaltEditor.Target.cs
│   ├── AsphaltCore/             herné pravidlá, čisté C++ bez enginu (len modul Core)
│   │   ├── Public/              HexCoord.h, HexMap.h, AsphaltBalance.h, AsphaltLayout.h,
│   │   │                        CoverageGrid.h, AsphaltGame.h
│   │   └── Private/             *.cpp + Tests/*.cpp
│   └── DoItAsphalt/             všetko, čo sa dotýka enginu
│       ├── Game/                DIAGameMode, DIAWorldSubsystem, DIACheats, DIAAssets, DIASaveGame
│       ├── World/               DIAGroundActor, DIAPlantActor, DIAKioskActor, DIAExitGate,
│       │                        DIAPortableStation, DIAAutoPaver, DIAPaverCar, DIAInteractable
│       ├── Player/              DIACharacter, DIAPlayerController, DIAInput
│       └── UI/                  DIAHUD, SDIAHudOverlay, SDIAShopPanel, SDIAMenu, SDIAWinScreen
├── Content/
│   ├── Characters/Mannequins/   skopírované zo šablóny enginu
│   ├── Maps/L_Main.umap         generuje Tools/UE/create_level.py
│   ├── Materials/               generuje Tools/UE/create_materials.py
│   └── Models/                  generuje Tools/UE/import_models.py
├── SourceAssets/
│   ├── Models/*.glb             výstup Blender skriptov
│   └── Previews/*.png           náhľady modelov na kontrolu
├── Tools/
│   ├── ModelGen/                Blender skripty (Fable 5.2)
│   └── UE/                      editorové Python skripty (Sonnet 5.5)
├── Scripts/                     build.sh, test.sh, uepy.sh, shot.sh, run.sh, package.sh
└── docs/
```

## 4. Dva moduly

### 4.1 `AsphaltCore` – pravidlá hry

Závisí len od modulu `Core`. Žiadne `UObject`, žiadni aktéri, žiadny svet. Všetko je deterministické a volá sa s explicitným `Dt`, takže sa dá celé otestovať bez spustenia hry. **Tu žije každé číslo a každé pravidlo z GDD.**

| Súbor | Zodpovednosť |
|---|---|
| `HexCoord.h` | Axiálne súradnice `FHexCoord{Q,R}`, 6 smerov, vzdialenosť, prevod bunka ↔ svet (`AsphaltHex::ToWorld/FromWorld`), vzdialenosť od hrany bunky. |
| `HexMap.h` | `FHexMap`: zoznam 499 buniek (469 parkovisko v špirále od stredu, potom 6 × 5 buniek ciest), vyhľadanie bunky, bunky cesty. |
| `AsphaltBalance.h` | Všetky tabuľky a konštanty z GDD kap. 4 – 8. Jediné miesto na ladenie. |
| `AsphaltLayout.h` | Polohy vo svete: výrobňa, odberná zóna, štart hráča, kiosky, brána, hranica areálu. |
| `CoverageGrid.h` | `FCoverageGrid`: mriežka pokrytia, razenie obdĺžnika, spotreba litrov, dokončovanie buniek. |
| `AsphaltGame.h` | `FAsphaltGame`: fasáda nad všetkým – peniaze, shardy, úrovne, špeciály, zásobník, batoh, auto, robot, tik, nákupy, uloženie do bajtov. |

### 4.2 `DoItAsphalt` – engine vrstva

Tenká. Nerozhoduje o pravidlách, len prekladá vstup hráča na volania `FAsphaltGame` a stav hry na obraz.

| Trieda | Zodpovednosť |
|---|---|
| `UDIAWorldSubsystem` | Vlastní jedinú inštanciu `FAsphaltGame`, tiká ju, rozosiela delegáty (`OnStateChanged`, `OnCellsCompleted`, `OnRoadCompleted`, `OnExitOpened`), ukladá a načítava. |
| `ADIAGameMode` | Pri štarte postaví scénu: zem, výrobňu, kiosky, bránu, plot, kulisy. |
| `ADIAGroundActor` | Jedna plocha s materiálom, ktorému za behu dodáva dve textúry: pokrytie a obrysy buniek. |
| `ADIACharacter` | Pohyb, kamera, asfaltovačka a batoh ako pripojené meshe, liatie, šprint, automatický odber. |
| `ADIAPlayerController` | Vstup (Enhanced Input vytvorený v C++), prepínanie hra / UI, interakcia klávesom E. |
| `IDIAInteractable` | Rozhranie pre kiosk, prenosné miesto a auto: `GetPrompt()`, `Interact()`. |
| `ADIAPlantActor`, `ADIAKioskActor`, `ADIAExitGate`, `ADIAPortableStation`, `ADIAAutoPaver`, `ADIAPaverCar` | Vizuál a kolízie; stav čítajú zo subsystému. |
| `ADIAHUD` + Slate widgety | HUD, obchod, menu, výhra. |
| `DIAAssets` | Tabuľka ciest k assetom s náhradou za základné tvary enginu, ak model ešte neexistuje. |
| `DIACheats` | Konzolové príkazy na ladenie a automatické snímky obrazovky. |

Tok jedného liatia: `ADIACharacter::Tick` vypočíta úsečku, ktorú lišta prešla → `UDIAWorldSubsystem::Pour` → `FAsphaltGame::PourFromBackpack` → `FCoverageGrid::Stamp` → vráti litre a dokončené bunky → subsystém pripíše odmeny, pošle delegáty → `ADIAGroundActor` nahrá zmenený výrez textúry, HUD sa prekreslí.

## 5. Kľúčové technické riešenia

### 5.1 Mriežka pokrytia (jadro celej hry)

Otázka „koľko asfaltu je v bunke a ako to nakresliť" má jedno spoločné riešenie:

- Nad celou mapou leží mriežka **640 × 640 texelov po 5 cm** (štvorec ±16 m okolo stredu).
- Každý texel vie, ktorej bunke patrí (podľa svojho stredu), alebo že nepatrí žiadnej.
- Bunka má okolo 400 texelov; jeden texel stojí `20 l ÷ počet texelov bunky`, takže každá bunka je presne 20 l.
- Liatie = „razenie" orientovaného obdĺžnika (úsečka pohybu lišty × šírka nástroja, predĺžená o 5 cm na oboch koncoch). Nové texely v ňom sa označia a odpočítajú z batohu, v pevnom poradí, kým je v batohu dosť.
- Pri 97 % pokrytia sa zvyšok bunky označí zadarmo a bunka je hotová.
- **Tá istá mriežka sa nahráva ako textúra** do materiálu zeme. Logika a obraz sa preto nemôžu rozísť.

Pamäť: 0,4 MB pokrytie + 0,8 MB mapa texel → bunka. Zanedbateľné.

### 5.2 Zem

Jedna plocha 32 × 32 m s materiálom `M_Ground`:

- `CoverageTex` (640², jednokanálová, bilineárna) – materiál ju prahuje cez `smoothstep`, takže okraj asfaltu je hladký, nie kockatý.
- `HexLinesTex` (2048², jednokanálová) – obrysy buniek, vypočítané raz pri štarte z `AsphaltHex::EdgeDistanceCm`.
- Výsledok: hlina + biele obrysy, cez to tmavý lesklý asfalt podľa pokrytia.

Hrúbka asfaltu je len vizuálna (farba, lesk), geometria zeme sa nemení.

### 5.3 Postava a nástroje

- Kostra, mesh a animácie: `SKM_Manny_Simple` + `ABP_Unarmed` zo šablóny (`$UE/Templates/TemplateResources/High/Characters/Content/Mannequins` → `Content/Characters/Mannequins`).
- Batoh je pripojený na kosť `spine_03`, prilba na `head`.
- Asfaltovačka je pripojená **na koreň aktéra, nie na ruku** – jazdí po zemi 80 cm pred hráčom a nehojdá sa s animáciou. Logická poloha lišty a jej mesh sú tak vždy totožné.
- Známe obmedzenie v1: ruky pri chôdzi voľne kývajú, vlastná animácia tlačenia by vyžadovala Animation Blueprint vytvorený ručne v editore.

### 5.4 Vstup a UI bez assetov

- `UInputAction` a `UInputMappingContext` sa vytvárajú v C++ cez `NewObject` pri štarte controllera. Žiadne `.uasset` pre vstup.
- UI je čistý Slate (`SCompoundWidget`), pridaný do viewportu z `ADIAHUD`. Texty cez `LOCTEXT`.
- Keď hra beží bez vykresľovania (testy s `-nullrhi`), HUD sa nevytvára.

### 5.5 Assety generované skriptami

`Scripts/uepy.sh <skript.py>` spustí editor bez okna: `UnrealEditor-Cmd DoItAsphalt.uproject -run=pythonscript -Script="<skript.py>"`.

| Skript | Výstup |
|---|---|
| `Tools/UE/create_level.py` | `Content/Maps/L_Main.umap`: prázdny level, smerové svetlo, obloha, atmosféra, hmla, `PlayerStart`. |
| `Tools/UE/create_materials.py` | `M_Ground` a paleta `MI_DIA_*` pre modely. |
| `Tools/UE/import_models.py` | Naimportuje každý `SourceAssets/Models/*.glb` cez Interchange do `Content/Models/`, priradí materiály palety podľa názvu slotu, nastaví kolízie. |

Skripty sú idempotentné – opätovné spustenie prepíše výsledok, nič nezdvojí.

### 5.6 Modely nikdy neblokujú kód

`DIAAssets::LoadMeshOrFallback(Cesta, NáhradnýTvar)` vráti model, ak už je naimportovaný, inak kocku alebo valec z `/Engine/BasicShapes` v rozmeroch z `03-3d-modely.md`. Hra je tak hrateľná od prvej fázy s „greybox" tvarmi a modely sa do nej doplnia bez zmeny kódu. Preto môže vetva modelov (Fable 5.2) bežať súbežne s vetvou kódu (Sonnet 5.5).

### 5.7 Ukladanie

- `FAsphaltGame::SaveToBytes()` / `LoadFromBytes()` – vlastný binárny formát s číslom verzie na začiatku a kontrolou dĺžok. Pokrytie sa ukladá ako bitové pole (51 kB).
- `LoadFromBytes` načíta do dočasnej inštancie a až po úspechu ju vymení – pri chybe zostane pôvodný stav nedotknutý a vráti `false`.
- Engine vrstva blob zabalí do `UDIASaveGame` a uloží do slotu `DoItAsphalt_Slot0`.
- Každé uloženie zapíše slot aj jeho zálohu `<slot>_bak` (`DoItAsphalt_Slot0_bak`), takže prerušený zápis nikdy nezničí rozohranú hru.
- Súbor slotu má za dátami vlastnú pätu: magic `DIAE` a CRC32 všetkého pred ňou. Engine čítačka sa na dĺžky spolieha, preto sa päta a CRC overia pred načítaním; poškodený primárny slot sa nahradí zálohou a ak je poškodená aj tá, ponúkne sa len nová hra.

### 5.8 Vozidlo a robot

- `ADIAPaverCar` je kinematický `APawn` (posun so sweepom, bez fyziky kolies). Chaos Vehicles je pre túto hru zbytočne zložité.
- Robot: pravidlá (výber cieľa, presun, liatie) sú v `FAsphaltGame::Tick`; `ADIAAutoPaver` len sleduje polohu z jadra.

## 6. Testovanie a overovanie

Tri úrovne, od najlacnejšej:

1. **Automatizované testy jadra** – Unreal Automation (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`), názvy `DoItAsphalt.Core.*`. Každé pravidlo z GDD má test s konkrétnym číslom. Píšu sa pred implementáciou (TDD).
2. **Dymové testy engine vrstvy** – `DoItAsphalt.Game.*`: načítať `L_Main`, overiť, že game mode postavil scénu a subsystém žije. Bežia s `-nullrhi`.
3. **Snímka obrazovky** – `Scripts/shot.sh <meno> "<cheaty>"` spustí hru v okne, vykoná cheaty (napr. dokonči 200 buniek, daj peniaze), po pár sekundách uloží PNG a skončí. Agent si obrázok pozrie a porovná s očakávaním v pláne. Takto sa overuje všetko vizuálne bez človeka.

| Príkaz | Čo robí |
|---|---|
| `Scripts/build.sh` | Zostaví `DoItAsphaltEditor` (Mac, Development). |
| `Scripts/test.sh [filter]` | Spustí testy (predvolene `DoItAsphalt`), z reportu vypíše `PASSED n / FAILED m`, pri zlyhaní alebo nule testov skončí kódom 1. |
| `Scripts/uepy.sh <skript>` | Spustí editorový Python skript bez okna. |
| `Scripts/shot.sh <meno> [cheaty]` | Uloží `Saved/Screenshots/<meno>.png`. |
| `Scripts/run.sh` | Spustí hru v okne. |
| `Scripts/package.sh` | Zabalí Shipping build pre Mac do `Builds/`. |

Cheaty (konzola, len mimo Shipping): `dia.Give <peniaze> <shardy>`, `dia.Complete <počet buniek>`, `dia.CompleteRoad <i>`, `dia.SetLevels <batoh> <nástroj> <výrobňa>`, `dia.Special <meno>`, `dia.Teleport <x> <y> [yaw]`, `dia.TopView`, `dia.ShotAndQuit <sekundy> <meno>`.

## 7. Výkon

- Liatie v jednom snímku prejde nanajvýš pár tisíc texelov (lišta 4 m × pohyb do 20 cm) – zanedbateľné.
- Textúra pokrytia sa nahráva len v zmenenom výreze.
- Scéna má desiatky aktérov a žiadne dynamické svetlá okrem slnka. Cieľ 60 fps na vývojovom Macu.
- `UDIAWorldSubsystem` orezáva `Dt` snímky na 1 s, aby zaseknutie alebo pauza nespôsobili skok. Jadro samo prijme ľubovoľné `Dt` a dlhší krok si delí na kroky po 0,25 s (kvôli robotovi), takže testy môžu volať `Tick(600)`.

## 8. Riziká

| Riziko | Zmiernenie |
|---|---|
| Editorové Python API (materiály, Interchange) sa v 5.8 líši od toho, čo agent pozná | Agent si API overí v zdrojákoch a v `unreal` module na disku; každý skript sa overuje výpisom vytvorených assetov. |
| Medzera v ceste projektu rozbije niektorý nástroj | Všetko úvodzovkované; ak by nástroj aj tak zlyhal, projekt sa spustí cez symlink bez medzery. |
| Okraj asfaltu pri 5 cm texeloch pôsobí rozmazane | Prahovanie v materiáli; ak nestačí, mriežka sa dá zjemniť zmenou jednej konštanty (`TexelCm`). |
| Balans (hra príliš krátka alebo dlhá) | Všetky čísla v `AsphaltBalance.h`; samostatná ladiaca úloha na konci plánu. |
| Ruky Mannequina nedržia nástroj | Vedome prijaté pre v1 (GDD kap. 14). |
| Blender nie je nainštalovaný | Jeden príkaz `brew install --cask blender`; vetva kódu na ňom nezávisí. |
| Windows build | Z Macu sa nedá; treba Windows stroj alebo CI. Mimo rozsahu v1. |

## 9. Kto čo robí

| Práca | Model |
|---|---|
| 3D modely: Blender skripty v `Tools/ModelGen/`, kontrola náhľadov | **Fable 5.2** |
| Všetok kód: C++, editorové Python skripty, shell skripty, testy | **Sonnet 5.5** |
| Kontrola kódu po každej úlohe | Sonnet 5.5 |
| Riadenie plánu, kontrola súladu s GDD | hlavná relácia |

Fable 5.2 nepíše herný kód a Sonnet 5.5 netvorí modely. Hranica medzi nimi je `SourceAssets/Models/*.glb` a tabuľka rozmerov v `03-3d-modely.md`.
