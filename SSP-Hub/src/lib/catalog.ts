import type { CatalogProduct, HubSnapshot, InstalledPlugin } from "../../shared/contracts";

export function compareVersions(left: string, right: string) {
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

export function getInstalledPlugin(snapshot: HubSnapshot, productId: string) {
  return snapshot.installedPlugins.find((plugin) => plugin.productId === productId) ?? null;
}

export function getLicenseStatus(snapshot: HubSnapshot, productId: string) {
  return snapshot.licenseStatuses.find((licenseStatus) => licenseStatus.productId === productId) ?? null;
}

export function getArtifactForCurrentOs(snapshot: HubSnapshot, product: CatalogProduct) {
  return product.installers.find((artifact) => artifact.os === snapshot.os) ?? null;
}

export function isEntitled(snapshot: HubSnapshot, productId: string) {
  return snapshot.account.entitledProductIds.includes(productId);
}

export function hasUpdate(product: CatalogProduct, installedPlugin: InstalledPlugin | null) {
  if (!installedPlugin) {
    return false;
  }

  return compareVersions(product.version, installedPlugin.version) > 0;
}

export function formatInstalledDate(value: string) {
  return new Intl.DateTimeFormat(undefined, {
    dateStyle: "medium",
    timeStyle: "short",
  }).format(new Date(value));
}
