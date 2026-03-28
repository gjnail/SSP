import { useHub } from "../../app/HubProvider";
import { ProductCard } from "../../components/catalog/ProductCard";
import { Panel } from "../../components/ui/Panel";
import { isEntitled } from "../../lib/catalog";

export function LibraryPage() {
  const { activateProduct, buildInstaller, busyAction, deactivateProduct, installProduct, snapshot, uninstallProduct, updateProduct } = useHub();

  if (!snapshot) {
    return null;
  }

  const ownedProducts = snapshot.catalog.filter((product) => isEntitled(snapshot, product.id));

  return (
    <div className="page page--stack">
      <div className="section-heading">
        <div>
          <span className="section-heading__eyebrow">Library</span>
          <h1>Plugins attached to {snapshot.account.email}</h1>
          <p>Your account-controlled install surface for Windows and macOS builds.</p>
        </div>
      </div>

      {ownedProducts.length === 0 ? (
        <Panel>
          <h2>No entitlements found</h2>
          <p>Switch demo accounts in Settings to preview other bundle states.</p>
        </Panel>
      ) : (
        <div className="product-grid">
          {ownedProducts.map((product) => (
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
      )}
    </div>
  );
}
