# Do it Asphalt – špecifikácia 3D modelov

Verzia 2.0 · 2026-11-21 · modely vytvára **Fable 5.2**

Tento dokument je zmluva medzi vetvou modelov a vetvou kódu. Kód počíta s rozmermi a názvami podľa tabuliek nižšie. Obsahuje 45 modelov: 12 nových pre hru Asphalt-self, ostatné ostali z verzie 1, a tri z druhého kola (`SM_Canister`, `SM_Tub`, `SM_VehicleStation`). K nim pribudli tri väčšie stupne asfaltovacieho auta (`SM_PaverCar_T2` až `_T4`, časť 4.4) a tri budovy na nákup vylepšení (`SM_ToolShop`, `SM_PaverDepot`, `SM_Garage`, časť 4.5), spolu teda 51 modelov. `SM_Scoop` ostal v manifeste, ale hra ho už nepoužíva (malá naberačka sa odstránila).

---

## 1. Ako modely vznikajú

Každý model je **Python skript pre Blender**, nie ručne modelovaný súbor. Skript sa dá kedykoľvek spustiť znova a dá rovnaký výsledok; zmena modelu je zmena v kóde, ktorú vidno v gite.

```
Tools/ModelGen/
├── common.py          paleta, pomocné tvary, export do GLB, render náhľadov
├── manifest.json      očakávané rozmery, limity trojuholníkov, povolené materiály
├── check.py           overí GLB voči manifestu (beží bez Blenderu)
├── build_all.sh       spustí všetky skripty, potom check.py
└── <model>.py         jeden skript na model alebo rodinu modelov

SourceAssets/Models/<Názov>.glb        výstup
SourceAssets/Previews/<Názov>_{a,b,c}.png   náhľady: trištvrte spredu, zboku, zhora
```

Postup pre každý model:

1. Doplniť riadok do `manifest.json` s obálkou modelu (to je „test").
2. Napísať skript, spustiť `blender -b -P Tools/ModelGen/<model>.py`.
3. `python3 Tools/ModelGen/check.py <Názov>` musí vypísať `OK`.
4. Pozrieť si tri náhľady a prejsť kontrolný zoznam z kap. 5.
5. Opakovať 2 – 4, kým model neprejde.

## 2. Konvencie

| | |
|---|---|
| Súradnice (ako ich uvidí hra) | **+X dopredu, +Y doprava, +Z hore**, jednotky v tabuľkách sú metre |
| Počiatok | podľa stĺpca „Počiatok"; ak nie je uvedené inak, stred pôdorysu na úrovni zeme |
| Štýl | low-poly, ostré hrany s jemným skosením na veľkých tvaroch, žiadne textúry |
| Farby | len materiály z palety (kap. 3), ploché farby |
| Normály | ploché tieňovanie; žiadne obrátené plochy |
| Formát | GLB, jeden mesh na súbor, transformácie aplikované |
| Mierka | postava hráča má 1,8 m; v náhľadoch je pre mierku sivá silueta (neexportuje sa) |

Skripty píšu geometriu priamo v týchto osiach. Prevod do súradníc Blenderu a glTF (iná orientácia osí, opačná ručnosť) rieši výhradne `common.export()`. Správnosť prevodu dokazuje model `SM_AxisTest`: po importe do Unrealu musí mať stred obálky v kladnej časti všetkých troch osí.

## 3. Paleta

Názov materiálu v GLB = názov slotu v Unreali = `MI_<názov>` v `Content/Materials/`.

| Materiál | Farba | Kov | Drsnosť | Použitie |
|---|---|---|---|---|
| `DIA_Yellow` | `#F2B705` | 0 | 0,6 | stroje, hlavná farba hry |
| `DIA_Orange` | `#E8590C` | 0 | 0,6 | výstražné prvky, kužele |
| `DIA_Steel` | `#8A9099` | 1 | 0,4 | rúry, nádrže, lišty |
| `DIA_DarkSteel` | `#3A3F47` | 1 | 0,5 | rámy, podvozky |
| `DIA_Rubber` | `#1B1B1D` | 0 | 0,9 | kolesá, hadice |
| `DIA_Asphalt` | `#232326` | 0 | 0,35 | asfalt, výpuste |
| `DIA_White` | `#F2F2EE` | 0 | 0,7 | značenie, tabule |
| `DIA_Red` | `#C62828` | 0 | 0,6 | závory, zámok |
| `DIA_Glass` | `#7FB6D9` | 0 | 0,1 | okná, ukazovatele |
| `DIA_Concrete` | `#A8A49C` | 0 | 0,9 | základy, obrubníky |
| `DIA_Dirt` | `#8A6B4A` | 0 | 1,0 | kopy hliny |
| `DIA_Green` | `#3E8E41` | 0 | 0,6 | „odomknuté", tlačidlá |

## 4. Modely

Rozmery sú obálka D × Š × V (X × Y × Z) v metroch, prevzatá z `Tools/ModelGen/manifest.json`; „Tri“ je horný limit trojuholníkov. Všetky modely sú v `Content/Models/` a v `SourceAssets/Models/`.

### 4.1 Nové modely (15)

| Asset | Rozmery | Obálka v osiach | Tri | Čo to je a kde sa používa | Skript |
|---|---|---|---|---|---|
| `SM_Spoon` | 0,199 × 0,045 × 0,018 | X −0,055 … 0,144 | 200 | lyžica v ruke k miske | `hand_tools.py` |
| `SM_Bowl` | 0,25 × 0,25 × 0,09 | stred pôdorysu, Z od 0 | 400 | miska v ruke (štartovacia nádoba) | `hand_tools.py` |
| `SM_Trowel` | 0,28 × 0,12 × 0,092 | X −0,14 … 0,14 | 250 | hladidlo (hra ho od 2026-11-22 nepoužíva, model ostáva v repozitári) | `hand_tools.py` |
| `SM_Scoop` | 0,34 × 0,12 × 0,07 | X −0,07 … 0,27 | 250 | malá naberačka (hra ju od 2026-11-23 nepoužíva, model ostáva v repozitári) | `hand_tools.py` |
| `SM_Bucket` | 0,288 × 0,31 × 0,385 | stred pôdorysu, Z od 0 | 600 | vedro v ruke | `hand_tools.py` |
| `SM_Trolley` | 0,958 × 0,52 × 0,898 | X −0,498 … 0,46, Z od 0 | 552 | vozíček na asfaltovanie, tlačený pred sebou (rukoväť na strane −X) | `trolley.py` |
| `SM_Cart` | 1,26 × 0,67 × 0,99 | X −0,65 … 0,61, Z od 0 | 1500 | vozík, tlačený | `cart.py` |
| `SM_Finisher` | 1,32 × 1,072 × 1,0 | X −1,22 … 0,1, Z od 0 | 2000 | pásový finišer (hra ho od 2026-11-22 nepoužíva, model ostáva v repozitári) | `finisher.py` |
| `SM_Trough` | 1,28 × 0,60 × 0,50 | stred pôdorysu, Z od 0 | 400 | koryto s asfaltom pred výrobňou | `lot_props.py` |
| `SM_Curb` | 0,15 × 2,0 × 0,12 | stred pôdorysu, Z od 0 | 40 | diel obrubníka okolo parkoviska | `lot_props.py` |
| `SM_PaverCar_WideBar` | 0,16 × 3,5 × 0,18 | X −2,32 … −2,16, Z 0,02 … 0,2 | 400 | široká lišta auta; hra ju už nepoužíva (stupeň 2 má vlastný model `SM_PaverCar_T2`), ostala len v manifeste a v teste importu | `lot_props.py` |
| `SM_Mall` | 100 × 100 × 14,3 | X, Y −50 … 50 | 4000 | nákupné centrum, zaberá kvadrant parkoviska | `mall.py` |
| `SM_Canister` | 0,29 × 0,23 × 0,40 | stred pôdorysu, Z od 0 | 700 | kanister 20 l v ruke | `vessels.py` |
| `SM_Tub` | 0,51 × 0,58 × 0,22 | X −0,245 … 0,265, Z od 0 | 700 | vaňa 36 l v ruke | `vessels.py` |
| `SM_VehicleStation` | 8,0 × 5,45 × 4,6 | X −4 … 4, Y −2,95 … 2,5, Z od 0 | 3000 | pevná výdajňa asfaltu pre auto a robota (bránka s 4 m jazdným pruhom) | `vehicle_station.py` |

`SM_Mall` má pri importe obdĺžnikovú kolíziu (`BOX_COLLISION_MODELS = {"SM_Mall"}` v `Tools/UE/import_models.py`); ostatné modely majú konvexnú kolíziu s 18 stenami.

### 4.2 Modely z verzie 1, ktoré hra používa

`SM_AutoPaver` (robot), `SM_Barrier`, `SM_ExitGate_Frame` a `SM_ExitGate_Barrier` (brána východu), `SM_Fence`, `SM_Kiosk`, `SM_PaverCar` (auto), `SM_Plant`, `SM_Plant_AddonA`, `SM_Plant_AddonB`, `SM_PortableStation` (výdajňa), `SM_RoadStub`. Ich rozmery sú v `manifest.json` a v `Source/DoItAsphalt/Game/DIAModelSizes.h`.

### 4.3 Modely z verzie 1, ktoré hra už nepoužíva

Ostali v repozitári, hra ich nespúšťa; používajú ich len testy: `SM_AxisTest` (overenie osí), `SM_Backpack_S/M/L/XL`, `SM_Barrel`, `SM_Cone`, `SM_DirtPile`, `SM_Helmet`, `SM_Lamp`, `SM_Lock` a sedem asfaltovačiek `SM_Spreader_020` až `_200`.

### 4.4 Stupne asfaltovacieho auta (3 nové modely)

Auto má štyri stupne, každý väčší a ťažší. Stupeň 1 je doterajší `SM_PaverCar` (jeho rozmery sa nezmenili), ďalšie tri sú nové. Všetky štyri generuje `paver_car.py` z tabuľky `TIERS`; počiatok je v strede pôdorysu na zemi, auto ide smerom +X, zadná stena lišty je na X = −D/2 a dvere sú na strane −Y. Šírka v tabuľke je šírka lišty, karoséria je užšia.

| Asset | Rozmery | Karoséria (Š) | Lišta | Nápravy | Tri | Čím sa líši |
|---|---|---|---|---|---|---|
| `SM_PaverCar` | 4,6 × 3,0 × 2,4 | 2,2 | 3,0 | 2 | 8000 | základné auto, jedna cisterna |
| `SM_PaverCar_T2` | 5,6 × 3,6 × 2,7 | 2,5 | 3,6 | 2 | 8000 | väčšia cisterna a kolesá, slnečná clona, zrkadlá, dva majáky na streche, druhý schodík |
| `SM_PaverCar_T3` | 6,8 × 4,2 × 3,0 | 2,8 | 4,2 | 3 | 8000 | zadná dvojnáprava, dva výfuky, ochranný rám pred maskou, dvojité svetlá, rebrík na cisterne, hydraulické valce a žlté koncovky lišty |
| `SM_PaverCar_T4` | 8,2 × 5,0 × 3,4 | 3,1 | 5,0 | 4 | 8000 | dve predné a dve zadné nápravy, dve cisterny vedľa seba s lávkou, tretí schodík |

Hra používa všetky štyri: stupeň sa kupuje ako vylepšenie auta (`Car`, `CarT2`, `CarT3`, `CarT4`) a aktér `ADIAPaverCar` ukazuje model aktuálneho stupňa; kolízna obálka (karoséria) a celé rozmery sú v `DIAModelSizes.h` (`PaverCarCm[4]`, `PaverCarBodyCm[4]`), cena, výkon, nádrž a vzdialenosť lišty v `LotBalance.h`. Široká lišta `SM_PaverCar_WideBar` sa už nepoužíva. Lišta stupňov T3 a T4 (4,2 m a 5,0 m) je širšia než 4 m jazdný pruh `SM_VehicleStation`. Spoločný náhľad všetkých štyroch v jednej mierke je `SourceAssets/Previews/PaverCar_Tiers.png`.

### 4.5 Budovy na nákup vylepšení (3 nové modely)

Tri budovy, v ktorých sa kupuje; v každej iná skupina vecí. Všetky generuje `shop_buildings.py`. Každá je otočená prednou stranou k **+X** (odtiaľ prichádza hráč alebo auto) a nesie **prázdnu bielu tabuľu** otočenú k +X, na ktorú hra napíše názov (rovnako ako `SM_Kiosk`).

| Asset | Rozmery | Obálka v osiach | Tri | Čo to je a čo sa v nej kupuje |
|---|---|---|---|---|
| `SM_ToolShop` | 3,53 × 6,08 × 4,0 | X −1,24 … 2,29, Y ±3,04, Z od 0 | 2500 | žltý stavebný kontajner s výdajným okienkom: ručné veci (miska, vedro, kanister, vaňa, vozíček, vozík) |
| `SM_PaverDepot` | 3,5 × 6,2 × 3,41 | X ±1,75, Y ±3,1, Z od 0 | 2500 | prístrešok s tromi státiami pre robotov: nové roboty a ich vylepšenia |
| `SM_Garage` | 12,15 × 8,0 × 5,59 | X −5,65 … 6,5, Y ±4,0, Z od 0 | 4500 | vjazdná hala: autá a ich vylepšenia |

Počiatok a body, s ktorými má kód počítať (metre, osi hry):

- **`SM_ToolShop`** – počiatok je stred pôdorysu kontajnera (2,4 × 6,0 m) na zemi. Okienko je v stene +X na Y −2,5 … −0,1 a Z 1,0 … 2,1; pred ním je oceľový pult s vrchom na Z = 1,04 a nad ním vyklopená oranžová markíza. Hráč stojí pred stredom prednej hrany pultu, `COUNTER = (1,62; −1,3; 1,04)`. Vpravo od okienka sú dvere so schodíkom (ozdoba, neotvárajú sa). Tabuľa má 2,4 × 0,5 m, stred `(1,16; −1,3; 3,12)`. Na streche stojí veľké vedro s asfaltom; jeho rúčka je najvyšší bod.
- **`SM_PaverDepot`** – počiatok je stred betónovej dosky (3,5 × 6,2 m). Tri státia sú otvorené k +X a sú od seba 1,6 m; zaparkovaný robot má počiatok na `DOCKS = (−0,35; −2,2)`, `(−0,35; −0,6)`, `(−0,35; 1,0)` a pozerá k +X. Hra ukazuje `SM_AutoPaver` 1,5× väčší než jeho model, teda 1,5 × 1,2 × 0,9 m; státie je medzi koľajničkami široké 1,35 m. Vpravo od státí je ovládací pult s obrazovkou otočenou k +X, stred pôdorysu `CONSOLE = (0,95; 2,45)`, a za ním biela skriňa so stožiarom (tanier, oranžový maják). Tabuľa má 2,4 × 0,45 m, stred `(1,74; −0,6; 2,98)`.
- **`SM_Garage`** – počiatok je **stred parkovacieho státia** na zemi: auto parkuje s počiatkom tu a pozerá k +X, von z brány. Brána v stene +X je široká 6,4 m a vysoká 4,1 m; roleta je vyhrnutá a jej spodná lišta je na Z = 3,70. Vnútri sú steny 7,3 m od seba a hala je dlhá 10,6 m (X −5,3 … 5,3), takže vojde aj najväčšie auto `SM_PaverCar_T4` (8,2 × 5,0 × 3,4 m). Pri zadnej stene (X −5,3 … −4,3) stojí ponk, skrinka, regál a pneumatiky. Na vnútornej strane pravej steny (+Y) hneď za bránou visí terminál s bielou obrazovkou, `TERMINAL = (4,30; 3,55)`, vo výške 1,45 m, otočený k −Y: pri ňom hráč kupuje, lebo ostáva prístupný aj so zaparkovaným autom T4. Tabuľa má 3,0 × 0,46 m, stred `(5,57; 0; 4,56)`. Pred bránou je metrová betónová zástera so šrafovaným pásom a dva stĺpiky.

**Kolízia.** Jedna konvexná obálka by zavrela vjazd do garáže aj priestor pod markízou a pod prístreškom. Všetky tri budovy preto majú v `manifest.json` pole `"collision"`: zoznam kvádrov `{"min": [x, y, z], "max": [x, y, z]}` v metroch a v osiach hry (steny, stĺpy, strecha, pult, nábytok). `Tools/UE/import_models.py` z neho pri importe spraví kolíziu modelu: presne tieto kvádre a žiadnu konvexnú obálku (`SM_ToolShop` 4, `SM_PaverDepot` 10, `SM_Garage` 14 kvádrov). Model bez poľa `"collision"` má konvexnú kolíziu ako doteraz.

**Kde budovy stoja.** Všetky tri sú zapojené do hry a stoja na južnom pruhu pod parkoviskom (Y −11 200 … −10 000 cm) pri výrobni a výdajni; polohy sú v `LotLayout::ShopPos` a `ShopYawDeg`, rozmery v `DIAModelSizes.h`. Počiatok modelu stojí na polohe budovy a model je otočený o jej yaw.

| Budova | Poloha počiatku (cm) | Yaw | Kam mieri predná strana (+X modelu) | Bod nákupu v osiach modelu (cm) |
|---|---|---|---|---|
| `SM_ToolShop` | (900; −10 500) | 90° | k parkovisku (+Y) | (222; −130) pred pultom |
| `SM_PaverDepot` | (1 800; −10 550) | 90° | k parkovisku (+Y) | (185; 245) pred ovládacím pultom |
| `SM_Garage` | (−3 000; −10 450) | 0° | pozdĺž pruhu na východ (+X), k výdajni | (430; 295) pri termináli |

Bod modelu `(Lx, Ly)` je vo svete `Pos + (Lx·cos θ − Ly·sin θ, Lx·sin θ + Ly·cos θ)`, θ je yaw budovy (`LotLayout::ShopLocalToWorld`). Garáž leží pozdĺž pruhu, lebo je dlhá 12,15 m a naprieč by sa do 12 m pruhu nezmestila; auto z nej vyjde rovno k výdajni: os státia je os jazdnej dráhy výdajne (`ShopPos(Garage).Y` je `VehicleStationPos.Y`, −10 450), takže karoséria T4 (310 cm) prejde medzi stĺpikmi bránky priamo. Bod nákupu garáže je (−2 570; −10 155). Kúpené auto sa objaví na počiatku garáže (stred státia) a pozerá von z brány; kúpený robot n sa objaví v státí n (`LotLayout::PaverDock(n)`, `DOCKS` v lokálnych osiach (−35; −220), (−35; −60), (−35; 100) cm) a pozerá k parkovisku. Na tabuli každej budovy je text TOOLS, ROBOTS alebo GARAGE (`ADIAShopBuilding`). Kolíziu tvoria kvádre z `manifest.json`, takže blokuje hráča aj auto, no nezatvára vjazd do garáže ani prístup k pultom. Čo sa v ktorej budove kupuje, je v `01-herny-dizajn.md`, kapitola 7.

## 5. Nové pomocné funkcie v `Tools/ModelGen/common.py`

- `tub(top_xy, bottom_xy, z0, z1, wall, floor, center_xy, mat)`: otvorená obdĺžniková nádoba so šikmými stenami (koryto, násypka): `top_xy` a `bottom_xy` sú vonkajšie rozmery hore a dole, `wall` hrúbka steny, `floor` hrúbka dna. Je vidieť zvnútra aj zvonka, má 36 trojuholníkov. Používa ju `SM_Trough` (`lot_props.py`) a korba vozíka `SM_Cart` (`cart.py`).
- `finish(parts, name, silhouette=True, **preview_options)`: skontroluje orientáciu plôch, zlúči časti do jedného meshu, zapíše GLB a vyrenderuje náhľady. S `silhouette=False` sa model rámuje sám bez sivej siluety postavy (ručné nástroje by vedľa nej boli len pár pixelov).
- `render_previews(..., silhouette, front)`: parameter `front=(x, y, z)` určuje, kde stojí kamera náhľadu „a“ voči modelu (v osiach hry), pre modely, ktorých zaujímavá strana nie je +X / +Y (používa ho `mall.py`).

## 6. Kontrolný zoznam pred odovzdaním modelu

- `check.py` vypísal `OK` (rozmery, počet trojuholníkov, materiály len z palety).
- Na náhľadoch nie sú čierne ani chýbajúce plochy (obrátené normály) a nič sa neprelína tak, aby blikalo.
- Silueta je rozpoznateľná aj v malom náhľade.
- Model stojí na zemi (najnižší bod Z = 0), ak ho hra nedrží v ruke; nevznáša sa a nie je zapustený.
- Použitých je najviac 5 materiálov palety.
