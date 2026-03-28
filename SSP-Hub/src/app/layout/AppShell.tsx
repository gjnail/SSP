import type { PropsWithChildren } from "react";

import { NavLink } from "react-router-dom";

import { BrandLogo } from "../../components/brand/BrandLogo";
import { Badge } from "../../components/ui/Badge";
import { useHub } from "../HubProvider";

const NAV_ITEMS = [
  { label: "Dashboard", to: "/" },
  { label: "Catalog", to: "/catalog" },
  { label: "Library", to: "/library" },
  { label: "Updates", to: "/updates" },
  { label: "Settings", to: "/settings" },
];

export function AppShell({ children }: PropsWithChildren) {
  const { errorMessage, loading, saveSettings, snapshot } = useHub();

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="sidebar__brand">
          <BrandLogo />
          <p>Installer, activation, updates, and uninstall control for the SSP catalog.</p>
        </div>

        <nav className="sidebar__nav">
          {NAV_ITEMS.map((item) => (
            <NavLink
              key={item.to}
              className={({ isActive }) => `sidebar__link ${isActive ? "sidebar__link--active" : ""}`.trim()}
              end={item.to === "/"}
              to={item.to}
            >
              {item.label}
            </NavLink>
          ))}
        </nav>

        <div className="sidebar__foot">
          <Badge tone="accent">{snapshot?.os ?? "loading"}</Badge>
          <Badge tone="muted">{snapshot?.account.planLabel ?? "Account"}</Badge>
        </div>
      </aside>

      <main className="workspace">
        <header className="topbar">
          <div className="topbar__account">
            <span className="topbar__label">Connected account</span>
            <strong>{snapshot?.account.email ?? "Loading..."}</strong>
          </div>
          <div className="topbar__status">
            <button
              className="theme-toggle"
              onClick={() =>
                snapshot
                  ? void saveSettings({
                      themeMode: snapshot.settings.themeMode === "dark" ? "light" : "dark",
                    })
                  : undefined
              }
              type="button"
            >
              {snapshot?.settings.themeMode === "dark" ? "Light mode" : "Dark mode"}
            </button>
            <Badge tone={loading ? "warning" : "success"}>{loading ? "Syncing" : "Ready"}</Badge>
          </div>
        </header>

        {errorMessage ? <div className="app-alert">{errorMessage}</div> : null}

        <div className="workspace__content">{children}</div>
      </main>
    </div>
  );
}
