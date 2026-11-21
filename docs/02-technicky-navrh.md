# Do it Asphalt – technické poznámky

Verzia 2.0 · 2026-11-21 · popisuje kód tak, ako je v repozitári

Tento dokument vysvetľuje, ako je hra Asphalt-self postavená, pre čitateľa, ktorý kód nevidel. Pravidlá hry a ovládanie sú v `01-herny-dizajn.md`, modely v `03-3d-modely.md`.

---

## 1. Prostredie

| | |
|---|---|
| Engine | Unreal Engine 5.8.4, C++ |
| Systém | macOS (Apple Silicon) |
| Skripty | `Scripts/` (kapitola 9) |
| Modely | skripty pre Blender 5.3, výstup GLB, import skriptom v editore |
| Materiály | vytvára `Tools/UE/create_materials.py` |

## 2. Dva moduly

**`AsphaltCore`** (`Source/AsphaltCore`) je pravidlá hry bez Unrealu. Závisí len od modulu `Core`, nepoužíva žiadny `UObject` a všetko sa dá testovať s explicitným časovým krokom `Dt`. Súbory:

| Súbor | Obsah |
|---|---|
| `LotMap.h/.cpp` | geometria L-tvaru, indexy políčok, sektory, vstupné úseky |
| `LotGrid.h/.cpp` | `FLotGrid`: mriežka pokrytia s tromi stavmi |
| `LotBalance.h` | všetky čísla hry (ceny, kapacity, rýchlosti, výrobňa) |
| `LotLayout.h/.cpp` | rozloženie sveta: polohy výrobne, koryta, štartu, centra, východu |
| `LotGame.h/.cpp` | `FLotGame`: celé pravidlá, obchod, odomykanie, ukladanie |
| `LotSaveIO.h` | čítanie a zápis bajtov uloženia |
| `Tests/` | automatické testy (`DIA_TEST`) |

**`DoItAsphalt`** (`Source/DoItAsphalt`) je tenká vrstva nad enginom: zobrazuje stav, číta vstup a odovzdáva ho jadru. Podadresáre:

| Adresár | Obsah |
|---|---|
| `Player/` | postava v prvej osobe, vstup (`DIAInput`), ovládač |
| `World/` | herné objekty: zem (`ADIAGroundActor`), predmety (`DIAMovable`), auto (`ADIAPaverCar`), robot, výrobňa, východ, tabule, centrum, okraj parkoviska, `DIAToolPath.h` |
| `Game/` | `UDIAWorldSubsystem`, ukladanie (`UDIASaveGame`), herný režim, cheaty, načítanie assetov |
| `UI/` | HUD, obchod, menu, obrazovka výhry, formátovanie textov (`DIAFormat`) |
| `Tests/` | testy, ktoré potrebujú svet enginu |

Modul `DoItAsphalt` sa kompiluje s `bUseUnity = false`. Každý testovací súbor má vo vlastnom anonymnom priestore mien pomocné funkcie s krátkymi názvami (napríklad `Move`, `TickFor`, `MiddleOfLot`) a pri unity buildoch sa dva takéto názvy zlúčili do jednej jednotky a kolidovali; preto sa každý súbor kompiluje samostatne.

## 3. Mriežka pokrytia

`FLotGrid` je štvorec **4 000 × 4 000** texelov. Jeden texel má 5 cm, políčko 1 × 1 m je teda 20 × 20 = 400 texelov. Každý texel je v jednom z troch stavov (`ELotTexel`): voľný (0), surový (1), hotový (2).

- Jeden texel nesie 0,025 l asfaltu; 95 % políčka je 380 texelov.
- Operácie: `Stamp` (razenie pásu), `PourDisc` (kruh z nádoby), `DepositIntoCell` (vsypanie do políčka), `FinishRaw` (uhladenie), `CompleteCell` (dokončenie).
- Surový asfalt políčko nikdy samo nedokončí. Pri dokončení políčka sa zvyšok uhladí zadarmo. Lialo sa môže len na voľné texely.
- Každý sektor (75 sektorov po 20 × 20 políčok) má vlastný **špinavý obdĺžnik**: najmenší obdĺžnik, ktorý sa zmenil od posledného odovzdania zmien. `ConsumeDirtyRects` vydá najviac jeden obdĺžnik na sektor.

Indexovanie políčok: riadky 0 – 99 majú 200 stĺpcov, index = riadok · 200 + stĺpec; od riadku 100 má riadok 100 stĺpcov (kvadrant s centrom chýba), index = 20 000 + (riadok − 100) · 100 + stĺpec. Spolu 30 000 políčok. Sektory sa indexujú rovnako po 20 × 20 políčok (prvých 5 radov po 10 sektoroch, ďalších po 5).

## 4. Zem a textúra pokrytia

`ADIAGroundActor` zobrazuje mriežku ako textúru `PF_R8G8`: kanál R je 255, ak texel nie je voľný (surový alebo hotový), kanál G je 255, ak je hotový. Materiál `M_Ground` z nich poskladá farbu zeme.

- Na CPU sa drží kópia textúry (kvôli behu `-nullrhi` v testoch) a do GPU sa posielajú len zmenené obdĺžniky.
- Celé nahratie (16 miliónov texelov) prebehne raz pri načítaní uloženia alebo novej hre. Trvalo asi **25 ms** (25,3 a 24,3 ms v dvoch zalogovaných behoch na tomto stroji; nie je to presné meranie).
- Obrys sektora robota je parameter `SectorRect` materiálu `M_Ground`: oranžový (0,6; 0,1; 0,0), široký 10 cm.

## 5. Jedno miesto pre jadro: `UDIAWorldSubsystem`

`UDIAWorldSubsystem` vlastní jedinú inštanciu `FLotGame` a rozosiela všetky udalosti. Nikto iný jadro nevytvára. Po každom kroku hry zavolá `Dispatch`, ktorý spustí `OnCellsCompleted`, `OnEntryCompleted`, `OnUnlocked`, `OnExitOpened` a vždy ako posledné `OnStateChanged`. Ďalšie delegáty: `OnItemBought`, `OnWon`, `OnGameReset`. Čas jedného kroku sa orezáva na 1 s (`MaxTickSeconds`), aby dlhý zásek nepreskočil hru.

Všetko, čo sa týka kreditov, litrov a udalostí, sa účtuje na **jednom mieste**: `FLotGame::ApplyPour`. Každé liatie (ruka, kufor, vozík, finišer, robot, auto) prejde touto funkciou, ktorá vráti `FLotEvents` (vyliate litre, získané kredity, dokončené políčka a vstupné úseky, odomknutia, otvorený východ). Odomknutie sa počíta zo zeme: krok i je odomknutý, ak je hotový vstupný úsek i a zároveň je hotových `UnlockCells[i]` políčok.

`FDIAToolPath` (`World/DIAToolPath.h`) je spoločná pomôcka vozíka, finišera a auta: z posunutia nástroja urobí dráhu a zaručí, že sa nič nepoloží cez skok, teda cez miesto, ktoré nástroj nepreišiel. Používa `JumpSlack` 1,5 a `MoveToleranceCm` 0,01.

## 6. Ukladanie (formát „DIA2“)

Uloženie má dve vrstvy.

**Jadro** (`FLotGame::SaveToBytes` / `LoadFromBytes`) zapíše bajty v tomto poradí:

1. hlavička: magické číslo `0x32414944` („DIA2“), verzia formátu (1), počet políčok (30 000) a veľkosť mriežky (4 000);
2. kredity, čas hry, vyliate litre, úroveň výrobne a jej zásoba;
3. kúpené veci, ruka a jej obsah, zostávajúci čas výmeny, litre v kufri a vo vozíku, čo je v ruke chytené;
4. stav robota (sektor, krok, poloha, či cestuje, rozpočet);
5. príznak výhry;
6. poloha a natočenie hráča, polohy a natočenie predmetov, poloha a natočenie auta a príznak, že hráč sedí v aute;
7. zem: bit na každé políčko (hotové), počet rozpracovaných políčok a pre každé jeho index a 400 texelov po 2 bity.

`LoadFromBytes` všetko najprv načíta do dočasnej hry, skontroluje a až po úspechu ju vymení za skutočnú, takže poškodený súbor hru nepokazí. Príznaky „drží sa“ sa pri načítaní odvodzujú. Uloženie z verzie 1 (magické číslo `0x31414944`) sa odmietne (test `Lot.Save.RejectsV1`).

**Engine vrstva** (`UDIASaveGame`) uloží bajty jadra ako pole `Blob` do slotu `DoItAsphalt_Slot0` a zároveň do zálohy `DoItAsphalt_Slot0_bak`. Na konci bajtov je pätka s magickým číslom „DIAE“ a kontrolným súčtom CRC32; kontrolný súčet sa overí pred čítaním enginom. Poškodený slot sa nahradí zálohou, ak sú poškodené obe, hra začne odznova. Parameter `-DIASlot=<názov>` zmení slot (skript `shot.sh` používa `DIA_Shot`).

- Automatické uloženie: každých 30 s hry (`AutosaveIntervalSeconds`), po každom nákupe (`BuyItem`, `BuyPlant`), po výhre a pri `GameMode::EndPlay`. Zapína sa až po odchode z hlavného menu a pod automatickými testami sa pre skutočný slot preskakuje.
- `MakeSaveBlob` ukladá držaný predmet položený na kópii hry: na pozícii jeho aktéra len ak `CanBePutDownHere` prejde, inak na poslednom platnom mieste, ktoré jadro pozná. Poloha auta sa zapisuje vždy a poloha hráča aj počas jazdy, takže hra uložená v aute sa načíta v aute.

## 7. Vstup, ruky a stroje

- Vstup sa definuje v kóde (`DIAInput.cpp`), nie v assetoch. Ovládač (`DIAPlayerController.cpp`) prekladá vstup na volania jadra.
- **Ruka** je v jadre stav: miska, lopatka, vedro alebo hladidlo, s kapacitou a hladinou. Hladidlo robí `FinishRaw` pod bodom dotyku, ktorý sleduje zámerný bod rýchlosťou najviac `TrowelSpeedCmS`.
- **Predmety** (kufor, vozík, výdajňa, finišer) sú aktéri `ADIAMovable`. Jadro pozná ich polohu a či sú držané; držanie sa určuje v jadre, nie v aktérovi.
- **Auto** (`ADIAPaverCar`) je kinematický pawn bez nádrže. `PourFromCar` najprv uhladí surový asfalt pod lištou, potom zaplatí zo zásoby výrobne a `Stamp`-om položí hotový. Rýchlosť auta pri liatí obmedzuje `CarPaveSpeedCmS = sadzba · 10 000 / (60 · šírka)`.
- **Robot** (`ADIAAutoPaver`) dostane sektor povelom `AssignAutoPaverSector` (reštartuje postup). `SnapToCore` ho po načítaní postaví na uložené miesto.

## 8. Testovanie

Testy sa deklarujú makrom `DIA_TEST` a bežia cez `Scripts/test.sh`. Posledný plný beh na tomto stroji: **219 testov, všetky prešli** (214 čistých a 5 s očakávaným varovaním: záloha slotu, poškodené oba sloty, poškodený slot, useknutý súbor, uloženie z v1).

| Skupina | Počet |
|---|---|
| `DoItAsphalt.Lot.*` (jadro) | 87 |
| `DoItAsphalt.Game.*` (engine) | 131 |
| `DoItAsphalt.Core.Smoke` | 1 |

Najväčšie podskupiny `Lot`: Hand 10, AutoPaver 10, Save 10, Movable 11, Stamp 7, Car 5. Najväčšie podskupiny `Game`: Movable 30, SaveSlot 17, Character 15, Car 10, Shop 9, AutoPaver 7, Exit 6, Hud 7.

`DoItAsphalt.Lot.BalanceSim` je simulácia celej hry vopred daným správaním (bez človeka). Kontroluje, že prvé políčko padne za 40 – 80 s, 10 políčok do 6 minút, 100 do 18, 800 do 42, 6 000 do 78 minút a že celá hra trvá 105 – 165 minút. V jednom behu vyšlo 41:17 pre 800 políčok a 2:21:27 pre celú hru (kapitola 9 herného dizajnu).

Skripty overujú aj obraz: `Scripts/shot.sh <názov> "<cheaty>"` spustí hru v okne, vykoná cheaty a uloží snímku.

## 9. Skripty a nástroje

| Príkaz | Čo robí |
|---|---|
| `Scripts/build.sh` | skompiluje `DoItAsphaltEditor` |
| `Scripts/test.sh [filter]` | spustí automatické testy |
| `Scripts/shot.sh [--menu] <názov> ["cheaty"]` | snímka hry do `Saved/Screenshots/<názov>.png` |
| `Scripts/uepy.sh <skript.py>` | spustí skript pre editor bez okna |
| `Scripts/package.sh` | zabalí Shipping `.app` (v iCloude zlyhá na `codesign`) |
| `Tools/ModelGen/build_all.sh` | vygeneruje a skontroluje všetky modely |

Poznámka k `Tools/UE/create_materials.py`: pri spustení cez `uepy.sh` editor spadne na tvrdení `!IsRooted()` už pri druhom materiáli (`M_DIA_Flat`), pretože `MaterialEditingLibrary.DeleteAllMaterialExpressions` (aj `DeleteMaterialExpression` samotné) zlyhá na načítanom materiáli. Skript vypíše `DIA_OK` len pre `M_Ground`. Materiály sú hotové v `Content/Materials/` a skript netreba spúšťať znova. Zmenu materiálu, ktorú skript vytvára, treba robiť v editore alebo skript opraviť; obídenie (zmazať materiál a vytvoriť ho odznova) sa nevyskúšalo. Po každom behu ostane proces `CrashReportClient`, ktorý treba ukončiť, a prepísaný `Content/Materials/M_Ground.uasset` treba vrátiť z gitu.

## 10. Shipping build

Shipping sa skompiluje príkazom `Build.sh DoItAsphalt Mac Shipping -project=<cesta>/DoItAsphalt.uproject` (pri ceste s medzerami v úvodzovkách a zo `bash`, nie zo `zsh`, lebo `Scripts/env.sh` vyžaduje `bash`). Cheaty `dia.*` (`DIACheats.cpp`) sú pod `#if !UE_BUILD_SHIPPING`, takže v Shipping binárke nie sú. Reťazce `TEXT()` sú na Macu v UTF-16, preto sa pri kontrole musí hľadať v UTF-16 aj v ASCII; kontrolu treba overiť na editorovej knižnici `libUnrealEditor-DoItAsphalt.dylib`, kde ich nájde.

## 11. Cheaty

Zoznam 27 príkazov je v kapitole 11 dokumentu `01-herny-dizajn.md`. Implementácia je v `Source/DoItAsphalt/Game/DIACheats.cpp`.

## 12. Zmeny oproti návrhu

Pozri kapitolu 10 dokumentu `01-herny-dizajn.md`. Navyše z technickej strany:

- Návrh počítal s mriežkou `FCoverageGrid`; hra používa `FLotGrid` s tromi stavmi (voľný, surový, hotový) namiesto dvoch.
- Robot v návrhu nemal obrys sektora; obrys je parameter materiálu zeme.
- `ContinueGame` v `ADIAHUD` je verejná, aby cheat `dia.Load` použil rovnakú cestu ako tlačidlo Continue.
