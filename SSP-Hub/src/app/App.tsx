import { HashRouter, Navigate, Route, Routes } from "react-router-dom";

import { CatalogPage } from "../features/catalog/CatalogPage";
import { ProductDetailPage } from "../features/catalog/ProductDetailPage";
import { DashboardPage } from "../features/dashboard/DashboardPage";
import { LibraryPage } from "../features/library/LibraryPage";
import { SettingsPage } from "../features/settings/SettingsPage";
import { UpdatesPage } from "../features/updates/UpdatesPage";
import { HubProvider, useHub } from "./HubProvider";
import { AppShell } from "./layout/AppShell";

function LoadingScreen() {
  return (
    <div className="loading-screen">
      <div className="loading-screen__panel">
        <span className="loading-screen__eyebrow">SSP Hub</span>
        <h1>Syncing your catalog</h1>
        <p>Checking account access, local state, and staged installers.</p>
      </div>
    </div>
  );
}

function AppRoutes() {
  const { loading, snapshot } = useHub();

  if (loading && !snapshot) {
    return <LoadingScreen />;
  }

  return (
    <AppShell>
      <Routes>
        <Route element={<DashboardPage />} path="/" />
        <Route element={<CatalogPage />} path="/catalog" />
        <Route element={<ProductDetailPage />} path="/products/:slug" />
        <Route element={<LibraryPage />} path="/library" />
        <Route element={<UpdatesPage />} path="/updates" />
        <Route element={<SettingsPage />} path="/settings" />
        <Route element={<Navigate replace to="/" />} path="*" />
      </Routes>
    </AppShell>
  );
}

export function App() {
  return (
    <HubProvider>
      <HashRouter>
        <AppRoutes />
      </HashRouter>
    </HubProvider>
  );
}
