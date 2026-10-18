# Licensgennemgang – BleepForge

**Dato:** 2026-10-18
**Omfang:** DSP-algoritmer i `Source/`, der bygger på andres publicerede arbejde. Afhængigheder
(JUCE, clap-juce-extensions, CLAP) står i `THIRD-PARTY-NOTICES.txt`.

> Regel for projektet: ingen kode fra GPL-projekter (eller andre plugins i det hele taget).
> Algoritmer implementeres ud fra publicerede artikler eller kode under MIT/BSD-lignende
> licens, og kilden skrives her. Dette er en teknisk gennemgang, ikke juridisk rådgivning.

---

## 1. Algoritmer og kilder

| Hvor | Hvad | Kilde | Licens / status |
|---|---|---|---|
| `Source/Effects.h`, `Plate` | Plate-reverb: input-diffusion, "figure-of-eight"-tank med modulerede allpasser, delay-længder og output-aftapninger | Jon Dattorro, "Effect Design, Part 1: Reverberator and Other Filters", *Journal of the Audio Engineering Society* 45(9), 1997, s. 660–684 (figur 1, tabel 1 og 2) | Publiceret artikel. Koden er skrevet direkte ud fra artiklen; ingen eksisterende implementation er læst eller kopieret |
| `Source/DspBasics.h` | PolyBLEP-residualet og envelope | Fra SubForge (samme forfatter, AGPLv3); udledning i SubForges `docs/LICENSE-REVIEW.md` §5.2 | Egen kode |
| `Source/Effects.h`, `Filter` | State-variable filter i trapezform | Standard-DSP (topologibevarende SVF) | Egen implementation |

## 2. Plate-reverben i detaljer

Fra artiklen er taget: strukturen (båndbegrænsning, fire input-allpasser, to krydskoblede
tank-halvdele), delay-længderne i samples ved 29761 Hz, diffusionskoefficienterne
(0,75 / 0,625 / 0,70 og "decay diffusion 2 = decay + 0,15, holdt mellem 0,25 og 0,5"),
modulationen af den første allpass i hver halvdel og output-aftapningerne med fortegn.

Egne tilføjelser og ændringer, uden ekstern kilde:

- længderne skaleres til værtens samplerate og til Size-knappen
- Decay angives i sekunder og omregnes til tankens loop-gain
- Tone styrer både input-båndbredden og dæmpningen i tanken
- fast pre-delay (15 ms) og low cut (200 Hz, to one-pole high-passes) på input
- Duck: en envelope follower på reverbens input, der sænker den våde del op til 12 dB
- modulationsdybde 12 samples (artiklen tillader op til 16) ved 0,8 Hz, i kvadratur mellem
  de to halvdele

Ingen af de mange open source-implementationer af Dattorros plade er brugt; flere af dem er
under GPL.
