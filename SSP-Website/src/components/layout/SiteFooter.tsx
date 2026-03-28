import { Link, NavLink } from "react-router-dom";
import { BrandLogo } from "@/features/brand/BrandLogo";
import { navItems } from "@/features/home/content";

export function SiteFooter() {
  return (
    <footer className="site-footer" id="contact">
      <div className="container site-footer__grid">
        <div className="site-footer__cta">
          <span className="site-footer__label">Launch Pad</span>
          <h2>The site can now sell the plugins and explain the Hub without pretending the delivery layer does not matter.</h2>
          <p>
            The storefront already has the product catalog, cart state, and checkout rhythm, and
            the new Hub pages give the download and installer story a clear home before hosted
            releases go live.
          </p>
          <div className="site-footer__actions">
            <Link className="button button--primary" to="/products">
              Browse Plugins
            </Link>
            <Link className="button button--ghost" to="/hub/download">
              Hub Downloads
            </Link>
          </div>
        </div>

        <div className="site-footer__meta">
          <BrandLogo className="site-footer__logo" />
          <p>
            SSP is shaping up as a focused plugin family with a matching desktop hub for installs,
            updates, and library management.
          </p>
          <nav aria-label="Footer navigation">
            {navItems.map((item) => (
              <NavLink key={item.to} to={item.to}>
                {item.label}
              </NavLink>
            ))}
          </nav>
          <span className="site-footer__copyright">
            Copyright {new Date().getFullYear()} Stupid Simple Plugins
          </span>
        </div>
      </div>
    </footer>
  );
}
