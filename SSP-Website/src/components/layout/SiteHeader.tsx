import { NavLink, Link } from "react-router-dom";
import { Button } from "@/components/ui/Button";
import { BrandLogo } from "@/features/brand/BrandLogo";
import { navItems } from "@/features/home/content";
import { useStorefront } from "@/features/storefront/StorefrontProvider";

export function SiteHeader() {
  const { itemCount } = useStorefront();

  return (
    <header className="site-header">
      <div className="container site-header__inner">
        <Link className="site-header__brand" to="/" aria-label="Stupid Simple Plugins home">
          <BrandLogo showWordmark={false} />
          <div className="site-header__brand-copy">
            <span>Stupid Simple Plugins</span>
            <strong>Focused audio tools and desktop delivery</strong>
          </div>
        </Link>

        <nav className="site-header__nav" aria-label="Primary navigation">
          {navItems.map((item) => (
            <NavLink
              key={item.to}
              className={({ isActive }) => (isActive ? "is-active" : undefined)}
              to={item.to}
            >
              {item.label}
            </NavLink>
          ))}
        </nav>

        <div className="site-header__meta">
          <span>Full plugin lineup plus SSP Hub pages</span>
          <Button className="site-header__cta" to="/cart" variant="primary">
            Cart ({itemCount})
          </Button>
        </div>
      </div>
    </header>
  );
}
