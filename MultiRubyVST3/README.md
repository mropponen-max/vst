# Multi Ruby – kitaramultiefekti (VST3)

10 ketjutettavaa efektipaikkaa. Signaali kulkee paikasta 1 paikkaan 10. Jokaiseen paikkaan voi valita minkä tahansa
efektin (samaa efektiä voi käyttää useassa paikassa), efektit voi laittaa päälle/pois (On), ja järjestystä voi
vaihtaa nuolinapeilla. Jokaisella paikalla on oma Mix-säädin.

## Efektit (22)
- Dynamiikka: Noise Gate, Compressor, Limiter
- Särö: Booster, Overdrive, Distortion, Fuzz (oktaavi-fuzz mukana)
- Sointi: EQ 3-Band, Wah / Auto Wah (poljin, LFO, envelope), Cab Sim (4 kaappia), Amp Tone Stack
- Modulaatio: Chorus, Flanger, Phaser (2–12 astetta), Tremolo, Auto Pan
- Tila ja aika: Delay (vapaa aika tai tempo-synkka, ping-pong), Reverb (pre-delay + tone)
- Erikoiset: Pitch Shifter (±24 puolisävelaskelta), Ring Mod, Bitcrusher
- Utility: gain, pan, width, mono, vaiheenkäännös

Oletusketju: Gate → Compressor → Overdrive → EQ → Chorus → Delay → Reverb.
Efektin valinta lataa sen oletusasetukset. Input/Output-säätimet ja mittarit ylhäällä.

## Käännös
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Valmis tiedosto: `build\MultiRuby_artefacts\Release\VST3\Multi Ruby.vst3`
Kopioi se käsin kansioon `C:\Program Files\Common Files\VST3\`.

## DSP-testi (valinnainen)
`cmake -B build -DMULTIRUBY_BUILD_TESTS=ON` ja aja `multiruby_test`: käy läpi kaikki efektit ääriarvoilla.
