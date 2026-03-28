import { Link } from "react-router-dom";
import { CartPanel } from "@/features/storefront/components/CartPanel";

export function CartPage() {
  return (
    <section className="section page-shell">
      <div className="container page-stack">
        <div className="page-section-heading">
          <p className="page-eyebrow">Cart</p>
          <h1>Review your order and walk through the dummy checkout.</h1>
          <p>
            This page is meant to feel more like a conventional plugin storefront checkout step,
            while still staying safe for testing.
          </p>
        </div>

        <div className="cart-page__layout">
          <CartPanel />
          <div className="cart-page__notes">
            <article>
              <h2>Digital delivery</h2>
              <p>Every available product is framed as an instant digital plugin purchase with standalone and VST3 positioning where applicable.</p>
            </article>
            <article>
              <h2>Dummy payment only</h2>
              <p>This form does not charge anything. It is only here to model the checkout flow and order confirmation state.</p>
            </article>
            <article>
              <h2>Need more products?</h2>
              <p>
                Go back to the <Link to="/products">plugin catalog</Link> or browse individual
                product pages for more detail.
              </p>
            </article>
          </div>
        </div>
      </div>
    </section>
  );
}
