import { useHub } from "../../app/HubProvider";
import { ProductCard } from "../../components/catalog/ProductCard";

export function CatalogPage() {
  const { activateProduct, buildInstaller, busyAction, deactivateProduct, installProduct, snapshot, uninstallProduct, updateProduct } = useHub();

  if (!snapshot) {
    return null;
  }

  return (
    <div className="page page--stack">
      <div className="section-heading">
        <div>
          <span className="section-heading__eyebrow">Full catalog</span>
          <h1>Every SSP plugin detected from your workspace.</h1>
          <p>
            Products unlock when the signed-in account owns them. Install actions are only enabled when a
            platform-specific package is staged in your Installers folder.
          </p>
        </div>
      </div>

      <div className="product-grid">
        {snapshot.catalog.map((product) => (
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
    </div>
  );
}
