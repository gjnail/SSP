import { Link } from "react-router-dom";

import type { CatalogProduct, HubSnapshot } from "../../../shared/contracts";
import { getArtifactForCurrentOs, getInstalledPlugin, getLicenseStatus, hasUpdate, isEntitled } from "../../lib/catalog";
import { Badge } from "../ui/Badge";
import { Button } from "../ui/Button";
import { PluginArtwork } from "./PluginArtwork";

interface ProductCardProps {
  onActivate: (productId: string) => Promise<void>;
  busyAction: string | null;
  onBuildInstaller: (productId: string) => Promise<void>;
  onDeactivate: (productId: string) => Promise<void>;
  onInstall: (productId: string) => Promise<void>;
  onUninstall: (productId: string) => Promise<void>;
  onUpdate: (productId: string) => Promise<void>;
  product: CatalogProduct;
  snapshot: HubSnapshot;
}

function getTone(productId: string) {
  const tones = ["gold", "cyan", "ember"] as const;
  const total = productId.split("").reduce((sum, character) => sum + character.charCodeAt(0), 0);
  return tones[total % tones.length];
}

export function ProductCard({
  onActivate,
  busyAction,
  onBuildInstaller,
  onDeactivate,
  onInstall,
  onUninstall,
  onUpdate,
  product,
  snapshot,
}: ProductCardProps) {
  const entitled = isEntitled(snapshot, product.id);
  const installedPlugin = getInstalledPlugin(snapshot, product.id);
  const licenseStatus = getLicenseStatus(snapshot, product.id);
  const artifact = getArtifactForCurrentOs(snapshot, product);
  const updateAvailable = hasUpdate(product, installedPlugin);
  const tone = getTone(product.id);

  return (
    <article className="product-card" data-tone={tone}>
      <div className="product-card__rail">
        <span className="product-card__index">{product.displayName.replace(/^SSP\s*/i, "").slice(0, 3).toUpperCase()}</span>
        <Badge tone="muted">{product.category}</Badge>
      </div>
      <div className="product-card__body">
        <div className="product-card__eyebrow">
          <Badge tone={entitled ? "accent" : "warning"}>{entitled ? "Owned" : "Locked"}</Badge>
          <Badge tone={artifact ? "success" : "muted"}>{artifact ? snapshot.os : "Not staged"}</Badge>
          <Badge tone={licenseStatus?.isActivated ? "success" : "muted"}>
            {licenseStatus?.isActivated ? `Activated ${licenseStatus.activationCount}/${licenseStatus.activationLimit}` : "Not activated"}
          </Badge>
        </div>

        <div className="product-card__header">
          <PluginArtwork productId={product.id} title={product.displayName} />
          <h3>{product.displayName}</h3>
        </div>
        <p className="product-card__summary">{product.summary}</p>
        <p className="product-card__description">{product.description}</p>

        <div className="product-card__meta">
          <span>{product.category}</span>
          <span>v{product.version}</span>
        </div>
      </div>
      <div className="product-card__sidebar">
        <div className="product-card__sidebar-block">
          <span className="product-card__meta-label">Release</span>
          <strong>{artifact ? artifact.fileName : `No ${snapshot.os} build staged`}</strong>
        </div>
        <div className="product-card__sidebar-block">
          <span className="product-card__meta-label">Status</span>
          <strong>
            {installedPlugin ? `Installed v${installedPlugin.version}` : entitled ? "Ready for install" : "Requires purchase"}
          </strong>
        </div>
        <div className="product-card__sidebar-block">
          <span className="product-card__meta-label">License</span>
          <strong>
            {licenseStatus?.isActivated
              ? `Local seat ${licenseStatus.activationCount}/${licenseStatus.activationLimit}`
              : `${licenseStatus?.activationCount || 0}/${licenseStatus?.activationLimit || snapshot.account.activationLimitPerProduct} activations used`}
          </strong>
        </div>
        <div className="product-card__actions">
          <Link className="button button--ghost" to={`/products/${product.slug}`}>
            View details
          </Link>
          {entitled && snapshot.os === "windows" && !artifact ? (
            <Button disabled={busyAction === `build:${product.id}`} onClick={() => void onBuildInstaller(product.id)} tone="secondary">
              {busyAction === `build:${product.id}` ? "Building..." : "Build installer"}
            </Button>
          ) : null}
          {entitled && !installedPlugin && artifact ? (
            <Button disabled={busyAction === `install:${product.id}`} onClick={() => void onInstall(product.id)}>
              {busyAction === `install:${product.id}` ? "Launching..." : "Install plugin"}
            </Button>
          ) : null}
          {entitled && installedPlugin && updateAvailable ? (
            <Button disabled={busyAction === `update:${product.id}`} onClick={() => void onUpdate(product.id)}>
              {busyAction === `update:${product.id}` ? "Updating..." : "Update"}
            </Button>
          ) : null}
          {entitled && installedPlugin && !licenseStatus?.isActivated ? (
            <Button disabled={busyAction === `activate:${product.id}`} onClick={() => void onActivate(product.id)} tone="secondary">
              {busyAction === `activate:${product.id}` ? "Activating..." : "Activate"}
            </Button>
          ) : null}
          {licenseStatus?.isActivated ? (
            <Button
              disabled={busyAction === `deactivate:${product.id}`}
              onClick={() => void onDeactivate(product.id)}
              tone="ghost"
            >
              {busyAction === `deactivate:${product.id}` ? "Deactivating..." : "Deactivate"}
            </Button>
          ) : null}
          {installedPlugin ? (
            <Button
              disabled={busyAction === `uninstall:${product.id}`}
              onClick={() => void onUninstall(product.id)}
              tone="danger"
            >
              {busyAction === `uninstall:${product.id}` ? "Removing..." : "Uninstall"}
            </Button>
          ) : null}
        </div>
      </div>
    </article>
  );
}
