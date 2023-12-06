Minimal Friction is a collection of original DSP modules for VCV Rack 2.

# kanon
![Minimal Friction Kanon](res/screenshots/kanon.png)
Kanon is a single-pitch input 4-voice oscillator, with 4 accurate waveforms and 4 pitch-voice assignment algorithms.

## Manual

To switch between waveforms (sine, triangle, square and sawtooth), press their corresponding buttons.

To switch between pitch assignment modes (Kanon, forward-forward, forward-backward and random), press their corresponding buttons.

### Pitch assignment modes

In **Kanon** mode, each next voice is "one pitch change behind" the voice before it.

In **forward-forward** mode, new pitches are always assigned to the next voice advancing forward, except in the case of voice 4, which advances to voice 1; 1-2-3-4-1-2-3-...

In **forward-backward** mode, new pitches are assigned to voices advancing back and forth; 1-2-3-4-3-2-1-2-...

In **random** mode, new pitches are assigned to voices at random.

## Notes
Moving the coarse or fine knobs will not have any influence on pitch assignments.

Feel free to experiment with:
* panning each output differently!
* running each output through a different chain of effects / giving it a different envelope!
* running chord note arpeggios to the 1V/oct input!