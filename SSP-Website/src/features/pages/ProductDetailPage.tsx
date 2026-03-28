import { Link, Navigate, useParams } from "react-router-dom";
import { Button } from "@/components/ui/Button";
import { products } from "@/features/home/content";
import { useStorefront } from "@/features/storefront/StorefrontProvider";

export function ProductDetailPage() {
  const { slug } = useParams();
  const { addToCart, getQuantity } = useStorefront();
  const product = products.find((entry) => entry.slug === slug);

  if (!product) {
    return <Navigate replace to="/products" />;
  }

  const quantity = getQuantity(product.slug);
  const related = products
    .filter((entry) => entry.slug !== product.slug && entry.category === product.category)
    .slice(0, 2);

  return (
    <section className="section product-page">
      <div className="container product-page__grid">
        <div className="product-page__hero">
          <img alt={`${product.name} temporary product art`} src={product.image} />
        </div>

        <div className="product-page__content">
          <p className="page-eyebrow">Product Detail</p>
          <h1>{product.name}</h1>
          <p className="product-page__strapline">{product.strapline}</p>
          <p className="product-page__summary">{product.summary}</p>

          <div className="product-page__purchase">
            <div>
              <span className="product-page__label">Price</span>
              <strong>{product.status === "available" ? `$${product.priceUsd}` : "Coming Soon"}</strong>
            </div>
            <div>
              <span className="product-page__label">Best for</span>
              <strong>{product.bestFor}</strong>
            </div>
          </div>

          <div className="product-page__actions">
            {product.status === "available" ? (
              <Button onClick={() => addToCart(product.slug)}>
                {quantity > 0 ? `Add Another (${quantity} in cart)` : "Add To Cart"}
              </Button>
            ) : (
              <Button disabled>Coming Soon</Button>
            )}
            <Button to="/cart" variant="secondary">
              View Cart
            </Button>
          </div>

          <div className="product-page__facts">
            <article>
              <span>Formats</span>
              <strong>{product.formats.join(" / ")}</strong>
            </article>
            <article>
              <span>Platforms</span>
              <strong>{product.platforms.join(" / ")}</strong>
            </article>
            <article>
              <span>Controls</span>
              <strong>{product.triggerModes.join(" / ")}</strong>
            </article>
            <article>
              <span>Status</span>
              <strong>{product.status === "available" ? "Available now" : "Coming soon"}</strong>
            </article>
          </div>

          <div className="product-page__details">
            <div>
              <h2>Highlights</h2>
              <ul>
                {product.highlights.map((highlight) => (
                  <li key={highlight}>{highlight}</li>
                ))}
              </ul>
            </div>
            <div>
              <h2>Release Note</h2>
              <p>{product.releaseNote}</p>
            </div>
          </div>
        </div>
      </div>

      {related.length > 0 ? (
        <div className="container product-page__related">
          <div className="page-section-heading">
            <p className="page-eyebrow">Related Plugins</p>
            <h2>More tools in the same lane.</h2>
          </div>
          <div className="product-page__related-grid">
            {related.map((entry) => (
              <Link key={entry.slug} className="related-product" to={`/products/${entry.slug}`}>
                <img alt={`${entry.name} cover`} src={entry.image} />
                <div>
                  <strong>{entry.name}</strong>
                  <span>{entry.bestFor}</span>
                </div>
              </Link>
            ))}
          </div>
        </div>
      ) : null}
    </section>
  );
}
