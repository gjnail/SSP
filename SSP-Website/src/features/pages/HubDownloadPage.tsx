import { Link } from "react-router-dom";

export function HubDownloadPage() {
  return (
    <section className="section page-shell">
      <div className="container page-stack">
        <div className="page-section-heading">
          <p className="page-eyebrow">Hub Download</p>
          <h1>Download SSP Hub and manage the SSP catalog from one desktop app.</h1>
          <p>
            This page positions the Hub as the desktop entry point for installs, updates, account
            access, and plugin path management. It is the place where platform downloads and release
            notes can live once hosted builds are published.
          </p>
        </div>

        <div className="download-grid">
          <article className="download-card download-card--featured">
            <span className="download-card__eyebrow">Recommended</span>
            <h2>Windows Desktop Build</h2>
            <p>
              The current Hub app is designed first around desktop install flow, catalog syncing,
              update visibility, and managing where SSP products land on the machine.
            </p>
            <ul className="download-card__list">
              <li>Catalog, library, updates, and settings pages already scoped in the app.</li>
              <li>Designed to surface owned products and platform installers from one dashboard.</li>
              <li>Natural home for future activation and release delivery.</li>
            </ul>
            <div className="hub-actions">
              <Link className="button button--primary" to="/contact">
                Request Windows Build
              </Link>
              <Link className="button button--ghost" to="/hub">
                Learn About Hub
              </Link>
            </div>
          </article>

          <article className="download-card">
            <span className="download-card__eyebrow">Planned</span>
            <h2>macOS Packaging</h2>
            <p>
              The Hub project is already set up with cross-platform packaging intent, so this page
              is ready to present a macOS build once that release is staged.
            </p>
            <ul className="download-card__list">
              <li>Same account-aware catalog and install concept as the Windows release.</li>
              <li>Positioned for platform-specific packaging when the hosted artifacts are ready.</li>
              <li>Lets the website present both platforms in the same product story.</li>
            </ul>
            <div className="hub-actions">
              <Link className="button button--secondary" to="/contact">
                Ask About macOS
              </Link>
            </div>
          </article>
        </div>

        <div className="info-grid">
          <article className="info-card">
            <h2>Why a desktop hub?</h2>
            <p>
              Because once the product count grows, plain download links become a weak experience.
              Hub keeps installs, owned products, and updates organized in one place.
            </p>
          </article>
          <article className="info-card">
            <h2>What this page is for</h2>
            <p>
              It gives the website a dedicated destination for platform downloads, release notes,
              system requirements, and installer-related calls to action.
            </p>
          </article>
          <article className="info-card">
            <h2>How it connects to the store</h2>
            <p>
              The store sells the tools. Hub is where customers can eventually claim, install, and
              maintain them without bouncing around a pile of separate links.
            </p>
          </article>
          <article className="info-card">
            <h2>What comes next</h2>
            <p>
              Once hosted packages are staged, this page can swap the request buttons for direct
              platform downloads without changing the overall layout.
            </p>
          </article>
        </div>
      </div>
    </section>
  );
}
