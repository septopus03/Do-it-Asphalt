# Do it Asphalt-self Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prestavať hotovú hru v1 (šesťuholníky, batoh, shardy) na Do it Asphalt-self: parkovisko v tvare L s 30 000 štvorcami pred nákupným centrom, nádoby v ruke, kufor, vozík, surový pás a finišer, automat, auto, jedna mena, prvá osoba.

**Architecture:** Nové jadro vzniká vedľa starého pod menami `Lot*` (čisté C++ v module `AsphaltCore`, TDD), takže build ostáva zelený po každej úlohe. Až keď je nové jadro hotové a prejde simuláciou celej hry, engine vrstva sa naň prepne jednou úlohou a staré jadro sa zmaže. Ostatné úlohy engine vrstvy pridávajú po jednom to, čo hráč vidí a ovláda.

**Tech Stack:** Unreal Engine 5.8.4 (C++, Slate, Enhanced Input, Automation Tests, PythonScriptPlugin, Interchange), bash, git + git-lfs. Modely sú hotové (Blender skripty v `Tools/ModelGen`).

**Spec:** `Do_it_Asphalt-self.md` (pravidlá a čísla od autora hry) a `docs/04-plan-zmien-asphalt-self.md` (návrh riešenia, rozhodnutia N1 – N12, tabuľka nových modelov). Vykonávateľ číta plán spolu s nimi. Pri rozpore platí `04`, lebo zapisuje neskoršie rozhodnutia.

## Modely

| Značka | Kto úlohu robí | Parameter pri spúšťaní subagenta |
|---|---|---|
| **[Sonnet 5.5]** | každá úloha v tomto pláne (C++, Python pre editor, shell, testy) aj kontrola kódu | `model: "sonnet"` |
| **[Fable 5.2]** | 3D modely – 12 nových je hotových, ďalšie len na požiadanie | `model: "fable"` |

## Global Constraints

- Engine: `/Users/Shared/Epic Games/UE_5.8` (5.8.4). API enginu overuj v zdrojákoch na disku, nie z pamäti.
- Žiadne Blueprinty, UMG ani ručne vytvorené `.uasset`. Výnimka: Mannequin skopírovaný zo šablóny.
- `AsphaltCore` závisí len od modulu `Core`. Žiadne `UObject`, `UWorld`, `AActor`.
- Každé herné číslo je v `Source/AsphaltCore/Public/LotBalance.h` alebo `LotLayout.h`; engine vrstva žiadne herné číslo neobsahuje.
- Hodnoty zo špecky sa nemenia: políčko 1 × 1 m, 10 l, hotové pri 95 %; 10 kreditov za políčko; lyžica 0,25 l, miska 2,5 l, lopatka 1 l (10), vedro 10 l (35), kufor 60 l (110), vozík 200 l (340), výdajňa 900, finišer 1 400 (85 políčok/min), automat 3 000 / 9 000 / 22 000 (120 / 170 / 220), auto 12 000 (260), širšia lišta 24 000 (300); výrobňa 150 / 300 / 750 / 2 000 / 4 200 / 5 200 l/min za 100 / 400 / 1 400 / 8 000 / 24 000, na štarte 80 l; odomykanie pri 10 / 100 / 350 / 800 / 6 000 / 30 000 políčkach.
- Ladiť sa smú len konštanty označené v `LotBalance.h` komentárom `// tunable` (časy, rýchlosti, prietoky, ktoré špecka neurčuje).
- Jednotky v kóde: centimetre, litre, sekundy. Identifikátory a komentáre po anglicky, texty v hre po anglicky cez `LOCTEXT`.
- Cesta k projektu obsahuje medzery – každá cesta v každom skripte je v úvodzovkách.
- Testy nového jadra: `DoItAsphalt.Lot.*`, testy engine vrstvy: `DoItAsphalt.Game.*`. Každý test sa deklaruje makrom `DIA_TEST`.
- Každá úloha končí commitom na vetve `asphalt-self`; pred commitom musí prejsť `Scripts/build.sh` aj `Scripts/test.sh`. Skripty nikdy nebežia dva naraz.
- Vizuálne overenie: `Scripts/shot.sh`, výsledný PNG si vykonávateľ otvorí a porovná s popisom v kroku.
- `Do it Asphalt - dokumentacia.docx`, `Config/DefaultEngine.ini`, `Config/DefaultInput.ini`, `Build/` a `.Rhistory` majú v pracovnom strome zmeny používateľa – nestageovať, nemeniť.

## Review Focus

1. **Liatie na obsadené miesto alebo mimo parkoviska** (surový aj hotový asfalt, nákupné centrum, pás za obrubníkom) – z nádoby neubudne nič, kredity nepribudnú. → Task 3 (`Lot.Stamp.OccupiedIsFree`, `Lot.Stamp.OffLotIsFree`), Task 5 (`Lot.Hand.NothingLeavesOnOccupied`).
2. **Dôjde asfalt uprostred práce** – nádobe pri liatí, zásobníku pri strojoch – zostatok nikdy nejde do mínusu, stroj čaká, nič nevznikne zadarmo. → Task 3 (`Lot.Stamp.StopsWhenLitresRunOut`), Task 7 (`Lot.AutoPaver.StarvedByPlant`, `Lot.Car.DrawsFromStock`).
3. **Políčko dokončia dvaja naraz alebo v ňom ostal surový asfalt** – odmena presne raz, surový zvyšok sa stane hotovým, surový asfalt sám políčko nedokončí. → Task 3 (`Lot.Complete.RawRemainderJoins`, `Lot.Complete.RawBlocksThreshold`), Task 4 (`Lot.Game.CellPaysOnce`).
4. **Poškodený, skrátený alebo starý (v1) uložený súbor** – hra nespadne, stav ostane nedotknutý, ponúkne sa len nová hra. → Task 8 (`Lot.Save.RejectsCorruptData`, `Lot.Save.RejectsV1`), Task 19 (`Game.CorruptSlotStartsNewGame`).
5. **Každé políčko sa musí dať dokončiť** – nič pevné nestojí na políčku, predmet odložený na nehotovom mieste neblokuje liatie ani stroje, inak sa výjazd nikdy neotvorí. → Task 2 (`Lot.Layout.NothingFixedOnLot`), Task 12 (`Game.Site.LotIsClear`), Task 15 (`Game.Movable.DoesNotBlockPaving`).

## Mimo rozsahu

Zvuk a hudba; viditeľné ruky a animácie úchopu (nástroj visí pred kamerou); valček, dusadlo a lata s vodováhou (v špecke nemajú cenu ani pravidlo – otázka O1 v `docs/04`); gamepad; Windows build; prefarbenie starých modelov.

## Poradie

```
Jadro:   1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9
Engine:  10 (kedykoľvek po 2) → 11 → 12 → 13 → 14 → 15 → 16 → 17 → 18 → 19 → 20
```

Úlohy idú za sebou; jediná, ktorá môže bežať skôr, je Task 10 (import modelov).

---

# Fáza 1 – Nové jadro

Všetky súbory tejto fázy sú nové, staré jadro (`HexMap`, `CoverageGrid`, `AsphaltGame`, `AsphaltBalance`, `AsphaltLayout`, `AsphaltDressing`) ostáva nedotknuté až do Task 11. Testy sú v `Source/AsphaltCore/Private/Tests/`, obalené `#if WITH_DEV_AUTOMATION_TESTS`.

### Task 1 [Sonnet 5.5]: Mapa parkoviska

**Files:**
- Create: `Source/AsphaltCore/Public/LotMap.h`, `Source/AsphaltCore/Private/LotMap.cpp`
- Test: `Source/AsphaltCore/Private/Tests/LotMapTest.cpp`

**Interfaces – Produces** (`namespace AsphaltLot`, všetko `ASPHALTCORE_API`):

```cpp
inline constexpr int32 Side = 200, Cut = 100, NumCells = 30000;
inline constexpr double CellCm = 100.0, HalfExtentCm = 10000.0;
inline constexpr int32 SectorSide = 20, NumSectors = 75, CellsPerSector = 400;
inline constexpr int32 NumEntries = 6, CellsPerEntry = 12;
bool IsLot(int32 Col, int32 Row);            // 0..199 a nie (Col >= 100 && Row >= 100)
int32 CellIndex(int32 Col, int32 Row);       // INDEX_NONE mimo parkoviska
FIntPoint CellCoord(int32 Cell);
int32 CellAt(FVector2D WorldCm);             // INDEX_NONE mimo parkoviska
FVector2D CellCentre(int32 Cell);
int32 SectorOf(int32 Cell);
int32 SectorAt(FVector2D WorldCm);           // INDEX_NONE mimo parkoviska
int32 SectorCell(int32 Sector, int32 Step);  // Step 0..399, riadok po riadku, každý druhý riadok opačne
FVector2D SectorCentre(int32 Sector);
int32 EntryCell(int32 Entry, int32 I);       // I 0..11
int32 EntryOf(int32 Cell);                   // INDEX_NONE, ak políčko nepatrí úseku
FVector2D EntryCentre(int32 Entry);          // bod na obryse
FVector2D EntryOutward(int32 Entry);         // jednotkový vektor von z parkoviska
```

Pevné pravidlá (uložené hry ukladajú indexy):
- Svet: X, Y ∈ ⟨−10000, 10000⟩ cm; stĺpec = ⌊(X + 10000) / 100⌋, riadok rovnako z Y. Nákupné centrum je kvadrant `Col >= 100 && Row >= 100`.
- Index: `Row < 100` → `Row * 200 + Col`; inak `20000 + (Row − 100) * 100 + Col`.
- Sektor: `SCol = Col / 20`, `SRow = Row / 20`; `SRow < 5` → `SRow * 10 + SCol`; inak `50 + (SRow − 5) * 5 + SCol`.
- Úseky (stĺpce × riadky, vrátane) a ich bod na obryse / smer von:

| Úsek | Políčka | `EntryCentre` | `EntryOutward` |
|---|---|---|---|
| 0 | 97 – 102 × 0 – 1 | (0, −10000) | (0, −1) |
| 1 | 152 – 157 × 98 – 99 | (5500, 0) | (0, 1) |
| 2 | 98 – 99 × 152 – 157 | (0, 5500) | (1, 0) |
| 3 | 0 – 1 × 97 – 102 | (−10000, 0) | (−1, 0) |
| 4 | 47 – 52 × 198 – 199 | (−5000, 10000) | (0, 1) |
| 5 | 198 – 199 × 47 – 52 | (10000, −5000) | (1, 0) |

- [ ] **Step 1: Testy** (`DoItAsphalt.Lot.Map.*`):
  - `Indexing`: `CellIndex(0,0) == 0`, `(199,99) == 19999`, `(0,100) == 20000`, `(99,199) == 29999`, `(100,100) == INDEX_NONE`, `(-1,0)` a `(200,0)` → `INDEX_NONE`; pre všetkých 30 000 indexov `CellIndex(CellCoord(i)) == i`.
  - `World`: `CellAt({-9950,-9950}) == 0`; `CellAt({50,50})`, `CellAt({-10001,0})`, `CellAt({0,-10500})` → `INDEX_NONE`; `CellCentre(0) == {-9950,-9950}`; `CellAt(CellCentre(i)) == i` pre každé i.
  - `Sectors`: každé políčko má sektor 0..74; každý sektor má presne 400 políčok a `SectorCell` ich vráti každé raz; `SectorCell(0,0) == CellIndex(0,0)`, `(0,19) == CellIndex(19,0)`, `(0,20) == CellIndex(19,1)`, `(0,399) == CellIndex(0,19)`; `SectorCell(s, 400)` a `SectorCell(75, 0)` → `INDEX_NONE`.
  - `Entries`: 72 rôznych políčok, všetky na parkovisku; `EntryOf(CellIndex(99,0)) == 0`, `EntryOf(CellIndex(0,0)) == INDEX_NONE`; `EntryCentre` / `EntryOutward` podľa tabuľky.
- [ ] **Step 2:** Run `Scripts/build.sh && Scripts/test.sh DoItAsphalt.Lot.Map` → FAIL (neexistuje).
- [ ] **Step 3:** Implementuj `LotMap.h/.cpp`. Úseky sú jedna statická tabuľka obdĺžnikov; `EntryOf` ju prejde.
- [ ] **Step 4:** Run `Scripts/build.sh && Scripts/test.sh` → všetko PASS (aj starých 125).
- [ ] **Step 5:** Commit `feat(core): lot map with cells, sectors and entries`.

### Task 2 [Sonnet 5.5]: Čísla a rozmiestnenie

**Files:**
- Create: `Source/AsphaltCore/Public/LotBalance.h`, `Source/AsphaltCore/Public/LotLayout.h`, `Source/AsphaltCore/Private/LotLayout.cpp`
- Test: `Source/AsphaltCore/Private/Tests/LotBalanceTest.cpp`

**Interfaces – Produces:**

```cpp
enum class ELotItem : uint8 { Scoop, Bucket, Suitcase, Cart, Station, Finisher, AutoPaver, AutoPaverFast, AutoPaverFaster, Car, CarWideBar, Count };
enum class ELotUnlock : uint8 { QuickSwap, Sprint, StripWork, AutoPaver, Car, Exit, Count };   // krok i patrí úseku i
enum class ELotHand : uint8 { Bowl, Scoop, Bucket, Trowel, Count };
enum class ELotMovable : uint8 { Suitcase, Cart, Station, Finisher, Count };
enum class ELotSource : uint8 { Plant, Suitcase, Cart };
```
(enumy žijú v `LotBalance.h`, aby ich videli tabuľky.)

`namespace LotBalance` – hodnoty zo špecky:

| Konštanta | Hodnota |
|---|---|
| `LitresPerCell`, `CreditsPerCell`, `CompletionThreshold` | 10, 10, 0.95 |
| `SpoonLitres`, `ScoopLitres`, `HandCapacityL[4]` | 0.25, 1.0, {2.5, 1.0, 10.0, 0.0} |
| `ItemPrice[11]` | {10, 35, 110, 340, 900, 1400, 3000, 9000, 22000, 12000, 24000} |
| `ItemUnlock[11]` (`Count` = bez podmienky) | {Count, Count, Count, Count, StripWork, StripWork, AutoPaver, AutoPaver, AutoPaver, Car, Car} |
| `ItemRequires[11]` (`Count` = nič) | AutoPaverFast → AutoPaver, AutoPaverFaster → AutoPaverFast, CarWideBar → Car, ostatné Count |
| `UnlockCells[6]` | {10, 100, 350, 800, 6000, 30000} |
| `PlantRateLPerMin[6]`, `PlantPrice[5]`, `PlantStartStockL` | {150, 300, 750, 2000, 4200, 5200}, {100, 400, 1400, 8000, 24000}, 80 |
| `SuitcaseCapacityL`, `CartCapacityL` | 60, 200 |
| `FinisherRateCellsPerMin`, `FinisherWidthCm` | 85, 100 |
| `AutoPaverRateCellsPerMin[3]` | {120, 170, 220} |
| `CarRateCellsPerMin[2]`, `CarWidthCm[2]` | {260, 300}, {300, 350} |

Hodnoty, ktoré špecka neurčuje (každá s komentárom `// tunable`):

| Konštanta | Hodnota | Význam |
|---|---|---|
| `ScoopCycleSeconds` | 0.6 | jeden záber lyžicou alebo lopatkou |
| `HandPourRateLPerS` | 1.0 | prúd z misky a lopatky |
| `PourRadiusCm[4]` | {15, 10, 0, 0} | stopa prúdu podľa `ELotHand` |
| `SwapSeconds` | 1.5 | výmena nástroja pred odomknutím rýchlej výmeny (potom 0) |
| `TrowelWidthCm` | 30 | |
| `ReachCm` | 250 | kam najďalej od hráča dosiahne zámerný bod |
| `SuitcaseDoseRateLPerS` | 2.0 | klapka kufra |
| `TapRateLPerS` | 5.0 | plnenie vedra z kufra a vozíka |
| `CartStripWidthCm` | 100 | |
| `WalkSpeedCmS`, `SprintMultiplier` | 450, 1.6 | |
| `SuitcasePullSpeedCmS`, `CartPushSpeedCmS`, `CartLaySpeedCmS` | 350, 250, 100 | |
| `AutoPaverSpeedCmS` | 600 | presun do sektora |
| `CarSpeedCmS` | 600 | jazda bez liatia |
| `PlantStorageMinutes` | 4 | zásobník = toľko minút výroby |

Prevzaté z v1 bez zmeny mena a hodnoty: `MaxStepSeconds` 0.25, `StampPadCm` 5, `InteractionRangeCm` 150, `AutosaveIntervalSeconds` 30, `CarBarBehindCm` 230, `CarTurnRateDegPerSec` 90, `CarReverseSpeedFactor` 0.5, `CarAccelerationCmS2` 1500. `PlantAddonALevel` = 2, `PlantAddonBLevel` = 4.

Odvodené (`constexpr` funkcie): `FinisherSpeedCmS()` = 85 × 100 / 60 ≈ 141.67; `CarPaveSpeedCmS(int32 Level)` = `CarRateCellsPerMin[Level]` × 10000 / (60 × `CarWidthCm[Level]`) → 144.44 a 142.86; `DispenseRateLPerS(int32 PlantLevel)` = `PlantRateLPerMin / 60`.

`namespace LotLayout`:

```cpp
inline const FVector2D TroughPos{-50.0, -10080.0};     // vaňa aj výpust; dlhá strana vane pozdĺž X
inline const FVector2D PlantPos{-50.0, -10420.0};
inline constexpr double PlantYawDeg = 90.0;            // výpust modelu (+X) mieri na +Y, k parkovisku
inline constexpr double RefillRadiusCm = 200.0;        // okolo TroughPos a okolo položenej výdajne
inline constexpr double TroughReachCm = 120.0;         // zámerný bod bližšie k TroughPos = naberanie
inline const FVector2D PlayerStart{-50.0, -9850.0};
inline constexpr double PlayerStartYawDeg = -90.0;     // pohľad na vaňu
inline constexpr double StripCm = 1200.0;              // pás zeme za vonkajšími stranami
inline const FVector2D MallCentre{5000.0, 5000.0};
inline constexpr int32 ExitEntry = 5;
inline constexpr double ExitTriggerRadiusCm = 150.0;
ASPHALTCORE_API int32 FirstCell();                     // AsphaltLot::CellIndex(99, 0)
ASPHALTCORE_API FVector2D GatePos();                   // EntryCentre(5) + Outward * StripCm
ASPHALTCORE_API FVector2D ExitTrigger();               // GatePos + Outward * ExitTriggerRadiusCm
ASPHALTCORE_API FVector2D BoardPos(int32 Entry);       // úseky 0, 3, 4, 5: EntryCentre + Outward * 150 + Right * 450, Right = (-Out.Y, Out.X)
ASPHALTCORE_API FVector2D ItemSpawn(ELotItem Item);    // kde sa objaví kúpený predmet
```
`ItemSpawn`: Suitcase (150, −9700), Cart (350, −9700), Station (−350, −9700), Finisher (600, −9700), AutoPaver (−600, −9700), Car (−1000, −9500); ostatné vrátia `PlayerStart`.

- [ ] **Step 1: Testy** (`DoItAsphalt.Lot.Balance.*`, `DoItAsphalt.Lot.Layout.*`):
  - `Balance.Tables`: súčet `ItemPrice` = 72 795, súčet `PlantPrice` = 33 900; `UnlockCells` rastie; `ItemRequires` podľa tabuľky.
  - `Balance.Derived`: `FinisherSpeedCmS()` ≈ 141.667 (±0.01); `CarPaveSpeedCmS(0)` ≈ 144.444, `(1)` ≈ 142.857; `DispenseRateLPerS(0) == 2.5`.
  - `Layout.NothingFixedOnLot`: `CellAt` je `INDEX_NONE` pre `TroughPos`, `PlantPos`, `GatePos()`, `ExitTrigger()` a `BoardPos(0|3|4|5)`; žiadny z nich neleží v kvadrante nákupného centra.
  - `Layout.Start`: `CellAt(PlayerStart)` patrí úseku 0; `FirstCell()` patrí úseku 0 a jeho stred je do 150 cm od `TroughPos`; každý `ItemSpawn` pohyblivého predmetu a auta leží na parkovisku a žiadne dva nie sú bližšie než 150 cm.
- [ ] **Step 2:** Run test → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** Commit `feat(core): lot balance and layout`.

### Task 3 [Sonnet 5.5]: Mriežka s tromi stavmi

**Files:**
- Create: `Source/AsphaltCore/Public/LotGrid.h`, `Source/AsphaltCore/Private/LotGrid.cpp`
- Test: `Source/AsphaltCore/Private/Tests/LotGridTest.cpp`

**Interfaces – Consumes:** `AsphaltLot` (Task 1), `LotBalance::StampPadCm`, `CompletionThreshold`, `LitresPerCell`.

**Produces:**

```cpp
enum class ELotTexel : uint8 { Free = 0, Raw = 1, Done = 2 };
struct FLotPour { double LitresUsed = 0.0; int32 TexelsChanged = 0; TArray<int32> CompletedCells; };

class ASPHALTCORE_API FLotGrid
{
public:
	static constexpr int32 Size = 4000;                 // texely na stranu, 5 cm
	static constexpr double TexelCm = 5.0;
	static constexpr int32 TexelsPerCell = 400;
	static constexpr double LitresPerTexel = 0.025;
	static constexpr int32 ThresholdTexels = 380;       // 95 %
	FLotGrid();
	FLotPour Stamp(FVector2D A, FVector2D B, double WidthCm, double MaxLitres, ELotTexel State);   // voľné → State (Raw alebo Done)
	FLotPour PourDisc(FVector2D Centre, double RadiusCm, double MaxLitres, FVector2D From);        // voľné → Done
	FLotPour DepositIntoCell(int32 Cell, double MaxLitres, FVector2D From, ELotTexel State);       // voľné texely políčka → State
	FLotPour FinishRaw(FVector2D A, FVector2D B, double WidthCm);                                  // surové → Done, zadarmo
	FLotPour CompleteCell(int32 Cell);                  // LitresUsed = cena dovtedy voľnej časti
	int32 GetCellDone(int32 Cell) const;
	int32 GetCellRaw(int32 Cell) const;
	bool IsCellComplete(int32 Cell) const;
	int32 NumCompleteCells() const;
	ELotTexel GetTexel(int32 X, int32 Y) const;
	TConstArrayView<uint8> GetTexels() const;           // Size * Size hodnôt ELotTexel, riadok Y po riadku
	void ConsumeDirtyRects(TArray<FIntRect>& Out);      // najviac jeden obdĺžnik na sektor, Max je exkluzívne
};
```

Pravidlá, ktoré testy a signatúry samy neurčia:
- Texel patrí políčku, v ktorom leží jeho stred; políčko texela sa počíta delením (žiadna tabuľka texel → políčko, žiadne zoznamy texelov – 16 miliónov texelov).
- Zmeniť sa dá len voľný texel (okrem `FinishRaw` a dokončenia políčka). Každý nový texel stojí `LitresPerTexel`; prestane sa, keď zvyšok rozpočtu nestačí na ďalší texel (tolerancia 1e-9).
- Poradie: `Stamp` berie kandidátov v obdĺžniku (os A→B predĺžená o `StampPadCm` na oboch koncoch, polovičná šírka `WidthCm / 2`; `A == B` → os +X) podľa priemetu na os od A, pri zhode podľa indexu texela. `PourDisc` a `DepositIntoCell` podľa vzdialenosti stredu texela od `From`, pri zhode podľa indexu.
- Keď `GetCellDone` dosiahne 380, všetky texely políčka sa stanú `Done` (voľné bez poplatku, surové tiež), políčko sa nahlási raz v `CompletedCells`.
- Zmeny sa evidujú po sektoroch (75 obdĺžnikov), aby dve vzdialené zmeny v jednom snímku neznamenali nahratie celej textúry.

- [ ] **Step 1: Testy** (`DoItAsphalt.Lot.*`; `C0 = CellCentre(0)`; „pás cez políčko 0" = `Stamp(C0 + (-45,0), C0 + (45,0), 100, …)`, ktorý s vypchávkou kryje presne políčko 0):
  - `Grid.Constants`: štyri konštanty; nová mriežka má 0 hotových políčok a všetky texely `Free`.
  - `Stamp.DoneCompletesAt95`: pás cez políčko 0, 100 l, `Done` → `LitresUsed == 9.5`, `CompletedCells == {0}`, `GetCellDone(0) == 400`.
  - `Stamp.RawDoesNotComplete`: to isté s `Raw` → `LitresUsed == 10.0`, žiadne hotové políčko, `GetCellRaw(0) == 400`.
  - `Stamp.OccupiedIsFree`: `Raw` cez políčko 0, potom `Done` cez to isté → `LitresUsed == 0`, `TexelsChanged == 0`; a naopak na inom políčku.
  - `Stamp.OffLotIsFree`: pás okolo (5000, 5000) a okolo (0, −10500) → 0 l.
  - `Stamp.StopsWhenLitresRunOut`: pás cez políčko 0 s 1.0 l → presne 40 texelov, `LitresUsed == 1.0`; najväčší priemet zaliateho texela ≤ najmenší priemet nezaliateho.
  - `Stamp.NeverOverspends`: rozpočet 0.03 → 1 texel, `LitresUsed == 0.025`; rozpočet 0.02 → 0 texelov.
  - `PourDisc.NearestFirst`: `PourDisc(C0, 15, 0.25, C0 + (-100,0))` → 10 texelov a sú to tie s najmenšou vzdialenosťou od `From`; druhé volanie s 10 l zaleje zvyšok disku; tretie → 0 l.
  - `Deposit.FromSide`: `DepositIntoCell(0, 2.5, C0 + (0,-150), Done)` → 100 texelov; najväčšia vzdialenosť zaliateho od `From` ≤ najmenšia vzdialenosť nezaliateho.
  - `Deposit.BucketFillsCell`: `DepositIntoCell(0, 10.0, …, Done)` → `LitresUsed == 9.5`, hotové {0}. Neplatné políčko (−1, 30000) → nič.
  - `Finish.RawBecomesDone`: surový pás cez políčko 0, potom `FinishRaw` rovnakým pásom → `LitresUsed == 0`, hotové {0}.
  - `Finish.IgnoresFree`: `FinishRaw` cez prázdne políčko → `TexelsChanged == 0`.
  - `Complete.RawRemainderJoins`: `DepositIntoCell(0, 0.5, C0 + (-150,0), Raw)` (20 surových), potom `DepositIntoCell(0, 100, C0 + (150,0), Done)` → `LitresUsed == 9.5`, hotové {0} raz, `GetCellRaw(0) == 0`.
  - `Complete.RawBlocksThreshold`: 100 surových + 300 hotových texelov v políčku → `IsCellComplete(0) == false`.
  - `Grid.CompleteCell`: po 100 hotových texeloch `CompleteCell(0)` → `LitresUsed == 7.5`, hotové {0}; druhé volanie → 0 a nič nenahlási.
  - `Grid.DirtyRects`: pás v sektore 0 a pás v sektore 74 → dva obdĺžniky, každý vnútri hraníc svojho sektora; druhé volanie → prázdne pole.
- [ ] **Step 2:** Run test → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** Commit `feat(core): three-state lot grid`.

### Task 4 [Sonnet 5.5]: Hra – kredity, výrobňa, obchod, odomykanie

**Files:**
- Create: `Source/AsphaltCore/Public/LotGame.h`, `Source/AsphaltCore/Private/LotGame.cpp`
- Test: `Source/AsphaltCore/Private/Tests/LotGameTest.cpp`

**Interfaces – Produces:**

```cpp
struct ASPHALTCORE_API FLotEvents
{
	double LitresPoured = 0.0;
	double CreditsEarned = 0.0;
	TArray<int32> CompletedCells;
	TArray<int32> CompletedEntries;
	TArray<ELotUnlock> Unlocked;
	bool bExitOpened = false;
	void Append(const FLotEvents& Other);
};

class ASPHALTCORE_API FLotGame
{
public:
	FLotGame();
	FLotGrid& GetGrid();
	const FLotGrid& GetGrid() const;
	double GetCredits() const;
	int32 GetCompletedCells() const;
	double GetPlaytimeSeconds() const;
	double GetTotalLitres() const;
	int32 GetEntryProgress(int32 Entry) const;      // 0..12
	bool IsEntryComplete(int32 Entry) const;
	int32 FindNearestIncomplete(FVector2D From) const;   // INDEX_NONE, keď je všetko hotové; pri zhode nižší index
	bool IsUnlocked(ELotUnlock Unlock) const;       // úsek hotový A ZÁROVEŇ počet políčok
	bool Owns(ELotItem Item) const;
	double GetItemPrice(ELotItem Item) const;       // -1 pre neplatný
	bool IsItemOffered(ELotItem Item) const;        // nevlastní, krok odomknutý, predpoklad vlastní
	bool CanBuyItem(ELotItem Item) const;
	bool BuyItem(ELotItem Item);
	int32 GetNumItemsOwned() const;
	int32 GetPlantLevel() const;                    // 0..5
	double GetPlantNextPrice() const;               // -1 na maxime
	bool CanBuyPlant() const;
	bool BuyPlant();
	double GetPlantRatePerMin() const;
	double GetPlantStorage() const;                 // PlantStorageMinutes * rate
	double GetPlantStock() const;
	FLotEvents Tick(double Dt);                     // kroky po MaxStepSeconds; zlé Dt ignoruje, viac než deň oreže
	bool IsExitOpen() const;                        // == IsUnlocked(Exit)
	bool HasWon() const;
	void SetWon();                                  // ignoruje sa, kým výjazd nie je otvorený
	void DebugGive(double Credits);
	void DebugGrantItem(ELotItem Item);
	void DebugSetPlantLevel(int32 Level);
	void DebugSetPlantStock(double Litres);         // oreže na 0..GetPlantStorage()
	FLotEvents DebugCompleteCell(int32 Cell);
	FLotEvents DebugCompleteCells(int32 Count);     // prvých Count nehotových podľa indexu
private:
	FLotEvents ApplyPour(const FLotPour& Pour);     // jediné miesto, kde sa pripisujú kredity a hlásia úseky, kroky a výjazd
};
```

- Kredity pribúdajú len za dokončené políčko: `CreditsPerCell` za každé v `CompletedCells`. Za litre nie.
- Odomknutie sa neukladá, počíta sa zo stavu mriežky. `ApplyPour` porovná stav pred a po a nové kroky dá do `Unlocked`.
- Nová hra: 0 kreditov, zásoba 80 l, úroveň výrobne 0, v ruke miska.

- [ ] **Step 1: Testy** (`DoItAsphalt.Lot.*`):
  - `Game.CreditsOnlyOnCompletion`: `DebugCompleteCell(0)` → `CreditsEarned == 10`, `GetCredits() == 10`, `GetCompletedCells() == 1`.
  - `Game.CellPaysOnce`: druhé `DebugCompleteCell(0)` → 0 kreditov, prázdne `CompletedCells`.
  - `Plant.StartAndProduce`: zásoba 80; `Tick(60)` → 230; `Tick(600)` → 600 (strop 4 × 150).
  - `Plant.Levels`: s 99 kreditmi `BuyPlant()` false; so 100 true → úroveň 1, 300 l/min, 0 kreditov; po `DebugSetPlantLevel(5)` je `GetPlantNextPrice() == -1` a `BuyPlant()` false; `DebugSetPlantLevel(6)` a `(-1)` sa ignorujú.
  - `Shop.Prices`: `GetItemPrice` pre všetkých 11 podľa tabuľky; `ELotItem::Count` → −1.
  - `Shop.LockedUntilUnlock`: milión kreditov; 350 políčok hotových, ale úsek 2 nie → `IsUnlocked(StripWork) == false`, `BuyItem(Station) == false`; po dokončení 12 políčok úseku 2 → oboje true.
  - `Shop.Prerequisites`: po odomknutí kroku `AutoPaver` je `BuyItem(AutoPaverFast)` false, kým nevlastní `AutoPaver`; `CarWideBar` rovnako voči `Car`.
  - `Shop.BuyRejected`: lopatka s 9 kreditmi → false a nič sa nezmení; kúpená dvakrát → druhý raz false, kredity sa neodpočítajú.
  - `Unlock.Events`: úsek 0 hotový (12 políčok cez `DebugCompleteCell`) – udalosť, ktorá dokončí 10. políčko, ak je úsek už hotový, alebo ktorá dokončí úsek, ak je políčok aspoň 10, obsahuje `Unlocked == {QuickSwap}`; nikdy druhý raz. `CompletedEntries == {0}` presne raz.
  - `Game.NearestIncomplete`: nová hra, `From = CellCentre(0)` → 0; po `DebugCompleteCell(0)` → 1 (sused s nižším indexom); po `DebugCompleteCells(30000)` → `INDEX_NONE`.
  - `Exit.OpensAtAll`: `SetWon()` pred koncom nič; `DebugCompleteCells(30000)` → `bExitOpened` raz, `GetCredits() == 300000`; `SetWon()` → `HasWon()`.
- [ ] **Step 2:** Run test → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** Commit `feat(core): lot game economy, plant, shop and unlocks`.

### Task 5 [Sonnet 5.5]: Hra – nádoby v ruke a hladidlo

**Files:**
- Modify: `Source/AsphaltCore/Public/LotGame.h`, `Source/AsphaltCore/Private/LotGame.cpp`
- Test: `Source/AsphaltCore/Private/Tests/LotHandTest.cpp`

**Interfaces – Produces** (metódy `FLotGame`):

```cpp
ELotHand GetHand() const;
bool CanSelectHand(ELotHand Hand) const;          // Bowl a Trowel vždy; Scoop a Bucket po kúpe
bool SelectHand(ELotHand Hand);                   // bez rýchlej výmeny zamestná ruky na SwapSeconds
bool IsHandBusy() const;
double GetHandLitres(ELotHand Hand) const;
double GetHandCapacity(ELotHand Hand) const;
double ScoopFromTrough(double Dt);                // vráti litre presunuté v tomto volaní
double FillBucket(ELotSource Source, double Dt);  // Plant: DispenseRateLPerS; Suitcase/Cart: TapRateLPerS (Task 6)
FLotEvents PourHand(FVector2D Aim, FVector2D From, double Dt);   // miska alebo lopatka v ruke
FLotEvents DumpBucket(FVector2D Aim, FVector2D From);
FLotEvents Trowel(FVector2D A, FVector2D B);
```

- `ScoopFromTrough`: čas držania sa sčíta; každých `ScoopCycleSeconds` jeden záber. S miskou v ruke presunie zo zásoby do misky `SpoonLitres` (s vlastnenou lopatkou `ScoopLitres`), najviac koľko sa zmestí a koľko je v zásobe. S lopatkou v ruke plní lopatku. S vedrom alebo hladidlom nič.
- `PourHand`: do rozpočtu prúdu pribudne `HandPourRateLPerS * Dt` (najviac obsah nádoby), minie sa cez `PourDisc(Aim, PourRadiusCm[hand], …, From)`, zvyšok menší než jeden texel sa prenesie do ďalšieho volania. Ak je celá stopa obsadená, rozpočet sa vynuluje.
- `DumpBucket`: `DepositIntoCell(CellAt(Aim), obsah vedra, From, Done)`; čo sa nezmestí, ostáva vo vedre.
- `Trowel`: `FinishRaw(A, B, TrowelWidthCm)`; funguje len s hladidlom v ruke.
- `PourHand`, `DumpBucket` a `Trowel` fungujú len s príslušnou vecou v ruke; `FillBucket` stačí vedro vlastniť.
- Zamestnané ruky (`IsHandBusy`, alebo držaný predmet z Task 6) → všetky akcie ruky vrátia prázdny výsledok.

- [ ] **Step 1: Testy** (`DoItAsphalt.Lot.Hand.*`):
  - `SpoonStrokes`: `ScoopFromTrough(0.59) == 0`; ďalších 0.02 → 0.25; zásoba 79.75; spolu 6.1 s → miska 2.5; ďalšie → 0.
  - `ScoopIsFaster`: s lopatkou vlastnenou a miskou v ruke: 3 zábery → 1.0, 1.0, 0.5.
  - `ScoopNeedsStock`: po `DebugSetPlantStock(0)` → 0; so zásobou 0.1 l presunie záber len 0.1.
  - `SwapDelay`: bez odomknutia `SelectHand(Trowel)` → `IsHandBusy()`, `PourHand` nič; po `Tick(1.5)` voľné. S odomknutým `QuickSwap` `IsHandBusy() == false` hneď. `SelectHand(Bucket)` bez vedra → false.
  - `PourSmallSteps`: plná miska, 60 volaní `PourHand(C0, C0 + (-100,0), 1/60)` s cieľom posúvaným po voľnej ploche → vyliate 1.0 l ± 0.025; miska o toľko ľahšia.
  - `NothingLeavesOnOccupied`: mierenie na hotový texel, na surový texel a na (5000, 5000) → obsah misky sa nezmení, `LitresPoured == 0`.
  - `FirstSquare`: štyri plné misky vyliate po políčku `FirstCell()` (cieľ prechádza mriežkou 5 × 5 bodov) → políčko hotové, kredity 10, v štvrtej miske ostane 0.5 l (±0.025).
  - `BucketDump`: `FillBucket(Plant, 4.0)` → 10 l (2.5 l/s); `DumpBucket` na prázdne políčko → hotové, vo vedre 0.5 l; druhý `DumpBucket` na to isté políčko → nič neubudne.
  - `Trowel`: surový asfalt (cez `GetGrid().Stamp(…, Raw)`) → `Trowel` pásom 30 cm zmení len texely pod sebou; s miskou v ruke nezmení nič.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** Commit `feat(core): hand vessels, scooping, pouring and trowel`.

### Task 6 [Sonnet 5.5]: Hra – kufor, vozík, výdajňa, finišer

**Files:**
- Modify: `LotGame.h`, `LotGame.cpp`
- Test: `Source/AsphaltCore/Private/Tests/LotMovableTest.cpp`

**Interfaces – Produces:**

```cpp
struct FLotMovable { FVector2D Pos = FVector2D::ZeroVector; double YawDeg = 0.0; bool bHeld = false; };
struct FLotWorldState
{
	FVector2D PlayerPos = LotLayout::PlayerStart;
	double PlayerYawDeg = LotLayout::PlayerStartYawDeg;
	FLotMovable Movables[static_cast<int32>(ELotMovable::Count)];
	FVector2D CarPos = FVector2D::ZeroVector;
	double CarYawDeg = 0.0;
	bool bPlayerInCar = false;
};
// FLotGame:
FLotWorldState& World();
const FLotWorldState& World() const;
bool OwnsMovable(ELotMovable Movable) const;
ELotMovable GetHeld() const;                          // Count = nič
bool Grab(ELotMovable Movable);                       // vlastní, nič nedrží, nesedí v aute
void Release(FVector2D Pos, double YawDeg);
double GetMovableLitres(ELotMovable Movable) const;   // kufor, vozík; inak 0
double GetMovableCapacity(ELotMovable Movable) const;
double FillMovable(ELotMovable Movable, double Dt);   // zo zásoby rýchlosťou DispenseRateLPerS
FLotEvents DoseSuitcase(FVector2D Mouth, FVector2D From, double Dt);   // surový do políčka pod ústím
FLotEvents LayCartStrip(FVector2D A, FVector2D B);    // surový pás CartStripWidthCm, rozpočet = obsah vozíka
FLotEvents FinisherPass(FVector2D A, FVector2D B);    // FinishRaw šírky FinisherWidthCm
```

- `BuyItem` pohyblivého predmetu ho položí na `LotLayout::ItemSpawn`.
- `DoseSuitcase` funguje len s nedržaným kufrom; `LayCartStrip` len s držaným vozíkom; `FinisherPass` len s držaným finišerom. Výdajňa nemá vlastné pravidlá – engine vrstva podľa jej polohy rozhoduje, či sa smie volať `FillBucket(Plant)` / `FillMovable`.
- `FillBucket(Suitcase|Cart, Dt)` z Task 5 berie z ich obsahu.
- `DoseSuitcase` prenáša zvyšok menší než jeden texel do ďalšieho volania, ako `PourHand`.
- V testoch sa nádoby plnia cez `DebugSetPlantLevel(5)`, `Tick(60)` a `FillMovable`.

- [ ] **Step 1: Testy** (`DoItAsphalt.Lot.Movable.*`):
  - `SpawnOnBuy`: po `BuyItem(Cart)` je `World().Movables[Cart].Pos == ItemSpawn(Cart)`.
  - `GrabRules`: nevlastnený → false; druhý predmet počas držania → false; držanie → `PourHand`, `ScoopFromTrough`, `DumpBucket`, `Trowel` nič.
  - `CartFill`: `FillMovable(Cart, 10)` na úrovni 0 → 25 l, zásoba 55; pri prázdnej zásobe 0; nikdy nad 200.
  - `CartLays20Cells`: plný vozík, `LayCartStrip` pozdĺž 20 políčok jedného riadku → 8000 surových texelov, vozík 0 l, 0 kreditov, 0 hotových políčok.
  - `FinisherCompletesStrip`: `FinisherPass` po tom istom páse → 20 hotových políčok, 200 kreditov; `FinisherPass` po prázdnej ploche → nič.
  - `SuitcaseDose`: plný kufor, `DoseSuitcase` 5 s nad jedným políčkom → 10 l surového v ňom (400 texelov), kufor 50 l; ďalšie dávkovanie do plného políčka → nič neubudne. Držaný kufor nedávkuje.
  - `TrowelFinishesDose`: hladidlom cez to políčko → hotové, 10 kreditov.
  - `BucketFromSuitcase`: `FillBucket(Suitcase, 1.0)` → 5 l z kufra do vedra; z prázdneho kufra 0.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** Commit `feat(core): suitcase, cart, raw strip and finisher`.

### Task 7 [Sonnet 5.5]: Hra – automat a auto

**Files:**
- Modify: `LotGame.h`, `LotGame.cpp`
- Test: `Source/AsphaltCore/Private/Tests/LotMachinesTest.cpp`

**Interfaces – Produces:**

```cpp
int32 GetAutoPaverLevel() const;                   // 0..2 podľa kúpených vylepšení; -1 bez automatu
double GetAutoPaverRateCellsPerMin() const;        // 0 bez automatu
bool AssignAutoPaverSector(int32 Sector);          // false: bez automatu, neplatný sektor
int32 GetAutoPaverSector() const;                  // INDEX_NONE = čaká
int32 GetAutoPaverStep() const;                    // 0..399
FVector2D GetAutoPaverPos() const;
bool IsAutoPaverTravelling() const;
int32 GetCarLevel() const;                         // -1, 0, 1
FLotEvents PourFromCar(FVector2D A, FVector2D B);  // FinishRaw + Stamp(Done) šírky CarWidthCm[level], rozpočet = zásoba výrobne
```

- Automat v `Tick`: do sektora ide rýchlosťou `AutoPaverSpeedCmS` k stredu `SectorCell(Sector, 0)`. Potom pracuje: rozpočet litrov rastie o `rate * LitresPerCell * CompletionThreshold / 60` za sekundu (120 políčok/min = 19 l/s, lebo prázdne políčko stojí 9,5 l; strop 10 l, najviac koľko má zásoba); políčko `SectorCell(Sector, Step)` najprv zbaví surového asfaltu (`FinishRaw` cez políčko), potom doň sype (`DepositIntoCell(…, Done)` od predošlého políčka); hotové políčko preskočí bez čakania. Poloha = stred práve spracúvaného políčka. Po kroku 399 je `GetAutoPaverSector() == INDEX_NONE`.
- Automat ani auto nemajú nádrž; berú priamo zo zásoby výrobne.
- Kúpený automat stojí na `ItemSpawn(AutoPaver)`, auto na `ItemSpawn(Car)` (`World().CarPos`).

- [ ] **Step 1: Testy** (`DoItAsphalt.Lot.AutoPaver.*`, `DoItAsphalt.Lot.Car.*`):
  - `AutoPaver.IdleWithoutSector`: `Tick(600)` → 0 políčok.
  - `AutoPaver.RejectsInvalid`: sektor −1 a 75 → false; bez automatu → false.
  - `AutoPaver.RatePerMinute`: úroveň výrobne 5, plná zásoba, automat presunutý do sektora (dotikať, kým `IsAutoPaverTravelling()`), `Tick(60)` → 120 ± 2 políčok, 10 kreditov za každé; s `AutoPaverFast` 170 ± 2, s `AutoPaverFaster` 220 ± 2 (na čerstvých sektoroch).
  - `AutoPaver.StarvedByPlant`: úroveň 0, `Tick(600)` → 150 až 175 políčok; `GetPlantStock() >= 0` po každom kroku.
  - `AutoPaver.FinishesSector`: úroveň 5, `Tick(300)` → všetkých 400 políčok sektora hotových, `GetAutoPaverSector() == INDEX_NONE`, ďalší `Tick(60)` nič.
  - `AutoPaver.SkipsCompleted`: polovica sektora vopred hotová → po príchode do sektora zvyšných 200 za ≤ 110 s.
  - `AutoPaver.FinishesRaw`: políčko sektora plné surového asfaltu → po prejazde hotové a z zásoby zaň neubudlo nič.
  - `Car.NothingWithoutCar`: `PourFromCar` → prázdne.
  - `Car.Width`: úroveň výrobne 5, plná zásoba; `PourFromCar` pozdĺž 10 políčok so stredom v strede riadku → 30 hotových políčok (3 riadky); s `CarWideBar` navyše 25 cm hotového asfaltu v riadkoch po oboch stranách (100 texelov na políčko, nehotové).
  - `Car.DrawsFromStock`: zásoba 80 l → `LitresUsed <= 80`, zásoba ≥ 0, druhé volanie 0 l.
  - `Car.FinishesRaw`: surový pás pod lištou → hotové políčka bez spotreby zásoby.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** Commit `feat(core): sector auto paver and stock-fed paver car`.

### Task 8 [Sonnet 5.5]: Uloženie v2

**Files:**
- Modify: `LotGrid.h`, `LotGrid.cpp`, `LotGame.h`, `LotGame.cpp`
- Test: `Source/AsphaltCore/Private/Tests/LotSaveTest.cpp`

**Interfaces – Produces:**

```cpp
// FLotGrid
void Write(TArray<uint8>& Out) const;
bool Read(TConstArrayView<uint8> In, int32& OutBytesRead);   // false = nič sa nezmenilo
// FLotGame
static constexpr uint32 SaveMagic = 0x32414944;   // "DIA2"
static constexpr int32 SaveVersion = 1;
TArray<uint8> SaveToBytes() const;
bool LoadFromBytes(TConstArrayView<uint8> Bytes);  // pri chybe false a *this nedotknuté
```

- Mriežka: 3750 bajtov bitov „políčko hotové" (bit = index políčka, najnižší bit prvý), `int32` počet rozrobených políčok, potom za každé `int32` index a 100 bajtov (400 texelov po 2 bitoch, poradie riadok po riadku v políčku). Rozrobené políčka idú vzostupne podľa indexu.
- Hra (little-endian): magic, verzia, `NumCells`, `Size`; kredity, čas, litre; úroveň výrobne, zásoba; 11 vlastníctiev; nádoba v ruke, tri obsahy, zvyšok výmeny; obsah kufra a vozíka, držaný predmet; automat (sektor, krok, poloha, presun, rozpočet); výhra; `FLotWorldState`; mriežka. Presná dĺžka, nič za koncom.
- `LoadFromBytes` načíta do dočasnej hry a vymení až po úspechu. Kontroly: magic, verzia, rozmery, konečné nezáporné čísla, úrovne a obsahy v rozsahu, sektor `INDEX_NONE` alebo 0..74, krok 0..399, držaný predmet vlastnený, rozrobené políčka vzostupne, v rozsahu a nie zároveň hotové, hodnoty texelov 0..2, políčko s ≥ 380 hotovými texelmi sa odmietne (malo byť hotové).

- [ ] **Step 1: Testy** (`DoItAsphalt.Lot.Save.*`):
  - `FreshIsSmall`: `SaveToBytes()` novej hry má < 8 kB.
  - `RoundTrip`: hra s kreditmi, 3 kúpenými predmetmi, úrovňou výrobne 2, miskou 1.25 l, vozíkom 80 l na vlastnej polohe, automatom v sektore 7 na kroku 123, 500 hotovými, 3 rozrobenými políčkami (surové aj hotové texely) → po `LoadFromBytes` do novej hry dá `SaveToBytes()` bajt po bajte to isté a `GetTexels()` sa zhodujú.
  - `RejectsCorruptData`: skrátenie na 0, 3, 40, polovicu a o 1 bajt; bajt navyše; zlý magic; verzia 2; úroveň výrobne 6; miska 3 l; kredity NaN a −1; sektor 75; index rozrobeného políčka 30000; dve rovnaké; rozrobené označené aj ako hotové; texel s hodnotou 3 → vždy false a `SaveToBytes()` cieľovej hry sa nezmení.
  - `RejectsV1`: blob začínajúci `0x31414944` → false.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** Commit `feat(core): lot save format`.

### Task 9 [Sonnet 5.5]: Simulácia celej hry

**Files:**
- Create: `Source/AsphaltCore/Private/Tests/LotBalanceSimTest.cpp`
- Modify (len ladenie): `LotBalance.h` – iba konštanty `// tunable`

**Interfaces – Consumes:** celé verejné API `FLotGame`. Mriežku smie simulácia len čítať (`IsCellComplete`); meniť ju smie iba akciami hry, nikdy cez `Debug*`.

Skriptovaný hráč (bez náhody, jediné hodiny sú `GetPlaytimeSeconds()`; čas plynie len cez `Tick`):
- **Nákupy:** po každom kroku kúpi najlacnejšiu ponúkanú vec, na ktorú má (predmety aj úroveň výrobne).
- **Cieľ:** kým je najbližší krok odomykania na dosah (počet políčok ≥ prah − 12) a jeho úsek nie je hotový, robí políčka úseku. Inak najbližšie nehotové políčko k zdroju (vaňa, alebo výdajňa, ktorú simulácia „drží" 5 m od posledného políčka).
- **Spôsob práce** – najlepší, ktorý vlastní: miska (naberanie, chôdza, liatie mriežkou bodov) → vedro (plnenie, chôdza, `DumpBucket`) → vozík s hladidlom, potom s finišerom (plnenie, tlačenie `CartPushSpeedCmS`, pás rýchlosťou `CartLaySpeedCmS`, návrat s finišerom `FinisherSpeedCmS()`) → auto (`PourFromCar` rýchlosťou `CarPaveSpeedCmS`, presun `CarSpeedCmS`). Kufor používa ako zásobu pre vedro: 6 políčok na jednu cestu.
- **Chôdza:** `WalkSpeedCmS`, po odomknutí šprintu × `SprintMultiplier`.
- **Automat:** vždy, keď čaká, dostane najbližší sektor k vani, ktorý nie je hotový a v ktorom hráč práve nepracuje.

- [ ] **Step 1: Test** `DoItAsphalt.Lot.BalanceSim.Playthrough` – simulácia skončí výhrou a loguje čas každého míľnika a nákupu. Tvrdenia (minúty hry):

| Míľnik | Musí nastať |
|---|---|
| 1. políčko | 40 – 80 s |
| 10 políčok | do 6 min |
| 100 políčok | do 18 min |
| 800 políčok | do 42 min |
| 6 000 políčok | do 78 min |
| 30 000 políčok | 105 – 165 min |

  Ďalej: kúpených všetkých 11 predmetov a 5 úrovní výrobne; zásoba nikdy záporná; simulácia má tvrdý strop 6 hodín hry.
- [ ] **Step 2:** Run `Scripts/test.sh DoItAsphalt.Lot.BalanceSim` → pravdepodobne FAIL na niektorom míľniku.
- [ ] **Step 3:** Laď konštanty `// tunable` (najviac 10 kôl), kým test neprejde. Ceny, kapacity a výkony zo špecky sa nemenia. Ak sa míľniky nedajú trafiť bez zmeny hodnôt zo špecky, zastav a nahlás, ktorý míľnik a o koľko.
- [ ] **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS; namerané časy zapíš do komentára na začiatku testu.
- [ ] **Step 5:** Commit `test(core): lot balance playthrough`.

---

# Fáza 2 – Engine vrstva

### Task 10 [Sonnet 5.5]: Import nových modelov

**Files:**
- Modify: `Source/DoItAsphalt/Game/DIAModelSizes.h`, `Source/DoItAsphalt/Tests/AssetsTest.cpp`
- Generated: `Content/Models/SM_{Spoon,Bowl,Trowel,Scoop,Bucket,Suitcase,Cart,Finisher,Trough,Curb,PaverCar_WideBar,Mall}.uasset`

**Interfaces – Produces** (`namespace DIAModelSizes`, cm, podľa tabuľky v `docs/04` kap. 4.1): `SpoonCm`, `BowlCm`, `TrowelCm`, `ScoopCm`, `BucketCm`, `SuitcaseCm`, `CartCm`, `FinisherCm`, `TroughCm`, `CurbCm`, `WideBarCm`, `MallCm` (`FVector`), a body: `BucketBailCm` (0, 0, 37), `SuitcaseHandleCm` (2.2, 0, 104), `SuitcaseMouthCm` (34.5, 0, 4.5), `CartHandleCm` (−63, 0, 97), `CartMouthCm` (58.5, 0, 6), `FinisherGripCm` (−115, 0, 98), `MallSignCm` (−5000, 500, 1060) v osiach modelu (fasáda −X), `MallSignSizeCm` (1600 × 400).

- [ ] **Step 1:** Rozšír test `Game.Assets.AllModelsImported` o 12 nových mien a over ich obálku voči `Tools/ModelGen/manifest.json` (rovnako ako pri starých modeloch). Run → FAIL.
- [ ] **Step 2:** Run `Scripts/uepy.sh Tools/UE/import_models.py` → `DIA_OK` pre všetkých 42 modelov. Kolízia `SM_Mall`: jeden kváder cez obálku (v skripte podľa mena, ak si to import vyžaduje).
- [ ] **Step 3:** Doplň `DIAModelSizes.h`. Run `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 4:** Commit `feat(assets): import Asphalt-self models` (len nové `.uasset`, žiadne zmenené staré).

### Task 11 [Sonnet 5.5]: Prepnutie na nové jadro

Najväčšia úloha plánu: po nej hra beží na `FLotGame`, hráč vie len chodiť a staré jadro je preč. Všetko, čo hráč robí, pridávajú úlohy 13 – 18.

**Files:**
- Modify: `Game/DIAWorldSubsystem.h/.cpp`, `Game/DIAGameMode.h/.cpp`, `Game/DIACheats.cpp`, `Game/DIARefill.h`, `World/DIAGroundActor.h/.cpp`, `World/DIAPlantActor.h/.cpp`, `Player/DIACharacter.h/.cpp`, `Player/DIAPlayerController.h/.cpp`, `UI/DIAFormat.h/.cpp`, `UI/DIAHUD.h/.cpp`, `UI/SDIAHudOverlay.cpp`, `UI/SDIAWinScreen.cpp`, `Tools/UE/create_materials.py`, `Scripts/shot.sh` (ak používa staré cheaty)
- Delete: `Source/AsphaltCore/Public/{HexCoord,HexMap,CoverageGrid,AsphaltGame,AsphaltBalance,AsphaltLayout,AsphaltDressing}.h`, ich `.cpp` a testy (`HexCoordTest`, `HexMapTest`, `CoverageGridTest`, `AsphaltGameTest`, `BalanceTest`, `BalanceSimTest`, `SpecialsTest`, `SaveTest`, `DressingTest`); `World/DIAKioskActor.*`, `World/DIASiteDressing.*`, `World/DIASiteFence.*`, `World/DIAPortableStation.*`, `World/DIAAutoPaver.*`, `World/DIAPaverCar.*`, `World/DIAExitGate.*`, `World/DIARoadActor.*`, `UI/SDIAShopPanel.*`; testy `KioskTest`, `SiteDressingTest`, `StationTest`, `AutoPaverTest`, `CarTest`, `ExitTest`, `ShopTest`. (Aktérov strojov, bránu a obchod znovu postavia úlohy 12 – 18; staré súbory sú v gite.)
- Test: prepíš `WorldBuildTest`, `CharacterTest`, `HudTest`, `FormatTest`, `SaveSlotTest` na nové jadro.

**Interfaces – Produces:**
- `UDIAWorldSubsystem`: `FLotGame& GetGame()`; `void Dispatch(const FLotEvents&)`; `bool BuyItem(ELotItem)`, `bool BuyPlant()`; delegáty `OnStateChanged`, `OnCellsCompleted(const TArray<int32>&)`, `OnEntryCompleted(int32)`, `OnUnlocked(ELotUnlock)`, `OnItemBought(ELotItem)`, `OnExitOpened`, `OnWon`, `OnGameReset`. Uloženie, slot, záloha, CRC a autosave ostávajú; blob je `FLotGame::SaveToBytes()`.
- `ADIAGroundActor`: plocha 200 × 200 m, okolo nej zem do ±11 200 cm. Textúra pokrytia `PF_R8G8` 4000 × 4000: R = 255, kde je asfalt (surový alebo hotový), G = 255, kde je hotový. Nahráva sa po obdĺžnikoch z `ConsumeDirtyRects`. `SyncCoverage(bool bFull)`, `GetCoverageTexture()`, `GetNumFullUploads()` ostávajú.
- `M_Ground` (`create_materials.py`): podklad starý svetlý asfalt, čiary políčok z `frac(WorldPosition.xy / 100)` (šírka 3 cm) len tam, kde asfalt nie je; surový = hrubý, svetlejší, drsnosť 1.0; hotový = tmavý, drsnosť 0.35; prechody cez `smoothstep` z bilineárne filtrovaných kanálov. Kvadrant nákupného centra a pás za obrysom bez čiar.
- `ADIAPlantActor`: výrobňa na `LotLayout::PlantPos` otočená o `PlantYawDeg`, `SM_Trough` na `TroughPos`, prístavky od úrovne `PlantAddonALevel` a `PlantAddonBLevel`.
- `ADIACharacter`: chôdza a kamera tretej osoby ako doteraz, bez batohu, prilby, asfaltovačky a liatia (`SetPouring`, `GetNozzlePoint`, `RefreshEquipment` a meshe výstroje preč). Štart na `LotLayout::PlayerStart` s `PlayerStartYawDeg`.
- HUD: kredity vľavo hore, postup „n / 30 000 · p %" vpravo hore; obchod sa neotvára (Tab nerobí nič do Task 14). `AsphaltFormat::Money` → `Credits(double)`.
- Cheaty: `dia.Give <credits>`, `dia.Complete <n>`, `dia.CompleteEntry <0..5>`, `dia.Item <meno ELotItem>`, `dia.Plant <0..5>`, `dia.Teleport`, `dia.Prompt`, `dia.Toast`, `dia.SkipMenu`, `dia.Camera`, `dia.TopView [výškaM=260]`, `dia.ShotAndQuit`. Preč: `dia.CompleteRoad`, `dia.SetLevels`, `dia.Special`, `dia.Shop`.

- [ ] **Step 1:** Vytvor vetvu `asphalt-self` z `main` a značku `v1-hex` na `main` (ak ešte nie sú).
- [ ] **Step 2: Testy** (`DoItAsphalt.Game.*`):
  - `World.Builds`: po `StartGameWorld` existuje zem, výrobňa a subsystém; `GetGame().GetPlantStock() == 80`.
  - `World.GroundMirrorsGrid`: po `Dispatch(Game.DebugCompleteCell(0))` a `SyncCoverage` má textúra v texeloch políčka 0 hodnoty (255, 255), po surovom páse (cez `GetGrid().Stamp(…, Raw)`) (255, 0); počet plných nahratí sa nezvýšil.
  - `World.SpawnFacing`: hráč stojí na `PlayerStart` s yaw −90 (test s `APlayerStart` inde a s yaw 0, ako dnes).
  - `Hud.Model`: po `dia.Complete 137` ukazuje postup „137 / 30,000" a kredity 1 370.
  - `SaveSlot.*`: existujúce scenáre slotu, zálohy a CRC nad novým blobom; `CorruptSlotStartsNewGame`.
- [ ] **Step 3:** Run → FAIL (nekompiluje sa). Prepni subsystém, zmaž staré jadro a staré aktéry, uprav zvyšok, kým `Scripts/build.sh` neprejde.
- [ ] **Step 4:** Run `Scripts/uepy.sh Tools/UE/create_materials.py` → `DIA_OK`. Run `Scripts/build.sh && Scripts/test.sh` → PASS (počet testov klesne o zmazané; vypíš nový počet do commitu).
- [ ] **Step 5:** Meranie nahrávania: `Scripts/shot.sh ground "dia.SkipMenu; dia.Complete 5000; dia.TopView"` → na PNG je zhora L s tmavým pásom hotových políčok (25 m) pozdĺž jednej dlhej strany, zvyšok so sieťou čiar, kvadrant vpravo hore bez čiar. Do reportu úlohy uveď čas snímky pri `dia.Complete 5000` z logu; ak plné nahratie trvá > 100 ms, nahlás to (nie je to dôvod úlohu neodovzdať).
- [ ] **Step 6:** Commit `refactor: switch the game to the lot core, remove the hex game`.

### Task 12 [Sonnet 5.5]: Okolie – nákupné centrum, obrubníky, úseky, plot

**Files:**
- Create: `World/DIAMallActor.h/.cpp`, `World/DIALotEdge.h/.cpp`, `World/DIAEntryBoard.h/.cpp`
- Modify: `Game/DIAGameMode.h/.cpp`, `Tools/UE/create_level.py` (hmla)
- Test: `Source/DoItAsphalt/Tests/SiteTest.cpp`

**Interfaces – Produces:**
- `ADIAMallActor`: `SM_Mall` na `LotLayout::MallCentre`, bez otočenia; blokujúca kolízia cez celý pôdorys.
- `ADIALotEdge`: obrubníky (`UInstancedStaticMeshComponent`, `SM_Curb` po 2 m, skosenou hranou k parkovisku) po štyroch vonkajších stranách obrysu, s medzerou ±300 cm okolo `TroughPos.X` a ±200 cm okolo úsekov 3, 4, 5; plot (`SM_Fence`, instancovaný) po vonkajšom okraji pásu `StripCm`; neviditeľné steny po tom istom okraji a po hranách pásu pri nákupnom centre; pri úsekoch 3 a 4 `SM_RoadStub` + `SM_Barrier`, pri úseku 5 `SM_RoadStub` a v medzere plota na `GatePos()` dočasne `SM_Barrier` (Task 18 ju nahradí bránou). Každý úsek je na zemi orámovaný bielym pásom širokým 5 cm (štyri ploché kvádre z palety `DIA_White`, 0,5 cm nad zemou, bez kolízie). `static int32 NumCurbs()`, `NumFencePanels()`.
- `ADIAEntryBoard` (jeden na úsek): pri úsekoch 0, 3, 4, 5 `SM_Kiosk` na `LotLayout::BoardPos`; pri úsekoch 1 a 2 len text na tabuli nákupného centra (`DIAModelSizes::MallSignCm`, druhá fasáda zrkadlovo). Text: čo krok odomyká, „úsek n / 12" a „políčka n / prah"; po odomknutí „UNLOCKED". `FText GetBoardText() const`.
- `ADIAGameMode::EnsureSite()` postaví všetko raz.
- `create_level.py`: exponenciálna výšková hmla svetlej chladnej farby tak, aby nákupné centrum z 200 m viditeľne bledlo.

- [ ] **Step 1: Testy** (`DoItAsphalt.Game.Site.*`):
  - `LotIsClear`: žiadny aktér s blokujúcou kolíziou okrem zeme nemá pôdorys obálky prekrytý s parkoviskom o viac než 1 cm (nákupné centrum sa ho len dotýka).
  - `Counts`: jedno centrum, 6 tabúľ, `NumCurbs()` a `NumFencePanels()` sedia s dĺžkami strán mínus medzery.
  - `Walls`: úsečka zo stredu parkoviska cez každú vonkajšiu stranu narazí najneskôr 1 250 cm za obrysom; do nákupného centra na jeho stene.
  - `BoardText`: nová hra → tabuľa úseku 0 obsahuje „0 / 12" a „0 / 10"; po `dia.CompleteEntry 0` „UNLOCKED".
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** `Scripts/shot.sh site "dia.SkipMenu; dia.Teleport -3000 -3000 45"` → v diaľke nákupné centrum s rohovou vežou a oranžovým prekladom vchodu, bledne v hmle; `Scripts/shot.sh start "dia.SkipMenu"` → výrobňa s vaňou, obrubník s medzerou pri vani.
- [ ] **Step 6:** Commit `feat(world): mall, kerbs, entries and site boundary`.

### Task 13 [Sonnet 5.5]: Prvá osoba a nástroj v ruke

**Files:**
- Modify: `Player/DIACharacter.h/.cpp`, `Player/DIAInput.h/.cpp`, `Player/DIAPlayerController.h/.cpp`, `Game/DIARefill.h`, `Game/DIACheats.cpp`
- Test: `Source/DoItAsphalt/Tests/CharacterTest.cpp`

**Interfaces – Produces:**
- Vstup: `Use` (ľavé tlačidlo, držať), `WorkView` (pravé tlačidlo, držať), `NextHand` (Q), `Hand1..Hand4` (1 – 4), `Flap` (F, držať), `SendPaver` (G), `Sprint`, `Interact`, `Shop`, `Pause` ako doteraz.
- `ADIACharacter`:
  - Kamera vo výške očí (`DIACharacter::EyeHeightCm = 160`), `bUsePawnControlRotation`; telo Mannequina vlastník nevidí, tieň vrhá. Pohyb sa neotáča za smerom chôdze, postava sa otáča s kamerou.
  - `USceneComponent* HandPivot` na kamere (vpravo dole), na ňom mesh podľa `GetGame().GetHand()` (miska + lyžica, lopatka, vedro, hladidlo); hladina v miske a vo vedre je disk `DIA_Asphalt`, ktorého výška a polomer sledujú obsah.
  - `FVector2D GetAimPoint() const`: priesečník pohľadu so zemou, pritiahnutý na `LotBalance::ReachCm` od hráča.
  - `void SetUsing(bool)`, `SetWorkView(bool)`, `SetSprinting(bool)`, `SelectHand(ELotHand)`, `NextHand()`.
  - `Tick` pri držanom `Use`: zámerný bod do `TroughReachCm` od `TroughPos` a miska/lopatka v ruke → `ScoopFromTrough`; vedro a zámerný bod pri výpuste (`DIARefill::IsAtOutlet`) → `FillBucket(Plant)`; inak miska/lopatka → `PourHand(Aim, poloha hráča, Dt)`, vedro → `DumpBucket` raz na stlačenie, hladidlo → `Trowel(predošlý Aim, Aim)`. Výsledky idú cez `Dispatch`.
  - Pracovný pohľad: pri držaní plynule (0,2 s) zúži zorné pole z 90° na 60° a nakloní pohľad o 15° dole; po pustení sa vráti.
  - Šprint len s `IsUnlocked(Sprint)`.
- `DIARefill::IsAtOutlet(const FLotGame&, FVector2D Aim)`: `Aim` do `TroughReachCm` od `TroughPos` alebo od položenej výdajne – pre vedro (hráč tak môže stáť pri vani a vylievať na políčka vedľa nej). `DIARefill::IsInZone(const FLotGame&, FVector2D Pos)`: do `RefillRadiusCm` od tých istých bodov – pre stojaci kufor a vozík (Task 15).
- Cheat `dia.Hand <Bowl|Scoop|Bucket|Trowel>`, `dia.Fill` (naplní nádobu v ruke).
- Známe obmedzenie: ruky nie sú vidieť, nástroj visí pred kamerou. Zapíš do `docs/02-technicky-navrh.md` v Task 20.

- [ ] **Step 1: Testy** (`DoItAsphalt.Game.Character.*`):
  - `AimPointReach`: pohľad vodorovne → bod vo vzdialenosti `ReachCm`; pohľad kolmo dole → pod hráčom.
  - `HandMeshFollowsCore`: po `SelectHand(Trowel)` má pivot mesh `SM_Trowel`; po `dia.Item Bucket` a `SelectHand(Bucket)` `SM_Bucket`.
  - `ScoopAtTrough`: hráč pri vani mieri na ňu, `SetUsing(true)`, 1 s tikov → v miske 0.25 l.
  - `PourOnLot`: plná miska, mierenie na voľné políčko, 1 s tikov → v textúre zeme pribudli hotové texely, miska ľahšia o 1 l ± 0.05.
  - `DumpOncePerPress`: plné vedro, držané `Use` 1 s nad prázdnym políčkom → jedno hotové políčko, nie viac; `Dispatch` pripísal 10 kreditov.
  - `SprintLocked`: bez odomknutia sa `MaxWalkSpeed` pri `SetSprinting(true)` nezmení; po `dia.CompleteEntry 1` a `dia.Complete 100` áno.
  - `WorkViewBlend`: po 0.3 s držania je FOV 60 ± 1, po pustení a 0.3 s 90 ± 1.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** `Scripts/shot.sh hand "dia.SkipMenu; dia.Fill"` → pohľad z očí na vaňu, vpravo dole miska s tmavou hladinou a lyžicou.
- [ ] **Step 6:** Commit `feat(player): first person, hand tools, scooping and pouring`.

### Task 14 [Sonnet 5.5]: HUD a obchod

**Files:**
- Create: `UI/SDIAShopPanel.h/.cpp`
- Modify: `UI/DIAFormat.h/.cpp`, `UI/DIAHUD.h/.cpp`, `UI/SDIAHudOverlay.h/.cpp`, `Player/DIAInput.h/.cpp`, `Player/DIAPlayerController.cpp`, `World/DIAGroundActor.cpp`, `Tools/UE/create_materials.py`, `Game/DIACheats.cpp`
- Test: `Source/DoItAsphalt/Tests/HudTest.cpp`, `ShopTest.cpp`, `FormatTest.cpp`

**Interfaces – Produces:**
- `FDIAHudModel`: kredity; postup; nádoba v ruke „1.3 / 2.5 l" (pri hladidle nič; pri držanom kufri alebo vozíku ich obsah); zásobník „42 / 600 l" v zóne plnenia; riadok najbližšieho kroku: „Next: Sprint – entry 3 / 12, cells 57 / 100".
- `FDIAShopModel`: riadky v poradí – výrobňa, potom predmety podľa `ELotItem`. Riadok má meno, popis hodnoty (napr. „150 → 300 l/min", „60 l", „120 → 170 cells/min"), cenu a stav `Buy` / `NoCredits` / `Locked` (s dôvodom „350 cells + entry 3") / `Owned`. Zamknuté riadky sú viditeľné, vlastnené sa presunú na koniec.
- `SDIAShopPanel`: zoznam s posuvníkom, otvára sa na Tab ako vo v1; hra beží ďalej.
- Toasty: dokončený úsek, odomknutý krok („Unlocked: Sprint"), kúpa.
- Kontrolný pohľad (vstup `CheckView`, kláves V, držať): `M_Ground` dostane parameter `MissingHighlight`; pri hodnote 1 sú všetky miesta bez hotového asfaltu výrazne oranžové. HUD pri tom ukáže „Nearest unfinished: 42 m" a šípku k `FindNearestIncomplete(poloha hráča)` (prepočet najviac dvakrát za sekundu).
- `AsphaltFormat::ItemName(ELotItem)`, `UnlockName(ELotUnlock)`, `HandName(ELotHand)`; cheat `dia.Shop` späť.

- [ ] **Step 1: Testy** (`DoItAsphalt.Game.Hud.*`, `Shop.*`, `Format.*`):
  - `Shop.Rows`: nová hra → 12 riadkov; výrobňa „150 → 300 l/min" za 100 so stavom `NoCredits`; výdajňa `Locked`.
  - `Shop.BuyThroughModel`: s 10 kreditmi kúpa lopatky cez riadok → kredity 0, riadok `Owned` a na konci.
  - `Shop.OnlyChangesWhenShownValueChanges`: `OnStateChanged` bez zmeny hodnôt model neprebuduje (počítadlo ako vo v1).
  - `Hud.NextUnlockLine`: nová hra → obsahuje „0 / 12" a „0 / 10"; po odomknutí všetkého okrem výjazdu ukazuje výjazd; po výhre je prázdny.
  - `Hud.HandGauge`: miska 1.25 l → „1.3 / 2.5 l" (zaokrúhlenie ako `Litres` vo v1); hladidlo → prázdny text.
  - `Hud.CheckView`: pri držanom `CheckView` má materiál zeme `MissingHighlight == 1` a model HUD vzdialenosť k najbližšiemu nehotovému políčku ± 1 m; po pustení 0 a prázdny text.
  - `Format.Names`: každý `ELotItem`, `ELotUnlock`, `ELotHand` má neprázdne meno.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/uepy.sh Tools/UE/create_materials.py`, `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** `Scripts/shot.sh shop "dia.SkipMenu; dia.Give 500; dia.Shop"` → panel so 12 riadkami, lopatka, vedro, kufor a vozík s aktívnym tlačidlom, stroje zamknuté s dôvodom.
- [ ] **Step 6:** Commit `feat(ui): HUD and shop for the lot game`.

### Task 15 [Sonnet 5.5]: Kufor a vozík vo svete

**Files:**
- Create: `World/DIAMovable.h/.cpp`
- Modify: `Game/DIAGameMode.h/.cpp`, `Player/DIACharacter.cpp`, `UI/DIAFormat.h/.cpp`
- Test: `Source/DoItAsphalt/Tests/MovableTest.cpp`

**Interfaces – Produces:**
- `ADIAMovable : AActor, IDIAInteractable` s `ELotMovable Kind` (nastaví `Init(ELotMovable)` hneď po spawne). Jadro vlastní polohu a držanie (`World().Movables[Kind]`), aktér ju zrkadlí pri `OnStateChanged` a `OnGameReset`, ako dnešná výdajňa.
  - `Interact`: `Grab` / `Release` (pustenie položí predmet tam, kde je; ak tam nie je miesto, toast ako pri výdajni vo v1).
  - Držaný predmet sleduje hráča: kufor za ním, naklonený okolo osi koliesok rukoväťou k hráčovi; vozík pred ním, madlom k hráčovi. Kolízia držaného je vypnutá.
  - Rýchlosť hráča pri držaní: `SuitcasePullSpeedCmS`, `CartPushSpeedCmS`; pri kladení pásu `CartLaySpeedCmS`.
  - `FVector2D GetMouthPoint() const` (ústie klapky / výpustu vo svete, z `DIAModelSizes`).
  - Vozík držaný + `Use` → každý snímok `LayCartStrip(predošlé ústie, ústie)`.
  - Kufor stojaci, hráč do `InteractionRangeCm`, držané `Flap` → `DoseSuitcase(GetMouthPoint(), poloha kufra, Dt)`.
  - Stojaci kufor alebo vozík v zóne plnenia sa plní sám (`FillMovable`).
  - Vedro v ruke, `Use`, zámerný bod na stojacom kufri alebo vozíku → `FillBucket(Suitcase|Cart, Dt)` (má prednosť pred vylievaním).
  - Hladina vo vozíku: disk ako pri vedre. Kufor: stĺpec v priezore.
- `ADIAGameMode::EnsureMovable(ELotMovable)` – spawn po kúpe, pri štarte a po načítaní; odstránenie, keď ho nová hra nevlastní.
- Prompty: „E – Pull" / „E – Push" / „E – Let go", pri stojacom kufri navyše „F – Open flap".

- [ ] **Step 1: Testy** (`DoItAsphalt.Game.Movable.*`):
  - `SpawnsWhenBought`: po `BuyItem(Cart)` existuje jeden `ADIAMovable` druhu `Cart` na `ItemSpawn(Cart)`; po `NewGame` žiadny.
  - `GrabFollowReleaseWritesCore`: uchopenie, presun hráča o 5 m, pustenie → `World().Movables[Cart].Pos` je do 150 cm pred hráčom a aktér tam stojí s kolíziou.
  - `CartLaysRawStrip`: plný vozík, držaný, `SetUsing(true)`, hráč posunutý o 3 m po voľnej ploche → v mriežke 3 m² ± 10 % surových texelov, žiadne hotové políčko.
  - `SuitcaseFlap`: stojaci plný kufor, `Flap` držaný 5 s → 10 l surového v políčku pod ústím.
  - `FillsInZone`: vozík položený pri vani sa za 10 s tikov naplní o 25 l ± 1; držaný sa neplní.
  - `BucketFromCart`: vedro v ruke, mierenie na vozík, `Use` 1 s → 5 l vo vedre.
  - `DoesNotBlockPaving`: vozík stojí na prázdnom políčku; `Dispatch(Game.DebugCompleteCell(to políčko))` ho dokončí a `PourHand` do susedného políčka mierením popri vozíku funguje.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** `Scripts/shot.sh cart "dia.SkipMenu; dia.Item Cart; dia.Item Suitcase; dia.Teleport 250 -9950 0"` → vozík a kufor stoja na parkovisku v správnej mierke voči kamere vo výške očí.
- [ ] **Step 6:** Commit `feat(world): suitcase and cart as physical objects`.

### Task 16 [Sonnet 5.5]: Presuvná výdajňa a finišer

**Files:**
- Modify: `World/DIAMovable.h/.cpp`, `Game/DIAGameMode.cpp`, `Game/DIARefill.h`, `UI/DIAFormat.cpp`
- Test: `Source/DoItAsphalt/Tests/MovableTest.cpp`

**Interfaces – Produces:**
- Výdajňa (`ELotMovable::Station`, `SM_PortableStation`): držaná sa ťahá za hráčom; položená vytvára zónu plnenia `RefillRadiusCm` – vedro, kufor aj vozík sa v nej plnia ako pri vani. Zásobník ukazuje HUD aj pri nej.
- Finišer (`ELotMovable::Finisher`, `SM_Finisher`): držaný je pred hráčom, lišta kolmo na smer chôdze, hráč ide najviac `FinisherSpeedCmS()`; každý snímok `FinisherPass(predošlý stred lišty, stred lišty)`. Prompt „E – Guide".
- Tabuľka držania v `DIAMovable.h` (`namespace DIAMovableHold`): pre každý druh vzdialenosť pred / za hráčom, otočenie a náklon; nie sú to herné čísla.

- [ ] **Step 1: Testy**:
  - `Movable.StationMakesRefillZone`: výdajňa položená 50 m od vane; hráč s vedrom mieri na ňu → `Use` vedro plní; vozík položený do 200 cm od nej sa plní sám; držaná výdajňa neplní nič.
  - `Movable.FinisherCompletesRawStrip`: surový pás 1 × 5 m (cez jadro), hráč s finišerom prejde po ňom → 5 hotových políčok, 50 kreditov cez `Dispatch`.
  - `Movable.FinisherSpeedCap`: s držaným finišerom je `MaxWalkSpeed` ≈ 141.7 aj pri šprinte.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** `Scripts/shot.sh finisher "dia.SkipMenu; dia.Complete 400; dia.Item Finisher; dia.Teleport 600 -9950 0"` → finišer s lištou a madlami pred kamerou.
- [ ] **Step 6:** Commit `feat(world): portable station and strip finisher`.

### Task 17 [Sonnet 5.5]: Automat

**Files:**
- Create: `World/DIAAutoPaver.h/.cpp` (podľa verzie z v1 v gite)
- Modify: `Game/DIAGameMode.cpp`, `Player/DIAPlayerController.cpp`, `UI/DIAFormat.cpp`, `UI/SDIAHudOverlay.cpp`, `Tools/UE/create_materials.py`, `World/DIAGroundActor.cpp`
- Test: `Source/DoItAsphalt/Tests/AutoPaverTest.cpp`

**Interfaces – Produces:**
- `ADIAAutoPaver`: zrkadlí `GetAutoPaverPos()`, otáča sa v smere pohybu, bez kolízie; model `SM_AutoPaver` zväčšený 1,5 ×.
- `SendPaver` (G): `AssignAutoPaverSector(SectorAt(poloha hráča))`; toast „Paver → sector n" alebo „No paver" / „Not on the lot".
- Sektor automatu je na zemi orámovaný: `M_Ground` dostane parameter `SectorRect` (min a max vo svete), 10 cm oranžový obrys; nulový obdĺžnik = nič.
- HUD: riadok „Paver: sector 12 · 143 / 400" alebo „Paver: waiting – press G".

- [ ] **Step 1: Testy** (`DoItAsphalt.Game.AutoPaver.*`):
  - `SpawnsWhenBought`, `FollowsCore` (po `Tick` je aktér na `GetAutoPaverPos()`).
  - `SendFromPlayerPosition`: hráč v sektore 3, `SendPaver` → `GetAutoPaverSector() == 3`; hráč v páse za obrubníkom → sektor sa nezmení.
  - `SectorOutlineParameter`: po priradení má materiál zeme `SectorRect` rovný hraniciam sektora; po dokončení sektora nulový.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/uepy.sh Tools/UE/create_materials.py`, `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** `Scripts/shot.sh paver "dia.SkipMenu; dia.Complete 900; dia.CompleteEntry 3; dia.Plant 5; dia.Item AutoPaver; dia.Teleport -5000 -5000; dia.TopView 60"` po stlačení G cez cheat `dia.SendPaver` (pridaj ho) → oranžový rám sektora a v ňom pás hotových políčok.
- [ ] **Step 6:** Commit `feat(world): sector auto paver`.

### Task 18 [Sonnet 5.5]: Auto, výjazd a výhra

**Files:**
- Create: `World/DIAPaverCar.h/.cpp`, `World/DIAExitGate.h/.cpp` (podľa verzií z v1 v gite)
- Modify: `Game/DIAGameMode.cpp`, `Player/DIAPlayerController.cpp`, `UI/SDIAWinScreen.cpp`, `UI/DIAFormat.h/.cpp`
- Test: `Source/DoItAsphalt/Tests/CarTest.cpp`, `ExitTest.cpp`

**Interfaces – Produces:**
- `ADIAPaverCar`: kinematické auto ako vo v1, bez nádrže a bez plnenia. Kamera v kabíne (prvá osoba, výška 190 cm, 120 cm pred stredom), kapota v dolnej časti obrazu. Pri liatí je najvyššia rýchlosť `CarPaveSpeedCmS(level)`, inak `CarSpeedCmS`. Každý snímok pri liatí `PourFromCar(predošlý bod lišty, bod lišty)`. So `CarWideBar` je k autu pripojený `SM_PaverCar_WideBar` bez posunu.
- `ADIAExitGate` na `LotLayout::GatePos()` namiesto dočasnej zábrany z Task 12, natočená podľa `EntryOutward(5)`; otvorí sa pri `OnExitOpened`; prechod sférou `ExitTrigger()` pešo alebo autom → `Win()`.
- Výhra: čas hry, hotové políčka, zarobené kredity, počet kúpených vecí „n / 16".

- [ ] **Step 1: Testy** (`DoItAsphalt.Game.Car.*`, `Exit.*`):
  - `Car.SpawnsWhenBought`, `Car.EnterExit` (ako vo v1).
  - `Car.PaveSpeedCap`: pri liatí rýchlosť neprekročí 144.5 cm/s, so širšou lištou 142.9; bez liatia dosiahne 600.
  - `Car.PavesThreeRows`: úroveň výrobne 5, jazda 10 m s liatím stredom riadku → 30 hotových políčok ± 3.
  - `Car.WideBarMesh`: po `dia.Item CarWideBar` má auto druhý mesh `SM_PaverCar_WideBar`.
  - `Exit.ClosedUntilAll`: prechod sférou pred koncom → `HasWon() == false`.
  - `Exit.OpensAndWins`: `dia.Complete 30000` → `OnExitOpened` raz, brána otvorená; prechod → `OnWon` raz, model výhry ukazuje 30 000 políčok a 300 000 kreditov.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** `Scripts/shot.sh car "dia.SkipMenu; dia.Complete 6100; dia.CompleteEntry 4; dia.Item Car"` po nastúpení cez cheat `dia.EnterCar` (pridaj ho) → pohľad z kabíny, dole kapota; `Scripts/shot.sh exit "dia.SkipMenu; dia.Complete 30000; dia.Teleport 9500 -5000 0"` → otvorená brána s nápisom EXIT.
- [ ] **Step 6:** Commit `feat(world): stock-fed paver car, exit gate and win`.

### Task 19 [Sonnet 5.5]: Uloženie v hre

**Files:**
- Modify: `Game/DIAWorldSubsystem.cpp`, `Game/DIAGameMode.cpp`, `World/DIAMovable.cpp`, `World/DIAPaverCar.cpp`, `Player/DIACharacter.cpp`
- Test: `Source/DoItAsphalt/Tests/SaveSlotTest.cpp`

**Interfaces – Consumes:** `FLotGame::SaveToBytes/LoadFromBytes`, slot a záloha z v1.

- `SaveGame` pred zápisom prepíše do jadra polohu hráča (alebo auta), a polohu každého držaného predmetu (uloží sa ako položený tam, kde je).
- `LoadGame` / `NewGame`: aktéri strojov a predmetov vzniknú alebo zaniknú podľa vlastníctva; hráč, predmety, automat a auto stoja podľa jadra; zem sa nahrá celá raz.
- Autosave každých `AutosaveIntervalSeconds`, po každej kúpe, pri výhre a pri konci sveta – len keď je zapnutý.

- [ ] **Step 1: Testy** (`DoItAsphalt.Game.SaveSlot.*`):
  - `LoadMovesEverything`: hra s vozíkom na (2000, −5000), automatom v sektore 5, autom na (−3000, 0) a hráčom na (100, −9000) → uložiť, `NewGame`, `LoadGame` → všetci aktéri sú na tých miestach, kredity a políčka sedia.
  - `HeldItemIsSavedPutDown`: uloženie s držaným vozíkom → po načítaní vozík stojí a `GetHeld() == Count`.
  - `NewGameRemovesMachines`: po `NewGame` žiadny `ADIAMovable`, automat ani auto.
  - `CorruptSlotStartsNewGame`: poškodený slot aj záloha → `HasSave() == false`, menu ponúkne len novú hru.
  - `V1SlotIsRejected`: slot s blobom v1 → `HasSave() == false`.
  - `AutosaveOnPurchase`: zapnutý autosave, kúpa → slot obsahuje novú vec.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implementuj. **Step 4:** `Scripts/build.sh && Scripts/test.sh` → PASS.
- [ ] **Step 5:** Commit `feat(save): lot game in the save slot`.

### Task 20 [Sonnet 5.5]: Dokumentácia, snímky, Shipping

**Files:**
- Modify: `docs/01-herny-dizajn.md`, `docs/02-technicky-navrh.md`, `docs/03-3d-modely.md`, `docs/README.md`, `README.md`, `Scripts/shot.sh` (len ak treba)

- [ ] **Step 1:** Prepíš `01-herny-dizajn.md` na verziu 2: pravidlá z `Do_it_Asphalt-self.md`, doplnené rozhodnutiami N1 – N12 z `docs/04` a skutočnými hodnotami z `LotBalance.h` a `LotLayout.h`. Ovládanie: WASD, myš, ľavé tlačidlo (použiť nástroj), pravé (pracovný pohľad), Q a 1 – 4 (nástroj), E (chytiť / pustiť / nastúpiť), F (klapka kufra), G (poslať automat), V (kontrolný pohľad), Shift, Tab, Esc.
- [ ] **Step 2:** `02-technicky-navrh.md`: moduly a súbory `Lot*`, mriežka 4000² s tromi stavmi a nahrávaním po sektoroch, formát uloženia, prvá osoba bez viditeľných rúk ako vedomé obmedzenie, nové cheaty. `03-3d-modely.md`: tabuľka 12 nových modelov z `docs/04` kap. 4.1, zoznam nepoužívaných, tri nové pomôcky v `common.py`.
- [ ] **Step 3:** `README.md` a `docs/README.md`: popis hry, ovládanie, cheaty, odkazy. Časť „Overené" len z toho, čo naozaj prešlo; ručné kontroly (Esc, myš, kurzor, celý prechod hry rukami) do „Neoverené".
- [ ] **Step 4:** Run `Scripts/build.sh && Scripts/test.sh` → PASS; over, že sa skompiluje Shipping cieľ (ako vo v1) a že v binárke nie sú cheaty `dia.*`.
- [ ] **Step 5:** Sada snímok (`start`, `hand`, `shop`, `cart`, `finisher`, `paver`, `car`, `exit`, `site`) – každú otvor a over voči popisu z úlohy, kde vznikla.
- [ ] **Step 6:** Commit `docs: Asphalt-self design, technical notes and README`.
