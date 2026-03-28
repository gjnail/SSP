# SSP Channel Strip Prompt

Build the **SSP Channel Strip** plugin as our flagship modern console strip. The plugin should be **functionally inspired by the SSL 4000 E Channel Strip**: the same signal-flow philosophy, the same "everything you need on one screen" density, and the same complete channel-strip mindset. It must **not** look vintage, modeled, retro, or skeuomorphic. This is **an SSL 4000 E reimagined for 2026**: modern, clean, dense, premium, fast, and deeply visual.

Use our existing SSP visual design language throughout:
- dark navy/slate background
- teal/cyan accents
- our existing font choices
- existing SSP border styles and component styling
- every module should feel like part of the same SSP family as our current plugins

All UI controls and visual displays must be rendered with our SSP vector/SVG component language. Use:
- `SSPKnob`
- `SSPButton`
- `SSPToggle`
- `SSPDropdown`
- `SSPMeter`
- `SSPSlider`
- `SSPGraph`

Do not use:
- CSS-only knobs
- canvas-based controls
- raster icons
- icon fonts
- PNG-based UI assets

Everything should be vector/SVG-style, retina-perfect, and resolution-independent.

## Core Product Direction

The strip should feel like an SSL-style "whole channel on one screen" processor:
- dense
- complete
- instantly readable
- fast to operate
- no scrolling in the default layout
- no hidden modules behind tabs or menus
- all core sections visible at once

The key differentiator from vintage-model channel strips:

**Every module shows the user what it is doing visually in real time.**

No blind knob turning. Every section includes a compact live visualization that updates smoothly at 60fps.

## Existing SSP Modules

Use existing SSP plugins/modules wherever they already exist instead of rebuilding them:

- **SSP EQ**
  - use the existing SSP EQ directly
  - keep all recent upgrades:
    - vector/SVG rendering
    - draggable graph nodes
    - mid/side color coding
    - musical note display
    - note-to-frequency input
    - SVG knobs and controls
- Any other existing SSP plugin logic we already have for:
  - gate
  - compressor
  - saturator
  - clipper / limiter
  - related utility stages

If a module does not already exist as a standalone SSP plugin, build it in the same SSP style and structure so it can later be extracted as its own plugin with minimal refactoring.

## Revised Module Layout: SSL-Inspired Signal Flow

Reorganize the strip to follow **SSL 4000 E style signal flow**. The whole strip should be visible in a single compact vertical console-strip view.

### 1. Input Section

Top of strip.

Controls:
- Line/Mic input trim: `SSPKnob`, range `-20 dB` to `+20 dB`
- Phase/Polarity flip: `SSPToggle`
- HPF:
  - frequency knob `20 Hz` to `600 Hz`
  - on/off toggle
  - display frequency and musical note
- LPF:
  - frequency knob `1 kHz` to `20 kHz`
  - on/off toggle
  - display frequency and musical note

Visualization:
- small inline SVG filter-response display
- show combined HPF + LPF curve in real time
- curve updates live as filters move

### 2. Dynamics Section: Compressor + Gate/Expander

This should feel like the SSL dynamics block, but modernized and more flexible.

#### Compressor

Controls:
- Threshold: `-30 dB` to `+10 dB`
- Ratio: `1:1` to `∞:1`
- Attack:
  - Fast/Slow SSL-style toggle
  - additional manual attack knob `0.1 ms` to `100 ms`
- Release knob `0.1 s` to `4 s`
- Auto release toggle
- Makeup gain
- Mix / Blend for parallel compression
- Character modes:
  - Clean
  - Punch
  - Glue

Visualization:
- real-time SVG gain reduction meter
- live compression transfer curve
- threshold, ratio, and knee visible in the curve
- moving dot showing current signal position on the curve
- gain reduction trace over time beneath the curve

#### Gate / Expander

Controls:
- Threshold
- Range
- Attack
- Hold
- Release
- Gate / Expander mode toggle
- Sidechain HP and LP filters
- Listen button for detector signal
- Key input selector:
  - internal
  - external sidechain

Visualization:
- live SVG gate-state indicator
- show open / closed / transitioning behavior
- tiny sidechain signal visualization or level display
- gain reduction trace over time

#### Shared Dynamics Controls

- Dynamics In/Out bypass
- Compressor/Gate order switch:
  - `Comp -> Gate`
  - `Gate -> Comp`
- Stereo link button

### 3. Equalizer Section: Use SSP EQ

Replace the SSL 4-band EQ with our **existing SSP EQ**, but also provide a quick classic workflow mode.

#### Modern Mode

Default.

Embed full SSP EQ:
- up to 24 bands
- all filter types
- mid/side
- note display
- note input
- full live graph
- expandable floating view for detailed editing

#### Classic Mode

SSL-inspired quick view built on the same SSP EQ engine.

4-band layout:
- LF:
  - gain
  - frequency
  - shelf/bell toggle
  - default `60 Hz` shelf
- LMF:
  - frequency
  - gain
  - Q
- HMF:
  - frequency
  - gain
  - Q
- HF:
  - gain
  - frequency
  - shelf/bell toggle
  - default `12 kHz` shelf

Visualization:
- always-visible live EQ response graph
- compact 4-band view in Classic Mode
- full SSP EQ graph in Modern Mode

EQ section controls:
- EQ In/Out bypass
- Pre/Post dynamics toggle

### 4. Saturation / Harmonics

Modern explicit harmonic-color stage.

Controls:
- Drive
- Character:
  - Warm
  - Edge
  - Crisp
- Mix
- Output trim

Visualization:
- real-time SVG harmonic spectrum
- show 2nd, 3rd, 4th, 5th+ harmonic bars reacting in real time

### 5. Output Section

Bottom/right-side output section.

Controls:
- prominent vertical SVG output fader
- Pan/Balance knob
- Stereo width control `0%` mono to `200%` wide
- Safety limiter / clipper toggle
- Ceiling control

Visualization:
- large SVG output meter
- segmented style
- show:
  - peak
  - RMS
  - LUFS
- green -> amber -> red
- peak hold with decay

## Signal Flow Routing

Replicate SSL routing flexibility and extend it:

- EQ / Dynamics order switch
- Compressor / Gate order switch
- Saturation position switch:
  - pre-EQ
  - post-EQ
  - post-dynamics

Add a compact live **SVG signal-flow diagram** showing the current routing order:
- module blocks
- arrows
- animated updates when order changes
- interactive drag-to-reorder support inside the diagram

## Visualization Philosophy

Every section must have a visible live visualization by default:

- Input filters:
  - live filter response curve
- Compressor:
  - transfer curve
  - moving signal dot
  - GR meter
  - GR trace
- Gate:
  - gate-state display
  - sidechain display
  - GR trace
- EQ:
  - full or compact response curve
- Saturation:
  - harmonic spectrum
- Output:
  - peak / RMS / LUFS meter
- Signal flow:
  - routing diagram
- Meter bridge:
  - stage meters across chain

These visualizations should be:
- compact
- readable
- always visible in default state
- smooth at `60 fps`
- teal/cyan as primary active colors
- amber and purple available where appropriate, including mid/side indication

## Compact Density

The strip should fit in a narrow, dense, console-like footprint:
- single vertical strip
- no scrolling in the default view
- everything visible at once
- modules stacked compactly
- collapse/expand still available, but default should already feel complete
- target size roughly:
  - `350-400 px` wide
  - `900-1100 px` tall
- output fader may live along the right side to save vertical space

## Additional SSL-Inspired Features

- channel strip linking for stereo bus workflows
- output meter mode toggle:
  - VU
  - Peak
- overload indicators at:
  - input
  - post-dynamics
  - post-EQ
  - output
- overload indicators latch and can be clicked to clear
- master strip power / bypass

## Preset System

Add channel-strip-specific factory presets organized as:

- Vocals
  - Male Vocal Chain
  - Female Vocal Chain
  - Vocal Bus
  - Podcast Voice
  - Vocal Aggressive
  - Vocal Intimate
- Drums
  - Kick Inside
  - Kick Outside
  - Snare Top
  - Snare Bottom
  - Hi-Hat
  - Toms
  - Overheads
  - Room
  - Drum Bus
- Bass
  - Bass DI
  - Bass Amp
  - Sub Bass
  - Bass Bus
- Guitars
  - Acoustic Strumming
  - Acoustic Fingerpick
  - Electric Clean
  - Electric Crunch
  - Electric Lead
  - Guitar Bus
- Keys
  - Piano Bright
  - Piano Warm
  - Rhodes
  - Wurlitzer
  - Organ
  - Synth Pad
  - Synth Lead
  - Synth Bass
- Mix
  - Mix Bus Glue
  - Mix Bus Punch
  - Mix Bus Wide
  - Mastering Gentle
  - Mastering Loud
- Utility
  - Clean Pass
  - Gain Stage Only
  - HPF + Compression Only

Each preset must save:
- all module settings
- routing order
- bypass states
- collapse states

## Technical Architecture

Build as a modular system with reusable extractable modules.

Every module should expose a standardized interface conceptually equivalent to:
- `getAudioNodes()`
- `getInputLevel()`
- `getOutputLevel()`
- `getState()`
- `setState()`
- `bypass(enabled)`
- `getGainDelta()`
- `render()`

Use a central routing manager to handle signal-flow order changes cleanly.

Requirements:
- real-time processing
- CPU-efficient enough for use on many tracks
- parameter smoothing everywhere
- no zipper noise
- no unnecessary latency
- optional oversampling where needed for saturation
- all controls automatable

## What Not To Do

- Do not make it look like hardware SSL
- Do not use metal textures, screws, vintage shadows, or faux analog realism
- Do not hide modules behind tabs
- Do not hide visualizations by default
- Do not make the strip overly wide
- Do not forget musical-note display on every frequency control
- Do not use Canvas for controls
- Do not sacrifice animation smoothness for feature count

## Final Product Goal

The finished SSP Channel Strip should feel like:
- the immediacy and density of an SSL 4000 E
- the flexibility of a modern modular strip
- the clarity of a visual mixing tool built for 2026

It should let the user both **hear** and **see** exactly what every section is doing, all at once, in one compact premium strip.
