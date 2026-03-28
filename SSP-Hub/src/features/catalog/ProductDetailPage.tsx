import { Link, useParams } from "react-router-dom";

import { useHub } from "../../app/HubProvider";
import { PluginArtwork } from "../../components/catalog/PluginArtwork";
import { Badge } from "../../components/ui/Badge";
import { Button } from "../../components/ui/Button";
import { Panel } from "../../components/ui/Panel";
import {
  formatInstalledDate,
  getArtifactForCurrentOs,
  getInstalledPlugin,
  getLicenseStatus,
  hasUpdate,
  isEntitled,
} from "../../lib/catalog";

export function ProductDetailPage() {
  const { activateProduct, buildInstaller, busyAction, deactivateProduct, installProduct, snapshot, uninstallProduct, updateProduct } = useHub();
  const { slug } = useParams();

  if (!snapshot) {
    return null;
  }

  const product = snapshot.catalog.find((candidate) => candidate.slug === slug);

  if (!product) {
    return (
      <Panel>
        <h1>Plugin not found</h1>
        <p>That product is not in the discovered SSP catalog.</p>
        <Link className="button button--ghost" to="/catalog">
          Back to catalog
        </Link>
      </Panel>
    );
  }

  const entitled = isEntitled(snapshot, product.id);
  const installedPlugin = getInstalledPlugin(snapshot, product.id);
  const licenseStatus = getLicenseStatus(snapshot, product.id);
  const artifact = getArtifactForCurrentOs(snapshot, product);
  const updateAvailable = hasUpdate(product, installedPlugin);

  return (
    <div className="page page--stack">
      <Panel className="detail-hero">
        <PluginArtwork productId={product.id} title={product.displayName} />
        <div className="detail-hero__copy">
          <div className="detail-hero__badges">
            <Badge tone="accent">{product.category}</Badge>
            <Badge tone={entitled ? "success" : "warning"}>{entitled ? "Owned" : "Locked"}</Badge>
            <Badge tone={artifact ? "success" : "muted"}>{artifact ? `${snapshot.os} available` : "No staged build"}</Badge>
            <Badge tone={licenseStatus?.isActivated ? "success" : "muted"}>
              {licenseStatus?.isActivated ? `Activated ${licenseStatus.activationCount}/${licenseStatus.activationLimit}` : "Not activated"}
            </Badge>
          </div>
          <h1>{product.displayName}</h1>
          <p>{product.description}</p>
          <div className="detail-hero__actions">
            {entitled && snapshot.os === "windows" && !artifact ? (
              <Button disabled={busyAction === `build:${product.id}`} onClick={() => void buildInstaller(product.id)} tone="secondary">
                {busyAction === `build:${product.id}` ? "Building..." : "Build installer"}
              </Button>
            ) : null}
            {entitled && !installedPlugin && artifact ? (
              <Button disabled={busyAction === `install:${product.id}`} onClick={() => void installProduct(product.id)}>
                {busyAction === `install:${product.id}` ? "Launching..." : "Install plugin"}
              </Button>
            ) : null}
            {entitled && installedPlugin && updateAvailable ? (
              <Button disabled={busyAction === `update:${product.id}`} onClick={() => void updateProduct(product.id)}>
                {busyAction === `update:${product.id}` ? "Updating..." : "Update to latest"}
              </Button>
            ) : null}
            {entitled && installedPlugin && !licenseStatus?.isActivated ? (
              <Button disabled={busyAction === `activate:${product.id}`} onClick={() => void activateProduct(product.id)} tone="secondary">
                {busyAction === `activate:${product.id}` ? "Activating..." : "Activate plugin"}
              </Button>
            ) : null}
            {licenseStatus?.isActivated ? (
              <Button disabled={busyAction === `deactivate:${product.id}`} onClick={() => void deactivateProduct(product.id)} tone="ghost">
                {busyAction === `deactivate:${product.id}` ? "Deactivating..." : "Deactivate license"}
              </Button>
            ) : null}
            {installedPlugin ? (
              <Button
                disabled={busyAction === `uninstall:${product.id}`}
                onClick={() => void uninstallProduct(product.id)}
                tone="danger"
              >
                {busyAction === `uninstall:${product.id}` ? "Removing..." : "Uninstall"}
              </Button>
            ) : null}
          </div>
        </div>
      </Panel>

      <div className="detail-grid">
        <Panel>
          <h2>Release status</h2>
          <dl className="detail-list">
            <div>
              <dt>Version</dt>
              <dd>{product.version}</dd>
            </div>
            <div>
              <dt>Account access</dt>
              <dd>{entitled ? snapshot.account.planLabel : "Not included with this account"}</dd>
            </div>
            <div>
              <dt>Staged installer</dt>
              <dd>{artifact ? artifact.fileName : `No ${snapshot.os} installer detected`}</dd>
            </div>
          </dl>
        </Panel>

        <Panel>
          <h2>Install state</h2>
          {installedPlugin ? (
            <dl className="detail-list">
              <div>
                <dt>Installed version</dt>
                <dd>{installedPlugin.version}</dd>
              </div>
              <div>
                <dt>Activation</dt>
                <dd>{licenseStatus?.isActivated ? `${installedPlugin.activationCode} (${licenseStatus.activationCount}/${licenseStatus.activationLimit})` : "Not activated"}</dd>
              </div>
              <div>
                <dt>Installed at</dt>
                <dd>{formatInstalledDate(installedPlugin.installedAt)}</dd>
              </div>
              <div>
                <dt>VST3 path</dt>
                <dd>{installedPlugin.installPath}</dd>
              </div>
              <div>
                <dt>License file</dt>
                <dd>{licenseStatus?.licenseFilePath || snapshot.settings.licensesPath}</dd>
              </div>
            </dl>
          ) : (
            <p className="muted-copy">This plugin has not been installed through SSP Hub yet.</p>
          )}
        </Panel>
      </div>
    </div>
  );
}
