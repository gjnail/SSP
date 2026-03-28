export type SupportedOs = "windows" | "macos";
export type ThemeMode = "light" | "dark";

export type InstallerKind = "installer" | "archive" | "handoff";

export interface InstallerArtifact {
  fileName: string;
  kind: InstallerKind;
  os: SupportedOs;
  path: string;
}

export interface CatalogProduct {
  category: string;
  description: string;
  displayName: string;
  folderName: string;
  id: string;
  installers: InstallerArtifact[];
  slug: string;
  summary: string;
  version: string;
}

export interface AccountProfile {
  activationLimitPerProduct: number;
  email: string;
  entitledProductIds: string[];
  planLabel: string;
}

export interface LicenseStatus {
  activationCount: number;
  activationLimit: number;
  activatedAt?: string;
  isActivated: boolean;
  licenseFilePath: string | null;
  productId: string;
}

export interface InstalledPlugin {
  activationCode: string;
  installedAt: string;
  installPath: string;
  productId: string;
  sourceArtifact?: string;
  version: string;
}

export interface HubSettings {
  autoInstallAfterDownload: boolean;
  autoRefreshCatalog: boolean;
  customVst3Paths: Record<SupportedOs, string>;
  licensesPath: string;
  themeMode: ThemeMode;
  workspaceRoot: string;
}

export interface HubSnapshot {
  account: AccountProfile;
  catalog: CatalogProduct[];
  installedPlugins: InstalledPlugin[];
  licenseStatuses: LicenseStatus[];
  os: SupportedOs;
  settings: HubSettings;
}

export type SaveSettingsInput = Partial<HubSettings> & {
  customVst3Paths?: Partial<Record<SupportedOs, string>>;
};

export interface SspHubApi {
  activateProduct(productId: string): Promise<HubSnapshot>;
  browseForFolder(initialPath?: string): Promise<string | null>;
  buildAllWindowsInstallers(): Promise<HubSnapshot>;
  buildInstaller(productId: string): Promise<HubSnapshot>;
  deactivateProduct(productId: string): Promise<HubSnapshot>;
  getSnapshot(): Promise<HubSnapshot>;
  installProduct(productId: string): Promise<HubSnapshot>;
  revealArtifact(productId: string): Promise<boolean>;
  saveSettings(input: SaveSettingsInput): Promise<HubSnapshot>;
  signIn(email: string): Promise<HubSnapshot>;
  uninstallProduct(productId: string): Promise<HubSnapshot>;
  updateProduct(productId: string): Promise<HubSnapshot>;
}
