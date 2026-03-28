export type Tone = "gold" | "cyan" | "ember";

export type ProductStatus = "available" | "coming-soon";
export type ProductCategory =
  | "sidechain"
  | "dynamics"
  | "tone"
  | "space"
  | "analysis"
  | "creative"
  | "instrument";

export type NavigationItem = {
  label: string;
  to: string;
};

export type HeroStat = {
  value: string;
  label: string;
};

export type Product = {
  slug: string;
  name: string;
  tone: Tone;
  status: ProductStatus;
  category: ProductCategory;
  label: string;
  strapline: string;
  summary: string;
  bestFor: string;
  triggerModes: string[];
  highlights: string[];
  formats: string[];
  platforms: string[];
  priceUsd: number;
  image: string;
  releaseNote: string;
};

export type Principle = {
  title: string;
  body: string;
};

export type WorkflowStep = {
  step: string;
  title: string;
  body: string;
};

export type FaqItem = {
  question: string;
  answer: string;
};

export const navItems: NavigationItem[] = [
  { label: "Home", to: "/" },
  { label: "Plugins", to: "/products" },
  { label: "Hub", to: "/hub" },
  { label: "About", to: "/about" },
  { label: "Contact", to: "/contact" },
  { label: "Cart", to: "/cart" },
];

export const heroStats: HeroStat[] = [
  { value: "18 Active", label: "plugin projects currently surfaced from the SSP workspace" },
  { value: "1 Desktop Hub", label: "launcher app focused on installs, updates, and activation flow" },
  { value: "VST3 + Shop", label: "plugin catalog, product pages, cart flow, and Hub positioning in one site" },
];

export const stageTags = [
  "Plugin Catalog",
  "Hub Landing Page",
  "Hub Download Page",
  "VST3 + Standalone",
  "Focused Tooling",
  "Temporary Product Art",
];

export const products: Product[] = [
  {
    slug: "multiband-sidechain",
    name: "SSP Multiband Sidechain",
    tone: "cyan",
    status: "available",
    category: "sidechain",
    label: "Three-band control rack",
    strapline: "Split ducking across low, mid, and high so the whole mix keeps its shape.",
    summary:
      "The flagship sidechain option for precise movement: per-band shaping, crossover control, linked editing, and enough flexibility to keep lows breathing without flattening the whole record.",
    bestFor: "Multiband pumping and cleaner mix movement",
    triggerModes: ["Audio", "BPM Sync", "MIDI"],
    highlights: [
      "Three independently shaped sidechain bands",
      "Linked editing, per-band mix, and crossover control",
      "Designed for surgical movement instead of one-size pumping",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows", "macOS packaging"],
    priceUsd: 59,
    image: "/product-art/ssp-multiband-sidechain.svg",
    releaseNote: "Flagship sidechain project with crossover control and per-band ducking already present in the SSP workspace.",
  },
  {
    slug: "simple-sidechain",
    name: "SSP Simple Sidechain",
    tone: "gold",
    status: "available",
    category: "sidechain",
    label: "Fast curve ducking",
    strapline: "One curve, fast setup, and the shortest route from kick to movement.",
    summary:
      "Built for speed: draw the shape once, retrigger it from audio, MIDI, or BPM sync, and keep producing without disappearing into routing or extra pages.",
    bestFor: "Fast pump and obvious ducking",
    triggerModes: ["Audio", "BPM Sync", "MIDI"],
    highlights: [
      "Single-curve workflow with visual shaping",
      "Audio, BPM sync, and MIDI retrigger modes",
      "Default, Punch, Hold, and Breathe style starting points",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows", "macOS packaging"],
    priceUsd: 29,
    image: "/product-art/ssp-simple-sidechain.svg",
    releaseNote: "Current SSP sidechain project positioned as the fast-entry option in the lineup.",
  },
  {
    slug: "transition",
    name: "SSP Transition",
    tone: "ember",
    status: "available",
    category: "creative",
    label: "One-knob section exits",
    strapline: "Preset-led fades, filter moves, and washouts without building a rack every session.",
    summary:
      "A transition macro built for arrangement moments: widen, filter, wash, and fade from one front panel so exits feel intentional instead of improvised.",
    bestFor: "Section exits, build-downs, and washouts",
    triggerModes: ["Amount", "Mix", "Preset"],
    highlights: [
      "Preset-led transition moves",
      "Filter, width, wash, and fade in one control surface",
      "Fast DJ-style and arrangement-style exits",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 39,
    image: "/product-art/ssp-transition.svg",
    releaseNote: "Transition macro project already present with preset-driven exit behavior.",
  },
  {
    slug: "saturate",
    name: "SSP Saturate",
    tone: "ember",
    status: "available",
    category: "tone",
    label: "Mode-based saturation",
    strapline: "Drive, color, bias, and selectable curves with a live transfer-function view.",
    summary:
      "A front-panel saturator for quick density and harmonics, with multiple curve modes and a waveshaper graph so the color change is visual as well as audible.",
    bestFor: "Harmonic color, edge, and mix-bus heat",
    triggerModes: ["Mode", "Drive", "Color", "Bias", "Mix"],
    highlights: [
      "Multiple saturation curve modes",
      "Live waveshaper display tied to current settings",
      "Drive, output, and wet/dry shaping",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 35,
    image: "/product-art/ssp-tone.svg",
    releaseNote: "Current saturation plugin includes selectable curves and a live transfer-curve preview.",
  },
  {
    slug: "clipper",
    name: "SSP Clipper",
    tone: "ember",
    status: "available",
    category: "dynamics",
    label: "Peak shaving utility",
    strapline: "4x oversampled peak control that can stay clean or lean into bite.",
    summary:
      "A simple clipper for shaving transients, catching peaks, and adding edge when the front end of a sound needs to stay up front without a dense limiter chain.",
    bestFor: "Peak shaving and transient bite",
    triggerModes: ["Drive", "Ceiling", "Softness", "Mix"],
    highlights: [
      "4x oversampled clipping",
      "Input, clip, and output metering",
      "Drive, ceiling, trim, and wet/dry control",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 25,
    image: "/product-art/ssp-clipper.svg",
    releaseNote: "Current clipper project with oversampling, metering, and simple front-panel control.",
  },
  {
    slug: "transient-control",
    name: "SSP Transient Control",
    tone: "gold",
    status: "available",
    category: "dynamics",
    label: "Attack and sustain shaping",
    strapline: "Pull the front edge forward or tuck it back, with an envelope preview that shows the move.",
    summary:
      "A direct transient shaper with attack and sustain emphasis, soft clipping, output trim, and an envelope view that helps the controls read visually instead of feeling abstract.",
    bestFor: "Punch, tail shaping, and drum contour",
    triggerModes: ["Attack", "Sustain", "Clip", "Mix"],
    highlights: [
      "Attack and sustain shaping with live detector feedback",
      "Envelope preview that moves with the controls",
      "Optional clipping and wet/dry blending",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 35,
    image: "/product-art/ssp-dynamics.svg",
    releaseNote: "Current transient shaper project with envelope preview and attack/sustain control.",
  },
  {
    slug: "vintage-compress",
    name: "SSP Vintage Compress",
    tone: "gold",
    status: "available",
    category: "dynamics",
    label: "Color-first compression",
    strapline: "Built for glue, tone, and slower musical control instead of surgical correction.",
    summary:
      "The vintage compressor slot in the lineup is meant for weight and movement, giving the catalog a more character-first compression option beside clipping and transient shaping.",
    bestFor: "Glue, color, and musical control",
    triggerModes: ["Threshold", "Ratio", "Attack", "Release"],
    highlights: [
      "Vintage-style compression lane in the lineup",
      "Front-panel control for slower, musical gain riding",
      "Complements transient and clipper tools instead of replacing them",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 45,
    image: "/product-art/ssp-dynamics.svg",
    releaseNote: "Current SSP folder includes a dedicated vintage compression project with active build output.",
  },
  {
    slug: "reducer",
    name: "SSP Reducer",
    tone: "cyan",
    status: "available",
    category: "creative",
    label: "Intentional degradation",
    strapline: "Pull audio toward grit, crush, and digital wear without a bloated modular effect chain.",
    summary:
      "Reducer fills the deliberate-lofi slot in the SSP lineup, giving quick access to downsampling, rough edges, and more obvious destruction when a cleaner signal path feels too safe.",
    bestFor: "Crunch, grit, and degraded textures",
    triggerModes: ["Amount", "Tone", "Mix"],
    highlights: [
      "Dedicated degradation slot in the product family",
      "Simple control aimed at immediate texture changes",
      "Designed to sit next to more polished SSP tone tools",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 29,
    image: "/product-art/ssp-creative.svg",
    releaseNote: "Reducer is present as a current plugin project with active build output in the SSP workspace.",
  },
  {
    slug: "eq",
    name: "SSP EQ",
    tone: "cyan",
    status: "available",
    category: "tone",
    label: "Quick corrective and sweetening EQ",
    strapline: "An SSP-style EQ for when the move should be fast, visible, and easy to trust.",
    summary:
      "EQ anchors the lineup with the most basic mix utility: shape lows, mids, and highs quickly, then move on without opening a more complex channel-strip style tool.",
    bestFor: "Balance, cleanup, and broad shaping",
    triggerModes: ["Frequency", "Gain", "Q"],
    highlights: [
      "Core tone-shaping utility in the SSP catalog",
      "Simple interface for broad moves and basic cleanup",
      "Fits the focused-tool philosophy instead of full-channel complexity",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows", "macOS packaging"],
    priceUsd: 29,
    image: "/product-art/ssp-tone.svg",
    releaseNote: "EQ project already exists with build output and rounds out the mix utility lane.",
  },
  {
    slug: "reverb",
    name: "SSP Reverb",
    tone: "cyan",
    status: "available",
    category: "space",
    label: "Modern algorithmic verb",
    strapline: "Lush mode-based reverb with quick tone control and a front panel built for speed.",
    summary:
      "A focused reverb tool with mode-based voicing and front-panel width and tone control for pads, transitions, and mix-space decisions that should happen quickly.",
    bestFor: "Pads, tails, and mix-space glue",
    triggerModes: ["Mode", "Width", "Tone", "Mix"],
    highlights: [
      "Mode-based reverb voicing",
      "Fast width and tone shaping",
      "Built around quick mix decisions instead of deep menu work",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 35,
    image: "/product-art/ssp-reverb.svg",
    releaseNote: "Current reverb project fills the dedicated space-processing lane in SSP.",
  },
  {
    slug: "echo",
    name: "SSP Echo",
    tone: "ember",
    status: "available",
    category: "space",
    label: "Colored feedback repeats",
    strapline: "The more flavored repeat tool in the lineup, with drift and drive baked into the move.",
    summary:
      "Echo is the character delay choice: timing, feedback, color, drive, and flutter for repeats that smear and move instead of staying clinical.",
    bestFor: "Textured repeats and vintage-style movement",
    triggerModes: ["Time", "Feedback", "Color", "Flutter", "Mix"],
    highlights: [
      "Color-first echo workflow",
      "Drive and flutter for movement in the repeats",
      "More character than the cleaner delay companion",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 35,
    image: "/product-art/ssp-space.svg",
    releaseNote: "Current Echo project is the more colored companion to SSP Delay.",
  },
  {
    slug: "delay",
    name: "SSP Delay",
    tone: "cyan",
    status: "available",
    category: "space",
    label: "Cleaner utility delay",
    strapline: "A tighter repeat tool with ms or note-sync timing and simple stereo control.",
    summary:
      "Delay covers the cleaner side of the repeat lane, with sync or free time, feedback, tone, width, and straight utility value for rhythmic space without a lot of color.",
    bestFor: "Utility repeats and tempo-synced space",
    triggerModes: ["Time", "Sync", "Feedback", "Tone", "Width"],
    highlights: [
      "Cleaner companion to SSP Echo",
      "Ms or note-sync timing",
      "Tone and width control for practical mix-space work",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 29,
    image: "/product-art/ssp-space.svg",
    releaseNote: "Delay project already exists with sync-aware timing and a cleaner repeat profile.",
  },
  {
    slug: "comb",
    name: "SSP Comb",
    tone: "cyan",
    status: "available",
    category: "creative",
    label: "Resonant notch and metallic motion",
    strapline: "Push material into hollow, resonant, or flanger-adjacent territory with a compact front panel.",
    summary:
      "Comb gives SSP a light resonator lane: frequency, feedback, damp, spread, and polarity for metallic or hollow motion that can stay subtle or turn more obvious.",
    bestFor: "Metallic resonances and comb-filter movement",
    triggerModes: ["Frequency", "Feedback", "Damp", "Spread", "Mix"],
    highlights: [
      "Stereo comb filter with positive and negative polarity",
      "Spread control for wider resonant movement",
      "Works as either utility filtering or more obvious effect texture",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 25,
    image: "/product-art/ssp-creative.svg",
    releaseNote: "Current comb-filter project adds a focused resonant effect lane to the catalog.",
  },
  {
    slug: "loud",
    name: "SSP Loud",
    tone: "gold",
    status: "available",
    category: "analysis",
    label: "Session loudness meter",
    strapline: "Momentary, short-term, integrated, and peak readouts in one pass-through utility.",
    summary:
      "Loud is the session overview meter in the lineup, giving you a clear read on loudness trends without turning the plugin into a full mastering suite.",
    bestFor: "Loudness checking and level sanity passes",
    triggerModes: ["Reset", "Readout View"],
    highlights: [
      "Momentary, short-term, integrated, and peak readings",
      "Pass-through meter utility",
      "Session trend view for quick loudness checks",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 29,
    image: "/product-art/ssp-analysis.svg",
    releaseNote: "Current loudness meter project covers the analysis lane beside Sub Validator.",
  },
  {
    slug: "sub-validator",
    name: "SSP Sub Validator",
    tone: "ember",
    status: "available",
    category: "analysis",
    label: "Focused low-end validator",
    strapline: "A 20 Hz to 200 Hz analyzer with instant pass/fail color feedback and spectrum freeze.",
    summary:
      "Built around the exact low-end validation job: keep the analyzer focused on the sub range, show the current level clearly, and freeze the picture when you need to inspect it.",
    bestFor: "Low-end checks and sub targeting",
    triggerModes: ["Freeze", "Live Level"],
    highlights: [
      "Focused 20 Hz to 200 Hz spectrum view",
      "Instant red or green low-end validation state",
      "Freeze button for holding the current trace",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 25,
    image: "/product-art/ssp-analysis.svg",
    releaseNote: "Current sub-analysis project is positioned around fast validation rather than full-spectrum metering.",
  },
  {
    slug: "hihat-god",
    name: "SSP Hihat God",
    tone: "gold",
    status: "available",
    category: "creative",
    label: "Tempo-synced hat movement",
    strapline: "A sine-LFO hat variance tool for volume swing and stereo drift without hit detection.",
    summary:
      "Hihat God applies gentle, musical variation over time instead of chasing individual hits, which makes hat loops feel more alive with only a few large controls.",
    bestFor: "Hat variation and stereo movement",
    triggerModes: ["Variation", "Rate", "Volume Range", "Pan Range"],
    highlights: [
      "Sine-style motion instead of stepped random behavior",
      "Tempo-synced rate divisions",
      "Separate control over loudness swing and pan width",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 25,
    image: "/product-art/ssp-creative.svg",
    releaseNote: "Current hat-variation project uses synced LFO motion for level and stereo drift.",
  },
  {
    slug: "beef",
    name: "SSP Beef",
    tone: "ember",
    status: "available",
    category: "tone",
    label: "Low-mid weight and thickness",
    strapline: "The lineup slot for making sounds feel bigger, heavier, and less fragile fast.",
    summary:
      "Beef gives the catalog a dedicated thickening tool: less about transparent correction and more about making sources feel wider, fuller, and stronger with minimal setup.",
    bestFor: "Weight, fullness, and body",
    triggerModes: ["Amount", "Tone", "Mix"],
    highlights: [
      "Dedicated thickness and body lane in the catalog",
      "Simple front panel for fast enhancement",
      "Pairs naturally with clipper, saturate, and EQ workflows",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 29,
    image: "/product-art/ssp-tone.svg",
    releaseNote: "SSP Beef already exists as a current plugin project with build output in the workspace.",
  },
  {
    slug: "3osc",
    name: "SSP 3OSC",
    tone: "cyan",
    status: "available",
    category: "instrument",
    label: "Simple three-osc synth",
    strapline: "A compact instrument lane for direct synthesis instead of another oversized workstation.",
    summary:
      "3OSC adds a lightweight instrument option to the SSP family, keeping the same focused philosophy but applying it to sound generation rather than only mix utilities.",
    bestFor: "Fast synth sketches and direct sound design",
    triggerModes: ["Oscillators", "Envelope", "Tone"],
    highlights: [
      "Three-oscillator synth concept in the SSP family",
      "Focused instrument approach instead of deep workstation complexity",
      "Balances the otherwise mix-tool-heavy lineup",
    ],
    formats: ["VST3", "Standalone"],
    platforms: ["Windows"],
    priceUsd: 39,
    image: "/product-art/ssp-instrument.svg",
    releaseNote: "Current SSP 3OSC folder includes source and build output, so it is represented as an active release.",
  },
];

export const principles: Principle[] = [
  {
    title: "Focused tools",
    body: "Each product is framed around one clear production job, which keeps the catalog easier to understand and easier to sell than a shelf full of vague overlap.",
  },
  {
    title: "One SSP ecosystem",
    body: "The site now presents both the plugin lineup and the SSP Hub desktop app so downloads, installs, and product discovery feel like one connected system.",
  },
  {
    title: "Ready to grow",
    body: "The catalog, product detail pages, cart flow, and Hub landing pages are already structured cleanly enough to take real artwork, pricing, and hosted downloads later.",
  },
];

export const workflowSteps: WorkflowStep[] = [
  {
    step: "01",
    title: "Start with the production job",
    body: "Need ducking, saturation, low-end validation, transient shaping, or space? The catalog is broad enough now that the first job is simply choosing the lane.",
  },
  {
    step: "02",
    title: "Scan the product shelf fast",
    body: "Every release is shown with pricing, positioning, formats, platforms, and a direct path into a deeper product page so visitors can compare quickly.",
  },
  {
    step: "03",
    title: "Use SSP Hub for installs later",
    body: "The new Hub pages position the desktop app as the place to handle activation, installers, and updates once the download pipeline is ready to go live.",
  },
];

export const faqItems: FaqItem[] = [
  {
    question: "Which SSP products are represented now?",
    answer:
      "The site now reflects the active SSP plugin folders with source and build output, including sidechain tools, tone and dynamics processors, analysis utilities, space effects, and the 3OSC instrument.",
  },
  {
    question: "What is SSP Hub?",
    answer:
      "SSP Hub is the desktop launcher and installer app for the SSP ecosystem. It is positioned as the place where customers browse the owned catalog, install products, manage updates, and set library paths.",
  },
  {
    question: "Are these checkout payments live yet?",
    answer:
      "No. The storefront checkout is still intentionally safe and fake. It lets you test the buying flow and order summary without connecting a payment provider yet.",
  },
  {
    question: "Are all product images final?",
    answer:
      "Not yet. Some products still use temporary category artwork so the catalog can stay complete while the release visuals catch up to the actual plugin lineup.",
  },
];
