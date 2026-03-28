import { Link } from "react-router-dom";

export function HubPage() {
  return (
    <section className="section page-shell">
      <div className="container page-stack">
        <div className="page-section-heading">
          <p className="page-eyebrow">SSP Hub</p>
          <h1>The desktop control room for the SSP plugin catalog.</h1>
          <p>
            SSP Hub is the launcher app that ties the ecosystem together: browse the catalog tied
            to an account, manage installs, keep updates visible, and point everything at the right
            plugin paths from one desktop app.
          </p>
        </div>

        <div className="hub-hero">
          <div className="hub-hero__card">
            <span className="hub-hero__label">What It Handles</span>
            <h2>Catalog, installs, activation state, and updates in one place.</h2>
            <p>
              Instead of treating downloads as an afterthought, SSP Hub turns the delivery layer
              into part of the product family. It is meant to be the one place where customers see
              what they own, what is ready to install, and what needs an update.
            </p>
            <div className="hub-actions">
              <Link className="button button--primary" to="/hub/download">
                Go To Downloads
              </Link>
              <Link className="button button--secondary" to="/products">
                Browse Plugins
              </Link>
            </div>
          </div>

          <div className="hub-panel">
            <span className="hub-panel__label">Inside The App</span>
            <div className="hub-panel__list">
              <article>
                <strong>Dashboard</strong>
                <p>Owned plugins, ready-to-install builds, activation counts, and update status.</p>
              </article>
              <article>
                <strong>Catalog</strong>
                <p>Account-aware product browser that matches the SSP release shelf.</p>
              </article>
              <article>
                <strong>Library</strong>
                <p>One place to manage installed products and see which versions are already on the machine.</p>
              </article>
              <article>
                <strong>Settings</strong>
                <p>Target plugin paths and platform-specific install behavior kept in one desktop profile.</p>
              </article>
            </div>
          </div>
        </div>

        <div className="info-grid">
          <article className="info-card">
            <h2>Built for the whole lineup</h2>
            <p>
              The Hub is not tied to one plugin. It is the delivery layer for sidechain tools,
              dynamics processors, analyzers, creative effects, and future SSP releases.
            </p>
          </article>
          <article className="info-card">
            <h2>Cleaner onboarding</h2>
            <p>
              New users should not need to hunt through folders and installers. Hub gives the SSP
              ecosystem a more complete onboarding story than raw download links alone.
            </p>
          </article>
          <article className="info-card">
            <h2>Ready for hosted downloads</h2>
            <p>
              The app already frames the right concepts for real distribution later: platform
              packages, owned products, update checks, and library management.
            </p>
          </article>
          <article className="info-card">
            <h2>Matches the storefront</h2>
            <p>
              The website now introduces the product family and the Hub together, so the user
              journey feels like one system instead of separate unrelated tools.
            </p>
          </article>
        </div>
      </div>
    </section>
  );
}
