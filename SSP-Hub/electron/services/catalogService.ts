import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import type { CatalogProduct, InstallerArtifact, SupportedOs } from "../../shared/contracts";

const PRODUCT_COPY: Record<string, { category: string; description: string; summary: string }> = {
  "simple-sidechain": {
    category: "Dynamics",
    description: "Fast ducking and pumping with a stripped-back control surface for quick mixes.",
    summary: "The sidechain tool you reach for when the session needs movement in seconds.",
  },
  "ssp-3osc": {
    category: "Instrument",
    description: "A three-oscillator synth tuned for direct sound design, basses, leads, and hooks.",
    summary: "Simple oscillator stacking with SSP flavor and quick results.",
  },
  "ssp-beef": {
    category: "Tone",
    description: "Low-end reinforcement and harmonics designed to make sources feel larger without mud.",
    summary: "Weight, density, and extra body when the mix needs more size.",
  },
  "ssp-clipper": {
    category: "Dynamics",
    description: "Loudness-focused clipping with simple drive control and mix-ready output behavior.",
    summary: "Push louder masters and drums without overthinking the gain stage.",
  },
  "ssp-comb": {
    category: "Creative",
    description: "Comb-filter coloration for metallic, resonant, and synthetic texture shaping.",
    summary: "Resonance and strange movement for transitions and sound design.",
  },
  "ssp-delay": {
    category: "Time",
    description: "Straightforward delay for groove, width, and classic feedback effects.",
    summary: "Tempo-friendly echoes built for musical movement.",
  },
  "ssp-echo": {
    category: "Time",
    description: "Atmospheric echo processing with more tone and space than a utility delay.",
    summary: "Character echoes that sit around a vocal or synth line easily.",
  },
  "ssp-eq": {
    category: "Mix",
    description: "Mix-focused EQ for shaping tone quickly without diving through complicated views.",
    summary: "Fast corrective and sweetening moves in a clean package.",
  },
  "ssp-hihat-god": {
    category: "Drums",
    description: "Transient and tone control aimed at hats, percussion, and high-frequency groove elements.",
    summary: "Sharper hats, cleaner tops, and a little extra attitude.",
  },
  "ssp-loud": {
    category: "Dynamics",
    description: "A loudness maximizer for fast finishing moves and aggressive final level shaping.",
    summary: "The finishing push when a track needs more volume and energy.",
  },
  "ssp-multichain": {
    category: "Dynamics",
    description: "Multiband sidechain control for cleaner ducks, more separation, and tighter low-end management.",
    summary: "Duck only what matters and keep the rest of the mix intact.",
  },
  "ssp-reducer": {
    category: "Dynamics",
    description: "Simple reduction and control for taming peaks, reshaping punch, and tightening dynamics.",
    summary: "A compact dynamics shaper for everyday control.",
  },
  "ssp-reverb": {
    category: "Space",
    description: "Room and wash generation tuned for usable size, depth, and modern tails.",
    summary: "Space that feels musical fast, not endlessly tweakable.",
  },
  "ssp-saturate": {
    category: "Tone",
    description: "Saturation and harmonic enhancement for grit, warmth, and edge.",
    summary: "Bring flat sounds forward with controlled texture.",
  },
  "ssp-sub-validator": {
    category: "Utility",
    description: "Low-end translation checks and sub-focused analysis for club-ready decisions.",
    summary: "A reality check for subs before the track leaves the studio.",
  },
  "ssp-transient-control": {
    category: "Dynamics",
    description: "Attack and sustain shaping for drums, loops, and punch-forward sources.",
    summary: "Dial in impact without opening a full dynamics chain.",
  },
  "ssp-transition": {
    category: "Creative",
    description: "Transition effects and energy moves for risers, drops, and arrangement glue.",
    summary: "Fast builds, impacts, and movement for arrangement moments.",
  },
  "ssp-vintage-compress": {
    category: "Dynamics",
    description: "Vintage-inspired compression behavior for tone, glue, and musical movement.",
    summary: "More vibe and motion than a plain utility compressor.",
  },
};

function getWorkspaceRoot() {
  return path.join(os.homedir(), "Documents", "SSP");
}

function normalize(value: string) {
  return value.toLowerCase().replace(/[^a-z0-9]/g, "");
}

function titleCase(value: string) {
  return value
    .split(" ")
    .filter(Boolean)
    .map((segment) => segment.charAt(0).toUpperCase() + segment.slice(1).toLowerCase())
    .join(" ");
}

function deriveDisplayName(folderName: string, productToken: string) {
  const folderOverrides: Record<string, string> = {
    "Simple-Sidechain": "SSP Simple Sidechain",
    "SSP-3OSC": "SSP Reactor",
    "SSP-Multichain": "SSP Multiband Sidechain",
  };

  if (folderOverrides[folderName]) {
    return folderOverrides[folderName];
  }

  const cleaned = productToken
    .replace(/^SSP_/, "SSP ")
    .replace(/_/g, " ")
    .trim();

  if (cleaned.startsWith("SSP ")) {
    return `SSP ${titleCase(cleaned.slice(4))}`;
  }

  return titleCase(cleaned);
}

function deriveOs(filePath: string): SupportedOs | null {
  const normalized = filePath.toLowerCase();

  if (normalized.includes("macos") || normalized.endsWith(".dmg") || normalized.endsWith(".pkg")) {
    return "macos";
  }

  if (normalized.includes("windows") || normalized.endsWith(".exe") || normalized.endsWith(".msi")) {
    return "windows";
  }

  return null;
}

function deriveKind(filePath: string): InstallerArtifact["kind"] {
  const extension = path.extname(filePath).toLowerCase();

  if (extension === ".zip") {
    return "archive";
  }

  if (extension === ".dmg" || extension === ".pkg" || extension === ".exe" || extension === ".msi") {
    return "installer";
  }

  return "handoff";
}

function collectInstallerFiles(root: string) {
  if (!fs.existsSync(root)) {
    return [];
  }

  const queue = [root];
  const files: string[] = [];

  while (queue.length > 0) {
    const current = queue.shift();

    if (!current) {
      continue;
    }

    for (const entry of fs.readdirSync(current, { withFileTypes: true })) {
      const fullPath = path.join(current, entry.name);

      if (entry.isDirectory()) {
        queue.push(fullPath);
      } else {
        files.push(fullPath);
      }
    }
  }

  return files;
}

function discoverArtifacts(
  folderName: string,
  displayName: string,
  productToken: string,
  installerFiles: string[],
) {
  const aliases = [
    folderName,
    displayName,
    productToken,
    displayName.replace(/^SSP /, ""),
    folderName.replace(/^SSP-/, ""),
  ]
    .map(normalize)
    .filter(Boolean);

  return installerFiles
    .filter((candidatePath) => aliases.some((alias) => normalize(candidatePath).includes(alias)))
    .map((candidatePath) => {
      const os = deriveOs(candidatePath);

      if (!os) {
        return null;
      }

      return {
        fileName: path.basename(candidatePath),
        kind: deriveKind(candidatePath),
        os,
        path: candidatePath,
      } satisfies InstallerArtifact;
    })
    .filter((artifact): artifact is InstallerArtifact => artifact !== null);
}

export function discoverCatalog() {
  const workspaceRoot = getWorkspaceRoot();

  if (!fs.existsSync(workspaceRoot)) {
    return [];
  }

  const installerFiles = collectInstallerFiles(workspaceRoot);

  return fs
    .readdirSync(workspaceRoot, { withFileTypes: true })
    .filter((entry) => entry.isDirectory())
    .filter((entry) => entry.name === "Simple-Sidechain" || entry.name.startsWith("SSP-"))
    .filter((entry) => !["SSP-Hub", "SSP-Website"].includes(entry.name))
    .map((entry) => {
      const folderPath = path.join(workspaceRoot, entry.name);
      const cmakePath = path.join(folderPath, "CMakeLists.txt");

      if (!fs.existsSync(cmakePath)) {
        return null;
      }

      const cmakeContent = fs.readFileSync(cmakePath, "utf8");
      const productToken =
        cmakeContent.match(/set\(PRODUCT_NAME\s+"?([^\s\)"]+)"?\)/)?.[1] ??
        cmakeContent.match(/project\(([^\s\)]+)/)?.[1] ??
        entry.name;
      const version = cmakeContent.match(/set\(PROJECT_VERSION\s+"?([^\s\)"]+)"?\)/)?.[1] ?? "0.1.0";
      const displayName = deriveDisplayName(entry.name, productToken);
      const id = entry.name.toLowerCase();
      const scopedInstallerFiles = [
        ...installerFiles,
        ...collectInstallerFiles(path.join(folderPath, "dist")),
        ...collectInstallerFiles(path.join(folderPath, "build", "dist")),
      ];
      const copy = PRODUCT_COPY[id] ?? {
        category: "Plugin",
        description: "A Stupid Simple Plugins release managed through SSP Hub.",
        summary: "Managed through SSP Hub.",
      };

      return {
        category: copy.category,
        description: copy.description,
        displayName,
        folderName: entry.name,
        id,
        installers: discoverArtifacts(entry.name, displayName, productToken, scopedInstallerFiles),
        slug: id,
        summary: copy.summary,
        version,
      } satisfies CatalogProduct;
    })
    .filter((product): product is CatalogProduct => product !== null)
    .sort((left, right) => left.displayName.localeCompare(right.displayName));
}

export function getWorkspaceRootPath() {
  return getWorkspaceRoot();
}
