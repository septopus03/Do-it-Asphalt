# Do it Asphalt – herný dizajn

Verzia 2.0 · 2026-11-21 · popisuje hru Asphalt-self tak, ako je naprogramovaná

Tento dokument hovorí, ako sa hra hrá: pravidlá, ovládanie, obrazovka, cheaty a známe obmedzenia. Všetky čísla sú prevzaté z kódu (`Source/AsphaltCore/Public/LotBalance.h` a `LotLayout.h`), nie z plánu. Ako je hra postavená, je v `02-technicky-navrh.md`. Kde sa hotová hra líši od návrhu v `04-plan-zmien-asphalt-self.md`, vysvetľuje to kapitola 10.

---

## 1. Prehľad

| | |
|---|---|
| Žáner | pokojná inkrementálna simulácia asfaltovania |
| Platforma | macOS (vývoj), cieľovo aj Windows |
| Engine | Unreal Engine 5.8 (C++) |
| Kamera | prvá osoba, pracovný pohľad podržaním pravého tlačidla myši |
| Hráči | 1 |
| Cieľová dĺžka | asi 2 h 15 min bola pôvodná cieľová dĺžka; bez hladenia dáva simulácia 1 h 51 min (pozri kapitolu 9) |
| Vzhľad | parkovisko pred nákupným centrom, tlmené farby, jedna akcentová (oranžová) |

**Jednou vetou:** hráč vyleje asfalt na parkovisko v tvare písmena L (30 000 štvorcov 1 × 1 m), najprv z misky lyžicou, neskôr z vedra, kufra, vozíka, robota a auta; za každý hotový štvorec dostane 10 kreditov, za kredity kupuje lepšie vybavenie a výrobňu a po vyasfaltovaní všetkého sa otvorí východ.

## 2. Mapa

- Parkovisko je štvorec 200 × 200 m bez jedného kvadrantu 100 × 100 m: v ňom stojí nákupné centrum. Zostáva tvar L s 30 000 štvorcami (políčkami) 1 × 1 m.
- Okolo parkoviska je 12 m široký pás s obrubníkom, plotom a vstupnými cestami.
- Políčka sú zoskupené do 75 **sektorov** 20 × 20 políčok (400 políčok v sektore). Sektory sa v hre číslujú od 1 (v súbore kódu od 0).
- Okolo L je **6 vstupných úsekov**, každý široký 12 políčok. Pred každým stojí tabuľa, ktorá hovorí, aký krok odomyká (kapitola 7), a koľko z 12 políčok úseku je hotových („Entry k / 12").
- Výrobňa asfaltu stojí pri štarte, na okraji parkoviska. Pred ňou je koryto (trough) s asfaltom, z ktorého sa nabera. Hráč začína tesne pred nimi a je k nim obrátený.
- Východ je vo vstupnom úseku 6. Je to brána s ramenom; vpustí hráča na cestu von, až keď je hotové celé parkovisko.

## 3. Herná slučka

1. Hráč nabere asfalt z koryta (alebo z výpustu výrobne) do nádoby v ruke.
2. Vylieva ho na voľné políčka. Zem ukazuje, či je políčko voľné alebo hotové.
3. Keď je vyliate dosť (95 % plochy políčka), políčko sa dokončí, zvyšok sa doleje samo a hráč dostane 10 kreditov.
4. Za kredity kupuje lepšie nádoby a stroje a vylepšuje výrobňu.
5. Počet hotových políčok odomyká ďalšie kroky: rýchla výmena (10), šprint (100), pásové stroje (350), robot (800), auto (6 000) a východ (30 000).
6. Keď je hotových všetkých 30 000 políčok, otvorí sa východ. Prejsť cez bránu znamená výhru.

## 4. Asfaltovanie

### 4.1 Políčko

- Políčko má 1 × 1 m a potrebuje **10 l** asfaltu. Je hotové, keď je pokrytých **95 %** (9,5 l) jeho plochy; zvyšok sa dokončí samo.
- Za každé hotové políčko je odmena **10 kreditov**, raz. Navyše je **1 kredit za každý vyliaty liter** (pripisuje sa hneď pri liatí, nenahrádza odmenu za políčko).
- Každý texel zeme je buď voľný, alebo **hotový**. Asfalt sa nehladí: čo hráč vyleje, je rovno hotové.
- Liať sa dá len na voľnú zem. Na hotový asfalt sa neleje a litre, ktoré by naň padli, sa neplatia.

### 4.1a Tekutý asfalt

Čo sa leje **rukou** (miska, lyžica, vedro, kufor), nelegne na zem hneď. Zaplatené litre sa položia ako niekoľko horúcich kôp na prvé texely výlevu a kopy **tečú**:

- Tečie len asfalt nad hrúbkou filmu (1 texel = 0,025 l). Hrúbka sa riadi výškovým rozdielom medzi susednými texelmi, tok má zotrvačnosť, takže kopa sa rozlieva, preleje cez okraj a vlní sa (živšie než skutočná kvapalina).
- Asfalt chladne: teplota klesne na tretinu za 8 s. Čím je chladnejší, tým pomalšie tečie. Pod 12 % teploty zatuhne tam, kde leží (ostatná tenká vrstva sa stratí).
- Voľný texel, ktorý zasiahne dosť asfaltu, sa stane hotovou zemou. Pokrytie teda závisí od toho, kam asfalt steká: pri liatí na hranu políčka vyjde asfalt aj na susedné políčka a políčko sa nemusí hneď dokončiť.
- Litre sa platia pri liatí, nie pri pokrytí. Pokrytie vyjde zhruba na zaplatenú plochu (pri skúške 466 texelov za 400 zaplatených).
- Stroje (vozík, auto, robot) kladú pás **hneď** a rovnomerne; tekutina sa ich netýka. Klapka kufra sype ako ruka, teda tekutinou.
- Uložená hra drží len zem: asfalt, ktorý ešte tečie, sa pri ukladaní nechá zatuhnúť na kópii hry.

### 4.2 Nádoby v ruke

Hráč drží vždy jednu „ruku" a prepína ich klávesmi (kapitola 5).

| Ruka | Kapacita | Poznámka |
|---|---|---|
| Miska (Bowl) | 2,5 l | štart; berie sa lyžicou po 0,25 l |
| Lopatka (Scoop) | 1 l | kúpi sa za 10; záber 1 l |
| Vedro (Bucket) | 10 l | kúpi sa za 35; leje sa ako prúd 4 l/s presne tam, kam hráč mieri (kruh 30 cm) |

- Naberanie: **`R` (držať)** pri koryte opakuje záber každých 0,5 s. Ľavé tlačidlo nenabera, len leje.
- Liatie z misky a lopatky: ľavé tlačidlo, 1 l za sekundu, kruh s polomerom 15 cm (miska) alebo 10 cm (lopatka). Vyliaty asfalt je rovno hotový, bez hladenia.
- Vedro leje, kým je tlačidlo stlačené (4 l/s), a asfalt dopadá na bod pod zameriavačom, odkiaľ sa ako kvapalina rozteká. Zem, ktorá je už pokrytá, nestojí nič, takže čo sa nepoložilo, ostáva vo vedre.
- Miska a lyžica sú v **dvoch rukách**: miska v ľavej, lyžica v pravej.
- Vedro sa plní v ruke zo zásoby: z kohútika stojaceho kufra alebo vozíka, z výpustu výrobne alebo z presuvnej výdajne.
- Výmena ruky trvá 1,5 s, po odomknutí rýchlej výmeny 0 s.
- Dosah ruky je 250 cm.

### 4.3 Kufor a vozík

Dva predmety kladú asfalt rovno hotový:

- **Kufor** (Suitcase): hráč ho potiahne za sebou (stojí 62 cm za ním, naklonený o 25°), postaví sa a podržaním klávesu `F` otvorí klapku, ktorá sype 2 l/s do políčka pred ním. Zásoba je 60 l.
- **Vozík** (Cart): hráč ho drží pred sebou, podrží ľavé tlačidlo a tlačí ho; kladie pás široký 100 cm, kým sa vozík hýbe rýchlosťou do 200 cm/s. Zásoba je 200 l. Na políčko dá 9,5 l (prah dokončenia), takže plný vozík pokryje 20 políčok a 10 l mu ostane.

Oba slúžia aj ako zásoba pre vedro (kohútik, 5 l/s).

### 4.4 Bez hladenia

Hra nemá hladidlo ani finišer. Asfalt sa nehladí: každý vyliaty asfalt je hotový, len čo na zem dopadne. Políčko sa dokončí pri 95 % pokrytia a zvyšok sa dolepí samo (kapitola 4.1).

### 4.5 Výrobňa a zásoba

- Výrobňa vyrába asfalt do zásobníka. Zásobník má kapacitu **4 minúty výroby** a na štarte je v ňom 80 l.
- Úrovne výrobne (litre za minútu): **150 / 300 / 750 / 2 000 / 4 200 / 5 200**. Ceny vylepšení: 100 / 400 / 1 400 / 8 000 / 24 000 kreditov.
- Na úrovniach 2 (750 l/min) a 4 (4 200 l/min) pribudne k výrobni viditeľná prístavba. Kód a oznam „Plant level N" po nákupe číslujú úrovne od 0 (štart, 150 l/min, je úroveň 0); špecifikácia ich číslovala od 1.
- Naberanie z koryta a plnenie vedra ťahá zo zásobníka. Keď je prázdny, nič sa nenabere.

### 4.6 Stroje

- **Presuvná výdajňa** (Station, 900 kreditov, od 350 políčok): hráč ju ťahá za sebou (105 cm) a vedro, kufor aj vozík sa pri nej plnia ako pri výrobni.
- **Robot** (AutoPaver, 3 000, od 800 políčok): sám asfaltuje pridelený sektor. Hráč sa postaví do sektora a stlačí `G`. Robot najprv dôjde do políčka 0 sektora a potom ide po políčkach sektora od začiatku, hotové preskakuje a ďalej kladie na nehotové. Kladie hotový asfalt rýchlosťou 120 políčok za minútu, pričom čerpá zo zásobníka výrobne (v kuse najviac 10 l). Vylepšenia: **AutoPaverFast** (9 000, 170 políčok/min) a **AutoPaverFaster** (22 000, 220 políčok/min). Pridelený sektor má na zemi oranžový obrys (10 cm). Ak hráč stlačí `G` v sektore, kde robot už je, zobrazí sa „Paver is already here".
- **Auto** (Car, 12 000, od 6 000 políčok): hráč doň nastúpi (`E`) a jazdí. Auto nemá nádrž; leje hotový asfalt zo zásobníka výrobne, kým hráč drží ľavé tlačidlo, pásom širokým 300 cm (260 políčok/min). **CarWideBar** (24 000) rozšíri pás na 350 cm (300 políčok/min). Pri liatí sa rýchlosť auta obmedzí tak, aby sa nelialo viac, než sa stíha.

## 5. Ovládanie

| Vstup | Čo robí |
|---|---|
| `W` `A` `S` `D` | chôdza; v aute `W`/`S` plyn a spiatočka, `A`/`D` zatáčanie |
| Myš | pohľad; v aute otáča hlavou (±100° do strán, −60° až +45° hore/dole) |
| Ľavé tlačidlo myši | použiť ruku: liať (vedro ako prúd na zameraný bod); podržané s vozíkom kladie pás, v aute leje |
| Pravé tlačidlo myši (držať) | pracovný pohľad: zorné pole 90° → 60°, naklonenie o 15°, prechod 0,2 s |
| `Q` | ďalšia ruka |
| `1` `2` `3` | miska / lopatka / vedro |
| `E` | chytiť predmet, pustiť ho, nastúpiť do auta alebo vystúpiť |
| `R` (držať) | naberať: z koryta do misky či lopatky, naplniť vedro pri výrobni, kohútiku kufra či vozíka a pri výdajni |
| `F` (držať) | klapka kufra |
| `G` | poslať robota do sektora, v ktorom hráč stojí |
| Ľavý `Shift` | šprint (po odomknutí) |
| `Tab` | obchod |
| `V` (držať) | kontrolný pohľad (nie počas jazdy) |
| `Esc` | zavrie obchod, inak otvorí pauzu; obrazovka výhry ho ignoruje |

- Chôdza je 450 cm/s, šprint 1,6-krát rýchlejší. Kufor sa ťahá rýchlosťou 350 cm/s, vozík sa tlačí 400 cm/s a výdajňa sa nesie 400 cm/s.
- Počas jazdy fungujú chôdza (plyn, zatáčanie), myš, ľavé tlačidlo, `E`, `Tab` a `Esc`. Klávesy rúk, `R`, `G`, `F`, pracovný pohľad a `V` nerobia nič.
- Ak hráč niečo drží, `E` patrí držanému predmetu (pustí ho). Do auta sa nastupuje len s voľnými rukami.
- Kamera je v kabíne auta vo výške 190 cm, 120 cm pred stredom auta, v pokoji sklonená o 8° nadol. Karoséria modelu je z pohľadu z kabíny skrytá (jej sklá by zvnútra zakryli výhľad), preto je dole v obraze plochá žltá kapota; zvonku je auto celé.
- Kontrolný pohľad (`V`) zobrazí šípku k najbližšiemu nedokončenému políčku.

## 6. Obrazovka (HUD)

Celé rozhranie (HUD, obchod, menu, nastavenia aj výhra) je štylizované ako žlto-čierne výstražné tabule: žlté alebo čierne zaoblené tabule s tenkou čiarou vo vnútri, text písmom Barlow Condensed veľkými písmenami, bez tieňov. Pruhy (šrafovaný pás žltej a čiernej) zdobia panel obchodu, obrazovku výhry a spodok lišty predmetov.

- **Vľavo hore:** žltá tabuľa s kreditmi (čierne číslo) a popisom „credits".
- **V strede obrazovky:** malý zameriavač (biela bodka v tmavom kruhu, bez zmeny); pri otvorenom menu, obchode a obrazovke výhry sa skryje.
- **Vpravo hore:** čierna tabuľa so žltou linkou. Hore biely text „x / 30,000 · n %", pod ním žltý popis „CELLS PAVED" a ukazovateľ postupu (žltý rámik, šrafovaná dráha, žltá výplň). Pod ním riadok ďalšieho kroku: žltý štítok „NEXT" a biely text kroku, napríklad „Quick swap – entry 0 / 12, cells 0 / 10" (ďalší odomykaný krok, koľko z 12 políčok jeho vstupného úseku je hotových a koľko hotových políčok z potrebných je celkovo). Slovo „Next:" na začiatku textu widget odreže, nahrádza ho štítok; po odomknutí všetkých krokov sa riadok skryje. Ak je robot pridelený, pod tým je riadok „Paver: sector N · k / 400", inak „Paver: waiting – press G"; kým hráč robota nevlastní, riadok sa nezobrazí.
- **Oznamy (toasty):** žlté tabule s čiernym textom, najviac 3 naraz, každý 2 s plus 0,5 s dobehu (dobeh priehľadnosti platí pre celú tabuľu). Dlhý oznam sa zalamuje a je na stred. Kredity sa spájajú do jedného bežiaceho súčtu („+40 credits"). Pri odomknutí: „Unlocked: X" a pod tým účinok kroku.
- **Kontrolný pohľad (`V`):** oranžový kosoštvorec (štvorcová tabuľa otočená o 45°) s čiernou šípkou k najbližšiemu nedokončenému políčku, vedľa čierna tabuľa s oranžovou linkou a oranžovým textom.
- **Tabule pri vstupoch (vo svete):** názov kroku, „Entry k / 12" (hotové políčka úseku) a „Cells a / b" (hotové políčka z potrebných). Kiosky stoja pri vstupoch 1, 4, 5 a 6; vstupy 2 a 3 majú tabuľu na nákupnom centre.
- **Dole v strede zhora nadol:** zásoba výrobne (čierna tabuľa, popis „Plant stock" žltou; zobrazí sa, len keď je výrobňa v dosahu), výzva, čo robí `E` pri predmete, riadok ruky, lišta predmetov a pod ňou šrafovaný pás.
- **Výzva** je pre každý riadok žltá štvorcová tabuľa s klávesom a vedľa nej čierna tabuľa s činnosťou (napríklad `E` a „Push"). Pri držanom kufri sú dva riadky pod sebou (`E` a `F`); riadok bez klávesa má len čiernu tabuľu. Kým je výzva prázdna, nezobrazí sa.
- **Riadok ruky** je čierna tabuľa nad lištou predmetov: názov bielou a obsah žltou (napríklad „Bowl" a „0 / 2.5 L"); platí aj pre držaný kufor či vozík.
- **Lišta predmetov:** tri políčka (`1` miska, `2` lopatka, `3` vedro) s medzerou 10. Políčko je čierna tabuľa so žltou linkou, žltým štvorčekom s číslom klávesu, názvom bielou a (pri nádobách) ukazovateľom naplnenia. Vybrané políčko je opačne: žltá tabuľa s čiernou linkou, čiernym štvorčekom, čiernym názvom a čiernou výplňou. Políčko, ktoré sa nedá vziať, má názov stlmený na 70 % bielej a ukazovateľ skrytý.
- **Obchod (`Tab`):** čierny panel široký 1300, hore s šrafovaným pásom. Nadpis „SHOP" žltou, pod ním biely podnadpis „Tools and machines for the car park" a vpravo žltá tabuľa s kreditmi. Pod tým mriežka kariet po 4 v riadku (karta je vysoká 172, medzera 12): výrobňa (najprv), potom predmety; kariet je 11, takže v poslednom riadku sú tri, zarovnané vľavo. Dole stojí „Tab / Esc – close". Okolo panela sa obraz nestmavuje a hra ostáva klikateľná. Každý nákup sa ukladá.
- **Karta obchodu** je tabuľa, ktorej vzhľad závisí od stavu. Vľavo hore je dlaždica (kruh s prvým písmenom názvu), vedľa nej názov, čo vec dáva a prípadná podmienka odomknutia (žltou, napríklad „350 cells + entry 3"), dole vľavo cena a vpravo tlačidlo alebo štítok:

| Stav | Tabuľa | Linka | Dlaždica | Cena | Vpravo dole |
|---|---|---|---|---|---|
| Dá sa kúpiť | žltá | čierna | čierna, písmeno žlté | čierna | čierne tlačidlo „BUY" so žltým textom |
| Chýbajú kredity | tmavosivá | biela (45 %) | biela, písmeno čierne | svetločervená | vypnuté tlačidlo „BUY" s tenkým obrysom |
| Zamknuté | takmer čierna | biela (18 %) | biela (22 %), písmeno biele | biela (70 %) | vypnuté tlačidlo „LOCKED" s tenkým obrysom |
| Vlastnené | čierna | biela | biela, písmeno čierne | bez ceny | biely štítok „OWNED" s fajkou |

  Kúpená vec **ostáva na svojom mieste** a len sa zmení na čiernu kartu so štítkom OWNED. Farba dlaždice nezávisí od druhu predmetu.
- **Pauza a menu:** žltá tabuľa na stmavenom pozadí s čiernym nadpisom („Do it Asphalt", pri pauze „Paused", v nastaveniach „Settings") a čiernymi tlačidlami s bielym textom. Prvé viditeľné tlačidlo (Continue alebo Resume, bez uloženej hry New Game) má text žltý. V hlavnom menu je mimo Shipping buildu aj tlačidlo Dev Start. Pod kurzorom je tlačidlo svetlejšie, stlačené čierne. Otázka „Start over? Progress will be lost." nahradí nadpis (čierne písmo, na stred) a tlačidlá Yes (žlté) a No stoja vedľa seba.
- **Nastavenia** (`Esc` ich zavrie) sú na rovnakej žltej tabuli: riadok „Mouse sensitivity" s hodnotou čiernou (ExtraBold) a pod ním posuvník s čiernou dráhou a čiernym jazdcom (0,1 – 3,0, štandardne 1,0), dole tlačidlo Back. Hodnota sa ukladá do `GameUserSettings.ini`, nie do uloženej hry.
- **Výhra:** žltá tabuľa s hore šrafovaným pásom (čierna a žltá) a čiernym nadpisom „Parking lot complete!". Pod ním riadky Time, Cells paved, Asphalt poured, Credits earned a Items bought: čierny popis vľavo, hodnota vpravo, každý riadok nad čiernou čiarou. Pod poslednou čiarou sú tlačidlá New Game (text žltý) a Quit.

## 7. Obchod a odomykanie

Predmety (ceny v kreditoch):

| Predmet | Cena | Potrebuje |
|---|---|---|
| Scoop (lopatka) | 10 | – |
| Bucket (vedro) | 35 | – |
| Suitcase (kufor) | 110 | – |
| Cart (vozík) | 340 | – |
| Station (výdajňa) | 900 | odomknutie StripWork |
| AutoPaver | 3 000 | odomknutie AutoPaver |
| AutoPaverFast | 9 000 | AutoPaver |
| AutoPaverFaster | 22 000 | AutoPaverFast |
| Car | 12 000 | odomknutie Car |
| CarWideBar | 24 000 | Car |

Kroky sa odomykajú, keď je hotových dosť políčok a zároveň je celý príslušný vstupný úsek (12 políčok):

| Vstup | Krok | Políčok |
|---|---|---|
| 1 | QuickSwap (výmena ruky 0 s) | 10 |
| 2 | Sprint | 100 |
| 3 | StripWork (výdajňa) | 350 |
| 4 | AutoPaver | 800 |
| 5 | Car | 6 000 |
| 6 | Exit | 30 000 |

Spolu je v obchode 15 vecí: 5 úrovní výrobne a 10 predmetov.

## 8. Ukladanie

- Hra ukladá do jedného slotu s kópiou. Ukladá sa každých 30 s hry, po každom nákupe, po výhre a pri ukončení hry.
- Uložia sa kredity, čas, výrobňa, nádoby, kúpené veci, polohy predmetov, auto, robot a celá zem. Ak hráč v aute uloží, po načítaní sedí v aute.
- Predmet držaný v ruke sa uloží položený na mieste, kde je, ale len ak by sa tam dal položiť (rovnaká kontrola ako pri pustení). Inak sa uloží na poslednom platnom mieste: kde bol naposledy položený, alebo kde sa objavil po kúpe.
- Poškodený slot hra odmietne a použije zálohu; ak sú poškodené obe, začne novú hru.
- Formát uloženia je verzia 2 (zem je bitová mapa hotových políčok a rozpracované políčka). Uloženie vo formáte v1, ktorý poznal surový asfalt, sa odmietne a treba začať novú hru.

## 9. Dĺžka hry (simulácia)

Test `DoItAsphalt.Lot.BalanceSim` hrá hru vopred naprogramovaným správaním a meria **simulovaný** čas, nie skutočnú hru. V jednom behu vyšlo:

| Míľnik | Simulovaný čas |
|---|---|
| prvé políčko | 0:42 |
| 10 políčok | 1:54 |
| 100 políčok | 6:26 |
| 350 políčok | 13:21 |
| 800 políčok | 21:42 (limit testu 42:00) |
| 6 000 políčok | 44:32 (limit 78:00) |
| všetkých 30 000 políčok (otvorí sa východ) | 1:51:05 (limit 1:45 – 2:45) |

Auto aj široký pás sa v simulácii kupujú v čase 45:32. Čísla sú po odstránení hladenia: vozík teraz kladie hotový pás sám, preto je hra o 30 minút kratšia než pôvodných 2:21:27 a pod pôvodným cieľom 2 h 15 min. Ladenie (ceny, rýchlosť výrobne) zostáva na neskôr. Hráč ručne hrá pomalšie alebo rýchlejšie než simulácia; skutočná dĺžka hry sa celá nehrala.

## 10. Zmeny oproti návrhu

Kde sa hotová hra líši od `04-plan-zmien-asphalt-self.md`:

| Čo | Návrh | Hra | Prečo |
|---|---|---|---|
| Hladidlo a finišer | v návrhu boli | **v hre nie sú** | asfalt sa nehladí, vyliaty je rovno hotový |
| Záber lyžice | 0,6 s | **0,5 s** | doladené pri skúšaní hry |
| Tlačenie vozíka | 250 cm/s | **400 cm/s** | doladené pri skúšaní hry |
| Kladenie pásu vozíkom | 100 cm/s | **200 cm/s** | doladené pri skúšaní hry |
| Obrys sektora robota | v návrhu nebol | **oranžový, 10 cm** | hráč vidí, v ktorom sektore robot pracuje |
| Tabuľa pri vstupe | názov kroku a účinok | **názov kroku, vstup, počet políčok**; účinok je v oznamoch | tabuľa sa musí zmestiť |

Rýchlosť výdajne má v kóde vlastnú pomenovanú hodnotu (`StationCarrySpeedCmS`).

## 11. Cheaty

Cheaty sú konzolové príkazy `dia.*`. Je ich 27 a existujú len mimo Shipping buildu. Pre prvých 26 reťazcov sa overilo, že v Shipping binárke nie je ani jeden; pre `dia.Screen`, ktorý pribudol neskôr, sa toto overenie nerobilo. Spúšťajú sa v konzole editora alebo cez `-ExecCmds` (skript `Scripts/shot.sh`).

| Príkaz | Čo robí |
|---|---|
| `dia.Give <kredity>` | pridá kredity |
| `dia.Complete <n>` | dokončí prvých n nedokončených políčok podľa indexu |
| `dia.CompleteEntry <0..5>` | dokončí vstupný úsek (číslovanie od 0) |
| `dia.Item <názov>` | dá predmet: Scoop, Bucket, Suitcase, Cart, Station, AutoPaver, AutoPaverFast, AutoPaverFaster, Car, CarWideBar (`CarWideBar` samotný auto nedá) |
| `dia.Grab`, `dia.Drop` | chytí najbližší predmet, pustí držaný |
| `dia.CartStrip <x> <y> [dĺžka m=4]` | dá a naplní vozík a položí ním pás od bodu smerom na +X |
| `dia.Place <druh> <x> <y> [yaw]` | dá a postaví kufor, vozík alebo výdajňu |
| `dia.Plant <0..5>` | nastaví úroveň výrobne |
| `dia.EnterCar` | nastúpi do auta, v aute z neho vystúpi |
| `dia.Teleport <x> <y> [yaw] [pitch]` | presunie hráča (v aute auto) |
| `dia.Prompt`, `dia.Toast` | vypíše výzvu / oznam |
| `dia.SendPaver` | pošle robota do sektora hráča |
| `dia.Save`, `dia.Load`, `dia.NewGame` | uloží, načíta, začne novú hru |
| `dia.SkipMenu` | preskočí hlavné menu |
| `dia.Camera [vzdialenosť] [yaw] [pitch]` | kamera mimo hráča |
| `dia.WorkView <1\|0>`, `dia.CheckView <1\|0>` | zapne alebo vypne pracovný / kontrolný pohľad |
| `dia.Shop` | prepne obchod |
| `dia.Screen <Pause\|Settings\|Question\|Win>` | otvorí danú obrazovku (pauzu, nastavenia, otázku alebo výhru), aby sa dala odfotiť; ladiaca pomôcka |
| `dia.Hand <názov>` | zmení ruku |
| `dia.Fill` | naplní ruku |
| `dia.TopView [výška m=260]` | pohľad zhora |
| `dia.ShotAndQuit <sekundy> <názov>` | uloží snímku do `Saved/Screenshots/` a skončí |

## 12. Známe obmedzenia

- V prvej osobe nie sú vidieť ruky ani paže a nie je animácia pohybu do strán.
- Hra nemá zvuk.
- Lišta auta sa pri plnom vychýlení volantu môže pohybovať o niečo rýchlejšie, než dovoľuje strop liatia.
- Pri liatí z auta hráča nič neupozorní, že zásoba výrobne sa minula.
- Obrys sektora (10 cm) je zďaleka zhora tenký.
- Kým sú zobrazené tri iné oznamy, oznamy o kreditoch čakajú.
- Valček, dusadlo a lata s vodováhou zo špecifikácie v hre nie sú (otvorená otázka O1 v `04-plan-zmien-asphalt-self.md`).
- Tekutý asfalt je vidieť len ako postupujúce pokrytie zeme (bez vlastnej hrúbky či lesku). Simulácia vyváženia (`LotBalanceSimTest`) počíta s okamžitým liatím, takže vplyv tekutiny na dĺžku hry nemeria; pokrytie vychádza zhruba na zaplatenú plochu, no políčko sa pri nerovnomernom rozliatí dokončuje ťažšie.
- Zameriavač je pevný (nemení sa podľa toho, na čo hráč mieri).
