import { Link } from "react-router-dom";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { useStorefront } from "@/features/storefront/StorefrontProvider";
import type { Product } from "@/features/home/content";

type StoreProductCardProps = {
  product: Product;
};

function formatPrice(priceUsd: number) {
  return `$${priceUsd}`;
}

export function StoreProductCard({ product }: StoreProductCardProps) {
  const { addToCart, decrementItem, getQuantity, incrementItem } = useStorefront();
  const quantity = getQuantity(product.slug);
  const isAvailable = product.status === "available";

  return (
    <article className="store-card" data-tone={product.tone} data-status={product.status}>
      <div className="store-card__media">
        <img alt={`${product.name} temporary cover art`} src={product.image} />
      </div>

      <div className="store-card__content">
        <div className="store-card__topline">
          <Badge className="store-card__badge">{product.category}</Badge>
          <span className={`store-card__status store-card__status--${product.status}`}>
            {product.status === "available" ? "Available" : "Coming Soon"}
          </span>
        </div>

        <div className="store-card__headline">
          <div>
            <h3>{product.name}</h3>
            <p>{product.strapline}</p>
          </div>
          <strong>{isAvailable ? formatPrice(product.priceUsd) : "TBD"}</strong>
        </div>

        <p className="store-card__summary">{product.summary}</p>

        <div className="store-card__grid">
          <div>
            <span className="store-card__label">Best for</span>
            <strong>{product.bestFor}</strong>
          </div>
          <div>
            <span className="store-card__label">Trigger</span>
            <strong>{product.triggerModes.join(" / ")}</strong>
          </div>
          <div>
            <span className="store-card__label">Formats</span>
            <strong>{product.formats.join(" / ")}</strong>
          </div>
          <div>
            <span className="store-card__label">Platforms</span>
            <strong>{product.platforms.join(" / ")}</strong>
          </div>
        </div>

        <ul className="store-card__list">
          {product.highlights.map((highlight) => (
            <li key={highlight}>{highlight}</li>
          ))}
        </ul>

        <p className="store-card__note">{product.releaseNote}</p>

        {isAvailable ? (
          <div className="store-card__actions">
            <Button to={`/products/${product.slug}`} variant="ghost">
              View Details
            </Button>
            {quantity > 0 ? (
              <div className="store-card__quantity">
                <button aria-label={`Decrease ${product.name} quantity`} onClick={() => decrementItem(product.slug)}>
                  -
                </button>
                <span>{quantity} in cart</span>
                <button aria-label={`Increase ${product.name} quantity`} onClick={() => incrementItem(product.slug)}>
                  +
                </button>
              </div>
            ) : null}

            <Button onClick={() => addToCart(product.slug)}>
              {quantity > 0 ? "Add Another" : "Add To Cart"}
            </Button>
          </div>
        ) : (
          <div className="store-card__actions">
            <Link className="button button--ghost" to={`/products/${product.slug}`}>
              View Product
            </Link>
            <Button disabled variant="ghost">
              Coming Soon
            </Button>
          </div>
        )}
      </div>
    </article>
  );
}
