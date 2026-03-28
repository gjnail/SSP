import { execFile } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { promisify } from "node:util";

import { shell } from "electron";

import type {
  CatalogProduct,
  HubSnapshot,
  InstalledPlugin,
  LicenseStatus,
  SaveSettingsInput,
  SupportedOs,
} from "../../shared/contracts";
import { resolveAccount } from "./accountService";
import { discoverCatalog, getWorkspaceRootPath } from "./catalogService";
import { getDefaultInstallPath, loadState, saveState } from "./stateStore";

const execFileAsync = promisify(execFile);

function getCurrentOs(): SupportedOs {
  return process.platform === "darwin" ? "macos" : "windows";
}

function getInstallPath(targetOs: SupportedOs, product: CatalogProduct, customPath: string) {
  const fileName = `${product.displayName}.vst3`;
  return path.join(customPath || getDefaultInstallPath(targetOs), fileName);
}

function compareVersions(left: string, right: string) {
  const leftParts = left.split(".").map((segment) => Number.parseInt(segment, 10) || 0);
  const rightParts = right.split(".").map((segment) => Number.parseInt(segment, 10) || 0);
  const length = Math.max(leftParts.length, rightParts.length);

  for (let index = 0; index < length; index += 1) {
    const difference = (leftParts[index] || 0) - (rightParts[index] || 0);

    if (difference !== 0) {
      return difference;
    }
  }

  return 0;
}

function getLicenseFilePath(licensesPath: string, product: CatalogProduct) {
  return path.join(licensesPath, `${product.slug}.ssplicense.json`);
}

function readLicenseStatus(
  product: CatalogProduct,
  accountEmail: string,
  activationLimit: number,
  licensesPath: string,
) {
  const licenseFilePath = getLicenseFilePath(licensesPath, product);

  if (!fs.existsSync(licenseFilePath)) {
    return {
      activationCount: 0,
      activationLimit,
      isActivated: false,
      licenseFilePath: null,
      productId: product.id,
    } satisfies LicenseStatus;
  }

  try {
    const raw = fs.readFileSync(licenseFilePath, "utf8");
    const parsed = JSON.parse(raw) as {
      accountEmail?: string;
      activatedAt?: string;
      activationCount?: number;
      activationLimit?: number;
      machineName?: string;
      productId?: string;
    };

    const isForCurrentAccount = (parsed.accountEmail || "").trim().toLowerCase() === accountEmail.trim().toLowerCase();
    const isForCurrentProduct = parsed.productId === product.id;

    if (!isForCurrentAccount || !isForCurrentProduct) {
      return {
        activationCount: 0,
        activationLimit,
        isActivated: false,
        licenseFilePath: null,
        productId: product.id,
      } satisfies LicenseStatus;
    }

    return {
      activationCount: Math.min(parsed.activationCount || 1, parsed.activationLimit || activationLimit, activationLimit),
      activationLimit: parsed.activationLimit || activationLimit,
      activatedAt: parsed.activatedAt,
      isActivated: true,
      licenseFilePath,
      productId: product.id,
    } satisfies LicenseStatus;
  } catch {
    return {
      activationCount: 0,
      activationLimit,
      isActivated: false,
      licenseFilePath: null,
      productId: product.id,
    } satisfies LicenseStatus;
  }
}

function createLicenseStatuses(catalog: CatalogProduct[], accountEmail: string, activationLimit: number, licensesPath: string) {
  return catalog.map((product) => readLicenseStatus(product, accountEmail, activationLimit, licensesPath));
}

function syncInstalledPlugins(
  persistedInstalledPlugins: InstalledPlugin[],
  catalog: CatalogProduct[],
  targetOs: SupportedOs,
  customPath: string,
) {
  const persistedByProductId = new Map(persistedInstalledPlugins.map((plugin) => [plugin.productId, plugin]));

  return catalog
    .flatMap((product) => {
      const expectedInstallPath = getInstallPath(targetOs, product, customPath);
      const existingPlugin = persistedByProductId.get(product.id);
      const candidatePaths = [existingPlugin?.installPath, expectedInstallPath].filter(
        (candidatePath): candidatePath is string => Boolean(candidatePath),
      );
      const discoveredInstallPath = candidatePaths.find((candidatePath) => fs.existsSync(candidatePath));

      if (!discoveredInstallPath) {
        return [];
      }

      return [
        {
          activationCode: existingPlugin?.activationCode || "SSP-HUB-DETECTED",
          installedAt: existingPlugin?.installedAt || new Date().toISOString(),
          installPath: discoveredInstallPath,
          productId: product.id,
          sourceArtifact: existingPlugin?.sourceArtifact,
          version: product.version,
        } satisfies InstalledPlugin,
      ];
    })
    .sort((left, right) => left.productId.localeCompare(right.productId));
}

function createSnapshot() {
  const persisted = loadState();
  const catalog = discoverCatalog();
  const account = resolveAccount(persisted.accountEmail, catalog);
  const os = getCurrentOs();
  const installedPlugins = syncInstalledPlugins(
    persisted.installedPlugins,
    catalog,
    os,
    persisted.settings.customVst3Paths[os],
  );

  if (JSON.stringify(installedPlugins) !== JSON.stringify(persisted.installedPlugins)) {
    saveState({
      ...persisted,
      installedPlugins,
    });
  }

  return {
    account,
    catalog,
    installedPlugins,
    licenseStatuses: createLicenseStatuses(catalog, account.email, account.activationLimitPerProduct, persisted.settings.licensesPath),
    os,
    settings: {
      ...persisted.settings,
      licensesPath: persisted.settings.licensesPath,
      workspaceRoot: persisted.settings.workspaceRoot || getWorkspaceRootPath(),
    },
  } satisfies HubSnapshot;
}

function ensureProduct(snapshot: HubSnapshot, productId: string) {
  const product = snapshot.catalog.find((candidate) => candidate.id === productId);

  if (!product) {
    throw new Error(`Unknown product: ${productId}`);
  }

  return product;
}

function ensureEntitled(snapshot: HubSnapshot, productId: string) {
  if (!snapshot.account.entitledProductIds.includes(productId)) {
    throw new Error("This account does not own that plugin.");
  }
}

function getLicenseStatus(snapshot: HubSnapshot, productId: string) {
  return snapshot.licenseStatuses.find((licenseStatus) => licenseStatus.productId === productId) ?? null;
}

function artifactForCurrentOs(product: CatalogProduct, targetOs: SupportedOs) {
  const matchingArtifacts = product.installers.filter((artifact) => artifact.os === targetOs);

  if (matchingArtifacts.length === 0) {
    return null;
  }

  if (targetOs === "windows") {
    return (
      matchingArtifacts.find(
        (artifact) =>
          artifact.kind === "installer" &&
          artifact.fileName.toLowerCase().includes("installer") &&
          artifact.fileName.toLowerCase().endsWith(".exe"),
      ) ??
      matchingArtifacts.find(
        (artifact) => artifact.kind === "installer" && artifact.fileName.toLowerCase().endsWith(".exe"),
      ) ??
      matchingArtifacts.find((artifact) => artifact.kind === "archive") ??
      matchingArtifacts[0]
    );
  }

  return (
    matchingArtifacts.find((artifact) => artifact.kind === "installer") ??
    matchingArtifacts.find((artifact) => artifact.kind === "archive") ??
    matchingArtifacts[0]
  );
}

function getWindowsInstallerScriptPath(product: CatalogProduct, workspaceRoot: string) {
  return path.join(workspaceRoot, product.folderName, "scripts", "build-windows-installer.ps1");
}

function getWindowsInstallerDefinitionPath(product: CatalogProduct, workspaceRoot: string) {
  const packagingPath = path.join(workspaceRoot, product.folderName, "packaging", "windows");

  if (!fs.existsSync(packagingPath)) {
    return null;
  }

  return (
    fs.readdirSync(packagingPath)
      .filter((fileName) => fileName.toLowerCase().endsWith(".iss"))
      .map((fileName) => path.join(packagingPath, fileName))[0] ?? null
  );
}

function getWindowsExecutableInstaller(product: CatalogProduct) {
  return product.installers.find((artifact) => artifact.os === "windows" && artifact.kind === "installer") ?? null;
}

function parseInnoValue(content: string, key: string) {
  const match = content.match(new RegExp(`^${key}=(.+)$`, "m"));

  if (!match) {
    return null;
  }

  return match[1].trim().replace(/^"|"$/g, "").replace(/^\{\{/, "{");
}

function resolveInnoInstallPath(rawPath: string) {
  return rawPath
    .replace("{autopf64}", process.env.ProgramW6432 || process.env["ProgramFiles"] || "C:\\Program Files")
    .replace("{autopf32}", process.env["ProgramFiles(x86)"] || process.env["ProgramFiles"] || "C:\\Program Files (x86)");
}

function parseExecutablePath(command: string) {
  const trimmed = command.trim();

  if (!trimmed) {
    return null;
  }

  if (trimmed.startsWith('"')) {
    const closingQuote = trimmed.indexOf('"', 1);
    return closingQuote > 1 ? trimmed.slice(1, closingQuote) : null;
  }

  const executableMatch = trimmed.match(/^[^\s]+/);
  return executableMatch?.[0] ?? null;
}

async function queryRegistryValue(keyPath: string, valueName: string) {
  try {
    const { stdout } = await execFileAsync("reg.exe", ["query", keyPath, "/v", valueName], {
      windowsHide: true,
    });
    const line = stdout
      .split(/\r?\n/)
      .map((entry) => entry.trim())
      .find((entry) => entry.startsWith(valueName));

    if (!line) {
      return null;
    }

    const parts = line.split(/\s{2,}/);
    return parts[parts.length - 1] ?? null;
  } catch {
    return null;
  }
}

async function resolveWindowsUninstallerPath(product: CatalogProduct, workspaceRoot: string) {
  const installerDefinitionPath = getWindowsInstallerDefinitionPath(product, workspaceRoot);

  if (!installerDefinitionPath || !fs.existsSync(installerDefinitionPath)) {
    return null;
  }

  const definition = fs.readFileSync(installerDefinitionPath, "utf8");
  const appId = parseInnoValue(definition, "AppId");
  const defaultDirName = parseInnoValue(definition, "DefaultDirName");

  if (appId) {
    const registryRoots = [
      `HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${appId}_is1`,
      `HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${appId}_is1`,
      `HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${appId}_is1`,
    ];

    for (const registryKey of registryRoots) {
      const uninstallCommand =
        (await queryRegistryValue(registryKey, "QuietUninstallString")) ??
        (await queryRegistryValue(registryKey, "UninstallString"));
      const executablePath = uninstallCommand ? parseExecutablePath(uninstallCommand) : null;

      if (executablePath && fs.existsSync(executablePath)) {
        return executablePath;
      }
    }
  }

  if (!defaultDirName) {
    return null;
  }

  const fallbackUninstaller = path.join(resolveInnoInstallPath(defaultDirName), "unins000.exe");
  return fs.existsSync(fallbackUninstaller) ? fallbackUninstaller : null;
}

async function buildWindowsInstallerForProduct(product: CatalogProduct, workspaceRoot: string) {
  const scriptPath = getWindowsInstallerScriptPath(product, workspaceRoot);

  try {
    await execFileAsync(
      "powershell.exe",
      ["-ExecutionPolicy", "Bypass", "-File", scriptPath, "-Config", "Release"],
      {
        cwd: path.join(workspaceRoot, product.folderName),
        windowsHide: true,
      },
    );
  } catch (error) {
    if (error instanceof Error) {
      throw new Error(`Installer build failed for ${product.displayName}: ${error.message}`);
    }

    throw new Error(`Installer build failed for ${product.displayName}.`);
  }
}

function updateInstalledState(snapshot: HubSnapshot, product: CatalogProduct, artifactPath: string) {
  const persisted = loadState();
  const installPath = getInstallPath(snapshot.os, product, snapshot.settings.customVst3Paths[snapshot.os]);
  const nextInstalled = persisted.installedPlugins.filter((plugin) => plugin.productId !== product.id);

  nextInstalled.push({
    activationCode: "SSP-HUB-LOCAL",
    installedAt: new Date().toISOString(),
    installPath,
    productId: product.id,
    sourceArtifact: artifactPath,
    version: product.version,
  });

  saveState({
    ...persisted,
    installedPlugins: nextInstalled,
  });
}

function removePathIfExists(targetPath: string) {
  if (!targetPath || !fs.existsSync(targetPath)) {
    return false;
  }

  fs.rmSync(targetPath, { force: true, recursive: true });
  return true;
}

function ensureFolderExists(targetPath: string) {
  fs.mkdirSync(targetPath, { recursive: true });
}

export function getSnapshot() {
  return createSnapshot();
}

export async function buildInstaller(productId: string) {
  const snapshot = createSnapshot();
  const product = ensureProduct(snapshot, productId);

  if (snapshot.os !== "windows") {
    throw new Error("Real installer builds are currently wired for Windows packaging scripts only.");
  }

  if (!fs.existsSync(getWindowsInstallerScriptPath(product, snapshot.settings.workspaceRoot))) {
    throw new Error(`No Windows installer script was found for ${product.displayName}.`);
  }

  await buildWindowsInstallerForProduct(product, snapshot.settings.workspaceRoot);
  return createSnapshot();
}

export function activateProduct(productId: string) {
  const snapshot = createSnapshot();
  ensureEntitled(snapshot, productId);
  const product = ensureProduct(snapshot, productId);
  const installed = snapshot.installedPlugins.find((plugin) => plugin.productId === productId);

  if (!installed) {
    throw new Error(`Install ${product.displayName} before activating it.`);
  }

  const existingLicense = getLicenseStatus(snapshot, productId);

  if (existingLicense?.isActivated) {
    return snapshot;
  }

  ensureFolderExists(snapshot.settings.licensesPath);

  const licenseFilePath = getLicenseFilePath(snapshot.settings.licensesPath, product);
  const payload = {
    accountEmail: snapshot.account.email,
    activatedAt: new Date().toISOString(),
    activationCount: 1,
    activationLimit: snapshot.account.activationLimitPerProduct,
    installPath: installed.installPath,
    machineName: os.hostname(),
    productDisplayName: product.displayName,
    productId: product.id,
    version: product.version,
  };

  fs.writeFileSync(licenseFilePath, JSON.stringify(payload, null, 2), "utf8");
  return createSnapshot();
}

export function deactivateProduct(productId: string) {
  const snapshot = createSnapshot();
  const product = ensureProduct(snapshot, productId);
  const licenseStatus = getLicenseStatus(snapshot, productId);

  if (!licenseStatus?.licenseFilePath) {
    return snapshot;
  }

  removePathIfExists(licenseStatus.licenseFilePath);
  return createSnapshot();
}

export async function buildAllWindowsInstallers() {
  const snapshot = createSnapshot();

  if (snapshot.os !== "windows") {
    throw new Error("Build-all is currently wired for Windows packaging scripts only.");
  }

  for (const product of snapshot.catalog) {
    const scriptPath = getWindowsInstallerScriptPath(product, snapshot.settings.workspaceRoot);

    if (!fs.existsSync(scriptPath)) {
      continue;
    }

    try {
      await buildWindowsInstallerForProduct(product, snapshot.settings.workspaceRoot);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      throw new Error(`Stopped while building installers. ${message}`);
    }
  }

  return createSnapshot();
}

export function signIn(email: string) {
  const persisted = loadState();
  saveState({
    ...persisted,
    accountEmail: email.trim().toLowerCase(),
  });

  return createSnapshot();
}

export function saveSettings(input: SaveSettingsInput) {
  const persisted = loadState();
  saveState({
    ...persisted,
    settings: {
      ...persisted.settings,
      ...input,
      customVst3Paths: {
        ...persisted.settings.customVst3Paths,
        ...input.customVst3Paths,
      },
      licensesPath: input.licensesPath || persisted.settings.licensesPath,
    },
  });

  return createSnapshot();
}

export async function installProduct(productId: string) {
  let snapshot = createSnapshot();
  ensureEntitled(snapshot, productId);

  const product = ensureProduct(snapshot, productId);
  let artifact = artifactForCurrentOs(product, snapshot.os);

  if (!artifact && snapshot.os === "windows") {
    await buildWindowsInstallerForProduct(product, snapshot.settings.workspaceRoot);
    snapshot = createSnapshot();
    const refreshedProduct = ensureProduct(snapshot, productId);
    artifact = getWindowsExecutableInstaller(refreshedProduct) ?? artifactForCurrentOs(refreshedProduct, snapshot.os);
  }

  if (!artifact) {
    throw new Error(`No ${snapshot.os} installer is staged for ${product.displayName}.`);
  }

  if (snapshot.os === "windows" && artifact.kind === "installer") {
    const launchError = await shell.openPath(artifact.path);

    if (launchError) {
      throw new Error(`Installer launch failed for ${product.displayName}: ${launchError}`);
    }
  } else {
    shell.showItemInFolder(artifact.path);
  }

  updateInstalledState(snapshot, product, artifact.path);
  return createSnapshot();
}

export async function updateProduct(productId: string) {
  const snapshot = createSnapshot();
  const installed = snapshot.installedPlugins.find((plugin) => plugin.productId === productId);

  if (!installed) {
    throw new Error("Install the plugin before updating it.");
  }

  const product = ensureProduct(snapshot, productId);

  if (compareVersions(product.version, installed.version) <= 0) {
    return snapshot;
  }

  return installProduct(productId);
}

export async function uninstallProduct(productId: string) {
  const snapshot = createSnapshot();
  const product = ensureProduct(snapshot, productId);
  const installed = snapshot.installedPlugins.find((plugin) => plugin.productId === productId);

  if (!installed) {
    throw new Error(`${product.displayName} is not marked as installed in SSP Hub.`);
  }

  if (snapshot.os === "windows") {
    const uninstallerPath = await resolveWindowsUninstallerPath(product, snapshot.settings.workspaceRoot);

    if (uninstallerPath) {
      const launchError = await shell.openPath(uninstallerPath);

      if (launchError) {
        throw new Error(`Uninstaller launch failed for ${product.displayName}: ${launchError}`);
      }
    } else {
      const installerDefinitionPath = getWindowsInstallerDefinitionPath(product, snapshot.settings.workspaceRoot);
      const definition =
        installerDefinitionPath && fs.existsSync(installerDefinitionPath)
          ? fs.readFileSync(installerDefinitionPath, "utf8")
          : null;
      const defaultDirName = definition ? parseInnoValue(definition, "DefaultDirName") : null;
      const fallbackStandalonePath = defaultDirName ? resolveInnoInstallPath(defaultDirName) : null;

      const removedAnything = [
        removePathIfExists(installed.installPath),
        fallbackStandalonePath ? removePathIfExists(fallbackStandalonePath) : false,
      ].some(Boolean);

      if (!removedAnything) {
        throw new Error(
          `No Windows uninstaller was found for ${product.displayName}, and SSP Hub could not find installed files to remove.`,
        );
      }
    }
  }

  const persisted = loadState();
  saveState({
    ...persisted,
    installedPlugins: persisted.installedPlugins.filter((plugin) => plugin.productId !== productId),
  });

  return createSnapshot();
}

export async function revealArtifact(productId: string) {
  const snapshot = createSnapshot();
  const product = ensureProduct(snapshot, productId);
  const artifact = artifactForCurrentOs(product, snapshot.os);

  if (!artifact) {
    return false;
  }

  shell.showItemInFolder(artifact.path);
  return true;
}
