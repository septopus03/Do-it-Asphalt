# HUD a UI „Výstražné značky" (smer B) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** HUD, obchod, menu a obrazovka výhry vyzerajú ako žlto-čierne tabule zo stavby podľa predlohy B; správanie, texty a modely ostávajú.

**Architecture:** Prestavba na mieste v čistom Slate. Najprv vzniknú stavebné prvky (písmo, paleta, tabuľa, šrafovanie) vedľa dnešných, potom sa na ne po jednom prepnú HUD, obchod a menu s výhrou, nakoniec sa zmaže stará paleta. Všetko, čo sa dá rozhodnúť bez widgetu (farby stavu karty, delenie výzvy, zvýraznené tlačidlo), je čistá funkcia s testom.

**Tech Stack:** Unreal Engine 5.8.4, C++, Slate, Automation Tests (`DIA_TEST`), bash skripty v `Scripts/`.

**Spec:** `docs/superpowers/specs/2026-11-22-hud-vystrazne-znacky-design.md`. Presné rozmery, farby a veľkosti písma sú v predlohe `docs/superpowers/specs/2026-11-22-hud-vystrazne-znacky-predloha/` (`B-HUD.html`, `B-Shop.html`, `B-Screens.html`): čítaj inline štýly, stránky sa samy nevykreslia. Vykonávateľ číta plán spolu so specom a predlohou.

## Modely

| Značka | Kto úlohu robí | Parameter pri spúšťaní subagenta |
|---|---|---|
| **[Sonnet 5.5]** | každá úloha (C++, testy, shell) aj kontrola kódu po úlohe | `model: "sonnet"` |
| **[Fable 5.2]** | riadi plán, porovnáva snímky s predlohou, záverečná kontrola | – |

## Global Constraints

- Engine: `/Users/Shared/Epic Games/UE_5.8` (5.8.4). API enginu overuj v zdrojákoch na disku, nie z pamäti.
- Žiadne Blueprinty, UMG, textúry ani ručne vytvorené `.uasset`. UI ostáva v `Source/DoItAsphalt/UI`.
- `UI/DIAFormat.h/.cpp` sa nemení. Očakávania existujúcich testov sa nemenia.
- Predloha je v CSS pixeloch pri 1920 × 1080: **dĺžky 1 px = 1 Slate jednotka; veľkosť písma v Slate = CSS px × 0,75**, zaokrúhlené na celé číslo.
- Farby sa zapisujú ako `FLinearColor(FColor(R, G, B, A))` z hex hodnôt specu, nikdy ako holé desatinné čísla.
- Všetky texty UI sa zobrazujú verzálkami cez `ETextTransformPolicy::ToUpper` a bez tieňa.
- Identifikátory a komentáre po anglicky, texty v hre po anglicky cez `LOCTEXT` / `NSLOCTEXT` (existujúce kľúče sa zachovajú).
- Cesta k projektu obsahuje medzery: každá cesta v každom príkaze je v úvodzovkách.
- Nové testy: `DoItAsphalt.Game.Ui.*` a `DoItAsphalt.Game.Menu.*`, deklarované makrom `DIA_TEST`, v `#if WITH_DEV_AUTOMATION_TESTS`.
- Pred koncom každej úlohy prejde `Scripts/build.sh` aj `Scripts/test.sh` (`FAILED 0`, počet `PASSED` neklesne pod základ z Task 0). Skripty nikdy nebežia dva naraz.
- Vizuálne overenie: `Scripts/shot.sh` (okno 1280 × 720); PNG zo `Saved/Screenshots/` vykonávateľ otvorí a porovná s predlohou.
- `Do it Asphalt - dokumentacia.docx`, `Config/DefaultEngine.ini`, `Config/DefaultInput.ini`, `Build/`, `.Rhistory`, `.Rapp.history` sú používateľove: nestageovať, nemeniť.
- **Commity:** podľa rozhodnutia používateľa v Task 0. Ak commity povolil, každá úloha končí commitom len svojich súborov (`git add` s presnými cestami, nikdy `git add -A`). Ak nie, krok „Commit" sa nahrádza výpisom `git status --short` zmenených súborov úlohy.

## Review Focus

1. **Súbory písma chýbajú** (zabalený build bez priečinka, zmazaný súbor): UI ďalej ukazuje text náhradným písmom z enginu, nespadne a neukáže prázdne tabule. → Task 1 (`Ui.Font.FallsBackWhenMissing`, staging v `DefaultGame.ini`).
2. **Výzva v inom tvare** (bez oddeľovača, dva riadky, tri riadky, prázdna): žiadny text sa nestratí, prázdna výzva nenakreslí tabuľu. → Task 3 (`Ui.Prompt.Split`).
3. **Riadok ďalšieho kroku bez predpony** (zmenený text): zobrazí sa celý, nikdy prázdny. → Task 3 (`Ui.NextLine.StripLabel`).
4. **Šrafovanie s nulovou alebo obrovskou veľkosťou** (šírka 0 pri prvom prechode rozloženia, pás 0): žiadne delenie nulou ani nekonečná slučka. → Task 2 (`Ui.Stripes.Degenerate`).
5. **Dlhé texty pri 1280 × 720** (dvojriadkový oznam o odomknutí, „Faster Auto Paver", „needs Fast Auto Paver"): zalomia sa, nepretŕčajú z tabule a tabule sa neprekrývajú. → Task 3 krok 7, Task 4 krok 6 (snímky s vymenovanými cheatmi).

---

### Task 0 [Fable 5.2]: Základ a commity

**Files:** žiadne zmeny kódu.

- [ ] **Step 1: Rozhodnutie o commitoch.** Používateľ povie, či sa majú rozpracované zmeny na `feature/settings-fluid` commitnúť. Ak áno: commit rozpracovaných zmien (bez súborov z Global Constraints), potom `git switch -c feature/hud-signage`. Ak nie: pracuje sa v strome bez commitov.
- [ ] **Step 2: Základ.** `Scripts/build.sh && Scripts/test.sh`. Zapíš si vypísané `PASSED n / FAILED m`. Ak `m > 0` alebo build neprejde, zastav a ohlás to používateľovi: plán predpokladá zelený základ.

### Task 1 [Sonnet 5.5]: Písmo a paleta

**Files:**
- Create: `Content/Slate/Fonts/BarlowCondensed-SemiBold.ttf`, `-Bold.ttf`, `-ExtraBold.ttf`, `OFL.txt`
- Create: `Source/DoItAsphalt/UI/DIAHudFont.h`, `Source/DoItAsphalt/UI/DIAHudFont.cpp`
- Modify: `Source/DoItAsphalt/UI/DIAHudStyle.h` (pridať, nič nemazať)
- Modify: `Config/DefaultGame.ini`
- Test: `Source/DoItAsphalt/Tests/UiStyleTest.cpp` (nový)

**Interfaces:**
- Produces:
  - `enum class EDIAFontWeight : uint8 { SemiBold, Bold, ExtraBold };`
  - `FString AsphaltHud::FontFileName(EDIAFontWeight)` → `"BarlowCondensed-SemiBold.ttf"`, `"BarlowCondensed-Bold.ttf"`, `"BarlowCondensed-ExtraBold.ttf"`
  - `FSlateFontInfo AsphaltHud::FontFromDir(const FString& Dir, EDIAFontWeight Weight, int32 Size)`: súbor z `Dir`; ak neexistuje, `FCoreStyle::GetDefaultFontStyle("BoldCondensed", Size)`
  - `FSlateFontInfo AsphaltHud::Font(EDIAFontWeight Weight, int32 Size)`: `FontFromDir(FPaths::ProjectContentDir() / TEXT("Slate/Fonts"), …)`
  - v `AsphaltHud` (`DIAHudStyle.h`): `SignYellow #FFC61A`, `SignBlack #111214`, `SignWhite #FFFFFF`, `SignOrange #FF7A1A`, `SignOrangeText #FF9A4D`, `SignLackPrice #FF9A80`, `SignCardLack #2C2D31`, `SignCardLocked #1D1E21`, `SignStripeDark #4A3F14`, `SignButtonHover #2C2D31`, `SignButtonPressed #000000`, `SignDimText` = biela s alfa 0,70
  - `FLinearColor AsphaltHud::Over(const FLinearColor& Top, const FLinearColor& Bottom)`: `Top` položená cez nepriehľadnú `Bottom` podľa `Top.A`
  - `float AsphaltHud::ContrastRatio(const FLinearColor& A, const FLinearColor& B)`: pomer kontrastu WCAG z lineárnych zložiek (jas `0.2126 R + 0.7152 G + 0.0722 B`, pomer `(L1 + 0.05) / (L2 + 0.05)`)

- [ ] **Step 1: Stiahni písmo** (používateľ stiahnutie schválil 2026-11-22). Pre každý zo štyroch súborov:

```bash
curl -fL -o "Content/Slate/Fonts/BarlowCondensed-Bold.ttf" "https://raw.githubusercontent.com/google/fonts/main/ofl/barlowcondensed/BarlowCondensed-Bold.ttf"
```

Over: `file "Content/Slate/Fonts/"*.ttf` vypíše pri každom „TrueType Font data" a každý má 40 – 400 kB; `OFL.txt` obsahuje „SIL Open Font License". Ak sťahovanie zlyhá alebo súbor nie je TTF, zastav a ohlás to; nič iné nesťahuj.

- [ ] **Step 2: Napíš padajúce testy** v `UiStyleTest.cpp`:

```cpp
DIA_TEST(FUiFontFilesTest, "DoItAsphalt.Game.Ui.Font.FilesExist")
// for each EDIAFontWeight W: FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Slate/Fonts") / AsphaltHud::FontFileName(W)) is true
// AsphaltHud::Font(EDIAFontWeight::Bold, 20).TypefaceFontName != FName("BoldCondensed"); Size == 20

DIA_TEST(FUiFontFallbackTest, "DoItAsphalt.Game.Ui.Font.FallsBackWhenMissing")
// const FSlateFontInfo F = AsphaltHud::FontFromDir(TEXT("/no/such/dir"), EDIAFontWeight::ExtraBold, 33);
// F.TypefaceFontName == FName("BoldCondensed"); F.Size == 33; F.HasValidFont() is true

DIA_TEST(FUiPaletteContrastTest, "DoItAsphalt.Game.Ui.Palette.Contrast")
// ContrastRatio(SignBlack, SignYellow) >= 4.5      ContrastRatio(SignWhite, SignBlack) >= 4.5
// ContrastRatio(SignYellow, SignBlack) >= 4.5      ContrastRatio(SignOrangeText, SignBlack) >= 4.5
// ContrastRatio(SignLackPrice, SignCardLack) >= 4.5
// ContrastRatio(Over(SignDimText, SignCardLocked), SignCardLocked) >= 4.5
// ContrastRatio(SignWhite, SignWhite) is 1 (within 0.001); ContrastRatio(A, B) == ContrastRatio(B, A)
```

- [ ] **Step 3: Over, že padajú.** `Scripts/build.sh` zlyhá na neznámych symboloch (to je očakávané zlyhanie).
- [ ] **Step 4: Implementuj** rozhranie z bloku Interfaces. Presné meno poľa a metódy `FSlateFontInfo` over v `SlateCore/Public/Fonts/SlateFontInfo.h`; ak sa líšia, uprav test tak, aby tvrdil to isté.
- [ ] **Step 5: Staging.** Do `Config/DefaultGame.ini`, sekcia `[/Script/UnrealEd.ProjectPackagingSettings]`, pridaj `+DirectoriesToAlwaysStageAsUFS=(Path="Slate")` (UAT priečinok `Content/Slate` projektu sám nezabalí).
- [ ] **Step 6: Over.** `Scripts/build.sh && Scripts/test.sh DoItAsphalt.Game.Ui` → `FAILED 0`, tri nové testy; potom `Scripts/test.sh` → `FAILED 0`.
- [ ] **Step 7: Commit** `feat(ui): sign palette and Barlow Condensed font`.

### Task 2 [Sonnet 5.5]: Tabuľa, text a šrafovanie

**Files:**
- Modify: `Source/DoItAsphalt/UI/DIAHudWidgets.h`, `Source/DoItAsphalt/UI/DIAButtonStyle.h` (pridať, staré funkcie zatiaľ nechať)
- Create: `Source/DoItAsphalt/UI/SDIAHazardStripes.h`, `Source/DoItAsphalt/UI/SDIAHazardStripes.cpp`
- Test: `Source/DoItAsphalt/Tests/UiStyleTest.cpp`

**Interfaces:**
- Consumes: `AsphaltHud::Font`, farby `Sign*` (Task 1).
- Produces:
  - `FLinearColor AsphaltHud::Faded(const FLinearColor& Color, float Alpha)`: tá istá farba s `A * clamp(Alpha, 0, 1)`
  - `TSharedRef<SWidget> AsphaltHud::MakePlate(TAttribute<FLinearColor> Plate, TAttribute<FLinearColor> Ring, const FMargin& Padding, const TSharedRef<SWidget>& Content, float RingWidth = 3.f, TAttribute<float> Opacity = 1.f)`: tri vnorené `SBorder` (farba tabule, polomer 6, odsadenie `RingWidth`; farba linky, polomer 3, odsadenie `RingWidth`; farba tabule, `Padding`); každá vrstva násobí alfu hodnotou `Opacity` cez `Faded`. Štetce sú statické `FSlateRoundedBoxBrush` vnútri funkcie.
  - `TSharedRef<STextBlock> AsphaltHud::MakeText(TAttribute<FText> Text, EDIAFontWeight Weight, int32 Size, TAttribute<FSlateColor> Color)`: verzálky, bez tieňa
  - `FButtonStyle AsphaltHud::MakeSignButtonStyle()` v `DIAButtonStyle.h`: `SignBlack` / `SignButtonHover` / `SignButtonPressed`, polomer 4; vypnuté je priehľadné s obrysom 2 bielej s alfa 0,40. Dnešný `MakeButtonStyle` ostáva do Task 6.
  - `TArray<float> AsphaltHud::HazardStripeStarts(float Width, float Height, float Band)`: X ľavého dolného rohu každého pásu; pásy sú rovnobežníky naklonené o 45° (horná hrana posunutá o `+Height`), prvý začína na `-Height`, krok `2 * Band`, kým `X < Width`; pri `Width <= 0`, `Height <= 0` alebo `Band <= 0` prázdne pole; najviac 4096 prvkov
  - `class SDIAHazardStripes : public SLeafWidget` s `SLATE_ARGUMENT(float, Band)`, `SLATE_ARGUMENT(float, Height)`, `SLATE_ATTRIBUTE(FLinearColor, StripeColor)`, `SLATE_ATTRIBUTE(FLinearColor, GroundColor)`, `SLATE_ATTRIBUTE(float, Opacity)`; `ComputeDesiredSize` vracia `(0, Height)`; `OnPaint` vyplní plochu `GroundColor` a nakreslí pásy z `HazardStripeStarts` orezané na vlastnú geometriu

- [ ] **Step 1: Napíš padajúce testy:**

```cpp
DIA_TEST(FUiFadedTest, "DoItAsphalt.Game.Ui.Plate.Faded")
// Faded(SignYellow, 0.5f): R, G, B unchanged, A == 0.5
// Faded(SignDimText, 0.5f).A == 0.35 (within 0.001); Faded(c, 2.f).A == c.A; Faded(c, -1.f).A == 0

DIA_TEST(FUiStripesTest, "DoItAsphalt.Game.Ui.Stripes.Starts")
// S = HazardStripeStarts(100.f, 12.f, 12.f): S[0] == -12; S[i + 1] - S[i] == 24 for all i; S.Last() < 100; S.Last() + 24 >= 100

DIA_TEST(FUiStripesDegenerateTest, "DoItAsphalt.Game.Ui.Stripes.Degenerate")
// HazardStripeStarts(0, 12, 12), (100, 0, 12), (100, 12, 0), (100, 12, -3) are all empty
// HazardStripeStarts(1.0e6f, 12.f, 0.001f).Num() == 4096
// SNew(SDIAHazardStripes).Band(12.f).Height(20.f): SlatePrepass(1.f); GetDesiredSize().Y == 20
```

- [ ] **Step 2: Over, že padajú** (`Scripts/build.sh` zlyhá na neznámych symboloch).
- [ ] **Step 3: Implementuj** rozhranie. Spôsob kreslenia rovnobežníkov (`FSlateDrawElement::MakeCustomVerts` alebo otočené obdĺžniky) si vyber podľa API v `SlateCore/Public/Rendering/DrawElements.h`; orezanie cez `EWidgetClipping::ClipToBounds`.
- [ ] **Step 4: Over.** `Scripts/build.sh && Scripts/test.sh` → `FAILED 0`.
- [ ] **Step 5: Commit** `feat(ui): plate, text and hazard stripes building blocks`.

### Task 3 [Sonnet 5.5]: HUD

**Files:**
- Create: `Source/DoItAsphalt/UI/DIAHudText.h`, `Source/DoItAsphalt/UI/DIAHudText.cpp`
- Modify: `Source/DoItAsphalt/UI/SDIAHudOverlay.h`, `Source/DoItAsphalt/UI/SDIAHudOverlay.cpp`
- Modify: `Source/DoItAsphalt/UI/DIAHudStyle.h` (nové rozmery HUD)
- Test: `Source/DoItAsphalt/Tests/UiTextTest.cpp` (nový)

**Interfaces:**
- Consumes: `MakePlate`, `MakeText`, `SDIAHazardStripes`, `Font`, farby `Sign*`.
- Produces:
  - `struct FDIAPromptLine { FText Key; FText Action; };`
  - `inline constexpr int32 DIAHudText::MaxPromptLines = 2;`
  - `TArray<FDIAPromptLine> DIAHudText::SplitPrompt(const FText& Prompt)`: delí na riadky podľa `\n`, každý riadok na prvom výskyte `" – "` (U+2013 s medzerami); riadok bez oddeľovača má prázdny `Key`; prázdne riadky sa vynechajú; riadky nad `MaxPromptLines` sa pripoja k `Action` posledného cez `" · "`
  - `FText DIAHudText::StripLabel(const FText& Line)`: text za prvým `": "`; celý `Line`, ak tam `": "` nie je alebo by zvyšok bol prázdny

- [ ] **Step 1: Napíš padajúce testy:**

```cpp
DIA_TEST(FUiPromptSplitTest, "DoItAsphalt.Game.Ui.Prompt.Split")
// "E – Push"                 -> 1 line: Key "E", Action "Push"
// "E – Pull\nF – Open flap"  -> 2 lines: ("E", "Pull"), ("F", "Open flap")
// "No room here"             -> 1 line: Key "", Action "No room here"
// ""                         -> 0 lines
// "E – Get in – now"         -> 1 line: Key "E", Action "Get in – now"
// "A – a\nB – b\nC – c"      -> 2 lines: ("A", "a"), ("B", "b · C – c")
// every DIAFormat::MovablePrompt(M, bHeld) and DIAFormat::CarPrompt(b) splits into lines whose Key is not empty

DIA_TEST(FUiNextLineTest, "DoItAsphalt.Game.Ui.NextLine.StripLabel")
// "Next: Exit – entry 6 / 12, cells 1 / 2" -> "Exit – entry 6 / 12, cells 1 / 2"
// "Exit" -> "Exit";  "" -> "";  "Next: " -> "Next: "
// "Paver: sector 12 · 143 / 400" -> "sector 12 · 143 / 400"
```

- [ ] **Step 2: Over, že padajú**, potom implementuj `DIAHudText` a over `Scripts/test.sh DoItAsphalt.Game.Ui` → `FAILED 0`.
- [ ] **Step 3: Prestav `SDIAHudOverlay`** podľa specu, časť 6 „HUD", a hodnôt z `B-HUD.html`. Rozhodnutia, ktoré predloha neukazuje:
  - `ScreenMargin` 28, `BottomMargin` 26; oznamy 110 a kontrolný pohľad 268 od horného okraja (ako predloha).
  - Riadok ďalšieho kroku: štítok „NEXT" (`NSLOCTEXT("DIAHud", "NextTag", "Next")`) + `StripLabel(View.Next)`. Riadok robota ostáva celý („Paver: …").
  - Výzva: `MaxPromptLines` vopred postavených riadkov (kláves + činnosť), riadok je `Collapsed`, kým preň `SplitPrompt` nič nevráti; tabuľa klávesu je `Collapsed` pri prázdnom `Key`. Delenie sa počíta len pri zmene textu výzvy, nie v každom snímku.
  - Oznam: `STextBlock` s `AutoWrapText(true)`, `WrapTextAt(900)`, zarovnanie na stred; `Opacity` tabule = `GetToasts().GetAlpha(Index)`.
  - Kosoštvorec: `MakePlate(SignOrange, SignBlack, …, 4.f)` v `SBox` 74 × 74 s `RenderTransform` otočenia o 45° okolo stredu; šípka vnútri má doterajší výpočet uhla mínus 45°.
  - Políčko lišty, ktoré sa nedá vziať: názov `SignDimText`, ukazovateľ `Hidden` (ako dnes).
  - Zameriavač, viditeľnosti, `HitTestInvisible` a väzba na model sa nemenia.
- [ ] **Step 4: Over.** `Scripts/build.sh && Scripts/test.sh` → `FAILED 0`.
- [ ] **Step 5: Snímka HUD.** `Scripts/shot.sh hud_b "dia.Give 9450; dia.Item Scoop; dia.Item Bucket; dia.Item AutoPaver; dia.Plant 3; dia.Complete 6412; dia.Hand Bucket; dia.Fill; dia.CheckView 1"`. Otvor `Saved/Screenshots/hud_b.png` a porovnaj s `B-HUD.html`: žltá tabuľa kreditov vľavo hore, čierna tabuľa postupu so šrafovanou dráhou vpravo hore, oranžový kosoštvorec so šípkou, lišta štyroch políčok s vybraným žltým a šrafovaný pás pod ňou.
- [ ] **Step 6: Snímka výzvy.** Postav vozík a kufor na dosah hráča (`dia.Item Cart; dia.Item Suitcase; dia.Place …` so súradnicami pri štarte hráča) a odfoť `hud_b_prompt`: žltý štvorec s klávesom a čierna tabuľa s činnosťou; pri kufri dva riadky pod sebou.
- [ ] **Step 7: Snímka dlhého oznamu.** `Scripts/shot.sh hud_b_toast "dia.CompleteEntry 2; dia.Complete 350"` (odomkne Strip Work, oznam má dva riadky). Text je zalomený vnútri žltej tabule, tabuľa nepretŕča z obrazu a neprekrýva tabuľu postupu ani kreditov.
- [ ] **Step 8: Commit** `feat(ui): HUD as hazard signage`.

### Task 4 [Sonnet 5.5]: Obchod

**Files:**
- Create: `Source/DoItAsphalt/UI/DIAShopLook.h`, `Source/DoItAsphalt/UI/DIAShopLook.cpp`
- Modify: `Source/DoItAsphalt/UI/SDIAShopPanel.h`, `Source/DoItAsphalt/UI/SDIAShopPanel.cpp`
- Modify: `Source/DoItAsphalt/UI/DIAHudStyle.h` (nové rozmery obchodu)
- Test: `Source/DoItAsphalt/Tests/UiStyleTest.cpp`

**Interfaces:**
- Consumes: `MakePlate`, `MakeText`, `SDIAHazardStripes`, `Font`, `Over`, `ContrastRatio`, farby `Sign*`; `EDIAShopState` z `UI/DIAFormat.h`.
- Produces:
  - `struct FDIAShopLook { FLinearColor Plate; FLinearColor Ring; FLinearColor Text; FLinearColor TileFill; FLinearColor TileText; FLinearColor Price; bool bShowsButton; bool bButtonEnabled; FText ButtonLabel; };`
  - `FDIAShopLook AsphaltHud::ShopLook(EDIAShopState State)` s hodnotami z tabuľky stavov v specu, časť 6 „Obchod" (linky 45 % a 18 % sú biela s alfa 0,45 a 0,18; dlaždica zamknutej karty biela s alfa 0,22 a biele písmeno). `ButtonLabel`: „BUY" (`"DIAHud", "ShopBuy"`) pre `Buy` a `NoCredits`, „LOCKED" (`"DIAHud", "ShopLocked"`) pre `Locked`, prázdny pre `Owned`.

- [ ] **Step 1: Napíš padajúci test:**

```cpp
DIA_TEST(FUiShopLookTest, "DoItAsphalt.Game.Ui.Shop.Look")
// Buy:       Plate == SignYellow, Ring == SignBlack, bShowsButton, bButtonEnabled, ButtonLabel "BUY"
// NoCredits: Plate == SignCardLack, Price == SignLackPrice, bShowsButton, !bButtonEnabled, ButtonLabel "BUY"
// Locked:    Plate == SignCardLocked, bShowsButton, !bButtonEnabled, ButtonLabel "LOCKED"
// Owned:     Plate == SignBlack, Ring == SignWhite, !bShowsButton, ButtonLabel empty
// the four Plate colours are pairwise different
// for every state: ContrastRatio(Over(Text, Plate), Plate) >= 4.5 and ContrastRatio(Over(Price, Plate), Plate) >= 4.5
// for every state: ContrastRatio(Over(TileText, Over(TileFill, Plate)), Over(TileFill, Plate)) >= 4.5
```

- [ ] **Step 2: Over, že padá**, implementuj `ShopLook`, over `Scripts/test.sh DoItAsphalt.Game.Ui` → `FAILED 0`.
- [ ] **Step 3: Prestav `SDIAShopPanel`** podľa specu, časť 6 „Obchod", a `B-Shop.html`. Rozhodnutia:
  - Panel čierny, polomer 8, šírka obsahu 1300; šrafovaný pás 20 hore cez celú šírku panela; žiadne stmavenie okolo, koreň ostáva `SelfHitTestInvisible`.
  - Karta = `MakePlate` s farbami z `ShopLook(State)` čítanými cez lambdy; výška 172, medzera 12, 4 stĺpce.
  - Tlačidlo BUY: štýl `MakeSignButtonStyle()` (Task 2), text `SignYellow`, vo vypnutom stave `SignDimText`. Ostáva `SDIAShopButton`, `IsFocusable(false)`, `OnPressed` / `OnClicked` bez zmeny.
  - „OWNED": text `SignWhite` (ExtraBold) a pred ním fajka ako text „✓"; ak ju písmo nemá, nakresli ju dvoma čiarami.
  - Miestne funkcie `ButtonLabel`, `PriceColor`, `OutlineColor`, `IconColor` v `SDIAShopPanel.cpp` sa mažú, nahrádza ich `ShopLook`.
- [ ] **Step 4: Over.** `Scripts/build.sh && Scripts/test.sh` → `FAILED 0` (vrátane `DoItAsphalt.Game.Shop.*`).
- [ ] **Step 5: Snímka všetkých štyroch stavov.** `Scripts/shot.sh shop_b "dia.Give 9450; dia.Item Scoop; dia.Item Bucket; dia.Item Suitcase; dia.Item Cart; dia.CompleteEntry 2; dia.CompleteEntry 3; dia.CompleteEntry 4; dia.Complete 6000; dia.Item Station; dia.Item Finisher; dia.Item AutoPaver; dia.Plant 3; dia.Shop"`. Na snímke musia byť: žlté karty (Plant, Fast Auto Paver), sivá karta s červenkastou cenou (Paver Car), tmavé zamknuté karty s dôvodom žltou (Faster Auto Paver, Wide Bar) a čierne karty „OWNED". Ak cheaty stav netrafia, uprav ich a zapíš použitý príkaz do správy.
- [ ] **Step 6: Dlhé texty.** Na tej istej snímke: „FASTER AUTO PAVER" a „needs Fast Auto Paver" sú celé vnútri karty, nič neprekrýva cenu ani tlačidlo.
- [ ] **Step 7: Commit** `feat(ui): shop as sign plates`.

### Task 5 [Sonnet 5.5]: Menu, nastavenia a výhra

**Files:**
- Modify: `Source/DoItAsphalt/UI/DIAMenuModel.h`
- Modify: `Source/DoItAsphalt/UI/DIAHudWidgets.h`, `Source/DoItAsphalt/UI/DIAButtonStyle.h`
- Modify: `Source/DoItAsphalt/UI/SDIAMenu.h/.cpp`, `Source/DoItAsphalt/UI/SDIAWinScreen.h/.cpp`
- Modify: `Source/DoItAsphalt/Game/DIACheats.cpp`, `Source/DoItAsphalt/UI/DIAHUD.h/.cpp` (len cheat `dia.Screen`)
- Test: `Source/DoItAsphalt/Tests/UiTextTest.cpp`

**Interfaces:**
- Consumes: `MakePlate`, `MakeText`, `MakeSignButtonStyle`, `SDIAHazardStripes`, `Font`, farby `Sign*`.
- Produces:
  - `enum class EDIAMenuButton : uint8 { None, Continue, Resume, NewGame, Yes };`
  - `EDIAMenuButton FDIAMenuModel::PrimaryButton() const`: `Yes` pri otázke; `None` v nastaveniach a pri zatvorenom menu; inak `Continue` (hlavné menu s uloženou hrou), `Resume` (pauza), `NewGame` (hlavné menu bez uloženej hry)
  - `FSliderStyle AsphaltHud::MakeSliderStyle()`: dráha a jazdec `SignBlack`
  - `TSharedRef<SBox> AsphaltHud::MakeMenuButton(const FText& Label, float Width, FOnClicked OnClicked, const FButtonStyle* ButtonStyle, TAttribute<bool> bPrimary = false)`: výška 64, text ExtraBold 24, `SignYellow` ak `bPrimary`, inak `SignWhite`
  - `TSharedRef<SOverlay> AsphaltHud::MakeScreenPanel(float ContentWidth, const TSharedRef<SWidget>& Content)`: stmavené pozadie `ScreenBackdropColor` + žltá tabuľa s linkou 4 (parameter štetca zaniká)
  - cheat `dia.Screen <Pause|Settings|Question|Win>` (mimo Shipping): otvorí danú obrazovku, aby sa dala odfotiť

- [ ] **Step 1: Napíš padajúci test:**

```cpp
DIA_TEST(FMenuPrimaryButtonTest, "DoItAsphalt.Game.Menu.PrimaryButton")
// closed model -> None
// Open(Main, true) -> Continue;  Open(Main, false) -> NewGame;  Open(Pause, false) -> Resume
// Open(Pause, true), bConfirming = true -> Yes
// Open(Pause, true), OpenSettings() -> None; CloseSettings() -> Resume
```

- [ ] **Step 2: Over, že padá**, implementuj `PrimaryButton`, over test.
- [ ] **Step 3: Prestav stavebné funkcie a obe obrazovky** podľa specu, časť 6 „Menu a výhra", a `B-Screens.html`. Rozhodnutia:
  - Menu: šírka obsahu 352 (tabuľa 440 s odsadením 44), medzera medzi tlačidlami 12, nadpis ExtraBold 60 `SignBlack`; otázka Bold 20 `SignBlack`.
  - Nastavenia: popis Bold 20 a hodnota ExtraBold 30 čiernou v jednom riadku (popis vľavo, hodnota vpravo; doterajší text „Mouse sensitivity: {0}" sa rozdelí na `NSLOCTEXT("DIAHud", "MenuSensitivityLabel", "Mouse sensitivity")` a číslo), pod nimi `SSlider` so štýlom `MakeSliderStyle()`.
  - Výhra: šírka obsahu 472 (tabuľa 560), hore `SDIAHazardStripes` (pás 16, výška 20, `SignBlack` na `SignYellow`), nadpis ExtraBold 48, riadky oddelené čiarou 3 `SignBlack`, popis Bold 20, hodnota ExtraBold 26; parameter `ValueColor` v `BuildStatRow` zaniká.
  - `dia.Screen`: nová verejná metóda `ADIAHUD::DebugShowScreen(const FString& Name)` v `#if !UE_BUILD_SHIPPING`, cheat ju len volá; `Win` volá `ShowWinScreen()`, ostatné `OpenMenu(EDIAMenuMode::Pause)` a nastavia model.
- [ ] **Step 4: Over.** `Scripts/build.sh && Scripts/test.sh` → `FAILED 0`.
- [ ] **Step 5: Snímky.** `Scripts/shot.sh --menu menu_b_main`, `Scripts/shot.sh menu_b_pause "dia.Screen Pause"`, `… menu_b_settings "dia.Screen Settings"`, `… menu_b_question "dia.Screen Question"`, `… win_b "dia.Screen Win"`. Každá: žltá tabuľa s čiernou linkou v strede stmaveného obrazu, čierne tlačidlá, prvé tlačidlo žltým textom (v nastaveniach žiadne), na výhre šrafovaný pás a päť riadkov.
- [ ] **Step 6: Commit** `feat(ui): menu, settings and win screen as sign plates`.

### Task 6 [Sonnet 5.5]: Upratanie a dokumentácia

**Files:**
- Modify: `Source/DoItAsphalt/UI/DIAHudStyle.h`, `Source/DoItAsphalt/UI/DIAHudWidgets.h`, `Source/DoItAsphalt/UI/DIAButtonStyle.h`
- Modify: `docs/01-herny-dizajn.md` (časť 6 a tabuľka cheatov), `README.md` (počet testov)

- [ ] **Step 1: Zmaž nepoužívané.** Z `DIAHudStyle.h` každú konštantu, ktorú po Task 3 – 5 nič nepoužíva (over `grep -rn "AsphaltHud::<Name>" Source` pre každú), `MakeShadowedText` z `DIAHudWidgets.h` a starý `MakeButtonStyle` z `DIAButtonStyle.h`. Žiadne volanie `FCoreStyle::GetDefaultFontStyle` nesmie ostať v `Source/DoItAsphalt/UI` okrem `DIAHudFont.cpp`: `grep -rn "GetDefaultFontStyle" Source/DoItAsphalt/UI` vypíše len ten jeden súbor.
- [ ] **Step 2: Dokumentácia.** V `docs/01-herny-dizajn.md`, časť 6: prepíš popis vzhľadu podľa specu (tabule, karta OWNED je čierna tabuľa, dlaždica je kruh s písmenom, riadok ruky je nad lištou predmetov, „Next" je štítok); do tabuľky cheatov pridaj `dia.Screen <Pause\|Settings\|Question\|Win>` a oprav počet cheatov vo vete nad tabuľkou. V `README.md` oprav riadok s počtom testov podľa výstupu `Scripts/test.sh`.
- [ ] **Step 3: Over.** `Scripts/build.sh && Scripts/test.sh` → `FAILED 0`; `PASSED` = základ z Task 0 + 10 nových testov.
- [ ] **Step 4: Commit** `refactor(ui): drop the old HUD palette; docs for the sign HUD`.

### Task 7 [Fable 5.2]: Záverečná kontrola

- [ ] **Step 1:** Otvor všetkých deväť snímok (`hud_b`, `hud_b_prompt`, `hud_b_toast`, `shop_b`, `menu_b_main`, `menu_b_pause`, `menu_b_settings`, `menu_b_question`, `win_b`) a porovnaj s predlohou; rozdiely vymenuj používateľovi.
- [ ] **Step 2:** Prejdi body Review Focus 1 – 5 proti kódu a snímkam.
- [ ] **Step 3:** Ohlás používateľovi výsledok testov, snímky a rozdiely oproti predlohe.
