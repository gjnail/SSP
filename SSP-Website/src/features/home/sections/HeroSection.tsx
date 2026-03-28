import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { BrandLogo } from "@/features/brand/BrandLogo";
import { heroStats, products, stageTags } from "@/features/home/content";

export function HeroSection() {
  const availableCount = products.filter((product) => product.status === "available").length;
  const conceptCount = products.filter((product) => product.status === "coming-soon").length;

  return (
    <section className="hero-section" id="top">
      <div className="container hero-section__grid">
        <div className="hero-copy">
          <Badge>SSP plugin storefront</Badge>
          <p className="hero-copy__eyebrow">Focused mix tools / digital delivery flow / 2026</p>
          <h1>Simple plugins for sidechain, tone, space, analysis, and fast mix decisions.</h1>
          <p>
            The SSP site now covers the full active plugin lineup and introduces SSP Hub, the
            desktop app built to handle installs, updates, and catalog access from one place. You
            can browse products, compare them, and walk the storefront flow without losing the
            sharper SSP visual language.
          </p>

          <div className="hero-copy__actions">
            <Button to="/products">Shop Plugins</Button>
            <Button to="/hub" variant="secondary">
              Explore Hub
            </Button>
          </div>

          <div className="hero-stage__tags" aria-label="Signature product language">
            {stageTags.map((tag) => (
              <span key={tag} className="hero-stage__tag">
                {tag}
              </span>
            ))}
          </div>

          <div className="hero-stats" aria-label="Key SSP stats">
            {heroStats.map((stat) => (
              <div key={stat.label} className="hero-stats__item">
                <strong>{stat.value}</strong>
                <span>{stat.label}</span>
              </div>
            ))}
          </div>
        </div>

        <div className="hero-stage">
          <div className="hero-stage__panel">
            <div className="hero-stage__topline">
              <span>SSP / CURRENT LINEUP</span>
              <span>
                {availableCount.toString().padStart(2, "0")} ACTIVE / {conceptCount.toString().padStart(2, "0")} CONCEPT
              </span>
            </div>
            <BrandLogo className="hero-stage__logo" showWordmark={false} />
            <div className="hero-stage__rows">
              {products.slice(0, 6).map((product) => (
                <article key={product.slug} className="hero-stage__row" data-tone={product.tone}>
                  <span className="hero-stage__row-name">{product.name}</span>
                  <strong>{product.status === "available" ? `$${product.priceUsd}` : "Coming Soon"}</strong>
                  <span>{product.bestFor}</span>
                </article>
              ))}
            </div>
            <p className="hero-stage__caption">
              The shelf preview now highlights the front edge of the active SSP lineup while the
              full catalog lives one click deeper on the plugins page.
            </p>
          </div>
        </div>
      </div>
    </section>
  );
}
