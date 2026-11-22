# HUD a UI v štýle „Výstražné značky" (smer B) – návrh

Dátum: 2026-11-22. Predloha: canvas [Do it Asphalt – návrhy HUD a UI](https://claude.ai/artifact/T8eVZftiHoGtQ4up6LKTg2), riadok **B** (obrazovky „B · HUD", „B · Obchod", „B · Menu, nastavenia, výhra").

## 1. Zámer

Používateľ si zo šiestich návrhov vybral smer B: celé UI hry má vyzerať ako žlto-čierne tabule zo stavby. Dnešné UI (tmavé polopriehľadné panely, oranžové tlačidlá, predvolené písmo) funguje, ale nemá vlastnú tvár.

## 2. Ciele, čo cieľom nie je, čomu sa vyhnúť

**Ciele**

- HUD, obchod, menu (hlavné, pauza, nastavenia, otázka Yes/No) a obrazovka výhry vyzerajú ako na predlohe B.
- Správanie ostáva: čo sa kedy zobrazuje, texty, klávesy, poradie vrstiev, pravidlá oznamov, nákup v obchode.
- Všetkých 219 doterajších testov prechádza bez úpravy ich očakávaní.

**Čo cieľom nie je**

- Nové prvky HUD (mapka, kruhové ukazovatele, kroky odomykania) – tie patrili iným smerom.
- Ikony predmetov. Dlaždica karty ostáva prvým písmenom názvu.
- Tabule pri vstupoch vo svete (3D text) a súbor `Do it Asphalt - dokumentacia.docx`.
- Preklad textov; hra ostáva po anglicky.

**Čomu sa vyhnúť**

- Zmena `DIAFormat` (texty, formáty čísel) alebo modelov len kvôli vzhľadu. Veľké písmená rieši widget, nie text.
- Tabuľa, ktorá zakryje viac obrazu než dnešný panel na tom istom mieste (hráč sa pri liatí pozerá dole).
- Stav karty v obchode rozlíšený len farbou: každý stav má aj text (BUY, LOCKED, OWNED) a červenkastú cenu pri nedostatku kreditov.

## 3. Obmedzenia

- UI je čistý Slate v C++ (`Source/DoItAsphalt/UI`), bez UMG a bez textúr. Tak to ostane.
- Rozmery v predlohe sú v CSS pixeloch pri 1920 × 1080, čo sa rovná Slate jednotkám pri mierke 1. **Veľkosť písma v Slate je v bodoch: Slate veľkosť = CSS px × 0,75** (62 px → 46).
- Farby v predlohe sú sRGB hex. V kóde sa zapisujú cez `FLinearColor(FColor(...))`, nie ako holé desatinné čísla, inak vyjdú svetlejšie.
- V pracovnom strome je asi 50 necommitnutých súborov na vetve `feature/settings-fluid`, vrátane všetkých UI súborov. Robí sa na nich, v tom istom strome; commit len na pokyn používateľa.
- Programuje Sonnet 5.5 (pravidlo projektu), Fable len plánuje a kontroluje.

## 4. Prístupy

1. **Prestavba na mieste – vybraný.** Strom widgetov, modely (`FDIAHudModel`, `FDIAShopModel`, `FDIAWinModel`, `FDIAMenuModel`) a texty ostávajú. Mení sa `DIAHudStyle.h`, spoločné stavebné funkcie v `DIAHudWidgets.h` a `DIAButtonStyle.h` a rozloženie v piatich widgetoch. Najmenej rizika, testy modelov sa nedotknú.
2. Prepísať UI do UMG s assetmi. Zahodilo by fungujúci kód a testy a pridalo binárne súbory, ktoré sa nedajú čítať v diffe. Zamietnuté.
3. Len prefarbiť a vymeniť písmo. Lacné, ale bez tabúľ a šrafovania to nie je smer B. Zamietnuté.

## 5. Vizuálny jazyk

**Farby** (nahrádzajú dnešnú paletu v `AsphaltHud`)

| Názov | Hex | Použitie |
|---|---|---|
| Žltá | `#FFC61A` | žlté tabule, linka čiernych tabúľ, výplne ukazovateľov, zvýraznený text |
| Čierna | `#111214` | čierne tabule, linka a text žltých tabúľ, tlačidlá |
| Biela | `#FFFFFF` | text na čiernych tabuliach |
| Oranžová | `#FF7A1A` | kosoštvorec a linka kontrolného pohľadu |
| Oranžový text | `#FF9A4D` | text kontrolného pohľadu na čiernej |
| Cena, ktorá chýba | `#FF9A80` | cena karty, na ktorú nie sú kredity |
| Karta bez kreditov | `#2C2D31` | podklad karty |
| Zamknutá karta | `#1D1E21` | podklad karty; text biely na 70 % |
| Tmavý pruh | `#4A3F14` | pruhy v dráhe ukazovateľa postupu |

**Tabuľa** je základný stavebný prvok: zaoblený obdĺžnik (polomer 6) vo farbe tabule, 3 jednotky od okraja 3 jednotky hrubá linka v druhej farbe, vnútri zasa farba tabule. Žltá tabuľa má čiernu linku a čierny text, čierna tabuľa žltú linku a biely text. Stavia sa ako tri vnorené `SBorder` so zaobleným bielym štetcom, každý s vlastnou farbou, aby sa dali farby meniť za behu (vybrané políčko lišty tabuľu obracia).

**Šrafovanie** (pásy pod uhlom 45°, žltá a čierna rovnako široké) kreslí nový listový widget `SDIAHazardStripes`: v `OnPaint` otočené obdĺžniky orezané na svoje hranice, parametre šírka pásu a dve farby. Použitie: pás pod lištou predmetov (výška 12, pás 12), hlavička obchodu a obrazovky výhry (výška 20, pás 16), dráha ukazovateľa postupu (pás 8, tmavý pruh a čierna).

**Písmo:** Barlow Condensed (licencia OFL), rezy SemiBold, Bold a ExtraBold, súbory v `Content/Slate/Fonts/`. Jedna funkcia `AsphaltHud::Font(rez, veľkosť)` nahradí všetky volania `FCoreStyle::GetDefaultFontStyle`. Ak sa súbory nenájdu, funkcia vráti Roboto „BoldCondensed" z enginu. **Stiahnutie súborov potrebuje súhlas používateľa** (pozri 8).

**Veľké písmená:** všetky texty UI sa zobrazujú verzálkami cez `ETextTransformPolicy::ToUpper` na `STextBlock`. Tieň textu sa ruší (text je vždy na tabuli).

## 6. Obrazovky

Presné rozmery a veľkosti sú v predlohe; tu je, čo je kde a čo sa oproti dnešku mení.

**HUD** (`SDIAHudOverlay`), okraj obrazovky 28

- Vľavo hore: žltá tabuľa, kredity (ExtraBold 46) a „CREDITS" (17).
- Vpravo hore: čierna tabuľa; postup „6,412 / 30,000 · 21%" (ExtraBold 33), „CELLS PAVED" žltou (16), ukazovateľ 440 × 18 so žltým rámikom 2, šrafovanou dráhou a žltou výplňou; riadok ďalšieho kroku so žltým štítkom „NEXT" a textom bez slova „Next:" na začiatku (štítok ho nahrádza; text sa neskracuje v `DIAFormat`, widget odreže predponu po dvojbodku); riadok robota („Paver: …"), kým ho hráč vlastní.
- Oznamy: žlté tabule, ExtraBold 24. Oznam o odomknutí má dva riadky (názov kroku a jeho účinok); text sa zalamuje pri šírke 900 a je zarovnaný na stred. Dobeh priehľadnosťou ostáva pre všetky tri vrstvy tabule.
- Kontrolný pohľad: oranžový kosoštvorec 74 (tabuľa otočená o 45°) s čiernou šípkou, ktorá sa otáča ako dnes, vedľa čierna tabuľa s oranžovou linkou a textom.
- Zameriavač: bez zmeny.
- Dole v strede zhora nadol: zásoba výrobne (čierna tabuľa, popis žltou), výzva (žltá štvorcová tabuľa s klávesom + čierna tabuľa s činnosťou), riadok ruky (čierna tabuľa, názov bielou, obsah žltou), lišta predmetov, pod ňou šrafovaný pás.
- Výzva dnes prichádza ako jeden text „E – Push" (alebo dva riadky pri kufri). Widget rozdelí každý riadok zvlášť na kláves a činnosť podľa oddeľovača „ – " (kufor tak dá dva riadky pod sebou: E a F); riadok bez oddeľovača ide celý do čiernej tabule. Miesta sú pripravené pre dva riadky, druhý je skrytý, kým je prázdny.
- Políčko lišty 140 × 94, medzera 10: čierna tabuľa, žltý štvorček 26 s číslom klávesu, názov ExtraBold 22, ukazovateľ naplnenia výšky 9. Vybrané políčko je žltá tabuľa s čiernym štvorčekom a čiernou výplňou. Políčko, ktoré sa nedá vziať, má názov bielou na 70 % a ukazovateľ skrytý (ako dnes).

**Obchod** (`SDIAShopPanel`), šírka 1300

- Čierny panel (polomer 8), hore šrafovaný pás; nadpis „SHOP" žltou (ExtraBold 57), podnadpis bielou, vpravo žltá tabuľa s kreditmi.
- Mriežka so 4 stĺpcami, karta vysoká 172, medzera 12. Kariet je 11 (výrobňa a desať položiek; finišer bol z hry odstránený 2026-11-22, predloha `B-Shop.html` ho ešte ukazuje), v poslednom riadku sú tri, zarovnané vľavo. Okolo panela sa obraz nestmavuje a hra ostáva klikateľná ako dnes (stmavenie v predlohe je len kvôli obrázku).
- Karta je tabuľa podľa stavu:

| Stav | Tabuľa | Linka | Dlaždica (kruh 46) | Cena | Vpravo dole |
|---|---|---|---|---|---|
| Dá sa kúpiť | žltá | čierna | čierna, písmeno žlté | čierna | čierne tlačidlo „BUY" žltým |
| Chýbajú kredity | `#2C2D31` | biela 45 % | biela, písmeno čierne | `#FF9A80` | vypnuté tlačidlo „BUY" s obrysom |
| Zamknuté | `#1D1E21` | biela 18 % | biela 22 % | biela 70 % | vypnuté tlačidlo „LOCKED" s obrysom |
| Vlastnené | čierna | biela | biela, písmeno čierne | – | „OWNED" bielou s fajkou |

- Farby dlaždíc podľa druhu predmetu (dnešné `ShopIcon*Color`) zanikajú. Dôvod zamknutia ostáva žltou pod hodnotou.
- Priradenie stav → farby je čistá funkcia `AsphaltHud::ShopLook(EDIAShopState)` v hlavičke, aby sa dala testovať.

**Menu** (`SDIAMenu`) a **výhra** (`SDIAWinScreen`)

- `MakeScreenPanel` stavia žltú tabuľu (linka 4) na stmavenom pozadí. Nadpis čiernou (menu ExtraBold 60, výhra 48).
- `MakeButtonStyle` a `MakeMenuButton`: čierne tlačidlo (polomer 4, výška 64), text bielou, pri prvom viditeľnom tlačidle obrazovky (Continue alebo Resume, bez uloženej hry New Game; na výhre New Game; pri otázke Yes) žltou. Pod kurzorom `#2C2D31`, stlačené `#000000`.
- Otázka „Start over? Progress will be lost." čiernou (Bold 20) namiesto nadpisu, ako dnes.
- Nastavenia: popis a hodnota čiernou na žltej, `SSlider` s čiernou dráhou a čiernym jazdcom.
- Výhra: hore šrafovaný pás (čierna a žltá), riadky oddelené čiernou čiarou 3, hodnoty ExtraBold 26, všetky čiernou (zlatá farba kreditov zaniká).

## 7. Čo sa mení v kóde

- `UI/DIAHudStyle.h`: nová paleta a rozmery; staré konštanty, ktoré nikto nepoužíva, sa mažú. Časovania a poradie vrstiev ostávajú.
- `UI/DIAHudWidgets.h`, `UI/DIAButtonStyle.h`: `Font`, `MakePlate`, text bez tieňa s verzálkami, nové tlačidlo a panel obrazovky.
- Nové `UI/SDIAHazardStripes.h/.cpp`.
- `UI/SDIAHudOverlay`, `SDIAShopPanel`, `SDIAMenu`, `SDIAWinScreen`: rozloženie podľa časti 6.
- `Content/Slate/Fonts/`: tri súbory TTF a `OFL.txt`; overiť, že sa priečinok dostane do zabaleného buildu (ak nie, doplniť do `DefaultGame.ini`).
- Testy: nové pre `ShopLook` (štyri stavy dávajú štyri rôzne dvojice tabuľa/linka a správny text tlačidla), pre rozdelenie výzvy na kláves a činnosť a pre odrezanie predpony „Next:". Rozdelenie a odrezanie sú čisté funkcie vedľa widgetu, nie v `DIAFormat`.
- `docs/01-herny-dizajn.md`, časť 6: prepísať popis vzhľadu (zelená karta OWNED → čierna tabuľa, farebná dlaždica → kruh s písmenom, poloha riadku ruky).

## 8. Otvorené pre používateľa

- **Písmo.** Barlow Condensed treba stiahnuť z `github.com/google/fonts` (priečinok `ofl/barlowcondensed`): `BarlowCondensed-SemiBold.ttf`, `-Bold.ttf`, `-ExtraBold.ttf` a `OFL.txt`, každý približne 100 kB. Bez súhlasu sa použije Roboto BoldCondensed z enginu: nič sa nesťahuje, ale je len jeden rez a nadpisy budú o niečo tenšie než na predlohe.

## 9. Hotovo znamená

- `Scripts/build.sh` prejde a `Scripts/test.sh` vypíše `FAILED 0` (219 pôvodných testov a nové).
- V spustenej hre (`Scripts/run.sh`) sú odfotené HUD, obchod (`dia.Shop`), pauza, nastavenia a výhra a zodpovedajú predlohe B; rozdiely sú vymenované.
- Pri 1280 × 720 nič nepretŕča a tabule sa neprekrývajú.

## 10. Ponechané na implementátora

- Presný spôsob kreslenia pásov v `OnPaint` (otočené obdĺžniky alebo vlastné vrcholy), ak výsledok vyzerá ako predloha a je orezaný.
- Názvy nových konštánt a pomocných funkcií.
- Drobné doladenie odsadení, ak text v Slate vychádza inak než v prehliadači.
