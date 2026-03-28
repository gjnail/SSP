import { Link } from "react-router-dom";

import { useHub } from "../../app/HubProvider";
import { Badge } from "../../components/ui/Badge";
import { Button } from "../../components/ui/Button";
import { Panel } from "../../components/ui/Panel";
import { formatInstalledDate, hasUpdate } from "../../lib/catalog";

export function UpdatesPage() {
  const { busyAction, snapshot, uninstallProduct, updateProduct } = useHub();

  if (!snapshot) {
    return null;
  }

  const installedProducts = snapshot.installedPlugins
    .map((plugin) => ({
      installed: plugin,
      product: snapshot.catalog.find((candidate) => candidate.id === plugin.productId),
    }))
    .filter(
      (
        entry,
      ): entry is {
        installed: (typeof snapshot.installedPlugins)[number];
        product: (typeof snapshot.catalog)[number];
      } => Boolean(entry.product),
    );

  return (
    <div className="page page--stack">
      <div className="section-heading">
        <div>
          <span className="section-heading__eyebrow">Updates and uninstall</span>
          <h1>Track installed versions and manage refreshes.</h1>
        </div>
      </div>

      {installedProducts.length === 0 ? (
        <Panel>
          <h2>No installs tracked yet</h2>
          <p>Install a plugin from the catalog or library and it will appear here.</p>
          <Link className="button button--ghost" to="/catalog">
            Browse catalog
          </Link>
        </Panel>
      ) : (
        <div className="update-stack">
          {installedProducts.map(({ installed, product }) => {
            const updateAvailable = hasUpdate(product, installed);

            return (
              <Panel key={product.id} className="update-row">
                <div>
                  <div className="update-row__heading">
                    <h2>{product.displayName}</h2>
                    <Badge tone={updateAvailable ? "warning" : "success"}>
                      {updateAvailable ? "Update available" : "Current"}
                    </Badge>
                  </div>
                  <p>{product.summary}</p>
                  <div className="update-row__meta">
                    <span>Installed {formatInstalledDate(installed.installedAt)}</span>
                    <span>v{installed.version}</span>
                    <span>{installed.installPath}</span>
                  </div>
                </div>
                <div className="update-row__actions">
                  <Button
                    disabled={!updateAvailable || busyAction === `update:${product.id}`}
                    onClick={() => void updateProduct(product.id)}
                  >
                    {busyAction === `update:${product.id}` ? "Updating..." : "Update"}
                  </Button>
                  <Button
                    disabled={busyAction === `uninstall:${product.id}`}
                    onClick={() => void uninstallProduct(product.id)}
                    tone="danger"
                  >
                    {busyAction === `uninstall:${product.id}` ? "Removing..." : "Uninstall"}
                  </Button>
                </div>
              </Panel>
            );
          })}
        </div>
      )}
    </div>
  );
}
