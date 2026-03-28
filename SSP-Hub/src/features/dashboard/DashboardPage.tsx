import { Link } from "react-router-dom";

import { useHub } from "../../app/HubProvider";
import { ProductCard } from "../../components/catalog/ProductCard";
import { MetricCard } from "../../components/ui/MetricCard";
import { Panel } from "../../components/ui/Panel";
import { getArtifactForCurrentOs, getInstalledPlugin, getLicenseStatus, hasUpdate, isEntitled } from "../../lib/catalog";

export function DashboardPage() {
  const { activateProduct, buildAllWindowsInstallers, buildInstaller, busyAction, deactivateProduct, installProduct, snapshot, uninstallProduct, updateProduct } = useHub();

  if (!snapshot) {
    return null;
  }

  const ownedProducts = snapshot.catalog.filter((product) => isEntitled(snapshot, product.id));
  const activatedProducts = ownedProducts.filter((product) => getLicenseStatus(snapshot, product.id)?.isActivated).length;
  const readyToInstall = ownedProducts.filter((product) => getArtifactForCurrentOs(snapshot, product)).length;
  const updates = snapshot.catalog.filter((product) =>
    hasUpdate(product, getInstalledPlugin(snapshot, product.id)),
  ).length;

  return (
    <div className="page page--stack">
      <Panel className="hero-panel">
        <div className="hero-panel__copy">
          <span className="hero-panel__eyebrow">Desktop delivery and activation</span>
          <h1>Install, unlock, update, and manage the SSP catalog from one place.</h1>
          <p>
            SSP Hub reads the plugins attached to your account, surfaces the installers available for your
            current operating system, and keeps your custom VST3 target path in one settings profile.
          </p>
          <div className="hero-panel__actions">
            <Link className="button button--accent" to="/catalog">
              Browse catalog
            </Link>
            <Link className="button button--ghost" to="/settings">
              Open settings
            </Link>
            {snapshot.os === "windows" ? (
              <button
                className="button button--secondary"
                disabled={busyAction === "build-all-installers"}
                onClick={() => void buildAllWindowsInstallers()}
                type="button"
              >
                {busyAction === "build-all-installers" ? "Building all..." : "Build all installers"}
              </button>
            ) : null}
          </div>
        </div>

        <div className="hero-panel__metrics">
          <MetricCard detail="Visible to this signed-in account" label="Owned plugins" value={`${ownedProducts.length}`} />
          <MetricCard detail="Installers staged for this OS" label="Ready to install" value={`${readyToInstall}`} />
          <MetricCard detail="Local licenses cached in your selected folder" label="Activated plugins" value={`${activatedProducts}`} />
          <MetricCard detail="Plugins needing a version refresh" label="Updates waiting" value={`${updates}`} />
        </div>
      </Panel>

      <section className="section-block">
        <div className="section-heading">
          <div>
            <span className="section-heading__eyebrow">Owned catalog</span>
            <h2>Start with the tools already unlocked for this account.</h2>
          </div>
          <Link className="section-heading__link" to="/library">
            Go to library
          </Link>
        </div>

        <div className="product-grid">
          {ownedProducts.slice(0, 3).map((product) => (
            <ProductCard
              key={product.id}
              onActivate={activateProduct}
              busyAction={busyAction}
              onBuildInstaller={buildInstaller}
              onDeactivate={deactivateProduct}
              onInstall={installProduct}
              onUninstall={uninstallProduct}
              onUpdate={updateProduct}
              product={product}
              snapshot={snapshot}
            />
          ))}
        </div>
      </section>
    </div>
  );
}
