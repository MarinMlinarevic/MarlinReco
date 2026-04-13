# Running the TauFinder

There are two XML configuration files, [`TauFinder_JETS.xml`](share/TauFinder_JETS.xml) and [`TauFinder_MUONS.xml`](share/TauFinder_MUONS.xml), to run the `TauFinder` on the PFOs produced by the jet and muon reconstruction paths.

For example, to run `TauFinder` on jet PFOs:

```bash
Marlin --global.LCIOInputFiles="reco_output.slcio" <your path to>/MarlinReco/Analysis/TauFinder/share/TauFinder_JETS.xml
```
