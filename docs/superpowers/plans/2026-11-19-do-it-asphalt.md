# Do it Asphalt Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hrateľná a dohrateľná hra Do it Asphalt v Unreal Engine 5.8 podľa GDD – od prázdneho priečinka po zabalený build pre Mac.

**Architecture:** Dva C++ moduly. `AsphaltCore` obsahuje všetky pravidlá hry ako čisté, deterministické C++ bez enginu a vzniká cez TDD. `DoItAsphalt` je tenká engine vrstva (aktéri, vstup, Slate UI), ktorá scénu stavia kódom. Všetky assety vznikajú zo skriptov: level a materiály editorovým Pythonom, 3D modely Blender skriptami.

**Tech Stack:** Unreal Engine 5.8.4 (C++, Slate, Enhanced Input, Automation Tests, PythonScriptPlugin, Interchange), Blender (headless Python), bash, git + git-lfs.

**Spec:** `docs/01-herny-dizajn.md` (pravidlá a čísla), `docs/02-technicky-navrh.md` (architektúra), `docs/03-3d-modely.md` (modely). Vykonávateľ číta plán spolu s nimi.

## Modely

| Značka | Kto úlohu robí | Parameter pri spúšťaní subagenta |
|---|---|---|
| **[Sonnet 5.5]** | každá úloha s kódom (C++, Python pre editor, shell, testy) aj kontrola kódu | `model: "sonnet"` |
| **[Fable 5.2]** | každá úloha s 3D modelmi (Blender skripty, náhľady) | `model: "fable"` |

Fable 5.2 nepíše herný kód, Sonnet 5.5 netvorí modely. Hlavná relácia plán riadi a nič neimplementuje.

## Global Constraints

- Engine: `/Users/Shared/Epic Games/UE_5.8` (5.8.4). Skripty ho berú z `UE_ROOT`, predvolene táto cesta.
- API enginu overuj v zdrojákoch na disku (`$UE_ROOT/Engine/Source`, `$UE_ROOT/Engine/Plugins`), nie z pamäti.
- Žiadne Blueprinty, UMG ani ručne vytvorené `.uasset`. Výnimka: Mannequin skopírovaný zo šablóny.
- `AsphaltCore` závisí len od modulu `Core`. Žiadne `UObject`, `UWorld`, `AActor`.
- Každé herné číslo je v `Source/AsphaltCore/Public/AsphaltBalance.h` alebo `AsphaltLayout.h`; engine vrstva žiadne herné číslo neobsahuje.
- Hodnoty zo špecky sa nemenia: batoh 5/7/10/15/20/30/40/60/80/100/135/150/200 l; nástroj 0,2/0,3/0,5/0,8/1/1,5/2 m; výrobňa 15/20/25/35/50/80/100/125/180/300/500/750/1000 l/min; bunka 20 l; robot 5 buniek za 5 minút.
- Jednotky v kóde: centimetre, litre, sekundy. Identifikátory a komentáre po anglicky, texty v hre po anglicky cez `LOCTEXT`.
- Cesta k projektu obsahuje medzery – každá cesta v každom skripte je v úvodzovkách.
- Testy jadra: `DoItAsphalt.Core.*`, testy engine vrstvy: `DoItAsphalt.Game.*`. Každý test sa deklaruje makrom `DIA_TEST` (Task 1).
- Každá úloha končí commitom; pred commitom musí prejsť `Scripts/build.sh` aj `Scripts/test.sh`.
- Vizuálne overenie: `Scripts/shot.sh`, výsledný PNG si vykonávateľ otvorí a porovná s popisom v kroku.

## Review Focus

1. **Batoh sa minie uprostred ťahu** – liatie sa zastaví presne, zostatok nikdy nejde do mínusu, nezapočíta sa texel, na ktorý nebolo dosť. → test v Task 4 (`Stamp.StopsWhenLitresRunOut`) a Task 5 (`Game.PourNeverGoesNegative`).
2. **Liatie na už zaliatu plochu alebo mimo buniek** – 0 litrov, 0 peňazí; peniaze sa nedajú farmiť prelievaním. → Task 4 (`Stamp.RepaintIsFree`, `Stamp.OutsideMapIsFree`).
3. **Poškodený, skrátený alebo starší uložený súbor** – hra nespadne, stav zostane nedotknutý, ponúkne sa len nová hra. → Task 7 (`Save.RejectsCorruptData`) a Task 18 (`Game.CorruptSlotStartsNewGame`).
4. **Nákup na maxime, bez peňazí, alebo špeciál pred dokončením cesty** – vráti `false`, nič sa nezmení. → Task 5 (`Game.BuyRejected`) a Task 6 (`Specials.LockedUntilRoadComplete`).
5. **Robot nemá čo liať alebo mu hráč dokončí cieľovú bunku** – nečinne čaká, resp. si vyberie novú; žiadny pád ani odmena z ničoho. → Task 6 (`AutoPaver.IdleWhenAllComplete`, `AutoPaver.RetargetsWhenCellTaken`).

## Poradie a závislosti

```
Kód (Sonnet 5.5):   1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11 → 12 → 13 → 14 → 15 → 16 → 17 → 18 → 19 → 20
Modely (Fable 5.2):       M1 ─────────────────────┐        M3 → M4 → M5 → M6 ──┐
                                                  M2 (Sonnet, po 8 a M1)        M7 (Sonnet, po 17 a M6)
```

Vetva modelov beží súbežne s kódom: M1 môže začať hneď po Task 1. Dve úlohy nikdy nepíšu do tých istých súborov – modely žijú v `Tools/ModelGen/` a `SourceAssets/`.

**Predpoklad pre vetvu modelov:** nainštalovaný Blender (`brew install --cask blender`). Inštaláciu spúšťa používateľ.

---

# Fáza 0 – Základ

### Task 1 [Sonnet 5.5]: Kostra projektu, skripty, git

**Files:**
- Create: `DoItAsphalt.uproject`, `Source/DoItAsphalt.Target.cs`, `Source/DoItAsphaltEditor.Target.cs`
- Create: `Source/AsphaltCore/AsphaltCore.Build.cs`, `Source/AsphaltCore/Public/AsphaltTest.h`, `Source/AsphaltCore/Private/AsphaltCoreModule.cpp`, `Source/AsphaltCore/Private/Tests/SmokeTest.cpp`
- Create: `Source/DoItAsphalt/DoItAsphalt.Build.cs`, `Source/DoItAsphalt/DoItAsphalt.h`, `Source/DoItAsphalt/DoItAsphalt.cpp`
- Create: `Config/DefaultEngine.ini`, `Config/DefaultGame.ini`
- Create: `Scripts/env.sh`, `Scripts/build.sh`, `Scripts/test.sh`, `Scripts/uepy.sh`, `Scripts/run.sh`
- Create: `.gitignore`, `.gitattributes`

**Interfaces:**
- Produces: makro `DIA_TEST(ClassName, "Pretty.Name")` v `AsphaltTest.h` = `IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, PrettyName, EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)`.
- Produces: `Scripts/build.sh` (bez argumentov), `Scripts/test.sh [filter=DoItAsphalt]`, `Scripts/uepy.sh <skript.py>`, `Scripts/run.sh`; všetky načítajú `Scripts/env.sh`, ktorý nastaví `UE_ROOT`, `PROJECT_ROOT`, `UPROJECT`.

- [ ] **Step 1:** `git init`, `git lfs install`; `.gitattributes` s LFS pre `*.uasset *.umap *.glb *.png`; `.gitignore` pre `Binaries/ Intermediate/ Saved/ DerivedDataCache/ Builds/ .DS_Store ~$*`.
- [ ] **Step 2:** `.uproject`: `EngineAssociation` `"5.8"`, moduly `AsphaltCore` a `DoItAsphalt` (oba `Runtime`, `Default`), pluginy `PythonScriptPlugin` a `EditorScriptingUtilities` zapnuté. Targety podľa `$UE_ROOT/Templates/TP_ThirdPerson/Source/*.Target.cs` (`BuildSettingsVersion.V7`, `EngineIncludeOrderVersion.Unreal5_8`), `ExtraModuleNames` oba moduly.
- [ ] **Step 3:** `AsphaltCore.Build.cs`: verejná závislosť len `Core`. `DoItAsphalt.Build.cs`: `Core, CoreUObject, Engine, InputCore, EnhancedInput, Slate, SlateCore, AsphaltCore`.
- [ ] **Step 4:** `DefaultEngine.ini`: `r.DynamicGlobalIlluminationMethod=0`, `r.ReflectionMethod=2`, `r.Shadow.Virtual.Enable=0`, `r.AllowStaticLighting=False`. Mapy a game mode doplní Task 8 a 9.
- [ ] **Step 5:** Test `FCoreSmokeTest`, `"DoItAsphalt.Core.Smoke"`: `TestTrue(TEXT("module loads"), true)`.
- [ ] **Step 6:** `build.sh` volá `"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DoItAsphaltEditor Mac Development -project="$UPROJECT"`. `test.sh` volá `UnrealEditor-Cmd "$UPROJECT" -ExecCmds="Automation RunTests $FILTER; Quit" -unattended -nullrhi -nosplash -nosound -stdout -ReportExportPath="$PROJECT_ROOT/Saved/TestReport"`, potom z `Saved/TestReport/index.json` vypíše `PASSED <n> / FAILED <m>` a pri `m > 0` alebo `n + m == 0` skončí kódom 1. `uepy.sh` volá `UnrealEditor-Cmd "$UPROJECT" -run=pythonscript -Script="$1" -unattended -nosplash -stdout`.
- [ ] **Step 7:** Run `Scripts/build.sh` → Expected: končí riadkom s `Result: Succeeded`, kód 0.
- [ ] **Step 8:** Run `Scripts/test.sh` → Expected: `PASSED 1 / FAILED 0`. Run `Scripts/test.sh NoSuchTest` → Expected: kód 1.
- [ ] **Step 9:** Commit `chore: project skeleton, build and test scripts`.

---

# Fáza 1 – Jadro hry (`AsphaltCore`, TDD)

Každá úloha tejto fázy ide rovnako: napíš testy → `Scripts/build.sh && Scripts/test.sh DoItAsphalt.Core` zlyhá na nových testoch → implementuj → všetko prejde → commit. Kroky nižšie uvádzajú len to, čo je pre úlohu špecifické.

### Task 2 [Sonnet 5.5]: Šesťuholníkové súradnice

**Files:**
- Create: `Source/AsphaltCore/Public/HexCoord.h`, `Source/AsphaltCore/Private/HexCoord.cpp`
- Test: `Source/AsphaltCore/Private/Tests/HexCoordTest.cpp`

**Interfaces:**
- Produces:
```cpp
struct ASPHALTCORE_API FHexCoord { int32 Q = 0; int32 R = 0; };
FHexCoord operator+(FHexCoord A, FHexCoord B);
FHexCoord operator*(FHexCoord A, int32 K);
bool operator==(FHexCoord A, FHexCoord B);
uint32 GetTypeHash(FHexCoord C);
namespace AsphaltHex {
    inline constexpr double SideCm = 62.0403;                 // cell area = 1 m²
    ASPHALTCORE_API extern const FHexCoord Directions[6];     // {1,0},{1,-1},{0,-1},{-1,0},{-1,1},{0,1}
    ASPHALTCORE_API int32 Distance(FHexCoord A, FHexCoord B); // (|dq| + |dr| + |dq+dr|) / 2
    ASPHALTCORE_API FVector2D ToWorld(FHexCoord C);           // X = √3·Side·(Q + R/2), Y = 1.5·Side·R
    ASPHALTCORE_API FHexCoord FromWorld(FVector2D P);         // cube rounding
    ASPHALTCORE_API double EdgeDistanceCm(FVector2D P);       // distance to nearest edge of the containing cell
}
```

- [ ] **Step 1: Testy** (`DoItAsphalt.Core.Hex.*`):
```cpp
// Area
TestEqual("area", 1.5 * FMath::Sqrt(3.0) * AsphaltHex::SideCm * AsphaltHex::SideCm, 10000.0, 1.0);
// Distance
TestEqual("d", AsphaltHex::Distance({0,0}, {12,-6}), 12);
TestEqual("d", AsphaltHex::Distance({13,-7}, {0,0}), 13);
TestEqual("d", AsphaltHex::Distance({-3,5}, {2,-1}), 6);
// ToWorld
TestTrue("x", AsphaltHex::ToWorld({1,0}).Equals(FVector2D(107.457, 0.0), 0.01));
TestTrue("y", AsphaltHex::ToWorld({0,1}).Equals(FVector2D(53.7285, 93.0605), 0.01));
// Round trip: for every Q,R in [-15,15]: FromWorld(ToWorld(c)) == c and FromWorld(ToWorld(c) + FVector2D(30,20)) == c
// EdgeDistance
TestEqual("center", AsphaltHex::EdgeDistanceCm(AsphaltHex::ToWorld({3,-2})), 53.7285, 0.01);
TestEqual("edge", AsphaltHex::EdgeDistanceCm(AsphaltHex::ToWorld({3,-2}) + FVector2D(53.7285, 0)), 0.0, 0.01);
```
- [ ] **Step 2:** Over, že testy zlyhajú; implementuj; over, že prejdú.
- [ ] **Step 3:** Commit `feat(core): hex coordinates`.

### Task 3 [Sonnet 5.5]: Mapa 499 buniek a rozloženie areálu

**Files:**
- Create: `Source/AsphaltCore/Public/HexMap.h`, `Source/AsphaltCore/Private/HexMap.cpp`, `Source/AsphaltCore/Public/AsphaltLayout.h`, `Source/AsphaltCore/Private/AsphaltLayout.cpp`
- Test: `Source/AsphaltCore/Private/Tests/HexMapTest.cpp`

**Interfaces:**
- Consumes: `FHexCoord`, `AsphaltHex::*` (Task 2).
- Produces:
```cpp
enum class EAsphaltCellKind : uint8 { Lot, Road };
struct FAsphaltCell { FHexCoord Coord; EAsphaltCellKind Kind; int32 RoadIndex; /* INDEX_NONE for Lot */ };
class ASPHALTCORE_API FHexMap {
public:
    static constexpr int32 LotRadius = 12, NumLotCells = 469, NumRoads = 6, CellsPerRoad = 5, NumCells = 499;
    FHexMap();
    int32 Num() const;
    const FAsphaltCell& GetCell(int32 Index) const;
    int32 FindCell(FHexCoord Coord) const;                 // INDEX_NONE if not on the map
    TConstArrayView<int32> GetRoadCells(int32 Road) const; // 5 indices
    static FHexCoord RoadAnchor(int32 Road);               // (Dir[i] + Dir[(i+1)%6]) * 6
    static FVector2D RoadDirection(int32 Road);            // unit vector pointing out of the lot
};
namespace AsphaltLayout {
    inline const FVector2D PlantPos{-2000.0, 0.0};         // plant faces +X
    inline const FVector2D RefillCenter{-1600.0, 0.0};
    inline constexpr double RefillRadiusCm = 200.0;
    inline const FVector2D PlayerStart{-1600.0, 0.0};
    inline constexpr double SiteRadiusCm = 2600.0;         // fence ring
    ASPHALTCORE_API FVector2D RoadEnd(int32 Road);         // ToWorld(RoadAnchor) + RoadDirection * 300
    ASPHALTCORE_API FVector2D KioskPos(int32 Road);        // RoadEnd + Right * 220, Right = (-Dir.Y, Dir.X)
    ASPHALTCORE_API FVector2D GatePos(int32 Road);         // RoadDirection * SiteRadiusCm (in the fence line)
    ASPHALTCORE_API FVector2D ExitTrigger();               // RoadDirection(5) * (SiteRadiusCm + 150), radius 150
    ASPHALTCORE_API FVector2D SpecialSpawn(int32 Road);    // ToWorld(RoadAnchor) - RoadDirection * 300 (on the lot)
}
```
- Poradie buniek je záväzné (od neho závisia indexy v uložených hrách): index 0 = `{0,0}`; potom kruhy `k = 1..12`, každý začína na `Directions[4] * k` a ide `k` krokov postupne v smeroch `Directions[0..5]`; potom cesty, index `469 + 5*i + j`, kde pre `a = Directions[i]`, `b = Directions[(i+1)%6]`, `M = RoadAnchor(i)` je poradie `M+a, M+b, M+a+b, M+a*2, M+b*2`.

- [ ] **Step 1: Testy** (`DoItAsphalt.Core.Map.*`):
```cpp
FHexMap Map;
TestEqual("num", Map.Num(), 499);
TestTrue("center", Map.GetCell(0).Coord == FHexCoord{0,0});
// cells 0..468: Kind == Lot, Distance to {0,0} <= 12; cells 469..498: Kind == Road, RoadIndex == (Index-469)/5
// all 499 coords are unique
TestTrue("edge is lot", Map.FindCell({12,-6}) >= 0 && Map.FindCell({12,-6}) < 469);
TestEqual("road0 first", Map.FindCell({13,-6}), 469);
TestEqual("off map", Map.FindCell({13,0}), (int32)INDEX_NONE);
// GetRoadCells(0) coords in order: {13,-6},{13,-7},{14,-7},{14,-6},{14,-8}
// GetRoadCells(2) coords in order: {-6,-7},{-7,-6},{-7,-7},{-6,-8},{-8,-6}
TestTrue("dir0", FHexMap::RoadDirection(0).Equals(FVector2D(0.8660, -0.5), 0.001));
// Layout: 16 points on the refill circle, KioskPos(i), GatePos(i), ExitTrigger():
//   Map.FindCell(AsphaltHex::FromWorld(P)) == INDEX_NONE
// KioskPos(i).Size() and SpecialSpawn(i).Size() < SiteRadiusCm; GatePos(i).Size() == SiteRadiusCm (0.01); ExitTrigger().Size() == 2750 (0.01)
// Map.FindCell(AsphaltHex::FromWorld(SpecialSpawn(i))) is a Lot cell
TestTrue("kiosk3", AsphaltLayout::KioskPos(3).Equals(FVector2D(-1337, 518), 1.0));
TestTrue("gate5", AsphaltLayout::GatePos(5).Equals(FVector2D(2252, 1300), 1.0));
TestTrue("spawn0", AsphaltLayout::SpecialSpawn(0).Equals(FVector2D(707, -408), 1.0));
```
- [ ] **Step 2:** Zlyhá → implementuj → prejde.
- [ ] **Step 3:** Commit `feat(core): hex map with 469 lot cells and 6 roads, site layout`.

### Task 4 [Sonnet 5.5]: Tabuľky balansu a mriežka pokrytia

**Files:**
- Create: `Source/AsphaltCore/Public/AsphaltBalance.h`, `Source/AsphaltCore/Public/CoverageGrid.h`, `Source/AsphaltCore/Private/CoverageGrid.cpp`
- Test: `Source/AsphaltCore/Private/Tests/BalanceTest.cpp`, `Source/AsphaltCore/Private/Tests/CoverageGridTest.cpp`

**Interfaces:**
- Consumes: `FHexMap` (Task 3).
- Produces `AsphaltBalance.h` (všetko `inline constexpr` v `namespace AsphaltBalance`):
```cpp
double LitresPerCell = 20.0;  double MoneyPerLitre = 1.0;  int32 ShardsPerCell = 1;
double CompletionThreshold = 0.97;
double BackpackCapacityL[13] = {5,7,10,15,20,30,40,60,80,100,135,150,200};
int32  BackpackPriceShards[12] = {1,2,4,6,9,13,18,24,31,39,48,58};
double ToolWidthM[7] = {0.2,0.3,0.5,0.8,1.0,1.5,2.0};
double ToolPriceMoney[6] = {25,60,120,220,380,600};
double PlantRateLPerMin[13] = {15,20,25,35,50,80,100,125,180,300,500,750,1000};
double PlantPriceMoney[12] = {15,25,40,60,90,130,180,240,310,390,480,580};
double SpecialPriceMoney[5] = {100,200,300,450,800};   // Sprint, TwoHands, PortableStation, AutoPaver, PaverCar
int32  SpecialRoad[5] = {3,2,4,1,0};                   int32 ExitRoad = 5;
double PlantStorageMinutes = 4.0;   double RefillSeconds = 1.5;
double NozzleAheadCm = 80.0;        double StampPadCm = 5.0;
double WalkSpeedCmS = 450.0;  double PourSpeedCmS = 200.0;  double SprintMultiplier = 1.6;
double AutoPaverRateLPerMin = 20.0; double AutoPaverSpeedCmS = 150.0;
double CarTankL = 500.0;  double CarWidthCm = 300.0;  double CarSpeedCmS = 600.0;  double CarBarBehindCm = 230.0;
constexpr int32 BackpackTier(int32 Level);   // 0..2 → 0, 3..6 → 1, 7..9 → 2, 10..12 → 3
```
- Produces `CoverageGrid.h`:
```cpp
struct FPourResult { double LitresUsed = 0.0; TArray<int32> CompletedCells; };
class ASPHALTCORE_API FCoverageGrid {
public:
    static constexpr int32 Size = 640;  static constexpr double TexelCm = 5.0;  static constexpr double HalfExtentCm = 1600.0;
    explicit FCoverageGrid(const FHexMap& Map);
    FPourResult Stamp(FVector2D A, FVector2D B, double WidthCm, double MaxLitres);
    FPourResult DepositIntoCell(int32 Cell, double MaxLitres);   // covers uncovered texels of one cell, row-major
    FPourResult CompleteCell(int32 Cell);                        // covers the whole cell
    int32  GetCellTexelCount(int32 Cell) const;
    double GetCellFill(int32 Cell) const;                        // 0..1
    bool   IsCellComplete(int32 Cell) const;
    int32  NumCompleteCells() const;
    int32  GetTexelCell(int32 X, int32 Y) const;                 // INDEX_NONE if the texel belongs to no cell
    TConstArrayView<uint8> GetTexels() const;                    // Size*Size, row-major, 0 or 255
    bool   ConsumeDirtyRect(FIntRect& Out);                      // false if nothing changed since last call
    void   WriteBits(TArray<uint8>& Out) const;                  // exactly 51200 bytes
    bool   ReadBits(TConstArrayView<uint8> In);                  // false on wrong size; rebuilds per-cell counters
};
```
- Pravidlá `Stamp`, ktoré signatúra neurčuje:
  - Stred texelu `(X,Y)` je vo svete `(-1600 + (X+0.5)*5, -1600 + (Y+0.5)*5)`. Texel patrí bunke, v ktorej leží jeho stred.
  - Razí sa orientovaný obdĺžnik: os `A→B` predĺžená o `StampPadCm` na oboch koncoch, polšírka `WidthCm/2`. Ak `A == B`, os smeruje v +X. Zasiahnutý je texel, ktorého stred leží v obdĺžniku.
  - Texely sa spracujú v poradí riadkov (Y vonkajší cyklus, X vnútorný). Nový texel bunky `c` stojí `20 / GetCellTexelCount(c)` litrov. Ak je zostatok `MaxLitres - LitresUsed` menší než cena texelu, `Stamp` skončí.
  - Texel, ktorý nepatrí žiadnej bunke alebo je už pokrytý, nestojí nič.
  - Keď pokrytie bunky dosiahne `CompletionThreshold`, zvyšné texely bunky sa označia bez ceny a bunka sa pridá do `CompletedCells`. Dokončená bunka sa hlási práve raz.

- [ ] **Step 1: Testy balansu** (`DoItAsphalt.Core.Balance.*`):
```cpp
// Sum(BackpackPriceShards) == 253;  Sum(ToolPriceMoney) + Sum(PlantPriceMoney) + Sum(SpecialPriceMoney) == 5795
// 5795 <= 0.6 * 499 * 20;  253 <= 499
// BackpackTier: 2→0, 3→1, 6→1, 7→2, 9→2, 10→3, 12→3
// every table is strictly increasing
```
- [ ] **Step 2: Testy mriežky** (`DoItAsphalt.Core.Stamp.*`, `DoItAsphalt.Core.Grid.*`):
```cpp
FHexMap Map; FCoverageGrid G(Map);
// Grid.TexelCounts: for every cell 385 <= GetCellTexelCount(c) <= 415; GetCellTexelCount(0) == 408
// Stamp.ExactLitres
const double PerTexel = 20.0 / G.GetCellTexelCount(0);
FPourResult R = G.Stamp({-20,0}, {20,0}, 20.0, 100.0);        // 50 x 20 cm → 40 texels
TestEqual("litres", R.LitresUsed, 40 * PerTexel, 1e-9);
TestEqual("fill", G.GetCellFill(0), 40.0 / G.GetCellTexelCount(0), 1e-9);
// Stamp.RepaintIsFree
TestEqual("again", G.Stamp({-20,0}, {20,0}, 20.0, 100.0).LitresUsed, 0.0);
// Stamp.OutsideMapIsFree
TestEqual("outside", G.Stamp({1500,1500}, {1550,1550}, 200.0, 100.0).LitresUsed, 0.0);
// Stamp.StopsWhenLitresRunOut (fresh grid)
TestEqual("limited", G2.Stamp({-20,0}, {20,0}, 20.0, 10.5 * PerTexel).LitresUsed, 10 * PerTexel, 1e-9);
// Stamp.ZeroLengthSegment (fresh grid): Stamp({0,0},{0,0}, 20.0, 100.0) covers 2 x 4 = 8 texels
// Stamp.CompletesCell (fresh grid): stamp {-70,y}→{70,y} width 10 for y = -65..65 step 10 with MaxLitres 1000:
//   cell 0 appears in CompletedCells exactly once over all calls, IsCellComplete(0), GetCellFill(0) == 1.0
// Grid.DepositIntoCell: DepositIntoCell(5, 7.0).LitresUsed in (7.0 - 20.0/Count(5), 7.0]; the second call does not
//   complete the cell; the third call returns CompletedCells == {5}; a fourth call returns 0 litres
// Grid.ThresholdIsFree: P9 = 20.0/Count(9); DepositIntoCell(9, 100.0).LitresUsed in [19.4 - P9, 19.4 + P9];
//   IsCellComplete(9); GetCellFill(9) == 1.0
// Grid.CompleteCell: CompleteCell(7).LitresUsed == 20 (1e-6), CompletedCells == {7}; second call returns 0 and empty
// Grid.DirtyRect: after one Stamp ConsumeDirtyRect returns true and a rect containing the stamped texels; second call false
// Grid.Bits: WriteBits → 51200 bytes; ReadBits on a fresh grid reproduces GetCellFill for all cells and NumCompleteCells;
//   ReadBits with 100 bytes returns false
```
- [ ] **Step 3:** Zlyhá → implementuj → prejde.
- [ ] **Step 4:** Commit `feat(core): balance tables and coverage grid`.

### Task 5 [Sonnet 5.5]: `FAsphaltGame` – ekonomika, batoh, výrobňa, bežné vylepšenia

**Files:**
- Create: `Source/AsphaltCore/Public/AsphaltGame.h`, `Source/AsphaltCore/Private/AsphaltGame.cpp`
- Test: `Source/AsphaltCore/Private/Tests/AsphaltGameTest.cpp`

**Interfaces:**
- Consumes: `FHexMap`, `FCoverageGrid`, `FPourResult`, `AsphaltBalance::*`.
- Produces:
```cpp
enum class EAsphaltUpgrade : uint8 { Backpack, Tool, Plant };
struct FAsphaltEvents {
    double LitresPoured = 0.0;  double MoneyEarned = 0.0;  int32 ShardsEarned = 0;
    TArray<int32> CompletedCells;  TArray<int32> CompletedRoads;  bool bExitOpened = false;
    void Append(const FAsphaltEvents& Other);
};
class ASPHALTCORE_API FAsphaltGame {
public:
    FAsphaltGame();
    const FHexMap& GetMap() const;          FCoverageGrid& GetGrid();          const FCoverageGrid& GetGrid() const;
    double GetMoney() const;                int32 GetShards() const;
    double GetPlaytimeSeconds() const;      double GetTotalLitresPoured() const;
    int32  GetLevel(EAsphaltUpgrade U) const;       int32 GetMaxLevel(EAsphaltUpgrade U) const;   // 12, 6, 12
    double GetNextPrice(EAsphaltUpgrade U) const;   // -1 at max level; shards for Backpack, money otherwise
    bool   CanBuy(EAsphaltUpgrade U) const;         bool Buy(EAsphaltUpgrade U);
    double GetBackpackCapacity() const;     double GetBackpackLitres() const;
    double GetToolWidthCm() const;          // 100 * ToolWidthM[level]; Task 6 doubles it with TwoHands
    double GetPlantRatePerMin() const;      double GetPlantStorage() const;    double GetPlantStock() const;
    FAsphaltEvents Tick(double Dt);         // any Dt; internally stepped by at most 0.25 s
    double RefillBackpack(double Dt);       // moves min(capacity/RefillSeconds*Dt, free space, stock); returns litres moved
    FAsphaltEvents PourFromBackpack(FVector2D A, FVector2D B);
    void   DebugGive(double Money, int32 Shards);
    void   DebugSetLevel(EAsphaltUpgrade U, int32 Level);
    FAsphaltEvents DebugCompleteCell(int32 Cell);   // rewards exactly as if the missing litres had been poured
};
```
- Pravidlá: každý vyliaty liter pripíše `MoneyPerLitre`; každá dokončená bunka `ShardsPerCell`; zásobník rastie o `rate/60 * Dt` a je orezaný na `rate * PlantStorageMinutes`; nová hra má všetko na nule a úrovne 0.

- [ ] **Step 1: Testy** (`DoItAsphalt.Core.Game.*`):
```cpp
FAsphaltGame G;
// Game.NewGame: Money 0, Shards 0, capacity 5, BackpackLitres 0, ToolWidthCm 20, rate 15, storage 60, stock 0
// Game.PlantProduces: G.Tick(60) → stock 15 (1e-6); G.Tick(600) → stock 60; playtime 660
// Game.Refill: after stock 60: RefillBackpack(0.75) == 2.5; RefillBackpack(10) == 2.5; backpack 5, stock 55
//   with stock 1 and empty backpack: RefillBackpack(10) == 1
// Game.PourPays: with 5 l: E = PourFromBackpack({-20,0},{20,0}); E.LitresPoured > 0;
//   E.MoneyEarned == E.LitresPoured; GetMoney() == E.MoneyEarned; GetBackpackLitres() == 5 - E.LitresPoured (1e-9)
// Game.PourNeverGoesNegative: with 0.03 l in the backpack: pour over a wide fresh area → LitresPoured <= 0.03,
//   GetBackpackLitres() >= 0; with an empty backpack → LitresPoured == 0
// Game.CellGivesShard: DebugCompleteCell(0) → ShardsEarned 1, MoneyEarned 20 (1e-6), CompletedCells {0}; again → all zero
// Game.BuyUpgrades: DebugGive(15, 1): Buy(Plant) true → money 0, rate 20, storage 80; Buy(Backpack) true → shards 0, capacity 7
//   DebugGive(25, 0): Buy(Tool) true → width 30
// Game.BuyRejected: money 14 → CanBuy(Plant) false, Buy(Plant) false, money still 14, level 0;
//   DebugSetLevel(Tool, 6); DebugGive(1e6, 0) → GetNextPrice(Tool) == -1, Buy(Tool) false, money unchanged
// Game.BackpackKeepsContentOnUpgrade: 5 l in the backpack, Buy(Backpack) → still 5 l, capacity 7
```
- [ ] **Step 2:** Zlyhá → implementuj → prejde.
- [ ] **Step 3:** Commit `feat(core): game facade with economy, plant and upgrades`.

### Task 6 [Sonnet 5.5]: Špeciály, cesty, východ, robot, auto

**Files:**
- Modify: `Source/AsphaltCore/Public/AsphaltGame.h`, `Source/AsphaltCore/Private/AsphaltGame.cpp`
- Test: `Source/AsphaltCore/Private/Tests/SpecialsTest.cpp`

**Interfaces:**
- Produces (pribudne do `AsphaltGame.h`):
```cpp
enum class EAsphaltSpecial : uint8 { Sprint, TwoHands, PortableStation, AutoPaver, PaverCar, Count };
struct FAsphaltWorldState {                    // positions owned by the engine layer, stored with the game
    FVector2D PlayerPos = AsphaltLayout::PlayerStart;  double PlayerYawDeg = 0.0;
    FVector2D StationPos;  bool bStationCarried = false;
    FVector2D CarPos;      double CarYawDeg = 0.0;  bool bPlayerInCar = false;
};
// FAsphaltGame:
    bool   HasSpecial(EAsphaltSpecial S) const;      double GetSpecialPrice(EAsphaltSpecial S) const;
    int32  GetSpecialRoad(EAsphaltSpecial S) const;
    bool   CanBuySpecial(EAsphaltSpecial S) const;   bool BuySpecial(EAsphaltSpecial S);
    int32  GetRoadProgress(int32 Road) const;        // completed cells of the road, 0..5
    bool   IsRoadComplete(int32 Road) const;
    bool   IsExitOpen() const;                       // all 499 cells complete
    bool   HasWon() const;  void SetWon();           // SetWon is ignored unless IsExitOpen()
    double GetCarLitres() const;  double RefillCar(double Dt);
    FAsphaltEvents PourFromCar(FVector2D A, FVector2D B);    // width CarWidthCm, from the car tank
    FVector2D GetAutoPaverPos() const;               int32 GetAutoPaverTarget() const;   // INDEX_NONE when idle
    FAsphaltWorldState& World();                     const FAsphaltWorldState& World() const;
    void   DebugGrantSpecial(EAsphaltSpecial S);
```
- Pravidlá:
  - `CanBuySpecial` = ešte ho nemá, cesta `SpecialRoad[S]` je dokončená a peniaze stačia. Kúpa `PortableStation` nastaví `World().StationPos`, kúpa `PaverCar` nastaví `World().CarPos`, obe na `AsphaltLayout::SpecialSpawn(cesta)`; robot začína tamtiež.
  - `TwoHands` zdvojnásobí `GetToolWidthCm()`.
  - `FAsphaltEvents::CompletedRoads` obsahuje cestu v tom volaní, ktoré dokončilo jej piatu bunku; `bExitOpened` je `true` vo volaní, ktoré dokončilo 499. bunku. Platí pre `PourFromBackpack`, `PourFromCar`, `Tick` aj `DebugCompleteCell`.
  - Robot (len ak ho hráč má), v každom kroku `Tick`: ak nemá cieľ alebo je cieľ dokončený, vyberie nedokončenú bunku s najmenšou vzdialenosťou stredu od svojej polohy (pri zhode nižší index); ak taká nie je, stojí. Inak sa posunie k stredu cieľa rýchlosťou `AutoPaverSpeedCmS`; keď je do 5 cm, pripíše si do vlastného rozpočtu `AutoPaverRateLPerMin/60 * krok` litrov a volá `DepositIntoCell(cieľ, rozpočet)`; nevyužitý zvyšok rozpočtu si prenáša do ďalšieho kroku (inak by sa strácal, lebo jeden texel stojí asi 0,05 l), najviac 1 l. Počas presunu rozpočet nerastie. Litre a dokončené bunky odmeňuje ako liatie hráča.
  - `RefillCar` ako `RefillBackpack`, s kapacitou `CarTankL`.

- [ ] **Step 1: Testy** (`DoItAsphalt.Core.Specials.*`, `DoItAsphalt.Core.AutoPaver.*`, `DoItAsphalt.Core.Exit.*`):
```cpp
// Specials.Roads: GetSpecialRoad(Sprint)==3, TwoHands==2, PortableStation==4, AutoPaver==1, PaverCar==0
// Specials.LockedUntilRoadComplete: DebugGive(1000,0): CanBuySpecial(Sprint) false, BuySpecial false, money 1000;
//   complete 4 cells of road 3 → GetRoadProgress(3) == 4, still false;
//   fifth cell → its events have CompletedRoads == {3}; IsRoadComplete(3); BuySpecial(Sprint) true; money 1000 + 5*20 - 100
//   BuySpecial(Sprint) again → false, money unchanged
// Specials.NotEnoughMoney: road 0 complete, money 799 → BuySpecial(PaverCar) false
// Specials.TwoHandsDoublesWidth: 20 → 40; with DebugSetLevel(Tool, 6): 200 → 400
// Specials.Car: DebugGrantSpecial(PaverCar); stock 60 → RefillCar(10) == 60; PourFromCar({0,-10},{0,10}) LitresPoured > 0,
//   GetCarLitres() == 60 - poured; World().CarPos == AsphaltLayout::SpecialSpawn(0) after BuySpecial
// AutoPaver.PavesFiveInFiveMinutes: DebugGrantSpecial(AutoPaver); E = Tick(300): NumCompleteCells() is 4 or 5;
//   E.ShardsEarned == NumCompleteCells(); E.MoneyEarned > 0;  after Tick(600) total: NumCompleteCells() in [13, 16]
// AutoPaver.RetargetsWhenCellTaken: Tick(5); T = GetAutoPaverTarget(); DebugCompleteCell(T); Tick(5) → target != T, no crash
// AutoPaver.IdleWhenAllComplete: complete all 499; E = Tick(60) → LitresPoured 0, CompletedCells empty, target INDEX_NONE
// Exit.OpensAtAllCells: complete cells 0..497 → IsExitOpen() false, SetWon() leaves HasWon() false;
//   cell 498 → events.bExitOpened; IsExitOpen(); SetWon() → HasWon()
// Exit.TotalRewards: after completing all cells via DebugCompleteCell: shards 499, money 9980 (1e-3)
```
- [ ] **Step 2:** Zlyhá → implementuj → prejde.
- [ ] **Step 3:** Commit `feat(core): specials, roads, exit, auto paver and car`.

### Task 7 [Sonnet 5.5]: Uloženie do bajtov

**Files:**
- Modify: `Source/AsphaltCore/Public/AsphaltGame.h`, `Source/AsphaltCore/Private/AsphaltGame.cpp`
- Test: `Source/AsphaltCore/Private/Tests/SaveTest.cpp`

**Interfaces:**
- Produces:
```cpp
// FAsphaltGame:
    static constexpr uint32 SaveMagic = 0x31414944;   // "DIA1"
    static constexpr int32  SaveVersion = 1;
    TArray<uint8> SaveToBytes() const;
    bool LoadFromBytes(TConstArrayView<uint8> Bytes);  // on failure returns false and leaves *this untouched
```
- Obsah: magic, verzia, peniaze, shardy, čas hry, vyliate litre, tri úrovne, maska špeciálov, `bWon`, zásobník, batoh, nádrž auta, poloha a cieľ robota, `FAsphaltWorldState`, 51 200 bajtov pokrytia. Po načítaní všetky čítané hodnoty prejdú kontrolou rozsahu (úrovne do maxima, litre do kapacít, žiadne NaN).

- [ ] **Step 1: Testy** (`DoItAsphalt.Core.Save.*`):
```cpp
// Save.RoundTrip: play a little (give, buy 2 upgrades, complete 30 cells, grant 2 specials, Tick(90), set World() fields);
//   B = SaveToBytes(); fresh game LoadFromBytes(B) == true; every getter listed in Tasks 5 and 6 returns the same value,
//   GetCellFill equal for all 499 cells, World() fields equal
// Save.RejectsCorruptData: for each of: empty array; first half of B; B with byte 0 flipped (magic);
//   B with version set to 2; 100000 bytes of 0xAB; B with the Tool level patched to 99:
//   a game with Money 123 calls LoadFromBytes → false and GetMoney() is still 123
```
- [ ] **Step 2:** Zlyhá → implementuj → prejde.
- [ ] **Step 3:** Commit `feat(core): save and load to bytes`.

---

# Fáza 2 – Hrateľný greybox

Od tejto fázy sa každá úloha overuje aj snímkou obrazovky. Modely ešte nemusia existovať – `DIAAssets` vráti náhradné tvary.

### Task 8 [Sonnet 5.5]: Level, materiály a Mannequin zo skriptov

**Files:**
- Create: `Tools/UE/create_level.py`, `Tools/UE/create_materials.py`, `Scripts/setup_content.sh`
- Modify: `Config/DefaultEngine.ini` (`GameDefaultMap`, `EditorStartupMap` = `/Game/Maps/L_Main`)
- Generated: `Content/Maps/L_Main.umap`, `Content/Materials/*.uasset`, `Content/Characters/Mannequins/**`

**Interfaces:**
- Produces: `/Game/Maps/L_Main` (smerové svetlo, sky atmosphere, sky light, exponenciálna hmla, `PlayerStart` v `(-1600, 0, 100)`; žiadna zem – tú stavia Task 9).
- Produces: `/Game/Materials/M_Ground` s parametrami `CoverageTex` (Texture2D), `HexLinesTex` (Texture2D), `EdgeSoftness` (Scalar, 0.1). Výstup: hlina `#8A6B4A`; kde `HexLinesTex > 0.5` biela `#F2F2EE`; cez to asfalt `#232326` s drsnosťou 0.35 s váhou `smoothstep(0.5 - EdgeSoftness, 0.5 + EdgeSoftness, CoverageTex)`. UV plochy 0..1 zodpovedá štvorcu sveta ±1600 cm.
- Produces: `/Game/Materials/M_DIA_Flat` (parametre `BaseColor`, `Metallic`, `Roughness`) a 12 inštancií `MI_DIA_Yellow` … `MI_DIA_Green` s hodnotami z `docs/03-3d-modely.md` kap. 3.
- Produces: `Scripts/setup_content.sh` skopíruje `$UE_ROOT/Templates/TemplateResources/High/Characters/Content/Mannequins` do `Content/Characters/Mannequins` (ak tam ešte nie je).

- [ ] **Step 1:** Napíš `setup_content.sh`, spusti. Expected: existuje `Content/Characters/Mannequins/Meshes/SKM_Manny_Simple.uasset` a `Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.uasset`.
- [ ] **Step 2:** Napíš `create_level.py`; oba skripty na konci vypíšu riadok `DIA_OK <cesta assetu>` za každý uložený asset a pri chybe skončia nenulovým kódom.
- [ ] **Step 3:** Run `Scripts/uepy.sh Tools/UE/create_level.py` → Expected: `DIA_OK /Game/Maps/L_Main`, súbor `Content/Maps/L_Main.umap` existuje.
- [ ] **Step 4:** Napíš `create_materials.py`. Run `Scripts/uepy.sh Tools/UE/create_materials.py` → Expected: 14 riadkov `DIA_OK` (2 materiály + 12 inštancií). Druhé spustenie dá rovnaký výstup bez chýb.
- [ ] **Step 5:** Commit `feat(tools): generated level, materials and mannequin content`.

### Task 9 [Sonnet 5.5]: Subsystém, game mode, zem, cheaty a snímky

**Files:**
- Create: `Source/DoItAsphalt/Game/DIAWorldSubsystem.{h,cpp}`, `Game/DIAGameMode.{h,cpp}`, `Game/DIAAssets.{h,cpp}`, `Game/DIACheats.cpp`, `World/DIAGroundActor.{h,cpp}`
- Create: `Scripts/shot.sh`
- Modify: `Config/DefaultEngine.ini` (`GlobalDefaultGameMode=/Script/DoItAsphalt.DIAGameMode`)
- Test: `Source/DoItAsphalt/Tests/WorldBuildTest.cpp`

**Interfaces:**
- Consumes: `FAsphaltGame` a všetko z Tasks 5 – 7; `M_Ground` (Task 8).
- Produces:
```cpp
UCLASS() class UDIAWorldSubsystem : public UTickableWorldSubsystem {
public:
    FAsphaltGame& GetGame();
    void Pour(FVector2D A, FVector2D B, bool bFromCar);      // calls the core, then Dispatch
    void Dispatch(const FAsphaltEvents& Events);             // broadcasts the delegates below
    bool Buy(EAsphaltUpgrade U);   bool BuySpecial(EAsphaltSpecial S);
    FSimpleMulticastDelegate OnStateChanged;                 // anything the HUD shows changed
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnCells, const TArray<int32>&);   FOnCells OnCellsCompleted;
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoad, int32);                   FOnRoad OnRoadCompleted;
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnSpecial, EAsphaltSpecial);      FOnSpecial OnSpecialBought;
    FSimpleMulticastDelegate OnExitOpened;
};
namespace DIAAssets {
    enum class EFallback : uint8 { Cube, Cylinder, Cone, Sphere };
    // Loads /Game/Models/<Name>; if missing, returns the engine basic shape and a scale that yields SizeCm.
    UStaticMesh* LoadMeshOrFallback(FName Name, EFallback Shape, FVector SizeCm, FVector& OutScale);
    UMaterialInterface* Palette(FName Name);                 // /Game/Materials/MI_<Name>
}
```
- `Tick` subsystému volá `Game.Tick(min(DeltaTime, 1.0))` a výsledok pošle do `Dispatch`.
- `ADIAGroundActor`: plocha 3200 × 3200 cm v Z = 0 s kolíziou; dve `UTexture2D::CreateTransient` textúry (640² pokrytie z `GetTexels()`, 2048² obrysy: texel je biely, ak jeho stred patrí bunke mapy a `EdgeDistanceCm < 1.5`); pri `OnStateChanged` nahrá výrez z `ConsumeDirtyRect`. Okolo plochy ďalšia veľká plocha hliny bez textúr až po 60 m.
- `ADIAGameMode::StartPlay` spawne `ADIAGroundActor`. Ďalší aktéri pribúdajú v neskorších úlohách.
- Cheaty (`FAutoConsoleCommandWithWorldAndArgs`, zakompilované len mimo Shipping): `dia.Give <money> <shards>`, `dia.Complete <n>` (prvých n nedokončených buniek podľa indexu), `dia.CompleteRoad <i>`, `dia.SetLevels <backpack> <tool> <plant>`, `dia.Special <Sprint|TwoHands|PortableStation|AutoPaver|PaverCar>`, `dia.Teleport <x> <y> [yaw]` (presunie hráča a natočí ho aj kameru; predvolený yaw 0), `dia.TopView` (kamera 45 m nad stredom, pohľad kolmo dole), `dia.ShotAndQuit <seconds> <name>` (po čase uloží `Saved/Screenshots/<name>.png` a ukončí hru).
- `Scripts/shot.sh <name> ["cheat; cheat"]` spustí `UnrealEditor "$UPROJECT" L_Main -game -windowed -ResX=1280 -ResY=720 -ExecCmds="<cheaty>; dia.ShotAndQuit $SHOT_DELAY <name>"` (`SHOT_DELAY` predvolene 4) a skončí kódom 1, ak PNG nevznikol.

- [ ] **Step 1: Test** `DoItAsphalt.Game.WorldBuilds`: vytvor herný svet s `ADIAGameMode` a spusti v ňom hru; over, že existuje práve jeden `ADIAGroundActor`, `GetSubsystem<UDIAWorldSubsystem>()` nie je null a `GetGame().GetMap().Num() == 499`.
- [ ] **Step 2: Test** `DoItAsphalt.Game.SubsystemDispatch`: zaregistruj lambdy na delegáty; `Dispatch(GetGame().DebugCompleteCell(469..473))` → `OnCellsCompleted` 5×, `OnRoadCompleted` raz s hodnotou 0, `OnStateChanged` aspoň 5×.
- [ ] **Step 3:** Zlyhá → implementuj → `Scripts/test.sh` prejde celé.
- [ ] **Step 4:** Run `Scripts/shot.sh ground_empty "dia.TopView"` → Expected na obrázku: hnedá zem, biely obrys veľkého šesťuholníka z malých buniek, šesť krátkych výbežkov ciest, žiadny asfalt.
- [ ] **Step 5:** Run `Scripts/shot.sh ground_150 "dia.TopView; dia.Complete 150"` → Expected: stred parkoviska súvisle tmavý približne do 7. kruhu, okraj asfaltu hladký, obrysy pod asfaltom nevidno.
- [ ] **Step 6:** Commit `feat(game): world subsystem, ground rendering, cheats and screenshots`.

### Task 10 [Sonnet 5.5]: Postava, liatie a výrobňa

**Files:**
- Create: `Source/DoItAsphalt/Player/DIACharacter.{h,cpp}`, `Player/DIAPlayerController.{h,cpp}`, `Player/DIAInput.{h,cpp}`, `World/DIAPlantActor.{h,cpp}`
- Modify: `Game/DIAGameMode.cpp` (pawn, controller, spawn výrobne)
- Test: `Source/DoItAsphalt/Tests/CharacterTest.cpp`

**Interfaces:**
- Consumes: `UDIAWorldSubsystem`, `DIAAssets`, `AsphaltBalance`, `AsphaltLayout`.
- Produces:
```cpp
struct FDIAInput {                       // created with NewObject at controller start, no assets
    UInputMappingContext* Context;
    UInputAction *Move, *Look, *Pour, *Sprint, *Interact, *Shop, *Pause;
    static FDIAInput Create(UObject* Outer);   // WASD, mouse XY, LMB, LeftShift, E, Tab, Escape
};
UCLASS() class ADIACharacter : public ACharacter {
public:
    void SetPouring(bool bPour);   bool IsPouring() const;
    void SetSprinting(bool bSprint);
    FVector2D GetNozzlePoint() const;        // actor XY + forward * NozzleAheadCm
    void RefreshEquipment();                 // re-reads levels and specials, swaps meshes
};
```
- Pohyb a kamera podľa `$UE_ROOT/Templates/TP_ThirdPerson/Source/TP_ThirdPerson/TP_ThirdPersonCharacter.cpp` (spring arm 400 cm, postava sa otáča v smere pohybu). Mesh `SKM_Manny_Simple`, anim trieda `ABP_Unarmed_C`.
- Rýchlosť: `WalkSpeedCmS`; pri liatí `PourSpeedCmS`. Počas liatia je postava natočená v smere kamery.
- Liatie v `Tick`: pri začatí si zapamätá `GetNozzlePoint()`; každý snímok volá `Subsystem->Pour(Prev, Curr, false)` a posunie `Prev`.
- Odber v `Tick`: ak je vzdialenosť od `AsphaltLayout::RefillCenter` ≤ `RefillRadiusCm`, volá `GetGame().RefillBackpack(Dt)` a `OnStateChanged`.
- Výstroj: asfaltovačka `SM_Spreader_<020|030|050|080|100|150|200>` pripojená na koreň v `(NozzleAheadCm, 0, -polvýška kapsuly)`, náhrada kocka `8 × W × 8 cm`; batoh `SM_Backpack_<S|M|L|XL>` podľa `BackpackTier` na kosti `spine_03`, náhrada kocka v rozmeroch z `03-3d-modely.md`; prilba `SM_Helmet` na kosti `head`.
- `ADIAPlantActor` v `AsphaltLayout::PlantPos`: `SM_Plant`, náhrada kocka 600 × 400 × 500 cm; kolízia blokuje; decal alebo plochý valec vyznačuje odbernú zónu.

- [ ] **Step 1: Test** `DoItAsphalt.Game.CharacterPours`: vo svete s `ADIAGameMode` spawni `ADIACharacter` nad bunkou 0, `GetGame().Tick(600)`, `RefillBackpack(10)` (batoh 5 l); `SetPouring(true)`, 20× posuň postavu o 5 cm v +X a tikni svet → `GetMoney() > 0`, `GetBackpackLitres() < 5`.
- [ ] **Step 2: Test** `DoItAsphalt.Game.CharacterRefillsInZone`: zásobník 60 l, postava v `RefillCenter`, 2 s tikania → batoh 5 l; postava 300 cm od stredu → batoh sa neplní.
- [ ] **Step 3:** Zlyhá → implementuj → prejde.
- [ ] **Step 4:** Run `Scripts/shot.sh character "dia.Teleport -1200 0 180"` → Expected: Mannequin zozadu, na chrbte kváder batohu, pred ním na zemi lišta, v pozadí kváder výrobne, pod nohami hlina s obrysmi buniek.
- [ ] **Step 5:** Ručná skúška cez `Scripts/run.sh` nie je potrebná pre commit; ak ju vykonávateľ robí, kontroluje WASD, myš a ľavé tlačidlo.
- [ ] **Step 6:** Commit `feat(game): third-person character, pouring and plant`.

### Task 11 [Sonnet 5.5]: HUD

**Files:**
- Create: `Source/DoItAsphalt/UI/DIAHUD.{h,cpp}`, `UI/SDIAHudOverlay.{h,cpp}`, `UI/DIAFormat.{h,cpp}`
- Modify: `Game/DIAGameMode.cpp` (`HUDClass`)
- Test: `Source/DoItAsphalt/Tests/FormatTest.cpp`

**Interfaces:**
- Produces:
```cpp
namespace DIAFormat {
    FText Money(double Value);                       // 1234.7 → "1,234"
    FText Litres(double Have, double Capacity);      // 3.24, 5 → "3.2 / 5 L"
    FText Progress(int32 Done, int32 Total);         // 137, 499 → "137 / 499 · 27%"
    FText Price(double Price, bool bShards);         // 25,false → "25"; 4,true → "4 shards"; 1,true → "1 shard"; -1 → "MAX"
}
UCLASS() class ADIAHUD : public AHUD {
public:
    void SetPrompt(const FText& Text);               // empty text hides the prompt
    void ShowToast(const FText& Text);               // fades after 2 s
};
```
- `SDIAHudOverlay`: rozloženie podľa GDD kap. 11; číta zo subsystému pri `OnStateChanged`. Zásobník výrobne sa zobrazí, len keď je hráč do 600 cm od odberného miesta. Pri `OnCellsCompleted` toast `+N shard`. Keď `!FApp::CanEverRender()`, HUD widgety nevytvára.

- [ ] **Step 1: Testy** `DoItAsphalt.Game.Format`: všetky príklady z komentárov vyššie plus `Litres(200, 200)` → `"200 / 200 L"`, `Progress(0, 499)` → `"0 / 499 · 0%"`, `Progress(499, 499)` → `"499 / 499 · 100%"`.
- [ ] **Step 2:** Zlyhá → implementuj → prejde.
- [ ] **Step 3:** Run `Scripts/shot.sh hud "dia.Give 1234 17; dia.Complete 137"` → Expected: vľavo hore `1,234` a `17`, vpravo hore `137 / 499 · 27%`, dole v strede ukazovateľ batohu `0 / 5 L`, text čitateľný, nič sa neprekrýva.
- [ ] **Step 4:** Commit `feat(ui): HUD overlay`.

---

# Fáza 3 – Vylepšenia

### Task 12 [Sonnet 5.5]: Obchod a viditeľné úrovne

**Files:**
- Create: `Source/DoItAsphalt/UI/SDIAShopPanel.{h,cpp}`
- Modify: `UI/DIAHUD.{h,cpp}` (`ToggleShop()`), `Player/DIAPlayerController.cpp` (Tab; kurzor a vstup pre UI, hra beží ďalej), `World/DIAPlantActor.cpp` (prístavky), `Game/DIACheats.cpp` (`dia.Shop` otvorí obchod)
- Test: `Source/DoItAsphalt/Tests/ShopTest.cpp`

**Interfaces:**
- Consumes: `UDIAWorldSubsystem::Buy`, `DIAFormat`, `ADIACharacter::RefreshEquipment`.
- Produces: `void ADIAHUD::ToggleShop(); bool ADIAHUD::IsShopOpen() const;`
- Panel má tri riadky v poradí Backpack, Sprayer, Plant: názov, `aktuálna hodnota → ďalšia hodnota` (`5 L → 7 L`, `0.2 m → 0.3 m`, `15 L/min → 20 L/min`), cena cez `DIAFormat::Price`, tlačidlo `BUY` neaktívne pri `!CanBuy`. Na maxime namiesto šípky len hodnota a `MAX`.
- `UDIAWorldSubsystem::Buy` po úspechu pošle `OnStateChanged`; postava naň volá `RefreshEquipment()`; výrobňa zobrazí `SM_Plant_AddonA` od úrovne 5 v lokálnej polohe `(-150, 280, 0)` a `SM_Plant_AddonB` od úrovne 10 v `(-150, -290, 0)`.

- [ ] **Step 1: Test** `DoItAsphalt.Game.BuyRefreshesEquipment`: `DebugGive(1000, 100)`; `Buy(Tool)` 3× → komponent asfaltovačky má mesh (alebo náhradnú mierku) pre 80 cm; `Buy(Backpack)` 3× → tier 1.
- [ ] **Step 2:** Zlyhá → implementuj → prejde.
- [ ] **Step 3:** Run `Scripts/shot.sh shop "dia.Give 30 3; dia.Shop"` → Expected: panel s tromi riadkami; Backpack `5 L → 7 L`, cena `1 shard`, tlačidlo aktívne; Sprayer cena `25` aktívne; Plant cena `15` aktívne.
- [ ] **Step 4:** Run `Scripts/shot.sh shop_max "dia.SetLevels 12 6 12; dia.Shop"` → Expected: všetky tri riadky `MAX`, tlačidlá neaktívne; pri výrobni v pozadí dva prístavky.
- [ ] **Step 5:** Commit `feat(ui): shop panel and visible upgrade levels`.

### Task 13 [Sonnet 5.5]: Kiosky, cesty, Šprint a Dve ruky

**Files:**
- Create: `Source/DoItAsphalt/World/DIAInteractable.h`, `World/DIAKioskActor.{h,cpp}`
- Modify: `Player/DIAPlayerController.cpp` (hľadanie interakcie, kláves E), `Player/DIACharacter.cpp` (šprint, druhá asfaltovačka), `Game/DIAGameMode.cpp` (kiosky, kusy ciest, zábrany)
- Test: `Source/DoItAsphalt/Tests/KioskTest.cpp`

**Interfaces:**
- Produces:
```cpp
UINTERFACE() class UDIAInteractable : public UInterface { GENERATED_BODY() };
class IDIAInteractable { GENERATED_BODY() public:
    virtual FText GetPrompt(const ADIACharacter* By) const = 0;   // empty text = not interactable now
    virtual void Interact(ADIACharacter* By) = 0;
};
UCLASS() class ADIAKioskActor : public AActor, public IDIAInteractable { public: EAsphaltSpecial Special; };
```
- Controller každý snímok nájde najbližšieho `IDIAInteractable`, ktorého kolízna obálka je do 150 cm od postavy a ktorý vráti neprázdny `GetPrompt`; ten pošle do `ADIAHUD::SetPrompt`, kláves E volá `Interact`.
- Kiosk stojí v `AsphaltLayout::KioskPos(cesta)` otočený k ceste. Výzvy: cesta nedokončená → `Finish the road (3 / 5)`; dokončená a nekúpené → `E – Buy Sprint (100)`, pri nedostatku peňazí `Sprint – need 100`; kúpené → `Sprint – owned`. Názvy: `Sprint`, `Two Hands`, `Portable Station`, `Auto Paver`, `Paver Truck`. Zamknutý kiosk zobrazuje `SM_Lock`.
- Za koncom každej cesty `SM_RoadStub` (náhrada plochá kocka 1200 × 340 × 5) od `RoadEnd` smerom von až po plot; na cestách 0 – 4 `SM_Barrier` v `GatePos`, blokuje.
- Šprint: len s `HasSpecial(Sprint)`, pri držaní Shift a keď postava neleje: rýchlosť `WalkSpeedCmS * SprintMultiplier`.
- Dve ruky: s `HasSpecial(TwoHands)` dve asfaltovačky v `Y = ±W/2` (W je šírka jednej); liatie používa `GetToolWidthCm()`, ktorá je už dvojnásobná.

- [ ] **Step 1: Test** `DoItAsphalt.Game.KioskFlow`: kiosk Sprint: výzva obsahuje `3 / 5` po troch bunkách cesty 3; po piatich a `DebugGive(100, 0)` `Interact` → `HasSpecial(Sprint)`, `OnSpecialBought` zavolaný raz; ďalšia `Interact` nič nezmení.
- [ ] **Step 2: Test** `DoItAsphalt.Game.SprintSpeed`: bez špeciálu `SetSprinting(true)` → `MaxWalkSpeed == 450`; so špeciálom → `720`; so špeciálom a `SetPouring(true)` → `200`.
- [ ] **Step 3:** Zlyhá → implementuj → prejde.
- [ ] **Step 4:** Run `Scripts/shot.sh kiosk "dia.CompleteRoad 3; dia.Give 100 0; dia.Teleport -1240 646 233"` → Expected: postava pri kiosku na konci vyasfaltovanej cesty, dole výzva `E – Buy Sprint (100)`, vedľa kus starej cesty vedúci k plotu.
- [ ] **Step 5:** Run `Scripts/shot.sh twohands "dia.Special TwoHands; dia.SetLevels 0 4 0; dia.Teleport 0 0 0"` → Expected: pred postavou dve lišty vedľa seba, spolu široké asi 2 m.
- [ ] **Step 6:** Commit `feat(game): kiosks, road unlocks, sprint and two hands`.

---

# Fáza 4 – Špeciály a výhra

### Task 14 [Sonnet 5.5]: Prenosné odberné miesto

**Files:**
- Create: `Source/DoItAsphalt/World/DIAPortableStation.{h,cpp}`
- Modify: `Game/DIAGameMode.cpp` (spawn pri `OnSpecialBought` a pri štarte, ak ho hráč už má), `Player/DIACharacter.cpp` (druhá odberná zóna)
- Test: `Source/DoItAsphalt/Tests/StationTest.cpp`

**Interfaces:**
- Consumes: `IDIAInteractable`, `FAsphaltWorldState::StationPos / bStationCarried`.
- Produces: `class ADIAPortableStation : public AActor, public IDIAInteractable`.
- `Interact`: ak stojí, hráč ju zdvihne (`bStationCarried = true`, aktér pripojený 100 cm za postavou, výzva `E – Put down`); ak ju nesie, položí ju 150 cm pred seba, len ak tam nestojí iný blokujúci aktér (inak toast `No room here`). Poloha sa zapisuje do `World().StationPos`.
- Postava sa plní aj do `RefillRadiusCm` od položenej stanice. Nesená stanica neplní. Mesh `SM_PortableStation`, náhrada kocka 120 × 80 × 130.

- [ ] **Step 1: Test** `DoItAsphalt.Game.StationRefills`: `DebugGrantSpecial(PortableStation)`, zásobník 60 l, stanica položená v `(500, 0)`, postava v `(550, 0)`, 2 s → batoh 5 l; počas nesenia sa neplní; po položení `World().StationPos` zodpovedá polohe aktéra.
- [ ] **Step 2:** Zlyhá → implementuj → prejde.
- [ ] **Step 3:** Run `Scripts/shot.sh station "dia.Special PortableStation; dia.Teleport -150 817 0"` → Expected: pred postavou kváder stanice na okraji parkoviska pri ceste 4, výzva `E – Pick up`.
- [ ] **Step 4:** Commit `feat(game): portable refill station`.

### Task 15 [Sonnet 5.5]: Automatizovaný asfaltovač

**Files:**
- Create: `Source/DoItAsphalt/World/DIAAutoPaver.{h,cpp}`
- Modify: `Game/DIAGameMode.cpp` (spawn)
- Test: `Source/DoItAsphalt/Tests/AutoPaverTest.cpp`

**Interfaces:**
- Consumes: `FAsphaltGame::GetAutoPaverPos`, `GetAutoPaverTarget`.
- Produces: `class ADIAAutoPaver : public AActor` – každý snímok sa presunie na `GetAutoPaverPos()`, natočí v smere pohybu; bez kolízie s hráčom. Mesh `SM_AutoPaver`, náhrada kocka 100 × 80 × 60.

- [ ] **Step 1: Test** `DoItAsphalt.Game.AutoPaverActorFollowsCore`: po `BuySpecial(AutoPaver)` (cesta 1 dokončená, peniaze 450) existuje práve jeden `ADIAAutoPaver`; po 10 s tikania je jeho XY do 1 cm od `GetAutoPaverPos()` a `GetTotalLitresPoured()` vzrástlo.
- [ ] **Step 2:** Zlyhá → implementuj → prejde.
- [ ] **Step 3:** Run `SHOT_DELAY=20 Scripts/shot.sh autopaver "dia.Special AutoPaver; dia.TopView"` → Expected: na okraji parkoviska pri ceste 1 malý kváder robota a pod ním čiastočne zaliata bunka.
- [ ] **Step 4:** Commit `feat(game): auto paver actor`.

### Task 16 [Sonnet 5.5]: Asfaltovacie auto

**Files:**
- Create: `Source/DoItAsphalt/World/DIAPaverCar.{h,cpp}`
- Modify: `Player/DIAPlayerController.cpp` (nastúpenie, vystúpenie, vstup pre auto), `UI/SDIAHudOverlay.cpp` (v aute ukazuje nádrž auta), `Game/DIAGameMode.cpp` (spawn)
- Test: `Source/DoItAsphalt/Tests/CarTest.cpp`

**Interfaces:**
- Consumes: `FAsphaltGame::PourFromCar`, `RefillCar`, `GetCarLitres`, `FAsphaltWorldState::CarPos / CarYawDeg / bPlayerInCar`.
- Produces:
```cpp
UCLASS() class ADIAPaverCar : public APawn, public IDIAInteractable {
public:
    void SetThrottle(float Value);  void SetSteer(float Value);  void SetPouring(bool bPour);
    FVector2D GetBarPoint() const;      // actor XY - forward * CarBarBehindCm
};
```
- Kinematický pohyb so sweepom: dopredu `CarSpeedCmS`, dozadu polovica, zatáčanie 90°/s násobené podielom aktuálnej rýchlosti. Kolízna kocka 460 × 220 × 240. Kamera na ramene 900 cm.
- `Interact` (výzva `E – Get in`): controller posadne auto, postava sa skryje a vypne kolízie. V aute kláves E: auto zastaví, postava sa objaví 200 cm vľavo od auta, controller posadne postavu.
- Liatie rovnako ako postava, bod `GetBarPoint()`, `Subsystem->Pour(Prev, Curr, true)`. Plnenie: stred auta do `RefillRadiusCm + 200` od výrobne alebo položenej stanice → `RefillCar(Dt)`.
- Mesh `SM_PaverCar`, náhrada kocka 460 × 220 × 240 a za ňou plochá kocka 10 × 300 × 10.

- [ ] **Step 1: Test** `DoItAsphalt.Game.CarPoursWide`: `DebugGrantSpecial(PaverCar)`, zásobník 500 l, auto pri výrobni 3 s → `GetCarLitres() == 500`; auto presunuté na stred, `SetPouring(true)`, `SetThrottle(1)`, 2 s → `GetTotalLitresPoured() > 100`, `World().CarPos` zodpovedá aktérovi.
- [ ] **Step 2: Test** `DoItAsphalt.Game.CarEnterExit`: `Interact` → `GetPawn()` je auto, `bPlayerInCar`; E → `GetPawn()` je postava, stojí do 250 cm od auta a neprekrýva sa s ním.
- [ ] **Step 3:** Zlyhá → implementuj → prejde.
- [ ] **Step 4:** Run `Scripts/shot.sh car "dia.Special PaverCar; dia.Teleport 620 -620 68"` → Expected: veľký kváder auta s lištou vzadu na okraji parkoviska pri ceste 0, výzva `E – Get in`.
- [ ] **Step 5:** Commit `feat(game): drivable paver truck`.

### Task 17 [Sonnet 5.5]: Plot, brána a výhra

**Files:**
- Create: `Source/DoItAsphalt/World/DIAExitGate.{h,cpp}`, `UI/SDIAWinScreen.{h,cpp}`
- Modify: `Game/DIAGameMode.cpp` (brána, plot, hranica), `UI/DIAHUD.{h,cpp}` (`ShowWinScreen()`), `Game/DIAWorldSubsystem.{h,cpp}` (`void Win()`, `OnWon`)
- Test: `Source/DoItAsphalt/Tests/ExitTest.cpp`

**Interfaces:**
- Produces: `void UDIAWorldSubsystem::Win();` (volá `SetWon()`, pri úspechu pošle `FSimpleMulticastDelegate OnWon`), `void ADIAHUD::ShowWinScreen();`
- Hranica areálu: neviditeľná blokujúca stena po kruhu `SiteRadiusCm` (72 segmentov) s medzerou 380 cm v smere každej zo 6 ciest; `SM_Fence` diely po tom istom kruhu, náhrada plochá kocka 8 × 200 × 180. Medzery ciest 0 – 4 uzatvárajú zábrany z Task 13, medzeru cesty 5 brána.
- Brána v `AsphaltLayout::GatePos(5)`, kolmo na cestu: `SM_ExitGate_Frame` + `SM_ExitGate_Barrier`; zatvorené rameno blokuje. Pri `OnExitOpened` sa rameno za 1,5 s zdvihne o 80° a prestane blokovať; toast `The exit is open!`. Ak je hra načítaná s `IsExitOpen()`, brána je otvorená hneď.
- Spúšťač: guľa s polomerom 150 cm v `AsphaltLayout::ExitTrigger()`; prekrytie postavou alebo autom → `Win()`.
- `SDIAWinScreen`: nadpis `Parking lot complete!`, riadky `Time`, `Asphalt poured`, `Money earned`, `Upgrades bought` (súčet troch úrovní + počet špeciálov, z 35), tlačidlá `New Game` a `Quit`. `New Game` zatiaľ znovu načíta level; Task 18 ho napojí na `NewGame()`.

- [ ] **Step 1: Test** `DoItAsphalt.Game.GateOpensAndWins`: rameno blokuje; `Win()` pred dokončením nič neurobí; po dokončení všetkých 499 buniek cez `Dispatch` rameno po 2 s neblokuje; postava presunutá do `ExitTrigger()` → `HasWon()`, `OnWon` raz.
- [ ] **Step 2: Test** `DoItAsphalt.Game.SiteIsClosed`: z bodu `(0,0)` vedú sweepy kapsuly postavy v 72 smeroch po 5° do vzdialenosti 4000 cm; každý narazí najďalej vo vzdialenosti 2700 cm (šesť smerov ciest na zábranu alebo zatvorené rameno, ostatné na stenu alebo iný objekt). Po otvorení brány sweep v smere `RoadDirection(5)` nenarazí, ostatných 71 stále áno.
- [ ] **Step 3:** Zlyhá → implementuj → prejde.
- [ ] **Step 4:** Run `Scripts/shot.sh gate_open "dia.Complete 499; dia.Teleport 2035 1175 30"` → Expected: pred postavou rám brány v línii plota so zdvihnutým ramenom, pod nohami stará cesta, vpravo hore `499 / 499 · 100%`.
- [ ] **Step 5:** Run `Scripts/shot.sh win "dia.Complete 499; dia.Teleport 2382 1375 30"` → Expected: obrazovka výhry so štyrmi riadkami štatistík a dvoma tlačidlami.
- [ ] **Step 6:** Commit `feat(game): site fence, exit gate and win screen`.

---

# Fáza 5 – Ukladanie a menu

### Task 18 [Sonnet 5.5]: Uloženie, načítanie, hlavné menu a pauza

**Files:**
- Create: `Source/DoItAsphalt/Game/DIASaveGame.h`, `UI/SDIAMenu.{h,cpp}`
- Modify: `Game/DIAWorldSubsystem.{h,cpp}`, `Game/DIAGameMode.cpp`, `UI/DIAHUD.{h,cpp}`, `Player/DIAPlayerController.cpp` (Esc), `UI/SDIAWinScreen.cpp`, `Game/DIACheats.cpp` (`dia.SkipMenu`)
- Modify: `Scripts/shot.sh` (pred cheaty vloží `dia.SkipMenu` a použije slot `DIA_Shot`, ktorý pred spustením zmaže; s prepínačom `--menu` ako prvým argumentom `dia.SkipMenu` nevloží)
- Test: `Source/DoItAsphalt/Tests/SaveSlotTest.cpp`

**Interfaces:**
- Consumes: `FAsphaltGame::SaveToBytes / LoadFromBytes`.
- Produces:
```cpp
UCLASS() class UDIASaveGame : public USaveGame { GENERATED_BODY() public: UPROPERTY() TArray<uint8> Blob; };
// UDIAWorldSubsystem:
    static FString SlotName();            // "DoItAsphalt_Slot0" unless overridden
    static void SetSlotNameOverride(const FString& Name);   // tests use "DIA_Test"; also set by the -DIASlot=<name> command line switch
    bool HasSave() const;                 // slot exists AND its blob loads into a scratch FAsphaltGame
    bool LoadGame();                      // false leaves the current game untouched
    void SaveGame();
    void NewGame();                       // resets the core, deletes the slot, respawns actors at layout positions
    FSimpleMulticastDelegate OnGameReset; // fired by LoadGame and NewGame; actors re-read positions and equipment
```
- Automatické uloženie: každých 30 s herného času, po každom úspešnom `Buy` / `BuySpecial`, pri `Win()` a pri `EndPlay`. Pred uložením subsystém zapíše do `World()` polohu hráča (ak je v aute, polohu auta).
- `SDIAMenu`: režim hlavného menu (`Continue` len pri `HasSave()`, `New Game`, `Quit`) a režim pauzy (`Resume`, `New Game`, `Quit`). Hra začína v hlavnom menu, pozastavená. `New Game` v pauze sa pýta `Start over? Progress will be lost.` s `Yes` / `No`.
- Po `LoadGame` / `NewGame`: postava, stanica, auto a robot sa presunú podľa `World()`, zem nahrá celú textúru pokrytia, brána a kiosky sa prekreslia.

- [ ] **Step 1: Test** `DoItAsphalt.Game.SaveSlotRoundTrip` (slot `DIA_Test`, na začiatku aj konci testu zmazaný): zahraj (peniaze, 40 buniek, 2 nákupy, postava presunutá do `(300, 200)`), `SaveGame()`; potom zmeň stav (`DebugGive(999, 0)`, postava do `(0, 0)`); `LoadGame()` → `true`, peniaze, počet dokončených buniek a úrovne sú ako pri uložení, postava stojí do 1 cm od `(300, 200)`, `OnGameReset` zavolaný raz.
- [ ] **Step 2: Test** `DoItAsphalt.Game.CorruptSlotStartsNewGame`: do slotu ulož `UDIASaveGame` s blobom 1000 bajtov `0xAB` → `HasSave() == false`, `LoadGame() == false`, hra je v stave novej hry a nespadla; slot bez súboru → to isté.
- [ ] **Step 3: Test** `DoItAsphalt.Game.NewGameResetsWorld`: po 100 bunkách a kúpe auta `NewGame()` → `NumCompleteCells() == 0`, žiadny `ADIAPaverCar` vo svete, postava v `PlayerStart`, slot neexistuje.
- [ ] **Step 4:** Zlyhá → implementuj → prejde.
- [ ] **Step 5:** Run `Scripts/shot.sh --menu menu` → Expected: hlavné menu, bez uloženej hry len `New Game` a `Quit`.
- [ ] **Step 6:** Over, že staršie snímky stále fungujú: `Scripts/shot.sh hud "dia.Give 1234 17; dia.Complete 137"` → rovnaký výsledok ako v Task 11.
- [ ] **Step 7:** Commit `feat(game): save slot, autosave, main menu and pause`.

---

# Vetva modelov

### Task M1 [Fable 5.2]: Reťazec na tvorbu modelov

**Predpoklad:** `blender --version` funguje. Ak nie, zastav a požiadaj používateľa o `brew install --cask blender`.

**Files:**
- Create: `Tools/ModelGen/common.py`, `Tools/ModelGen/manifest.json`, `Tools/ModelGen/check.py`, `Tools/ModelGen/build_all.sh`
- Create: `Tools/ModelGen/axis_test.py`, `Tools/ModelGen/cone.py`
- Generated: `SourceAssets/Models/SM_AxisTest.glb`, `SM_Cone.glb`, `SourceAssets/Previews/*.png`

**Interfaces:**
- Produces `common.py` (používajú všetky modelové skripty):
```python
PALETTE: dict[str, tuple]            # 12 materials from docs/03-3d-modely.md §3: name → (hex, metallic, roughness)
def reset_scene() -> None
def material(name: str)              # palette material, created once per scene
def box(size_xyz, center_xyz, mat) ; def cylinder(radius, height, center_xyz, axis, mat, segments=12)
def cone(r_bottom, r_top, height, center_xyz, axis, mat, segments=12)
def join(objects, name) -> object    # single mesh, transforms applied, flat shading
def export(obj, name: str) -> str    # game axes (+X fwd, +Y right, +Z up, metres) → SourceAssets/Models/<name>.glb
def render_previews(obj, name: str)  # 3 PNGs 800x600 with a 1.8 m grey silhouette for scale
```
- Produces `manifest.json`: záznam na model `{"name", "min": [x,y,z], "max": [x,y,z], "max_tris", "tolerance": 0.02}` v herných osiach a metroch.
- Produces `check.py [Názov …]`: bez Blenderu načíta GLB, prevedie obálku späť do herných osí, overí obálku, počet trojuholníkov, že všetky materiály sú z palety a najviac 5; vypíše `OK <Názov>` alebo `FAIL <Názov>: <dôvod>`, pri zlyhaní kód 1. Bez argumentov skontroluje celý manifest a zlyhá aj na chýbajúcom súbore.

- [ ] **Step 1:** Zapíš do manifestu `SM_AxisTest` (min `[0,0,0]`, max `[1.0,0.5,0.25]`, 12 tri) a `SM_Cone` (min `[-0.15,-0.15,0]`, max `[0.15,0.15,0.5]`, 150 tri).
- [ ] **Step 2:** Napíš `check.py`. Run `python3 Tools/ModelGen/check.py` → Expected: `FAIL SM_AxisTest: file missing`, kód 1.
- [ ] **Step 3:** Napíš `common.py`, `axis_test.py`, `cone.py`. Run `blender -b -P Tools/ModelGen/axis_test.py` a `cone.py`.
- [ ] **Step 4:** Run `python3 Tools/ModelGen/check.py` → Expected: `OK SM_AxisTest`, `OK SM_Cone`.
- [ ] **Step 5:** Otvor `SourceAssets/Previews/SM_Cone_a.png`, `_b`, `_c` → Expected: oranžový kužeľ s bielym pruhom a štvorcovou pätkou, siluete postavy siaha pod kolená, žiadne čierne plochy.
- [ ] **Step 6:** `build_all.sh` spustí každý `Tools/ModelGen/*.py` okrem `common.py` a `check.py`, potom `check.py`. Run → Expected: dva riadky `OK`, kód 0.
- [ ] **Step 7:** Commit `feat(models): Blender generation pipeline, axis test and cone`.

### Task M2 [Sonnet 5.5]: Import modelov do Unrealu

**Files:**
- Create: `Tools/UE/import_models.py`
- Test: `Source/DoItAsphalt/Tests/AssetsTest.cpp`

**Interfaces:**
- Consumes: `SourceAssets/Models/*.glb` (M1), `MI_DIA_*` (Task 8), `DIAAssets::LoadMeshOrFallback` (Task 9).
- Produces: pre každý `SourceAssets/Models/<Name>.glb` statický mesh `/Game/Models/<Name>`; každý slot materiálu priradený na `/Game/Materials/MI_<názov slotu>`; jednoduchá konvexná kolízia; žiadne ďalšie assety v `/Game/Models` (materiály a textúry, ktoré Interchange vytvorí, skript zmaže). Za každý mesh riadok `DIA_OK /Game/Models/<Name> <sizeX> <sizeY> <sizeZ>` v cm.

- [ ] **Step 1: Test** `DoItAsphalt.Game.AxisTestImported`: `LoadMeshOrFallback("SM_AxisTest", …)` vráti mesh z `/Game/Models`; jeho obálka má veľkosť `(100, 50, 25)` ± 1 cm a stred `(50, 25, 12.5)` ± 1 cm. Stred kladný vo všetkých osiach dokazuje správne osi aj ručnosť.
- [ ] **Step 2:** Run `Scripts/test.sh DoItAsphalt.Game.AxisTestImported` → Expected: FAILED 1.
- [ ] **Step 3:** Napíš `import_models.py` (Interchange glTF; API over v `$UE_ROOT/Engine/Plugins/Interchange`). Run `Scripts/uepy.sh Tools/UE/import_models.py` → Expected: `DIA_OK /Game/Models/SM_AxisTest 100 50 25` a `DIA_OK /Game/Models/SM_Cone 30 30 50` (± 1).
- [ ] **Step 4:** Ak sú osi alebo znamienka inak, oprav prevod v importe (nie v `common.py`, pokiaľ `check.py` hlási `OK`) a spusti znova. Run test → Expected: PASSED.
- [ ] **Step 5:** Commit `feat(tools): glb import into Unreal with palette materials`.

### Task M3 [Fable 5.2]: Výstroj hráča

**Files:**
- Create: `Tools/ModelGen/spreader.py`, `Tools/ModelGen/backpack.py`, `Tools/ModelGen/helmet.py`
- Modify: `Tools/ModelGen/manifest.json`
- Generated: `SM_Spreader_020/030/050/080/100/150/200.glb`, `SM_Backpack_S/M/L/XL.glb`, `SM_Helmet.glb` + náhľady

**Interfaces:**
- Consumes: `common.py`, postup a kontrolný zoznam z `docs/03-3d-modely.md` kap. 1 a 5, rozmery z kap. 4.1.
- Produces: 12 GLB. Asfaltovačka: obálka X `[-0.50, 0.08]`, Y `[-(W/2 + 0.05), W/2 + 0.05]`, Z `[0, 1.0]`. Batohy: X `[-hĺbka, 0]`, Y a Z symetricky okolo počiatku. Prilba: Z `[0, 0.16]`.

- [ ] **Step 1:** Doplň 12 záznamov do manifestu. Run `python3 Tools/ModelGen/check.py` → Expected: 12× `FAIL … file missing`.
- [ ] **Step 2:** `spreader.py` – jedna funkcia `build(width_m)` volaná pre 7 šírok. Spusti, `check.py` → 7× `OK`.
- [ ] **Step 3:** Pozri náhľady `SM_Spreader_020_a` a `SM_Spreader_200_a` → Expected: v oboch rozpoznateľná lišta s kolieskami a rukoväťou s madlom vo výške pása siluety; 2 m verzia má stredové koliesko a rukoväť nie je zdeformovaná šírkou.
- [ ] **Step 4:** `backpack.py` – štyri veľkosti; spusti, `check.py` → 4× `OK`. Pozri `SM_Backpack_S_a` a `SM_Backpack_XL_a` → Expected: nádrž s popruhmi a priezorom; XL zreteľne mohutnejší, dve nádoby v ráme.
- [ ] **Step 5:** `helmet.py`; `check.py` → `OK`; náhľad → žltá prilba so šiltom do +X.
- [ ] **Step 6:** Prejdi kontrolný zoznam z kap. 5 pre všetkých 12; čo neprejde, oprav a zopakuj kroky 2 – 5.
- [ ] **Step 7:** Commit `feat(models): spreaders, backpacks and helmet`.

### Task M4 [Fable 5.2]: Výrobňa a prenosné miesto

**Files:**
- Create: `Tools/ModelGen/plant.py`, `Tools/ModelGen/portable_station.py`
- Modify: `Tools/ModelGen/manifest.json`
- Generated: `SM_Plant.glb`, `SM_Plant_AddonA.glb`, `SM_Plant_AddonB.glb`, `SM_PortableStation.glb` + náhľady

**Interfaces:**
- Consumes: `common.py`, `docs/03-3d-modely.md` kap. 4.2.
- Produces: `SM_Plant` obálka X `[-3.0, 3.4]`, Y `[-2.0, 2.0]`, Z `[0, 5.0]`; koniec výpustu v `(3.4, 0, 1.8)`. Prístavky a stanica so stredom pôdorysu v počiatku.

- [ ] **Step 1:** Doplň 4 záznamy do manifestu; `check.py` → 4× `FAIL … file missing`.
- [ ] **Step 2:** `plant.py` (telo aj oba prístavky v jednom skripte, tri exporty). Spusti, `check.py` → 3× `OK`.
- [ ] **Step 3:** Pozri `SM_Plant_a`, `_b`, `_c` → Expected: na prvý pohľad priemyselná výrobňa – šikmý bubon, silo, komín, dopravník, búdka; výpust trčí do +X vo výške ramien siluety a je vizuálne zdôraznený (`DIA_Asphalt` ústie, `DIA_Orange` lem).
- [ ] **Step 4:** `portable_station.py`; `check.py` → `OK`; náhľad → nádrž na dvoch kolesách s ojom, siluete po hruď.
- [ ] **Step 5:** Kontrolný zoznam kap. 5 pre všetky 4; oprav, čo neprejde.
- [ ] **Step 6:** Commit `feat(models): asphalt plant, addons and portable station`.

### Task M5 [Fable 5.2]: Cesty, východ a kulisy

**Files:**
- Create: `Tools/ModelGen/kiosk.py`, `Tools/ModelGen/road_props.py`, `Tools/ModelGen/exit_gate.py`, `Tools/ModelGen/site_props.py`
- Modify: `Tools/ModelGen/manifest.json`
- Generated: `SM_Kiosk`, `SM_Lock`, `SM_RoadStub`, `SM_Barrier`, `SM_ExitGate_Frame`, `SM_ExitGate_Barrier`, `SM_Fence`, `SM_Barrel`, `SM_Lamp`, `SM_DirtPile` (`.glb` + náhľady)

**Interfaces:**
- Consumes: `common.py`, `docs/03-3d-modely.md` kap. 4.3 a 4.5.
- Produces: 10 GLB. `SM_RoadStub`: X `[0, 12.0]`. `SM_ExitGate_Barrier`: Y `[0, 3.4]`, pánt v počiatku. `SM_Kiosk`: tabuľa so stredom vo výške 1.9 m, obrazovka do +X.

- [ ] **Step 1:** Doplň 10 záznamov do manifestu; `check.py` → 10× `FAIL … file missing`.
- [ ] **Step 2:** `kiosk.py` (kiosk a zámok); `check.py` → 2× `OK`; náhľad `SM_Kiosk_a` → stĺp so šikmou obrazovkou, zeleným tlačidlom a prázdnou bielou tabuľou nad hlavou siluety.
- [ ] **Step 3:** `road_props.py` (kus cesty, zábrana); `check.py` → 2× `OK`; náhľad `SM_RoadStub_c` (zhora) → pruh asfaltu s prerušovanou stredovou čiarou a obrubníkmi.
- [ ] **Step 4:** `exit_gate.py` (rám, rameno); `check.py` → 2× `OK`; náhľad `SM_ExitGate_Frame_a` → silueta prejde pod nosníkom s rezervou, tabuľa pre nápis je hore v strede.
- [ ] **Step 5:** `site_props.py` (plot, sud, reflektor, kopa hliny); `check.py` → 4× `OK`.
- [ ] **Step 6:** Kontrolný zoznam kap. 5 pre všetkých 10; oprav, čo neprejde.
- [ ] **Step 7:** Commit `feat(models): kiosk, road props, exit gate and site props`.

### Task M6 [Fable 5.2]: Robot a auto

**Files:**
- Create: `Tools/ModelGen/auto_paver.py`, `Tools/ModelGen/paver_car.py`
- Modify: `Tools/ModelGen/manifest.json`
- Generated: `SM_AutoPaver.glb`, `SM_PaverCar.glb` + náhľady

**Interfaces:**
- Consumes: `common.py`, `docs/03-3d-modely.md` kap. 4.4.
- Produces: `SM_AutoPaver` obálka X `[-0.5, 0.5]`, Y `[-0.4, 0.4]`, Z `[0, 0.6]`, lišta vpredu. `SM_PaverCar` obálka X `[-2.3, 2.3]`, Y `[-1.5, 1.5]`, Z `[0, 2.4]`; lišta široká 3.0 m v X = −2.3.

- [ ] **Step 1:** Doplň 2 záznamy do manifestu; `check.py` → 2× `FAIL … file missing`.
- [ ] **Step 2:** `auto_paver.py`; `check.py` → `OK`; náhľady → nízky pásový robot s lištou vpredu a majákom, siluete pod kolená, pôsobí priateľsky.
- [ ] **Step 3:** `paver_car.py`; `check.py` → `OK`; náhľady `SM_PaverCar_a`, `_b`, `_c` → nákladné auto s kabínou vpredu, valcovou nádržou a širokou lištou vzadu; štyri kolesá stoja na zemi; kabína má okná; silueta vedľa neho siaha po spodok okien.
- [ ] **Step 4:** Kontrolný zoznam kap. 5 pre oba; oprav, čo neprejde.
- [ ] **Step 5:** Run `Tools/ModelGen/build_all.sh` → Expected: 30× `OK`, kód 0.
- [ ] **Step 6:** Commit `feat(models): auto paver and paver truck`.

### Task M7 [Sonnet 5.5]: Modely v hre a kulisy

**Files:**
- Modify: `Game/DIAGameMode.cpp` (rozmiestnenie kulís), `Player/DIACharacter.cpp` (doladenie posunov batohu a prilby), `World/DIAKioskActor.cpp` a `World/DIAExitGate.cpp` (texty na tabuliach cez `UTextRenderComponent`)
- Create: `Source/AsphaltCore/Public/AsphaltDressing.h`, `Source/AsphaltCore/Private/AsphaltDressing.cpp`
- Test: `Source/AsphaltCore/Private/Tests/DressingTest.cpp`, `Source/DoItAsphalt/Tests/AssetsTest.cpp`

**Interfaces:**
- Consumes: všetkých 30 GLB (M1, M3 – M6), `Tools/UE/import_models.py` (M2).
- Produces:
```cpp
enum class EDressingKind : uint8 { Cone, Barrel, Lamp, DirtPile };
struct FDressingItem { EDressingKind Kind; FVector2D Pos; double YawDeg; };
namespace AsphaltDressing { ASPHALTCORE_API TArray<FDressingItem> Build(const FHexMap& Map); }   // deterministic, fixed seed
```
- `Build` vráti 6 reflektorov (pri rohoch parkoviska, 250 cm za okrajom), 8 kôp hliny, 20 sudov a 30 kužeľov rozmiestnených medzi parkoviskom a plotom.

- [ ] **Step 1: Test** `DoItAsphalt.Core.Dressing`: 64 položiek v počtoch vyššie; dve volania dajú rovnaký výsledok; žiadna položka neleží na bunke mapy, do 150 cm od bunky, do 400 cm od `RefillCenter`, `KioskPos`, `GatePos`, `SpecialSpawn`, `PlayerStart`, do 700 cm od `PlantPos`, do 250 cm od osi niektorej cesty medzi jej `RoadEnd` a `GatePos`, ani ďalej než `SiteRadiusCm - 100` od stredu; žiadne dve nie sú bližšie než 120 cm.
- [ ] **Step 2: Test** `DoItAsphalt.Game.AllModelsImported`: pre každý z 30 názvov z `docs/03-3d-modely.md` vráti `LoadMeshOrFallback` mesh z `/Game/Models` (nie náhradu) a jeho obálka sa zhoduje s manifestom ± 3 cm.
- [ ] **Step 3:** Run `Scripts/uepy.sh Tools/UE/import_models.py` → Expected: 30 riadkov `DIA_OK`. Implementuj `AsphaltDressing` a rozmiestnenie. Testy prejdú.
- [ ] **Step 4:** Nápisy: tabuľa kiosku nesie názov špeciálu, tabuľa brány `EXIT`.
- [ ] **Step 5:** Zopakuj snímky `character`, `kiosk`, `twohands`, `station`, `autopaver`, `car`, `gate_open` z úloh 10 – 17 → Expected pri každej: namiesto kvádrov skutočné modely, stoja na zemi, správne otočené (výpust výrobne k parkovisku, obrazovka kiosku k ceste, lišta auta vzadu), batoh sedí na chrbte a neprechádza telom, prilba na hlave.
- [ ] **Step 6:** Run `Scripts/shot.sh overview "dia.TopView"` → Expected: parkovisko obkolesené plotom, výrobňa vľavo, šesť ciest s kioskami, kulisy mimo buniek.
- [ ] **Step 7:** Commit `feat(game): real models and site dressing`.

---

# Fáza 6 – Dokončenie

### Task 19 [Sonnet 5.5]: Simulácia balansu a ladenie cien

**Files:**
- Create: `Source/AsphaltCore/Private/Tests/BalanceSimTest.cpp`
- Modify: `Source/AsphaltCore/Public/AsphaltBalance.h` (len ceny, ak simulácia vyjde mimo cieľa), `docs/01-herny-dizajn.md` (tabuľky cien, ak sa zmenili)

**Interfaces:**
- Consumes: celé `FAsphaltGame`.
- Simulovaný hráč (len v teste, deterministický): vždy kúpi najlacnejšie dostupné vylepšenie; cestu ide dokončiť, keď mu po jej dokončení vystačia peniaze na jej špeciál, v poradí ciest 3, 2, 4, 1, 0; inak leje najbližšiu nedokončenú bunku k výrobni. Jedna otočka trvá `2 × vzdialenosť / rýchlosť + čas liatia + čakanie na zásobník`; rýchlosť je `WalkSpeedCmS`, so šprintom ×`SprintMultiplier`; po kúpe stanice sa vzdialenosť počíta od posledne liatej bunky + 300 cm; po kúpe auta hráč jazdí autom. Robot beží v `Tick`.

- [ ] **Step 1: Test** `DoItAsphalt.Core.BalanceSim.Playthrough`: simulácia skončí s `IsExitOpen()`; celkový čas je medzi 45 a 100 minútami; všetkých 35 vylepšení je kúpených pred dokončením 75 % buniek; žiadny úsek dlhší než 4 minúty bez nákupu počas prvých 30 minút. Test vypíše do logu časovú os nákupov (`[mm:ss] bought Plant 3`).
- [ ] **Step 2:** Run → ak zlyhá na čase alebo rozostupoch, uprav **len** polia `*Price*` v `AsphaltBalance.h` (pomerovo celé pole) a zopakuj. Kapacity, šírky a výkony zo špecky sa nemenia. Testy súčtov v `BalanceTest.cpp` uprav na nové hodnoty, podmienka `≤ 60 %` zostáva.
- [ ] **Step 3:** Ak sa ceny zmenili, prepíš tabuľky v GDD kap. 7 a 8 a súčty v kap. 6.
- [ ] **Step 4:** Commit `test(core): balance playthrough simulation` (a `balance: tune prices`, ak sa menili).

### Task 20 [Sonnet 5.5]: Build pre Mac a záverečná kontrola

**Files:**
- Create: `Scripts/package.sh`, `README.md`
- Modify: `Config/DefaultGame.ini` (názov `Do it Asphalt`, verzia `0.1.0`)

- [ ] **Step 1:** `package.sh` volá `"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$UPROJECT" -platform=Mac -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="$PROJECT_ROOT/Builds"`.
- [ ] **Step 2:** Run `Scripts/package.sh` → Expected: `BUILD SUCCESSFUL`, v `Builds/` je `.app`.
- [ ] **Step 3:** Over, že cheaty v Shipping builde neexistujú: `strings` nad binárkou v `.app` nenájde `dia.Give`.
- [ ] **Step 4:** Spusti `.app`, v hlavnom menu `New Game`, prejdi kontrolu a výsledok zapíš do `README.md` pod nadpis „Overené": pohyb a kamera; batoh sa pri výrobni naplní; liatie zanecháva asfalt a pripisuje peniaze; dokončená bunka dá shard; Tab otvorí obchod a nákup sa prejaví; hra sa po zavretí a otvorení ponúkne cez `Continue` v rovnakom stave.
- [ ] **Step 5:** `README.md`: čo je projekt, požiadavky (UE 5.8, Xcode, Blender), príkazy zo `Scripts/`, odkaz na `docs/`.
- [ ] **Step 6:** Run `Scripts/build.sh && Scripts/test.sh` → Expected: `FAILED 0`.
- [ ] **Step 7:** Commit `chore: mac packaging and readme`.
