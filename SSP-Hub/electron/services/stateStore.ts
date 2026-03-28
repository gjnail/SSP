import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { app } from "electron";

import type { HubSettings, InstalledPlugin, SupportedOs } from "../../shared/contracts";

interface PersistedState {
  accountEmail: string;
  installedPlugins: InstalledPlugin[];
  settings: HubSettings;
}

function getCurrentOs(): SupportedOs {
  return process.platform === "darwin" ? "macos" : "windows";
}

function defaultVst3Path(targetOs: SupportedOs) {
  return targetOs === "macos"
    ? "/Library/Audio/Plug-Ins/VST3"
    : "C:\\Program Files\\Common Files\\VST3";
}

function defaultLicensesPath() {
  return path.join(os.homedir(), "Documents", "SSP Licenses");
}

function createDefaultState(): PersistedState {
  const workspaceRoot = path.join(os.homedir(), "Documents", "SSP");

  return {
    accountEmail: "studio@stupidsimpleplugins.com",
    installedPlugins: [
      {
        activationCode: "SSP-HUB-LOCAL",
        installedAt: new Date().toISOString(),
        installPath: defaultVst3Path(getCurrentOs()),
        productId: "ssp-transition",
        version: "0.0.9",
      },
    ],
    settings: {
      autoInstallAfterDownload: true,
      autoRefreshCatalog: true,
      customVst3Paths: {
        macos: defaultVst3Path("macos"),
        windows: defaultVst3Path("windows"),
      },
      licensesPath: defaultLicensesPath(),
      themeMode: "light",
      workspaceRoot,
    },
  };
}

function getStateFilePath() {
  return path.join(app.getPath("userData"), "ssp-hub-state.json");
}

export function loadState() {
  const filePath = getStateFilePath();

  if (!fs.existsSync(filePath)) {
    const defaults = createDefaultState();
    saveState(defaults);
    return defaults;
  }

  try {
    const raw = fs.readFileSync(filePath, "utf8");
    const parsed = JSON.parse(raw) as PersistedState;
    const defaults = createDefaultState();

    return {
      accountEmail: parsed.accountEmail || defaults.accountEmail,
      installedPlugins: parsed.installedPlugins || defaults.installedPlugins,
      settings: {
        ...defaults.settings,
        ...parsed.settings,
        customVst3Paths: {
          ...defaults.settings.customVst3Paths,
          ...parsed.settings?.customVst3Paths,
        },
      },
    } satisfies PersistedState;
  } catch {
    const defaults = createDefaultState();
    saveState(defaults);
    return defaults;
  }
}

export function saveState(state: PersistedState) {
  const filePath = getStateFilePath();
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, JSON.stringify(state, null, 2), "utf8");
}

export function getDefaultInstallPath(targetOs: SupportedOs) {
  return defaultVst3Path(targetOs);
}
